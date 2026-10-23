// Test-only ConPTY bridge for an installed Vim. No terminal injection or tool source changes.
#include "hub.hpp"
#include <algorithm>
#include <iostream>

using namespace hub;
namespace {
void forward(HANDLE from,HANDLE to){char bytes[16384];DWORD count=0;while(ReadFile(from,bytes,sizeof(bytes),&count,nullptr)&&count){DWORD cursor=0;while(cursor<count){DWORD written=0;if(!WriteFile(to,bytes+cursor,count-cursor,&written,nullptr)||!written)return;cursor+=written;}}}
}
int wmain(int argc,wchar_t**argv){try{
 if(argc!=4)throw std::runtime_error("Expected installed Vim, disposable text file and size-control file.");
 auto executable=fs::weakly_canonical(argv[1]),file=fs::weakly_canonical(argv[2]),size_file=fs::path(argv[3]);
 if(_wcsicmp(executable.filename().c_str(),L"vim.exe")||file.parent_path()!=fs::weakly_canonical(root()/L"tests/ssh-fixture")||!file.filename().wstring().starts_with(L"vim-fixture-")||file.extension()!=L".txt"||size_file!=fs::path(file.wstring()+L".size.json"))throw std::runtime_error("Invalid disposable Vim fixture paths.");
 Handle console_in,input,output,console_out;HANDLE read=nullptr,write=nullptr;
 check(CreatePipe(&read,&write,nullptr,0)!=FALSE,"Create Vim input");console_in.reset(read);input.reset(write);
 check(CreatePipe(&read,&write,nullptr,0)!=FALSE,"Create Vim output");output.reset(read);console_out.reset(write);
 HPCON console=nullptr;hcheck(CreatePseudoConsole({80,24},console_in.get(),console_out.get(),0,&console),"Create Vim pseudoconsole");console_in.reset();console_out.reset();
 struct ConsoleOwner{HPCON value;~ConsoleOwner(){if(value)ClosePseudoConsole(value);}} console_owner{console};
 SIZE_T length=0;InitializeProcThreadAttributeList(nullptr,1,0,&length);Bytes storage(length);auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
 check(InitializeProcThreadAttributeList(attributes,1,0,&length)!=FALSE,"Initialize Vim console attributes");
 struct AttributeOwner{LPPROC_THREAD_ATTRIBUTE_LIST value;~AttributeOwner(){DeleteProcThreadAttributeList(value);}} attribute_owner{attributes};
 check(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,console,sizeof(console),nullptr,nullptr)!=FALSE,"Bind Vim console");
 STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
 // The bridge itself uses redirected pipes. Do not propagate those standard handles into the PTY child.
 struct StandardHandles{HANDLE input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE),error=GetStdHandle(STD_ERROR_HANDLE);void restore()const{SetStdHandle(STD_INPUT_HANDLE,input);SetStdHandle(STD_OUTPUT_HANDLE,output);SetStdHandle(STD_ERROR_HANDLE,error);}~StandardHandles(){restore();}} original;
 SetStdHandle(STD_INPUT_HANDLE,nullptr);SetStdHandle(STD_OUTPUT_HANDLE,nullptr);SetStdHandle(STD_ERROR_HANDLE,nullptr);
 auto process=start_process(executable,{L"-u",L"NONE",L"-i",L"NONE",L"-n",L"--noplugin",L"-Z",L"-T",L"xterm",file.wstring()},CREATE_SUSPENDED,&startup);
 original.restore();
 Handle job(CreateJobObjectW(nullptr,nullptr));JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(!job||!SetInformationJobObject(job.get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits))||!AssignProcessToJobObject(job.get(),process.process.get())){TerminateProcess(process.process.get(),1);throw std::runtime_error("Cannot own disposable Vim process.");}
 check(ResumeThread(process.thread.get())!=static_cast<DWORD>(-1),"Start disposable Vim");
 std::thread inbound([&]{forward(GetStdHandle(STD_INPUT_HANDLE),input.get());});
 std::thread outbound([&]{forward(output.get(),GetStdHandle(STD_OUTPUT_HANDLE));});
 auto deadline=GetTickCount64()+90000;std::string previous;
 while(process.running()&&GetTickCount64()<deadline){try{if(fs::exists(size_file)){auto bytes=read_bytes(size_file);std::string value(bytes.begin(),bytes.end());if(value!=previous){auto size=Json::parse(value);auto columns=std::clamp(size.at("columns").get<int>(),20,400),rows=std::clamp(size.at("rows").get<int>(),5,200);hcheck(ResizePseudoConsole(console,{static_cast<SHORT>(columns),static_cast<SHORT>(rows)}),"Resize Vim pseudoconsole");previous=std::move(value);}}}catch(const std::exception&){/* Ignore incomplete size records; the next tick retries. */}Sleep(20);}
 if(process.running())TerminateJobObject(job.get(),1);
 WaitForSingleObject(process.process.get(),5000);auto code=process.exit_code();
 CancelSynchronousIo(inbound.native_handle());inbound.join();input.reset();
 ClosePseudoConsole(console);console_owner.value=nullptr;
 CancelSynchronousIo(outbound.native_handle());outbound.join();
 return static_cast<int>(code);
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
