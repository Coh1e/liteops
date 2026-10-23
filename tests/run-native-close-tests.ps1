param([Parameter(Mandatory=$true)][string]$Python,[ValidateSet('putty','winscp')][string]$Tool='putty')
$ErrorActionPreference='Stop'
$native=Split-Path $PSScriptRoot -Parent
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeCloseFixture {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenThread(uint access,bool inherit,uint id);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern uint ResumeThread(IntPtr thread);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool QueryFullProcessImageName(IntPtr process,uint flags,System.Text.StringBuilder path,ref uint size);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
$prefix=if($Tool -eq 'winscp'){'winscp-close'}else{'native-close'}
$record=Join-Path $native ('evidence/'+$prefix+'-child.json')
Remove-Item -LiteralPath $record -ErrorAction SilentlyContinue
$settings=[Diagnostics.ProcessStartInfo]::new();$settings.FileName=$Python;$settings.UseShellExecute=$false;$settings.CreateNoWindow=$true
$settings.ArgumentList.Add('-B');$settings.ArgumentList.Add((Join-Path $PSScriptRoot 'ssh-fixture.py'))
$server=[Diagnostics.Process]::Start($settings)
$child=$null
try {
 Start-Sleep -Seconds 2
 if($server.HasExited){throw 'Fixture exited before test'}
 $app=Start-Process (Join-Path $native 'bin/PuTTYHub.exe') -ArgumentList $(if($Tool -eq 'winscp'){'--integration-winscp-close'}else{'--integration-putty-close'}) -WindowStyle Hidden -PassThru
 $waitUntil=[DateTime]::UtcNow.AddSeconds(15)
 do {if(Test-Path -LiteralPath $record){$owned=Get-Content -LiteralPath $record -Raw | ConvertFrom-Json;if(![NativeCloseFixture]::IsWindow([IntPtr]::new([long]$owned.window))){break}};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $waitUntil)
 if(!$owned -or [NativeCloseFixture]::IsWindow([IntPtr]::new([long]$owned.window))){throw "Hub window remains open; inspect PID $($app.Id)"}
 $child=[Diagnostics.Process]::GetProcessById($owned.pid)
 $expected=[IO.Path]::GetFullPath((Join-Path $native $(if($Tool -eq 'winscp'){'components/winscp-6.7/WinSCP.exe'}else{'components/putty-0.85/putty.exe'})))
 $image=[Text.StringBuilder]::new(32768);$length=[uint32]32768
 if(![NativeCloseFixture]::QueryFullProcessImageName($child.Handle,0,$image,[ref]$length)){throw 'Cannot inspect owned suspended process'}
 if($image.ToString() -ne $expected){$child=$null;throw 'Fixture child executable mismatch'}
 $thread=[NativeCloseFixture]::OpenThread(2,$false,$owned.thread)
 if($thread -eq [IntPtr]::Zero){throw 'Cannot open owned suspended fixture thread'}
 try {if([NativeCloseFixture]::ResumeThread($thread) -eq [uint32]::MaxValue){throw 'Cannot resume owned fixture'}}finally{[void][NativeCloseFixture]::CloseHandle($thread)}
 $deadline=[DateTime]::UtcNow.AddSeconds(12);$passed=$false
 do {
  $events=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'ssh-fixture/events.jsonl') -Raw
  if(($Tool -eq 'winscp' -and $events -match '"event": "sftp_list"') -or ($Tool -eq 'putty' -and $events -match '"event": "shell_open"')){$passed=$true;break}
  Start-Sleep -Milliseconds 100
 } while([DateTime]::UtcNow -lt $deadline -and !$child.HasExited)
 if(!$passed){throw 'Native PuTTY could not authenticate after immediate Hub window close'}
 if($Tool -eq 'putty' -and !$child.HasExited){$child.Kill();if(!$child.WaitForExit(5000)){throw 'Owned fixture remains live'}}
 if(!$app.WaitForExit(5000)){throw "Resource caretaker remains live PID $($app.Id)"}
 if($app.ExitCode -ne 0){throw 'Resource caretaker failed'}
 if($Tool -eq 'putty'){$profilePath='HKCU:\Software\SimonTatham\PuTTY\Sessions\'+$owned.profile;if(Test-Path -LiteralPath $profilePath){throw 'Owned native profile was not cleaned'}}else{if($child.HasExited){throw 'WinSCP exited with Hub caretaker'}}
 [ordered]@{status='passed';tool=$Tool;scope='Actual Hub window closes before suspended tool reads password; checks authentication and resource caretaker lifetime';authenticatedAfterWindowClose=$true;caretakerExited=$true;toolSurvivesCaretaker=($Tool -eq 'winscp');profileCleanupChecked=($Tool -eq 'putty')} | ConvertTo-Json | Set-Content (Join-Path $native ('evidence/'+$prefix+'-results.json'))
 Get-Content (Join-Path $native ('evidence/'+$prefix+'-results.json'))
}finally{
 if($child -and !$child.HasExited){$child.Kill();if(!$child.WaitForExit(5000)){throw 'Owned PuTTY fixture remains live'}}
 Set-Content (Join-Path $PSScriptRoot 'ssh-fixture/stop') 'done'
 if(!$server.WaitForExit(5000)){throw "Fixture remains live PID $($server.Id); inspect before rerunning"}
}
