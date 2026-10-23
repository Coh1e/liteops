#pragma once
#include "hub.hpp"
#include <map>

namespace hub {
struct ComponentSpec{std::string name,version,path,url,sha256;};
class Components {
 fs::path directory_;
public:
 explicit Components(fs::path p):directory_(std::move(p)){}
 const fs::path& directory()const{return directory_;}
 static const std::vector<ComponentSpec>& catalog();
 fs::path path(std::string_view)const;bool ready(std::string_view)const;fs::path require(std::string_view)const;
 void install(const std::function<void(std::string)>&,const std::atomic<bool>& cancel);
};
class ManagedProfile {
 std::wstring name_,registry_path_,owner_;fs::path record_;
public:
 explicit ManagedProfile(const ConnectionContext&);~ManagedProfile();ManagedProfile(const ManagedProfile&)=delete;ManagedProfile&operator=(const ManagedProfile&)=delete;
 const std::wstring&name()const{return name_;}
 std::string fingerprint()const;void sharing(bool upstream,bool downstream);
 static void recover();
};
std::vector<Session> import_putty_sessions();std::vector<Session> import_winscp_ini(const fs::path&);
fs::path write_putty_profile(const Session&,std::string_view target,bool allow_existing=false);
struct PuttyWriteback {std::string target,state;};
PuttyWriteback inspect_putty_writeback(const fs::path&);
void restore_putty_writeback(const fs::path&);
size_t pending_putty_writebacks();
void initialize_winscp(const fs::path&);
bool owned_sharing_ready(DWORD pid);
std::vector<std::wstring> pageant_key_arguments(const fs::path&key,bool deferred=false);
}
