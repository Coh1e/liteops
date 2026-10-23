$ErrorActionPreference='Stop'
$native=Split-Path $PSScriptRoot -Parent
$legacy=Join-Path $PSScriptRoot 'legacy-source'
function Run([string]$Executable,[string[]]$Arguments){
 $settings=[Diagnostics.ProcessStartInfo]::new()
 $settings.FileName=$Executable
 $settings.UseShellExecute=$false
 $settings.CreateNoWindow=$true
 $settings.RedirectStandardOutput=$true
 $settings.RedirectStandardError=$true
 $settings.Environment['TEMP']=Join-Path $native 'runtime'
 $settings.Environment['TMP']=$settings.Environment['TEMP']
 foreach($argument in $Arguments){$settings.ArgumentList.Add($argument)}
 $process=[Diagnostics.Process]::Start($settings)
 $output=$process.StandardOutput.ReadToEndAsync()
 $errors=$process.StandardError.ReadToEndAsync()
 $process.WaitForExit()
 if($process.ExitCode -ne 0){throw ($output.Result+$errors.Result)}
 if($output.Result){Write-Output $output.Result}
}
$helper=Join-Path $native 'bin/vault-interop.exe'
Run 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' @('/nologo','/target:exe',('/out:'+$helper),'/r:System.Runtime.Serialization.dll','/r:System.Security.dll',(Join-Path $legacy 'Model.cs'),(Join-Path $legacy 'Vault.cs'),(Join-Path $PSScriptRoot 'vault-interop.cs'))
$fixture=Join-Path $PSScriptRoot ('csharp-migration-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
Run $helper @('create',$fixture)
Run (Join-Path $native 'bin/core-tests.exe') @($fixture)
Run $helper @('verify',$fixture)
Set-Content (Join-Path $native 'evidence/interop-directory.txt') $fixture
Get-Content (Join-Path $fixture 'reverse-result.txt')
