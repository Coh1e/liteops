$ErrorActionPreference='Stop'
$native=$PSScriptRoot
$plan=Join-Path $native 'CPP-MIGRATION-PLAN.md'
$output=Join-Path $native 'publish'
New-Item -ItemType Directory -Force $output | Out-Null
$proof=Get-Content -LiteralPath (Join-Path $native 'evidence/source-verification.json') -Raw | ConvertFrom-Json
if($proof.status -ne 'passed' -or !$proof.inputs){throw 'Independent source verification with input hashes is required before packaging.'}
. (Join-Path $native 'source-inputs.ps1')
Assert-SourceInputs $native $proof.inputs
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$stage=Join-Path $output ('native-stage-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $stage | Out-Null
$portable=Join-Path $stage 'portable'
$source=Join-Path $stage 'source'
New-Item -ItemType Directory $portable,$source | Out-Null
New-Item -ItemType Directory (Join-Path $portable 'bin') | Out-Null
if(!$proof.binaries -or @($proof.binaries).Count -ne 2){throw 'Verified application binary hashes are required before packaging.'}
foreach($name in @('PuTTYHub.exe','PlinkResizeBridge.dll')){
 $record=@($proof.binaries | Where-Object {$_.name -eq $name})
 if($record.Count -ne 1){throw "Missing verified binary: $name"}
 $binary=Join-Path $proof.verificationDirectory ('bin/'+$name)
 if((Get-FileHash -LiteralPath $binary).Hash.ToLowerInvariant() -ne $record[0].sha256){throw "Verified binary changed: $name"}
 Copy-Item -LiteralPath $binary -Destination (Join-Path $portable 'bin')
}
foreach($name in @('components','licenses')){
 $path=Join-Path $native $name
 if(Get-ChildItem -LiteralPath $path -Recurse -Attributes ReparsePoint){throw 'Component tree contains a link; inspect before packaging.'}
 Copy-Item -LiteralPath $path -Destination $portable -Recurse
}
foreach($name in @('src','include','vendor','licenses')){Copy-Item -LiteralPath (Join-Path $native $name) -Destination $source -Recurse}
New-Item -ItemType Directory (Join-Path $source 'tests') | Out-Null
Get-ChildItem -LiteralPath (Join-Path $native 'tests') -File | Where-Object {$_.Extension -in '.ps1','.py','.cs','.txt'} | Copy-Item -Destination (Join-Path $source 'tests')
Copy-Item -LiteralPath (Join-Path $native 'tests/legacy-source') -Destination (Join-Path $source 'tests') -Recurse
foreach($name in @('build.ps1','package.ps1','verify-source.ps1','source-inputs.ps1')){Copy-Item -LiteralPath (Join-Path $native $name) -Destination $source}
foreach($target in @($portable,$source)){
 Get-ChildItem -LiteralPath $native -Filter '*.md' -File | Copy-Item -Destination $target
 Copy-Item -LiteralPath $plan -Destination $target
 New-Item -ItemType Directory (Join-Path $target 'evidence') | Out-Null
 foreach($name in @('core-results.json','pty-results.json','app-integration.json','app-conemu-integration.json','app-fallback-integration.json','component-results.json','app-agent-integration.json','app-native-putty-integration.json','terminal-input-results.json','ui-layout-results.json','writeback-crash-results.json','pipe-results.json','pipe-timeout-results.json','credential-lock-results.json','credential-manager-results.json','credential-actions-results.json','group-results.json','native-close-results.json','winscp-close-results.json','app-blackhole.json','app-conemu-blackhole.json','app-fallback-blackhole.json','app-pending-auth.json','app-conemu-pending-auth.json','app-fallback-pending-auth.json','app-sharing-fault.json','app-conemu-sharing-fault.json','app-fallback-sharing-fault.json','app-server-loss.json','app-conemu-server-loss.json','app-fallback-server-loss.json','source-input-results.json','source-verification.json')){Copy-Item -LiteralPath (Join-Path $native ('evidence/'+$name)) -Destination (Join-Path $target 'evidence')}
 $files=@(Get-ChildItem -LiteralPath $target -File -Recurse | ForEach-Object {[ordered]@{path=[IO.Path]::GetRelativePath($target,$_.FullName);bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
 [ordered]@{status='development-preview';builtAt=[DateTime]::UtcNow.ToString('O');files=$files} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $target 'manifest.json')
 if((Test-Path (Join-Path $target 'data')) -or (Test-Path (Join-Path $target 'runtime'))){throw 'User data must not be packaged.'}
}
$archives=@()
foreach($variant in @('portable','source')){
 $archive=Join-Path $output ('PuTTYHub-native-preview-'+$variant+'-'+$stamp+'.zip')
 Compress-Archive -Path (Join-Path (Join-Path $stage $variant) '*') -DestinationPath $archive -CompressionLevel Optimal
 $archives+=[ordered]@{kind=$variant;path=$archive;sha256=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()}
}
$verification=Join-Path $output ('native-verify-'+[Guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath $archives[0].path -DestinationPath $verification
$manifest=Get-Content (Join-Path $verification 'manifest.json') -Raw | ConvertFrom-Json
foreach($file in $manifest.files){if((Get-FileHash -LiteralPath (Join-Path $verification $file.path)).Hash.ToLowerInvariant() -ne $file.sha256){throw 'Extracted package checksum mismatch.'}}
$settings=[Diagnostics.ProcessStartInfo]::new()
$settings.FileName=Join-Path $verification 'bin/PuTTYHub.exe'
$settings.UseShellExecute=$false
$settings.CreateNoWindow=$true
$settings.ArgumentList.Add('--smoke')
$settings.Environment['TEMP']=Join-Path $verification 'runtime'
$settings.Environment['TMP']=$settings.Environment['TEMP']
New-Item -ItemType Directory -Force $settings.Environment['TEMP'] | Out-Null
$process=[Diagnostics.Process]::Start($settings)
if(!$process.WaitForExit(15000)){throw "Extracted package still running, PID $($process.Id)."}
if($process.ExitCode -ne 0){throw 'Extracted package smoke test failed.'}
[ordered]@{status='passed';scope='preview manifests and extracted portable startup; full migration incomplete';archives=$archives;verificationDirectory=$verification} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $native 'evidence/package-results.json')
# Keep the extracted verification checkout for review. Delete only this invocation's checked staging tree.
$resolved=[IO.Path]::GetFullPath($stage)
$boundary=[IO.Path]::GetFullPath($output).TrimEnd('\')+'\'
if(!$resolved.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $resolved -Leaf).StartsWith('native-stage-')){throw 'Unsafe staging cleanup path.'}
Remove-Item -LiteralPath $resolved -Recurse -Force
$archives | ConvertTo-Json -Depth 3
