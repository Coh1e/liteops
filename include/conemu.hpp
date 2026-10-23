#pragma once
#include "terminal.hpp"

namespace hub {
enum class ConEmuAction{focus,copy,paste,enter,probe_input};
class ConEmuFrontend {
 HWND host_=nullptr;fs::path plink_path_,directory_,helper_;Process gui_,plink_;std::vector<Process> helpers_;Handle job_;PasswordPipe* pipe_=nullptr;ULONGLONG deadline_=0;
public:
 ConEmuFrontend()=default;~ConEmuFrontend();ConEmuFrontend(const ConEmuFrontend&)=delete;ConEmuFrontend&operator=(const ConEmuFrontend&)=delete;
 static bool available(const fs::path& components);
 void start(HWND,const fs::path& components,const fs::path& plink,const std::vector<std::wstring>&,PasswordPipe*);
 void poll();void action(ConEmuAction);void close()noexcept;
 bool running()const{return gui_.running()&&(!plink_.process||plink_.running());}
 DWORD pid()const{return plink_.pid;}HANDLE process_handle()const{return plink_.process.get();}HWND embedded_window()const;
};
}
