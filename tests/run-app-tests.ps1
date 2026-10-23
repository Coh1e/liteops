param([string]$Python='python.exe',[ValidateSet('modern','conemu','fallback','putty')][string]$Frontend='modern',[switch]$SharingFault,[switch]$ServerLoss,[switch]$PendingAuth,[switch]$Blackhole,[string]$Vim)
$ErrorActionPreference='Stop'
if($Vim -and ($Frontend -ne 'modern' -or $Blackhole -or $PendingAuth -or $SharingFault -or $ServerLoss)){throw 'Vim requires modern frontend and no other fault mode.'}
if($Blackhole -and ($Frontend -eq 'putty' -or $PendingAuth -or $SharingFault -or $ServerLoss)){throw 'Blackhole requires an embedded frontend and no other fault scenario.'}
if($PendingAuth -and ($Frontend -eq 'putty' -or $SharingFault -or $ServerLoss)){throw 'PendingAuth requires an embedded frontend and no other fault scenario.'}
if($SharingFault -and $ServerLoss){throw 'Choose one fault scenario.'}
if(($SharingFault -or $ServerLoss) -and $Frontend -eq 'putty'){throw 'Sharing fault test applies to embedded frontends only.'}
$native=Split-Path $PSScriptRoot -Parent
$binary=Join-Path $native 'bin/PuTTYHub.exe'
function Run-App([string]$Mode){
 $process=Start-Process $binary -ArgumentList $Mode -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(55000)){throw "Application test is still running, PID $($process.Id). Inspect that process before rerunning."}
 if($process.ExitCode -ne 0){throw "Application test failed with exit $($process.ExitCode); inspect the matching app test error file in evidence/."}
}
Run-App '--smoke'
$settings=[Diagnostics.ProcessStartInfo]::new()
$settings.FileName=$Python
$settings.UseShellExecute=$false
$settings.CreateNoWindow=$true
$settings.Environment['TEMP']=Join-Path $native 'runtime'
$settings.Environment['TMP']=$settings.Environment['TEMP']
$settings.ArgumentList.Add('-B')
$settings.ArgumentList.Add((Join-Path $PSScriptRoot $(if($Vim){'vim-fixture.py'}else{'ssh-fixture.py'})))
if($Vim){$settings.ArgumentList.Add($Vim)}
if($ServerLoss){$settings.ArgumentList.Add('--server-loss')}
if($PendingAuth){$settings.ArgumentList.Add('--auth-pending')}
if($Blackhole){$settings.ArgumentList.Add('--blackhole')}
$server=[Diagnostics.Process]::Start($settings)
try {
 Start-Sleep -Seconds 2
 if($server.HasExited){throw "SSH fixture exited before the test."}
 $mode=switch($Frontend){'putty'{'--integration-putty-test'}'conemu'{'--integration-conemu-test'}'fallback'{'--integration-fallback-test'}default{'--integration-test'}};if($SharingFault -or $ServerLoss){$mode=switch($Frontend){'conemu'{'--integration-conemu-fault'}'fallback'{'--integration-fallback-fault'}default{'--integration-sharing-fault'}}};Run-App $mode
 $report=switch($Frontend){'putty'{'app-native-putty-integration.json'}'conemu'{'app-conemu-integration.json'}'fallback'{'app-fallback-integration.json'}default{'app-integration.json'}};if($SharingFault){$report=switch($Frontend){'conemu'{'app-conemu-sharing-fault.json'}'fallback'{'app-fallback-sharing-fault.json'}default{'app-sharing-fault.json'}}};if($ServerLoss){$report=switch($Frontend){'conemu'{'app-conemu-server-loss.json'}'fallback'{'app-fallback-server-loss.json'}default{'app-server-loss.json'}}};if($PendingAuth){$report=switch($Frontend){'conemu'{'app-conemu-pending-auth.json'}'fallback'{'app-fallback-pending-auth.json'}default{'app-pending-auth.json'}}};if($Blackhole){$report=switch($Frontend){'conemu'{'app-conemu-blackhole.json'}'fallback'{'app-fallback-blackhole.json'}default{'app-blackhole.json'}}};if($Vim){$report='app-vim-integration.json'};Get-Content (Join-Path $native ('evidence/'+$report))
}finally{
 Set-Content (Join-Path $PSScriptRoot 'ssh-fixture/stop') 'done'
 if(!$server.WaitForExit(5000)){throw "Fixture shutdown is still running, PID $($server.Id). Inspect that live process before starting another fixture."}
}
