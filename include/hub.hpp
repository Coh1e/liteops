#pragma once
#include "win.hpp"
#include "json.hpp"
#include <span>
#include <memory>
#include <thread>
#include <functional>
#include <atomic>

namespace hub {
using Json=nlohmann::json;using Bytes=std::vector<unsigned char>;
std::string uuid();void private_directory(const fs::path&);void atomic_write(const fs::path&,std::span<const unsigned char>);
struct Session {
 std::string id=uuid(),name,group="默认",host,user,auth="agent",credential_id,key_file,remote_directory,putty_profile,source;
 int port=22;std::vector<std::string> bookmarks;
 void validate()const;
};
void to_json(Json&,const Session&);void from_json(const Json&,Session&);
struct SessionStore {
 fs::path directory;std::vector<Session> sessions;std::vector<std::string> groups{"默认"};
 explicit SessionStore(fs::path);void save()const;void export_to(const fs::path&)const;size_t import_from(const fs::path&);
};
std::wstring quote(std::wstring_view);std::wstring command_line(const std::vector<std::wstring>&);
std::string uri_encode(std::string_view);
std::wstring putty_profile_key(std::string_view name);
std::string putty_profile_name(std::wstring_view key);
struct ConnectionContext {
 Session session;
 int proxy_method=0,proxy_port=0;std::string proxy_host,proxy_user;
 std::string unsupported_reason;
};
ConnectionContext snapshot(const Session&);
std::vector<std::wstring> plink_arguments(const ConnectionContext&,std::wstring_view profile,std::wstring_view password_pipe={},bool share=true);
std::vector<std::wstring> winscp_arguments(const ConnectionContext&,const fs::path& config,std::wstring_view password_pipe={});
struct Process {Handle process,thread;DWORD pid=0;bool running()const;DWORD exit_code()const;};
// raw_tail is reserved for fixed adapter protocol syntax, never user text or credentials.
Process start_process(const fs::path&,const std::vector<std::wstring>&,DWORD flags=0,STARTUPINFOEXW* extended=nullptr,std::wstring_view raw_tail={});

struct SecretEntry {std::string id,name,password;};
class Vault {
 fs::path file_;Bytes key_,salt_;std::vector<SecretEntry> entries_;
 void save();
public:
 explicit Vault(fs::path);~Vault();Vault(const Vault&)=delete;Vault&operator=(const Vault&)=delete;
 bool exists()const;bool unlocked()const{return !key_.empty();}
 void create(std::string_view);void unlock(std::string_view);void lock()noexcept;
 std::string put(std::string id,std::string name,std::string password);void remove(std::string_view id);
 const std::vector<SecretEntry>& entries()const;std::string get(std::string_view)const;
};
std::string sha256(std::span<const unsigned char>);std::string file_hash(const fs::path&);
void wipe(Bytes&)noexcept;void wipe(std::string&)noexcept;
size_t migrate_legacy(const fs::path& source,const fs::path& destination,std::string_view master);
void recover_legacy_migration(const fs::path& destination);
}
