param([Parameter(Mandatory=$true)][string]$Python)
$ErrorActionPreference='Stop'
$native=$PSScriptRoot
$destination=Join-Path $native ('publish/source-verify-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $destination | Out-Null
. (Join-Path $native 'source-inputs.ps1')
$inputs=@(Get-SourceInputs $native)
$records=@($inputs | Sort-Object FullName -Unique | ForEach-Object {
 $relative=[IO.Path]::GetRelativePath($native,$_.FullName)
 $target=Join-Path $destination $relative
 New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
 Copy-Item -LiteralPath $_.FullName -Destination $target
 $hash=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()
 if((Get-FileHash -LiteralPath $target).Hash.ToLowerInvariant() -ne $hash){throw "Copy hash mismatch: $relative"}
 [ordered]@{path=$relative;sha256=$hash}
})
foreach($directory in @('components','tests/deps')){
 $source=Join-Path $native $directory
 if(Get-ChildItem -LiteralPath $source -Recurse -Attributes ReparsePoint){throw "Verification dependency contains links: $directory"}
 Copy-Item -LiteralPath $source -Destination (Join-Path $destination $directory) -Recurse
}
New-Item -ItemType Directory -Force (Join-Path $destination 'evidence') | Out-Null
function Run-Binary([string]$Name,[string[]]$Arguments=@(),[int]$Timeout=15000){
 $binary=Join-Path $destination ('bin/'+$Name+'.exe')
 $settings=[Diagnostics.ProcessStartInfo]::new();$settings.FileName=$binary;$settings.UseShellExecute=$false;$settings.CreateNoWindow=$true
 foreach($argument in $Arguments){$settings.ArgumentList.Add($argument)}
 $process=[Diagnostics.Process]::Start($settings)
 if(!$process.WaitForExit($Timeout)){throw "Verification remains live: $Name PID $($process.Id); inspect before rerunning."}
 if($process.ExitCode -ne 0){throw "Verification failed: $Name exit $($process.ExitCode); inspect $destination/evidence"}
}
& (Join-Path $destination 'tests/run-source-input-tests.ps1')
& (Join-Path $destination 'build.ps1')
Run-Binary 'PuTTYHub' @('--credential-lock-test')
Run-Binary 'PuTTYHub' @('--layout-test')
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -Frontend conemu
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -Frontend fallback
foreach($frontend in @('modern','conemu','fallback')){& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -Frontend $frontend -Blackhole}
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -PendingAuth
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -Frontend conemu -PendingAuth
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -Frontend fallback -PendingAuth
& (Join-Path $destination 'tests/run-app-tests.ps1') -Python $Python -Frontend putty
& (Join-Path $destination 'tests/run-native-close-tests.ps1') -Python $Python
& (Join-Path $destination 'tests/run-native-close-tests.ps1') -Python $Python -Tool winscp
& (Join-Path $destination 'build.ps1') -PtyTest
& (Join-Path $destination 'tests/run-pty-tests.ps1') -Python $Python
& (Join-Path $destination 'build.ps1') -CoreTest
& (Join-Path $destination 'tests/run-migration-tests.ps1')
& (Join-Path $destination 'build.ps1') -PipeTest
Run-Binary 'pipe-tests'
Run-Binary 'pipe-tests' @('--timeout') 70000
& (Join-Path $destination 'build.ps1') -TerminalInputTest
Run-Binary 'terminal-input-tests'
$requiredReports=@('app-conemu-integration.json','app-fallback-integration.json','app-blackhole.json','app-conemu-blackhole.json','app-fallback-blackhole.json','core-results.json','pty-results.json','app-integration.json','app-native-putty-integration.json','app-pending-auth.json','app-conemu-pending-auth.json','app-fallback-pending-auth.json','native-close-results.json','winscp-close-results.json','credential-lock-results.json','credential-manager-results.json','credential-actions-results.json','group-results.json','pipe-results.json','pipe-timeout-results.json','terminal-input-results.json','ui-layout-results.json','source-input-results.json')
foreach($name in $requiredReports){$report=Get-Content -LiteralPath (Join-Path $destination ('evidence/'+$name)) -Raw | ConvertFrom-Json;if($report.status -ne 'passed'){throw "Missing or unsuccessful verification report: $name"}}
Assert-SourceInputs $native $records
Assert-SourceInputs $destination $records
$core=Get-Content -LiteralPath (Join-Path $destination 'evidence/core-results.json') -Raw | ConvertFrom-Json
$binaries=@(foreach($name in @('PuTTYHub.exe','PlinkResizeBridge.dll')){[ordered]@{name=$name;sha256=(Get-FileHash -LiteralPath (Join-Path $destination ('bin/'+$name))).Hash.ToLowerInvariant()}})
$proof=[ordered]@{status='passed';verifiedAt=[DateTime]::UtcNow.ToString('O');scope='Independent copied-source builds and listed runtime checks; not full migration or manual UI acceptance';verificationDirectory=$destination;inputFilesMatched=$records.Count;binaries=$binaries;runtimeComponents='Project-local fixed components';pythonDependencies='Project-local tests/deps';checks=@("$($core.count) core and actual C# bidirectional interoperability checks",'Rebuilt application SSH/WinSCP and native PuTTY authentication','Credential lock/actions, group persistence and DPI geometry','Password pipe cancellation and real default timeout','Native terminal input and DPI API','Real Plink native profile, tabbed CJK/emoji UTF-8 input and remote PTY resize','Pending authentication cancellation in modern, ConEmu and automatic fallback','PuTTY and WinSCP authentication after Hub window closes');inputs=$records}
$proof | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $native 'evidence/source-verification.json')
$proof | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'evidence/source-verification.json')
Write-Output "Independent source verification passed: $destination"
