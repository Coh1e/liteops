#include "hub.hpp"
#include <set>
#include <algorithm>

namespace hub {
namespace {
bool hex(std::string_view value,size_t length){return value.size()==length&&std::all_of(value.begin(),value.end(),[](unsigned char ch){return (ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F');});}
void save(const fs::path&path,const Json&value){auto text=value.dump(2);atomic_write(path,{reinterpret_cast<const unsigned char*>(text.data()),text.size()});}
}
void recover_legacy_migration(const fs::path&destination){
 auto journal=destination/L"legacy-migration.json";if(!fs::exists(journal))return;
 auto document=Json::parse(read_bytes(journal));if(document.at("Version")!=1)throw std::runtime_error("未知迁移事务版本。");auto folder=document.at("Stage").get<std::string>();if(!folder.starts_with("migration-")||!hex(std::string_view(folder).substr(10),32))throw std::runtime_error("无效迁移事务路径。");auto stage=destination/wide(folder);auto files=document.at("Files");if(!files.is_array()||files.empty()||files.size()>2)throw std::runtime_error("无效迁移事务。");std::set<std::string> names;
 for(auto&file:files){auto name=file.at("name").get<std::string>();auto hash=file.at("sha256").get<std::string>();if((name!="sessions.json"&&name!="vault.bin")||!names.insert(name).second||!hex(hash,64))throw std::runtime_error("迁移事务包含无效文件。");auto target=destination/wide(name);auto pending=stage/wide(name);if(fs::exists(target)){if(file_hash(target)!=hash)throw std::runtime_error("迁移目标已改变，请保留迁移事务并处理冲突："+name);}else if(!fs::exists(pending)||file_hash(pending)!=hash)throw std::runtime_error("迁移暂存文件缺失或损坏。");}
 if(!names.contains("sessions.json"))throw std::runtime_error("迁移事务缺少会话库。");
 for(auto&file:files){auto name=wide(file.at("name").get<std::string>());auto target=destination/name;if(!fs::exists(target))check(MoveFileExW((stage/name).c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE,"Commit validated legacy data");}
 // Remove only the two validated transaction files, never recursively delete a supplied path.
 for(auto&file:files){std::error_code ignored;fs::remove(stage/wide(file.at("name").get<std::string>()),ignored);}std::error_code ignored;fs::remove(stage,ignored);check(DeleteFileW(journal.c_str())!=FALSE,"Complete legacy migration journal");
}
size_t migrate_legacy(const fs::path&source,const fs::path&destination,std::string_view master){
 if(fs::weakly_canonical(source)==fs::weakly_canonical(destination))throw std::runtime_error("源目录和目标目录不能相同。");
 private_directory(destination);recover_legacy_migration(destination);if(fs::exists(destination/L"sessions.json")||fs::exists(destination/L"vault.bin"))throw std::runtime_error("完整迁移仅用于尚未建立会话库和密码库的新配置；现有库请使用普通导入，不能被覆盖。");
 auto source_file=source/L"sessions.json";if(fs::file_size(source_file)>16*1024*1024)throw std::runtime_error("旧会话库过大。");auto sessions_bytes=read_bytes(source_file);auto document=Json::parse(sessions_bytes);if(document.at("Version")!=1)throw std::runtime_error("不支持的旧会话库版本。");auto sessions=document.at("Sessions").get<std::vector<Session>>();(void)document.at("Groups").get<std::vector<std::string>>();std::set<std::string>ids;for(auto&session:sessions)if(session.id.empty()||!ids.insert(session.id).second)throw std::runtime_error("旧会话身份为空或重复。");
 auto folder="migration-"+uuid();auto stage=destination/wide(folder);private_directory(stage);bool journal_written=false;
 try{atomic_write(stage/L"sessions.json",sessions_bytes);Json files=Json::array();if(fs::exists(source/L"vault.bin")){if(fs::file_size(source/L"vault.bin")>16*1024*1024)throw std::runtime_error("旧密码库过大。");auto encrypted=read_bytes(source/L"vault.bin");atomic_write(stage/L"vault.bin",encrypted);Vault verified(stage/L"vault.bin");verified.unlock(master);std::set<std::string> credentials;for(auto&entry:verified.entries())if(entry.id.empty()||!credentials.insert(entry.id).second)throw std::runtime_error("旧密码库身份为空或重复。");for(auto&session:sessions)if(!session.credential_id.empty()&&!credentials.contains(session.credential_id))throw std::runtime_error("旧会话引用了不存在的凭据。");files.push_back(Json{{"name","vault.bin"},{"sha256",sha256(encrypted)}});}else{for(auto&session:sessions)if(!session.credential_id.empty())throw std::runtime_error("旧会话含密码引用，但旧密码库不存在。");}
  files.push_back(Json{{"name","sessions.json"},{"sha256",sha256(sessions_bytes)}});if(fs::exists(destination/L"sessions.json")||fs::exists(destination/L"vault.bin"))throw std::runtime_error("迁移期间目标配置已改变。");save(destination/L"legacy-migration.json",Json{{"Version",1},{"Stage",folder},{"Files",files}});journal_written=true;recover_legacy_migration(destination);return sessions.size();
 }catch(...){if(!journal_written){std::error_code ignored;fs::remove(stage/L"sessions.json",ignored);fs::remove(stage/L"vault.bin",ignored);fs::remove(stage,ignored);}throw;}
}
}
