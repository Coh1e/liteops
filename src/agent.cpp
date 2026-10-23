#include "tools.hpp"
#include <array>

namespace hub {
std::vector<std::wstring> pageant_key_arguments(const fs::path&key,bool deferred){
 if(!key.is_absolute()||!fs::is_regular_file(key))throw std::runtime_error("请选择存在的 PPK 私钥完整路径。");
 std::ifstream file(key,std::ios::binary);std::array<char,64>header{};file.read(header.data(),static_cast<std::streamsize>(header.size()));std::string_view prefix(header.data(),static_cast<size_t>(file.gcount()));if(!prefix.starts_with("PuTTY-User-Key-File-"))throw std::runtime_error("此文件不是 PuTTY PPK 私钥，请使用 PuTTYgen 转换。");
 std::vector<std::wstring>arguments{L"-restrict-acl"};if(deferred)arguments.emplace_back(L"-encrypted");arguments.push_back(key.wstring());SecureZeroMemory(header.data(),header.size());return arguments;
}
}
