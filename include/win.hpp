#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <fstream>

namespace hub {
namespace fs=std::filesystem;
inline std::string utf8(std::wstring_view value){if(value.empty())return {};const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Invalid UTF-16");std::string s(static_cast<size_t>(n),0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),s.data(),n,nullptr,nullptr);return s;}
inline std::wstring wide(std::string_view value){if(value.empty())return {};const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring s(static_cast<size_t>(n),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),s.data(),n);return s;}
inline void check(bool ok,const char* action){if(!ok)throw std::runtime_error(std::string(action)+" (Win32 "+std::to_string(GetLastError())+")");}
inline void hcheck(HRESULT hr,const char* action){if(FAILED(hr))throw std::runtime_error(std::string(action)+" (HRESULT "+std::to_string(static_cast<unsigned long>(hr))+")");}
class Handle {
 HANDLE h_=nullptr;
public:
 Handle()=default;explicit Handle(HANDLE h):h_(h){}~Handle(){reset();}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 Handle(Handle&& x)noexcept:h_(std::exchange(x.h_,nullptr)){}Handle& operator=(Handle&& x)noexcept{if(this!=&x){reset();h_=std::exchange(x.h_,nullptr);}return *this;}
 HANDLE get()const{return h_;}explicit operator bool()const{return h_&&h_!=INVALID_HANDLE_VALUE;}void reset(HANDLE h=nullptr){if(*this)CloseHandle(h_);h_=h;}HANDLE release(){return std::exchange(h_,nullptr);}
};
inline fs::path root(){std::vector<wchar_t>b(32768);auto n=GetModuleFileNameW(nullptr,b.data(),static_cast<DWORD>(b.size()));check(n&&n<b.size(),"GetModuleFileName");return fs::path(std::wstring(b.data(),n)).parent_path().parent_path();}
inline std::vector<unsigned char> read_bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
inline void write_bytes(const fs::path& p,const std::vector<unsigned char>& bytes){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("Cannot write file");f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!f)throw std::runtime_error("File write failed");}
inline void write_text(const fs::path& p,std::string_view text){write_bytes(p,{text.begin(),text.end()});}
}
