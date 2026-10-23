#include "conemu.hpp"
#include <tlhelp32.h>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>

namespace hub {
namespace {
constexpr std::pair<const wchar_t*,const char*> files[]{
 {L"ConEmu64.exe","06c186ce306b75920b90ec70b54554891b81ae4946b4b5d4803e91765756f234"},
 {L"ConEmu/ConEmuC64.exe","bb8b592787c82438ecf7965eb8364a7e8dfb6c6a7b7b2a4904a170a2ec000cf5"},
 {L"ConEmu/ConEmuCD64.dll","eb1215ad89c84666ab3888b3b7e52f4378dc73ef4ed3c385fe711866f51cdc36"},
 {L"ConEmu/ConEmuHk64.dll","31f65b7afe91e23ae88d8229fd54c5eb494ffd4594bdc3d6a91d2d69892b284b"}};
DWORD main_thread(DWORD pid){Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0));check(static_cast<bool>(snapshot),"Inspect owned console threads");THREADENTRY32 item{};item.dwSize=sizeof(item);if(Thread32First(snapshot.get(),&item))do{if(item.th32OwnerProcessID==pid)return item.th32ThreadID;}while(Thread32Next(snapshot.get(),&item));return 0;}
Process descendant(DWORD parent,HANDLE job,const fs::path&expected){
 Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));check(static_cast<bool>(snapshot),"Inspect terminal descendants");std::map<DWORD,DWORD> parents;std::vector<DWORD> candidates;PROCESSENTRY32W item{};item.dwSize=sizeof(item);if(Process32FirstW(snapshot.get(),&item))do{parents[item.th32ProcessID]=item.th32ParentProcessID;if(_wcsicmp(item.szExeFile,L"plink.exe")==0)candidates.push_back(item.th32ProcessID);}while(Process32NextW(snapshot.get(),&item));
 for(auto pid:candidates){auto cursor=pid;std::set<DWORD>visited;bool owned=false;while(parents.contains(cursor)&&visited.insert(cursor).second){cursor=parents[cursor];if(cursor==parent){owned=true;break;}}if(!owned)continue;Handle process(OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid));BOOL belongs=FALSE;if(!process||!IsProcessInJob(process.get(),job,&belongs)||!belongs)continue;wchar_t image[32768]{};DWORD length=32768;if(!QueryFullProcessImageNameW(process.get(),0,image,&length)||!fs::equivalent(fs::path(image),expected))continue;auto tid=main_thread(pid);if(!tid)continue;Handle thread(OpenThread(THREAD_QUERY_INFORMATION,FALSE,tid));check(static_cast<bool>(thread),"Open owned Plink main thread");return {std::move(process),std::move(thread),pid};}return {};
}
}
bool ConEmuFrontend::available(const fs::path&components){try{for(auto&[name,hash]:files)if(file_hash(components/L"conemu-230724"/name)!=hash)return false;return true;}catch(...){return false;}}
ConEmuFrontend::~ConEmuFrontend(){close();}
void ConEmuFrontend::start(HWND host,const fs::path&components,const fs::path&plink,const std::vector<std::wstring>&arguments,PasswordPipe*pipe){
 if(gui_.process)throw std::runtime_error("ConEmu frontend already started.");if(!available(components))throw std::runtime_error("ConEmu 组件缺失或校验失败，请下载 / 修复兼容终端。");
 if(file_hash(plink)!="969f36879d5716aa1a9811f43a6a6510e8f08372dbeb9695b810b9c776f39c75")throw std::runtime_error("Plink 版本不匹配。");host_=host;helper_=components/L"conemu-230724/ConEmu/ConEmuC64.exe";plink_path_=plink;pipe_=pipe;directory_=root()/L"runtime"/wide("conemu-"+uuid());private_directory(directory_);
 std::wostringstream handle;handle<<L"0x"<<std::hex<<reinterpret_cast<uintptr_t>(host);std::vector<std::wstring> command{L"-basic",L"-nosingle",L"-noupdate",L"-nodefterm",L"-noregfonts",L"-nokeyhooks",L"-nocloseconfirm",L"-insidewnd",handle.str(),L"-dir",directory_.wstring(),L"-run",plink.wstring()};command.insert(command.end(),arguments.begin(),arguments.end());
 gui_=start_process(components/L"conemu-230724/ConEmu64.exe",command,CREATE_SUSPENDED);job_.reset(CreateJobObjectW(nullptr,nullptr));check(static_cast<bool>(job_),"Create compatibility terminal job");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;check(SetInformationJobObject(job_.get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=FALSE,"Set compatibility terminal lifetime");if(!AssignProcessToJobObject(job_.get(),gui_.process.get())){TerminateProcess(gui_.process.get(),1);throw std::runtime_error("无法管理 ConEmu 进程生命周期。");}check(ResumeThread(gui_.thread.get())!=static_cast<DWORD>(-1),"Start compatibility terminal");gui_.thread.reset();deadline_=GetTickCount64()+15000;
}
void ConEmuFrontend::poll(){std::erase_if(helpers_,[](const Process&p){return !p.running();});if(!gui_.process||plink_.process)return;if(!gui_.running())throw std::runtime_error("兼容终端在启动 SSH 前退出。");auto found=descendant(gui_.pid,job_.get(),plink_path_);if(found.process){plink_=std::move(found);if(pipe_)pipe_->bind(plink_.process.get(),plink_.pid);initialize_plink_resize(plink_,plink_path_);plink_.thread.reset();if(IsWindowVisible(host_))action(ConEmuAction::focus);return;}if(GetTickCount64()>deadline_)throw std::runtime_error("兼容终端未能启动受控 Plink。");}
HWND ConEmuFrontend::embedded_window()const{for(auto child=GetWindow(host_,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){DWORD pid=0;GetWindowThreadProcessId(child,&pid);if(pid==gui_.pid)return child;}return nullptr;}
void ConEmuFrontend::action(ConEmuAction action){if(!gui_.running()||!embedded_window())return;const wchar_t*macro=nullptr;switch(action){case ConEmuAction::focus:macro=L"SetFocus()";break;case ConEmuAction::copy:macro=L"Copy(0,0)";break;case ConEmuAction::paste:macro=L"Paste(0)";break;case ConEmuAction::enter:macro=L"Paste(2,\"\\r\\n\")";break;case ConEmuAction::probe_input:macro=L"Paste(2,\"probe-input\\r\\n\")";break;}auto executable=helper_;auto process=start_process(executable,{},CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,L"-GuiMacro:"+std::to_wstring(gui_.pid)+L" "+macro);if(!AssignProcessToJobObject(job_.get(),process.process.get())){TerminateProcess(process.process.get(),1);throw std::runtime_error("无法管理终端辅助进程。");}check(ResumeThread(process.thread.get())!=static_cast<DWORD>(-1),"Start terminal action");process.thread.reset();if(action==ConEmuAction::focus){helpers_.push_back(std::move(process));return;}auto wait=WaitForSingleObject(process.process.get(),2000);if(wait!=WAIT_OBJECT_0){TerminateProcess(process.process.get(),1);throw std::runtime_error("兼容终端操作超时。");}if(process.exit_code()!=0)throw std::runtime_error("兼容终端操作失败。");}
void ConEmuFrontend::close()noexcept{if(job_)TerminateJobObject(job_.get(),0);else if(gui_.process)TerminateProcess(gui_.process.get(),0);if(gui_.process)WaitForSingleObject(gui_.process.get(),2000);job_.reset();plink_.thread.reset();plink_.process.reset();gui_.thread.reset();gui_.process.reset();helpers_.clear();pipe_=nullptr;}
}
