param([string]$Python='python.exe')
$ErrorActionPreference='Stop'
$native=Split-Path $PSScriptRoot -Parent
$settings=[Diagnostics.ProcessStartInfo]::new()
$settings.FileName=$Python
$settings.UseShellExecute=$false
$settings.CreateNoWindow=$true
$settings.Environment['TEMP']=Join-Path $native 'runtime'
$settings.Environment['TMP']=$settings.Environment['TEMP']
$settings.ArgumentList.Add('-B')
$settings.ArgumentList.Add((Join-Path $PSScriptRoot 'ssh-fixture.py'))
$server=[Diagnostics.Process]::Start($settings)
try {
 Start-Sleep -Seconds 2
 if($server.HasExited){throw 'Local SSH fixture exited before the test.'}
 $settings=[Diagnostics.ProcessStartInfo]::new()
 $settings.FileName=Join-Path $native 'bin/conemu-probe.exe'
 $settings.UseShellExecute=$false
 $settings.CreateNoWindow=$true
 $settings.Environment['TEMP']=Join-Path $native 'runtime'
 $settings.Environment['TMP']=$settings.Environment['TEMP']
 $probe=[Diagnostics.Process]::Start($settings)
 if(!$probe.WaitForExit(30000)){throw "ConEmu probe still running, PID $($probe.Id); inspect it before restarting."}
 if($probe.ExitCode -ne 0){throw 'ConEmu probe failed; inspect evidence/conemu-error.txt.'}
 Get-Content (Join-Path $native 'evidence/conemu-results.json')
}finally{
 Set-Content (Join-Path $PSScriptRoot 'ssh-fixture/stop') 'done'
 if(!$server.WaitForExit(5000)){throw "Fixture shutdown still running, PID $($server.Id); inspect before restarting."}
}
