#include "tools.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <set>

using namespace hub;
namespace {
constexpr DWORD killed_exit=91;
struct Registry {
 HKEY h=nullptr;
 ~Registry(){if(h)RegCloseKey(h);}
};
struct Fixture {
 std::string name="PuTTYHub.CrashFixture."+uuid();
 std::wstring path=L"Software\\SimonTatham\\PuTTY\\Sessions\\"+putty_profile_key(name);
 ~Fixture(){RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());try{auto folder=root()/L"runtime/putty-writeback";if(fs::exists(folder))for(auto&file:fs::directory_iterator(folder)){if(file.path().extension()!=L".json"||!file.is_regular_file()||file.file_size()>32*1024*1024)continue;auto j=Json::parse(read_bytes(file.path()));auto state=j.value("state","");if(j.value("target","")==name&&(state=="prepared"||state=="restoring"||state=="restore_required")){j["state"]="test_aborted";j["testOnly"]=true;auto text=j.dump(2);atomic_write(file.path(),{reinterpret_cast<const unsigned char*>(text.data()),text.size()});}}}catch(...){/* Preserve failed test artifacts if annotation cannot be saved. */}}
};
struct Notification {
 HANDLE h=INVALID_HANDLE_VALUE;
 ~Notification(){if(h!=INVALID_HANDLE_VALUE)FindCloseChangeNotification(h);}
};
struct Child {
 Process process;
 ~Child(){if(process.running()){TerminateProcess(process.process.get(),92);WaitForSingleObject(process.process.get(),5000);}}
};
struct Pause {
 HANDLE thread;
 explicit Pause(HANDLE h):thread(h){check(SuspendThread(thread)!=static_cast<DWORD>(-1),"Suspend owned fixture thread");}
 ~Pause(){ResumeThread(thread);}
};
class Scheduling {
 DWORD_PTR old_affinity_=0;int old_priority_=THREAD_PRIORITY_NORMAL;
public:
 DWORD_PTR mask=0;
 Scheduling(){DWORD_PTR allowed=0,system=0;check(GetProcessAffinityMask(GetCurrentProcess(),&allowed,&system)!=FALSE&&allowed!=0,"Read fixture CPU affinity");mask=allowed&(~allowed+1);old_priority_=GetThreadPriority(GetCurrentThread());check(old_priority_!=THREAD_PRIORITY_ERROR_RETURN,"Read fixture thread priority");old_affinity_=SetThreadAffinityMask(GetCurrentThread(),mask);check(old_affinity_!=0,"Pin fixture observer thread");if(!SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST)){SetThreadAffinityMask(GetCurrentThread(),old_affinity_);throw std::runtime_error("Cannot prioritize fixture observer");}}
 ~Scheduling(){SetThreadPriority(GetCurrentThread(),old_priority_);SetThreadAffinityMask(GetCurrentThread(),old_affinity_);}
};
std::string read_text(HKEY key,const wchar_t*name){wchar_t text[512]{};DWORD n=sizeof(text);check(RegGetValueW(key,nullptr,name,RRF_RT_REG_SZ,nullptr,text,&n)==ERROR_SUCCESS,"Read fixture text field");return utf8(text);}
DWORD read_integer(HKEY key,const wchar_t*name){DWORD value=0,n=sizeof(value);check(RegGetValueW(key,nullptr,name,RRF_RT_REG_DWORD,nullptr,&value,&n)==ERROR_SUCCESS,"Read fixture integer field");return value;}
bool same_value(HKEY key,const Json&expected){auto name=wide(expected.at("name").get<std::string>());DWORD type=0,n=0;auto rc=RegQueryValueExW(key,name.c_str(),nullptr,&type,nullptr,&n);if(!expected.at("present").get<bool>())return rc==ERROR_FILE_NOT_FOUND;if(rc!=ERROR_SUCCESS||n>1024*1024)return false;Bytes bytes(n);if(RegQueryValueExW(key,name.c_str(),nullptr,&type,bytes.data(),&n)!=ERROR_SUCCESS)return false;bytes.resize(n);return type==expected.at("type").get<DWORD>()&&bytes==expected.at("bytes").get<Bytes>();}
std::optional<Json> observe_journal(const fs::path&path){Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(!file)return {};LARGE_INTEGER size{};if(!GetFileSizeEx(file.get(),&size)||size.QuadPart<=0||size.QuadPart>32*1024*1024)return {};Bytes bytes(static_cast<size_t>(size.QuadPart));DWORD n=0;if(!ReadFile(file.get(),bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr)||n!=bytes.size())return {};try{return Json::parse(bytes);}catch(...){return {};}}
void validate_fixture(std::string_view name){constexpr std::string_view prefix="PuTTYHub.CrashFixture.";check(name.starts_with(prefix)&&name.size()==prefix.size()+32&&std::all_of(name.begin()+static_cast<ptrdiff_t>(prefix.size()),name.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}),"Restrict child to fixture namespace");}
Session updated_session(){Session updated;updated.name="Owned crash fixture";updated.host="after.example.test";updated.user="after-user";updated.port=2200;updated.key_file="C:\\fixture\\after.ppk";return updated;}
int child_mode(std::string name){validate_fixture(name);write_putty_profile(updated_session(),name,true);return 0;}
int restore_child(const fs::path&backup){validate_fixture(inspect_putty_writeback(backup).target);restore_putty_writeback(backup);return 0;}
struct Interruption{fs::path backup;DWORD pid=0;size_t changed=0;};
Interruption interrupt_operation(const std::string&target,HKEY key,const std::vector<std::wstring>&arguments,std::string_view expected_state,std::string_view reference,fs::path backup={}){
 auto folder=root()/L"runtime/putty-writeback";std::set<fs::path> baseline;for(auto&file:fs::directory_iterator(folder))baseline.insert(file.path());
 Notification change;change.h=FindFirstChangeNotificationW(folder.c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE);check(change.h!=INVALID_HANDLE_VALUE,"Watch durable journal publication");Handle registry_event(CreateEventW(nullptr,TRUE,FALSE,nullptr));check(static_cast<bool>(registry_event),"Create registry notification event");check(RegNotifyChangeKeyValue(key,FALSE,REG_NOTIFY_CHANGE_LAST_SET,registry_event.get(),TRUE)==ERROR_SUCCESS,"Watch fixture registry mutation");
 Handle job(CreateJobObjectW(nullptr,nullptr));check(static_cast<bool>(job),"Create owned fixture job");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;check(SetInformationJobObject(job.get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=FALSE,"Confine fixture child lifetime");
 Scheduling scheduling;Child child;child.process=start_process(root()/L"bin/writeback-crash-tests.exe",arguments,CREATE_SUSPENDED|CREATE_NO_WINDOW);check(AssignProcessToJobObject(job.get(),child.process.process.get())!=FALSE,"Assign owned fixture child");check(SetThreadAffinityMask(child.process.thread.get(),scheduling.mask)!=0,"Pin only owned fixture child");check(SetThreadPriority(child.process.thread.get(),THREAD_PRIORITY_BELOW_NORMAL)!=FALSE,"Set owned fixture child priority");check(ResumeThread(child.process.thread.get())!=static_cast<DWORD>(-1),"Start fixture writeback");
 Json captured;size_t changed_fields=0;bool killed=false;const auto deadline=GetTickCount64()+10000;
 while(GetTickCount64()<deadline&&child.process.running()){
  HANDLE handles[]{registry_event.get(),change.h,child.process.process.get()};auto result=WaitForMultipleObjects(3,handles,FALSE,100);if(result==WAIT_OBJECT_0+2)break;check(result!=WAIT_FAILED,"Wait for actual writeback activity");
  Pause pause(child.process.thread.get());
  if(backup.empty())for(auto&file:fs::directory_iterator(folder)){if(file.path().extension()!=L".json"||baseline.contains(file.path()))continue;auto candidate=observe_journal(file.path());if(candidate&&candidate->value("target","")==target){backup=file.path();break;}}
  if(auto observed=backup.empty()?std::optional<Json>{}:observe_journal(backup);observed){
   captured=*observed;changed_fields=0;for(auto&field:captured.at(reference))if(!same_value(key,field))++changed_fields;
   if(captured.at("state").get<std::string>()==expected_state&&changed_fields>0){check(TerminateProcess(child.process.process.get(),killed_exit)!=FALSE,"Kill actual owned writer after durable journal and mutation");check(WaitForSingleObject(child.process.process.get(),5000)==WAIT_OBJECT_0,"Observe killed writer exit");killed=true;break;}
  }
  if(WaitForSingleObject(change.h,0)==WAIT_OBJECT_0)check(FindNextChangeNotification(change.h)!=FALSE,"Rearm journal notification");
  if(WaitForSingleObject(registry_event.get(),0)==WAIT_OBJECT_0){ResetEvent(registry_event.get());check(RegNotifyChangeKeyValue(key,FALSE,REG_NOTIFY_CHANGE_LAST_SET,registry_event.get(),TRUE)==ERROR_SUCCESS,"Rearm fixture registry notification");}
 }
 check(killed,"Did not observe an interrupted writer with changed fields; no crash result claimed");check(child.process.exit_code()==killed_exit,"Owned writer exited with forced termination code");auto disk=Json::parse(read_bytes(backup));check(disk.at("state").get<std::string>()==expected_state,"Interrupted journal retained expected incomplete state on disk");
 return {backup,child.process.pid,changed_fields};
}
void assert_restored(HKEY key,const Session&original,DWORD advanced){check(read_text(key,L"HostName")==original.host&&read_text(key,L"UserName")==original.user&&read_integer(key,L"PortNumber")==static_cast<DWORD>(original.port)&&read_text(key,L"PublicKeyFile").empty()&&read_text(key,L"Protocol")=="ssh"&&read_integer(key,L"Present")==1,"Original connection fields restored after real process termination");check(read_integer(key,L"ScrollbackLines")==advanced,"Unrelated native preference preserved");}
void run(){
 Fixture fixture;Session original;original.name="Owned crash fixture";original.host="before.example.test";original.user="before-user";original.port=2222;write_putty_profile(original,fixture.name);
 Registry key;check(RegOpenKeyExW(HKEY_CURRENT_USER,fixture.path.c_str(),0,KEY_READ|KEY_SET_VALUE|KEY_NOTIFY,&key.h)==ERROR_SUCCESS,"Open owned crash fixture");DWORD advanced=9876;check(RegSetValueExW(key.h,L"ScrollbackLines",0,REG_DWORD,reinterpret_cast<const BYTE*>(&advanced),sizeof(advanced))==ERROR_SUCCESS,"Set unrelated fixture preference");
 auto writer=interrupt_operation(fixture.name,key.h,{L"--child",wide(fixture.name)},"prepared","fields");restore_putty_writeback(writer.backup);assert_restored(key.h,original,advanced);check(inspect_putty_writeback(writer.backup).state=="restored","Writer interruption finalized as restored");
 auto backup=write_putty_profile(updated_session(),fixture.name,true);auto restorer=interrupt_operation(fixture.name,key.h,{L"--restore-child",backup.wstring()},"restoring","after",backup);restore_putty_writeback(restorer.backup);assert_restored(key.h,original,advanced);check(inspect_putty_writeback(restorer.backup).state=="restored","Restorer interruption finalized as restored");
 write_text(root()/L"evidence/writeback-crash-results.json",Json{{"status","passed"},{"scope","Real owned writer and restorer forcibly terminated after durable incomplete journal and observed registry mutation; process-crash recovery, not power-loss durability"},{"target",fixture.name},{"forcedExitCode",killed_exit},{"interruptions",Json::array({Json{{"phase","write"},{"pid",writer.pid},{"changedFields",writer.changed},{"stateAtKill","prepared"},{"journal",utf8(writer.backup.wstring())}},Json{{"phase","restore"},{"pid",restorer.pid},{"changedFields",restorer.changed},{"stateAtKill","restoring"},{"journal",utf8(restorer.backup.wstring())}}})},{"testBinarySha256",file_hash(root()/L"bin/writeback-crash-tests.exe")},{"checks",{"Children called unchanged production writeback and recovery routines","Durable incomplete journals observed before termination","Connection fields changed before each forced termination","Killed processes retained incomplete journal state","Original connection fields restored after interrupted write","Interrupted restoration resumed and completed","Unrelated native preference preserved","Both journals finalized as restored"}}}.dump(2));
}
}
int wmain(int argc,wchar_t**argv){try{if(argc==3&&std::wstring_view(argv[1])==L"--child")return child_mode(utf8(argv[2]));if(argc==3&&std::wstring_view(argv[1])==L"--restore-child")return restore_child(argv[2]);check(argc==1,"Unexpected crash-test arguments");std::error_code ignored;fs::remove(root()/L"evidence/writeback-crash-results.json",ignored);fs::remove(root()/L"evidence/writeback-crash-error.txt",ignored);run();std::cout<<"Real process writeback crash recovery passed.\n";return 0;}catch(const std::exception&e){write_text(root()/L"evidence/writeback-crash-error.txt",e.what());std::cerr<<e.what()<<"\n";return 1;}}
