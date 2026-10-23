#include "win.hpp"
#include <commctrl.h>
#include <roapi.h>
#include <array>

using namespace hub;
namespace {
struct Size{int x,y;};struct Theme{DWORD background,foreground,selection,cursor;DWORD colors[16];};
struct Probe {
 HMODULE module=nullptr;HWND host=nullptr,child=nullptr;void* terminal=nullptr;bool automatic=false;
 HRESULT(__stdcall*create)(HWND,void**,void**)=nullptr;
 void(__stdcall*destroy)(void*)=nullptr;void(__stdcall*output)(void*,const wchar_t*)=nullptr;
 HRESULT(__stdcall*resize)(void*,int,int,Size*)=nullptr;
 void(__stdcall*theme)(void*,Theme,const wchar_t*,short,int)=nullptr;
 void(__stdcall*write_callback)(void*,void(__stdcall*)(const wchar_t*))=nullptr;
 void(__stdcall*focus)(void*)=nullptr;void(__stdcall*blur)(void*)=nullptr;
 void(__stdcall*key)(void*,WORD,WORD,WORD,bool)=nullptr;void(__stdcall*character)(void*,wchar_t,WORD,WORD)=nullptr;
 template<class T>void load(T& fn,const char* name){fn=reinterpret_cast<T>(GetProcAddress(module,name));if(!fn)throw std::runtime_error(std::string("Missing terminal export: ")+name);}
 void initialize(HWND h){host=h;auto path=root()/L"components/terminal-1.24.12741.0/Microsoft.Terminal.Control.dll";module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);check(module!=nullptr,"Load terminal DLL");
  load(create,"CreateTerminal");load(destroy,"DestroyTerminal");load(output,"TerminalSendOutput");load(resize,"TerminalTriggerResize");load(theme,"TerminalSetTheme");load(write_callback,"TerminalRegisterWriteCallback");load(focus,"TerminalSetFocus");load(blur,"TerminalKillFocus");load(key,"TerminalSendKeyEvent");load(character,"TerminalSendCharEvent");
  void* hwnd=nullptr;hcheck(create(h,&hwnd,&terminal),"CreateTerminal");child=static_cast<HWND>(hwnd);check(IsWindow(child)!=FALSE,"Terminal child HWND");check(GetParent(child)==host,"Terminal parent");
  Theme t{RGB(18,23,35),RGB(221,229,241),RGB(58,77,112),5,{}};
  const std::array<COLORREF,16> palette{RGB(20,24,32),RGB(240,90,100),RGB(100,210,150),RGB(240,200,100),RGB(100,160,245),RGB(190,140,230),RGB(80,200,210),RGB(210,220,230),RGB(100,115,140),RGB(255,130,140),RGB(150,240,190),RGB(255,225,150),RGB(150,200,255),RGB(220,180,255),RGB(140,235,240),RGB(245,248,255)};
  std::copy(palette.begin(),palette.end(),t.colors);theme(terminal,t,L"Cascadia Mono",12,static_cast<int>(GetDpiForWindow(host)));
  write_callback(terminal,[](const wchar_t* data){OutputDebugStringW(data);});
  SetWindowSubclass(child,subclass,1,reinterpret_cast<DWORD_PTR>(this));fit();
  output(terminal,L"\x1b[2J\x1b[HPuTTY Hub · C++23 / 官方 HwndTerminal\r\n\r\n\x1b[38;2;100;210;150mTrue color rendering · 中文字符 · UTF-8\x1b[0m\r\n\r\nThis is an embedded native terminal control, not a captured PuTTY window.\r\n");
 }
 void fit(){if(!terminal)return;RECT r{};GetClientRect(host,&r);Size dims{};hcheck(resize(terminal,std::max(1L,r.right),std::max(1L,r.bottom),&dims),"Terminal resize");check(dims.x>0&&dims.y>0,"Terminal dimensions");}
 static LRESULT CALLBACK subclass(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){auto& p=*reinterpret_cast<Probe*>(data);if(msg==WM_SETFOCUS)p.focus(p.terminal);if(msg==WM_KILLFOCUS)p.blur(p.terminal);if(msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_SYSKEYDOWN||msg==WM_SYSKEYUP){p.key(p.terminal,static_cast<WORD>(w),static_cast<WORD>((l>>16)&255),static_cast<WORD>((l>>16)&0xff00),msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN);return 0;}if(msg==WM_CHAR){p.character(p.terminal,static_cast<wchar_t>(w),static_cast<WORD>((l>>16)&255),static_cast<WORD>((l>>16)&0xff00));return 0;}return DefSubclassProc(h,msg,w,l);}
 ~Probe(){if(terminal){RemoveWindowSubclass(child,subclass,1);destroy(terminal);}if(module)FreeLibrary(module);}
};
LRESULT CALLBACK window_proc(HWND h,UINT m,WPARAM w,LPARAM l){auto p=reinterpret_cast<Probe*>(GetWindowLongPtrW(h,GWLP_USERDATA));try{if(m==WM_CREATE){p=static_cast<Probe*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));p->initialize(h);if(p->automatic)SetTimer(h,1,1000,nullptr);return 0;}if(m==WM_SIZE&&p){p->fit();return 0;}if(m==WM_SETFOCUS&&p){SetFocus(p->child);return 0;}if(m==WM_TIMER&&p){write_text(root()/L"evidence/terminal-probe-result.txt","PASS: official HwndTerminal loaded; child created; theme/output/resize succeeded\n");DestroyWindow(h);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}}catch(const std::exception& e){write_text(root()/L"evidence/terminal-probe-error.txt",e.what());PostQuitMessage(1);return m==WM_CREATE?-1:0;}return DefWindowProcW(h,m,w,l);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int){try{SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);hcheck(RoInitialize(RO_INIT_SINGLETHREADED),"RoInitialize");InitCommonControls();Probe probe;probe.automatic=std::wstring_view(args).find(L"--test")!=std::wstring_view::npos;WNDCLASSW wc{};wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName=L"PuTTYHub.TerminalProbe";wc.hCursor=LoadCursorW(nullptr,IDC_IBEAM);check(RegisterClassW(&wc)!=0,"Register window");auto h=CreateWindowExW(0,wc.lpszClassName,L"PuTTY Hub · native terminal validation",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1000,650,nullptr,nullptr,instance,&probe);check(h!=nullptr,"Create host");ShowWindow(h,SW_SHOW);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);}catch(const std::exception&e){write_text(root()/L"evidence/terminal-probe-error.txt",e.what());return 1;}}
