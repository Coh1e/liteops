#include "terminal.hpp"
#include <roapi.h>
#include <commctrl.h>
#include <algorithm>
#include <climits>

using namespace hub;
namespace {
struct Test {
 TerminalControl terminal;std::string input;int result=1;
 void run(HWND window){
  terminal.create(window,[this](std::string_view text){input+=text;});
  terminal.dpi_changed(96);auto base=terminal.resize(800,500);terminal.dpi_changed(192);auto doubled=terminal.resize(800,500);check(doubled.columns<base.columns&&doubled.rows<base.rows,"Native DPI change resizes font cells and terminal grid");terminal.dpi_changed(GetDpiForWindow(window));RECT client{};GetClientRect(window,&client);terminal.resize(client.right,client.bottom);
  terminal.paste(L"中文\t😀\r\nsecond");check(input=="中文\t😀\r\nsecond","Native paste preserves tab, Unicode and CRLF");input.clear();
  terminal.accept_input(L"\xd83d");check(input.empty(),"Incomplete UTF-16 high surrogate is buffered");terminal.accept_input(L"\xde00");check(input=="😀","UTF-16 surrogate pair spans callbacks");input.clear();
  terminal.accept_input(L"中文");check(input=="中文","Committed CJK callback text is UTF-8");input.clear();
  terminal.accept_input(L"\xde00");terminal.accept_input(L"ok");check(input=="ok","Malformed surrogate does not poison later callbacks");
  terminal.output("\x1b[2J\x1b[H中文 \xf0\x9f");terminal.output("\x98\x80\r\n");
  for(int line=0;line<120;++line)terminal.output("history "+std::to_string(line)+"\r\n");auto before=terminal.scroll_state();check(before.top>3&&before.buffer>=before.rows,"Native scroll callback reports history");terminal.scroll_rows(-3);terminal.output("after-scroll\r\n");check(terminal.scroll_state().top==before.top-3,"Host scroll moves native viewport");terminal.scroll_rows(INT_MIN);terminal.output("after-start\r\n");check(terminal.scroll_state().top==0,"Scroll clamps to history start");terminal.scroll_rows(INT_MAX);terminal.output("after-end\r\n");auto bottom=terminal.scroll_state();check(bottom.top==std::max(0,bottom.buffer-bottom.rows),"Scroll clamps to history end");
  write_text(root()/L"evidence/terminal-input-results.json",Json{{"status","passed"},{"scope","Native control API and callback conversion, not OS IME, clipboard interaction or actual monitor movement"},{"checks",{"Paste preserves tabs, CJK, non-BMP characters and line endings","Surrogate pair split over input callbacks preserved","Committed CJK callback converted correctly","Malformed callback does not poison subsequent input","Split UTF-8 output accepted by native control","Native history callback and viewport scroll verified","History scroll clamps at both bounds","Native 96-to-192 DPI update changes font cells and grid dimensions"}}}.dump(2));result=0;
 }
};
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto test=reinterpret_cast<Test*>(GetWindowLongPtrW(h,GWLP_USERDATA));try{if(m==WM_CREATE){test=static_cast<Test*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(test));SetTimer(h,1,100,nullptr);return 0;}if(m==WM_TIMER&&test){KillTimer(h,1);test->run(h);DestroyWindow(h);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}}catch(const std::exception&e){write_text(root()/L"evidence/terminal-input-error.txt",e.what());DestroyWindow(h);return m==WM_CREATE?-1:0;}return DefWindowProcW(h,m,w,l);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){try{std::error_code ignored;fs::remove(root()/L"evidence/terminal-input-results.json",ignored);fs::remove(root()/L"evidence/terminal-input-error.txt",ignored);hcheck(RoInitialize(RO_INIT_SINGLETHREADED),"Initialize terminal input test runtime");InitCommonControls();Test test;WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=proc;cls.lpszClassName=L"PuTTYHub.InputTest";RegisterClassW(&cls);auto window=CreateWindowExW(0,cls.lpszClassName,L"Terminal input API test",WS_OVERLAPPEDWINDOW,0,0,800,500,nullptr,nullptr,instance,&test);if(!window)return 1;ShowWindow(window,SW_SHOW);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return test.result;}catch(const std::exception&e){write_text(root()/L"evidence/terminal-input-error.txt",e.what());return 1;}}
