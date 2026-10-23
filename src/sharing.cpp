#include "tools.hpp"
#include <algorithm>

namespace hub {
namespace {
struct RegistryKey {HKEY value=nullptr;~RegistryKey(){if(value)RegCloseKey(value);}};
Json registry_snapshot(HKEY key,unsigned depth=0){
 if(depth>8)throw std::runtime_error("PuTTY 配置层级超过限制。");
 DWORD names=0,bytes=0;check(RegQueryInfoKeyW(key,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&names,&bytes,nullptr,nullptr)==ERROR_SUCCESS,"Inspect native configuration");
 if(names>32767||bytes>1024*1024)throw std::runtime_error("PuTTY 配置值超过限制。");
 Json values=Json::object();std::vector<wchar_t>name(static_cast<size_t>(names)+2);Bytes data(static_cast<size_t>(bytes)+2);
 for(DWORD i=0;;++i){DWORD length=static_cast<DWORD>(name.size()),size=static_cast<DWORD>(data.size()),type=0;auto result=RegEnumValueW(key,i,name.data(),&length,nullptr,&type,data.data(),&size);if(result==ERROR_NO_MORE_ITEMS)break;check(result==ERROR_SUCCESS,"Read native configuration snapshot");auto label=utf8(std::wstring_view(name.data(),length));if(label=="PuTTYHubOwner"||label.starts_with("ConnectionSharing"))continue;values[label]=Json{{"type",type},{"bytes",Bytes(data.begin(),data.begin()+size)}};}
 wipe(data);Json children=Json::object();for(DWORD i=0;;++i){wchar_t child[256]{};DWORD length=256;auto result=RegEnumKeyExW(key,i,child,&length,nullptr,nullptr,nullptr,nullptr);if(result==ERROR_NO_MORE_ITEMS)break;check(result==ERROR_SUCCESS,"Enumerate native configuration children");RegistryKey nested;check(RegOpenKeyExW(key,child,0,KEY_READ,&nested.value)==ERROR_SUCCESS,"Read configuration child");children[utf8(std::wstring_view(child,length))]=registry_snapshot(nested.value,depth+1);}
 return Json{{"values",values},{"children",children}};
}
}
std::string ManagedProfile::fingerprint()const{
 RegistryKey key;check(RegOpenKeyExW(HKEY_CURRENT_USER,registry_path_.c_str(),0,KEY_READ,&key.value)==ERROR_SUCCESS,"Open configuration fingerprint");auto serialized=registry_snapshot(key.value).dump();auto result=sha256({reinterpret_cast<const unsigned char*>(serialized.data()),serialized.size()});wipe(serialized);return result;
}
void ManagedProfile::sharing(bool upstream,bool downstream){
 RegistryKey key;check(RegOpenKeyExW(HKEY_CURRENT_USER,registry_path_.c_str(),0,KEY_SET_VALUE,&key.value)==ERROR_SUCCESS,"Open sharing configuration");
 for(auto [name,value]:std::initializer_list<std::pair<const wchar_t*,DWORD>>{{L"ConnectionSharing",1},{L"ConnectionSharingUpstream",upstream?1u:0u},{L"ConnectionSharingDownstream",downstream?1u:0u}})check(RegSetValueExW(key.value,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value))==ERROR_SUCCESS,"Set owned sharing role");
}
bool owned_sharing_ready(DWORD pid){
 WIN32_FIND_DATAW entry{};auto search=FindFirstFileW(L"\\\\.\\pipe\\putty-connshare.*",&entry);if(search==INVALID_HANDLE_VALUE)return false;bool owned=false;
 do{std::wstring name=entry.cFileName;if(!name.starts_with(L"putty-connshare."))continue;auto path=L"\\\\.\\pipe\\"+name;Handle pipe(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));ULONG server=0;if(pipe&&GetNamedPipeServerProcessId(pipe.get(),&server)&&server==pid){const auto deadline=GetTickCount64()+300;char banner[256]{};do{DWORD read=0;if(!PeekNamedPipe(pipe.get(),banner,sizeof(banner),&read,nullptr,nullptr))break;std::string_view received(banner,read);if(received.starts_with("SSHCONNECTION@putty.projects.tartarus.org-2.0-")&&received.find(char(10))!=std::string_view::npos){owned=true;break;}Sleep(5);}while(GetTickCount64()<deadline);break;}}while(FindNextFileW(search,&entry));
 FindClose(search);return owned;
}
}
