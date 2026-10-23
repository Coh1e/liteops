param([switch]$Probe,[switch]$CoreTest,[switch]$PtyTest,[switch]$ComponentTest,[switch]$ConEmuProbe,[switch]$AgentTest,[switch]$TerminalInputTest,[switch]$WritebackCrashTest,[switch]$PipeTest,[switch]$TuiHost)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$build=Join-Path $root 'bin'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$msvc='C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231'
$sdk='C:\Program Files (x86)\Windows Kits\10'
$sdkVersion='10.0.26100.0'
$flags=@('/nologo','/std:c++latest','/EHsc','/MT','/W4','/WX','/permissive-','/sdl','/utf-8','/Z7','/guard:cf','/DUNICODE','/D_UNICODE','/D_WIN32_WINNT=0x0A00',"/I$msvc\include","/I$root\include","/I$root\vendor")
foreach($part in @('ucrt','shared','um','winrt')){$flags+="/I$sdk\Include\$sdkVersion\$part"}
function Run([string]$exe,[string[]]$arguments){
 $s=[Diagnostics.ProcessStartInfo]::new();$s.FileName=$exe;$s.WorkingDirectory=$build;$s.UseShellExecute=$false;$s.CreateNoWindow=$true;$s.RedirectStandardOutput=$true;$s.RedirectStandardError=$true
 $s.Environment['TEMP']=Join-Path $root 'runtime';$s.Environment['TMP']=$s.Environment['TEMP'];New-Item -ItemType Directory -Force $s.Environment['TEMP'] | Out-Null
 foreach($argument in $arguments){$s.ArgumentList.Add($argument)}
 $p=[Diagnostics.Process]::Start($s);$stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync();$p.WaitForExit();$output=$stdout.Result+$stderr.Result
 if($output){Write-Output $output};if($p.ExitCode -ne 0){throw "Compiler exit $($p.ExitCode)"}
}
$sources=if($Probe){@('terminal_probe')}elseif($TuiHost){@('core','crypto','tui_host')}elseif($PipeTest){@('core','crypto','terminal','pipe_tests')}elseif($WritebackCrashTest){@('core','crypto','tools','writeback_crash_tests')}elseif($CoreTest){@('core','crypto','tools','sharing','migration','agent','core_tests')}elseif($PtyTest){@('core','crypto','tools','terminal','pty_tests')}elseif($ComponentTest){@('core','crypto','tools','component_tests')}elseif($ConEmuProbe){@('core','crypto','terminal','tools','conemu_probe')}elseif($TerminalInputTest){@('core','crypto','terminal','terminal_input_tests')}elseif($AgentTest){@('core','crypto','tools','agent','agent_tests')}else{@('core','crypto','terminal','conemu','tools','sharing','migration','agent','app')}
foreach($name in $sources){Run "$msvc\bin\Hostx64\x64\cl.exe" ($flags+@('/c',"$root\src\$name.cpp","/Fo$build\$name.obj"))}
$target=if($Probe){'terminal-probe'}elseif($TuiHost){'tui-host'}elseif($PipeTest){'pipe-tests'}elseif($WritebackCrashTest){'writeback-crash-tests'}elseif($CoreTest){'core-tests'}elseif($PtyTest){'pty-tests'}elseif($ComponentTest){'component-tests'}elseif($ConEmuProbe){'conemu-probe'}elseif($TerminalInputTest){'terminal-input-tests'}elseif($AgentTest){'agent-tests'}else{'PuTTYHub'}
$objects=$sources|ForEach-Object {"$build\$_.obj"}
$subsystem=if($CoreTest -or $ComponentTest -or $AgentTest -or $WritebackCrashTest -or $PipeTest -or $TuiHost){'/SUBSYSTEM:CONSOLE'}else{'/SUBSYSTEM:WINDOWS'}
Run "$msvc\bin\Hostx64\x64\link.exe" (@('/NOLOGO','/INCREMENTAL:NO','/DYNAMICBASE','/NXCOMPAT','/GUARD:CF',$subsystem,"/OUT:$build\$target.exe","/LIBPATH:$msvc\lib\x64","/LIBPATH:$sdk\Lib\$sdkVersion\ucrt\x64","/LIBPATH:$sdk\Lib\$sdkVersion\um\x64",'user32.lib','gdi32.lib','comctl32.lib','shell32.lib','ole32.lib','runtimeobject.lib','advapi32.lib','crypt32.lib','bcrypt.lib','winhttp.lib','shlwapi.lib','dwmapi.lib','comdlg32.lib','wtsapi32.lib')+$objects)
Write-Output "Built $build\$target.exe"
if($PtyTest -or (!$Probe -and !$CoreTest -and !$ComponentTest -and !$AgentTest -and !$TerminalInputTest -and !$WritebackCrashTest -and !$PipeTest -and !$TuiHost)){
 Run "$msvc\bin\Hostx64\x64\cl.exe" ($flags+@('/c',"$root\src\plink_resize_bridge.cpp","/Fo$build\plink_resize_bridge.obj"))
 Run "$msvc\bin\Hostx64\x64\link.exe" @('/NOLOGO','/DLL','/DYNAMICBASE','/NXCOMPAT','/GUARD:CF',"/OUT:$build\PlinkResizeBridge.dll","/LIBPATH:$msvc\lib\x64","/LIBPATH:$sdk\Lib\$sdkVersion\ucrt\x64","/LIBPATH:$sdk\Lib\$sdkVersion\um\x64","$build\plink_resize_bridge.obj",'kernel32.lib','user32.lib')
}
