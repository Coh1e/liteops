param([string]$Python='python.exe')
$ErrorActionPreference='Stop'
$native=Split-Path $PSScriptRoot -Parent
$runtime=Join-Path $native 'runtime'
$destination=Join-Path $PSScriptRoot 'deps'
New-Item -ItemType Directory -Force $runtime,$destination | Out-Null
$settings=[Diagnostics.ProcessStartInfo]::new()
$settings.FileName=$Python
$settings.UseShellExecute=$false
$settings.CreateNoWindow=$true
$settings.Environment['TEMP']=$runtime
$settings.Environment['TMP']=$runtime
$settings.Environment['PIP_CACHE_DIR']=Join-Path $runtime 'pip-cache'
$settings.Environment['PIP_CONFIG_FILE']='NUL'
$settings.Environment['PIP_DISABLE_PIP_VERSION_CHECK']='1'
foreach($argument in @('-B','-m','pip','install','--only-binary=:all:','--disable-pip-version-check','--no-warn-script-location','--target',$destination,'--requirement',(Join-Path $PSScriptRoot 'requirements.txt'))){$settings.ArgumentList.Add($argument)}
$process=[Diagnostics.Process]::Start($settings)
$process.WaitForExit()
if($process.ExitCode -ne 0){throw 'Local test dependency setup failed.'}
Write-Output "Test dependencies installed in $destination"
