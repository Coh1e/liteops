param([string]$Python='python.exe',[switch]$ManualEncrypted)
$ErrorActionPreference='Stop'
$native=Split-Path $PSScriptRoot -Parent
$settings=[Diagnostics.ProcessStartInfo]::new()
$settings.FileName=$Python
$settings.UseShellExecute=$false
$settings.CreateNoWindow=$true
$settings.Environment['TEMP']=Join-Path $native 'runtime'
$settings.Environment['TMP']=$settings.Environment['TEMP']
$settings.ArgumentList.Add('-B')
$settings.ArgumentList.Add((Join-Path $PSScriptRoot 'agent-fixture.py'))
$server=[Diagnostics.Process]::Start($settings)
try{
 Start-Sleep -Seconds 2
 if($server.HasExited){throw 'Public-key-only fixture exited.'}
 if($ManualEncrypted){Write-Host 'Use only the disposable Pageant prompt. Fixture passphrase: fixture passphrase 2026'; & (Join-Path $native 'bin/agent-tests.exe') --manual-encrypted}else{& (Join-Path $native 'bin/agent-tests.exe')}
 if($LASTEXITCODE -ne 0){throw 'Pageant test failed or refused to alter an existing agent; inspect evidence/agent-driver-error.txt.'}
 Get-Content (Join-Path $native 'evidence/app-agent-integration.json')
}finally{
 Set-Content (Join-Path $PSScriptRoot 'ssh-fixture/stop') 'done'
 if(!$server.WaitForExit(5000)){Write-Warning "Fixture shutdown still running, PID $($server.Id)."}
}
