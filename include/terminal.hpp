#pragma once
#include "hub.hpp"
#include <condition_variable>
#include <mutex>
#include <deque>

namespace hub {
constexpr UINT terminal_output_message=WM_APP+41;
struct TerminalSize{int columns=80,rows=24;};
struct TerminalScroll{int top=0,rows=1,buffer=1;};
void initialize_plink_resize(const Process&,const fs::path&);
class PtyProcess {
 HPCON console_=nullptr;Handle input_,output_,job_;Process process_;std::thread reader_;std::atomic<bool> closing_=false;
 std::mutex mutex_;std::condition_variable available_;std::deque<std::string> pending_;size_t pending_bytes_=0;
 HWND notify_=nullptr;
 void read_loop();
public:
 PtyProcess()=default;~PtyProcess();PtyProcess(const PtyProcess&)=delete;PtyProcess&operator=(const PtyProcess&)=delete;
 static bool available();
 void start(HWND,const fs::path&,const std::vector<std::wstring>&,TerminalSize);
 void send(std::string_view);void resize(TerminalSize);std::string take_output();void close()noexcept;
 bool running()const{return process_.running();}DWORD pid()const{return process_.pid;}HANDLE process_handle()const{return process_.process.get();}
};
class TerminalControl {
 struct Api;std::unique_ptr<Api> api_;HWND parent_=nullptr,window_=nullptr;void* terminal_=nullptr;
 std::function<void(std::string_view)> write_;size_t slot_=0;std::string utf8_pending_;std::wstring input_pending_;
 std::atomic<int> scroll_top_=0,scroll_rows_=1,scroll_buffer_=1;std::atomic<size_t> input_serial_=0;int wheel_remainder_=0;
 static LRESULT CALLBACK subclass(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
public:
 TerminalControl();~TerminalControl();TerminalControl(const TerminalControl&)=delete;TerminalControl&operator=(const TerminalControl&)=delete;
 void create(HWND,std::function<void(std::string_view)>);TerminalSize resize(int,int);void output(std::string_view);void focus();void dpi_changed(UINT dpi=0);
 HWND window()const{return window_;}void accept_input(const wchar_t*);void paste(std::wstring_view);std::wstring selection();void clear_selection();
 void accept_scroll(int,int,int)noexcept;TerminalScroll scroll_state()const;void scroll_rows(int);
};
class PasswordPipe {
 Handle pipe_,cancel_,bound_,child_;std::thread worker_;std::wstring name_;Bytes secret_;std::atomic<DWORD> expected_=0;
 std::atomic<bool> delivered_=false;std::string error_;std::mutex mutex_;void serve()noexcept;
public:
 explicit PasswordPipe(std::string_view);~PasswordPipe();PasswordPipe(const PasswordPipe&)=delete;PasswordPipe&operator=(const PasswordPipe&)=delete;
 const std::wstring& name()const{return name_;}void bind(HANDLE,DWORD);void cancel()noexcept;bool delivered()const{return delivered_;}std::string error();
};
}
