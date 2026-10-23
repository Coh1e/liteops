// Compatibility adapter for the pinned, unmodified PuTTY 0.85 x64 binary.
// The Windows Plink frontend does not forward console resize events. This DLL
// observes its console dimensions on its own event-loop thread and calls the
// existing BackendVtable::size entry. No SSH protocol or PuTTY source is changed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <algorithm>

namespace {
using WaitFn=DWORD(WINAPI*)(DWORD,const HANDLE*,BOOL,DWORD);
using ReadFn=BOOL(WINAPI*)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
ReadFn original_read=nullptr;
WaitFn original_wait=nullptr;DWORD main_thread=0;BYTE* image=nullptr;size_t image_size=0;uintptr_t* backend_slot=nullptr;
struct State{DWORD version=1,columns=0,rows=0,updates=0,error=0;};
template<class T>bool read(const void*address,T&value){SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),address,&value,sizeof(value),&n)&&n==sizeof(value);}
bool in_image(uintptr_t p,size_t n=1){return p>=reinterpret_cast<uintptr_t>(image)&&p+n>=p&&p+n<=reinterpret_cast<uintptr_t>(image)+image_size;}
bool function(uintptr_t p){if(!in_image(p))return false;MEMORY_BASIC_INFORMATION info{};return VirtualQuery(reinterpret_cast<void*>(p),&info,sizeof(info))&&((info.Protect&0xff)==PAGE_EXECUTE_READ||(info.Protect&0xff)==PAGE_EXECUTE_READWRITE||(info.Protect&0xff)==PAGE_EXECUTE);}
bool backend(uintptr_t object,uintptr_t&size_function){uintptr_t vt=0,id=0;if(!object||!read(reinterpret_cast<void*>(object),vt)||!in_image(vt,20*sizeof(uintptr_t)))return false;if(!read(reinterpret_cast<void*>(vt+17*sizeof(uintptr_t)),id)||!in_image(id,4))return false;DWORD name=0;if(!read(reinterpret_cast<void*>(id),name)||name!=0x00687373)return false; // "ssh\0"
 if(!read(reinterpret_cast<void*>(vt+5*sizeof(uintptr_t)),size_function)||!function(size_function))return false;
 for(size_t i=0;i<5;++i){uintptr_t f=0;if(!read(reinterpret_cast<void*>(vt+i*sizeof(uintptr_t)),f)||!function(f))return false;}return true;
}
uintptr_t* find_backend(){auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);auto sections=IMAGE_FIRST_SECTION(nt);uintptr_t* result=nullptr;uintptr_t unique=0;for(WORD s=0;s<nt->FileHeader.NumberOfSections;++s){auto&section=sections[s];if(!(section.Characteristics&IMAGE_SCN_MEM_WRITE)||!(section.Characteristics&IMAGE_SCN_CNT_INITIALIZED_DATA))continue;for(size_t offset=0;offset+sizeof(uintptr_t)<=section.Misc.VirtualSize;offset+=sizeof(uintptr_t)){auto slot=reinterpret_cast<uintptr_t*>(image+section.VirtualAddress+offset);uintptr_t value=0,f=0;if(read(slot,value)&&backend(value,f)){if(unique&&unique!=value)return nullptr;unique=value;result=slot;}}}return result;}
}
extern "C" __declspec(dllexport) State HubResizeState;
State HubResizeState{};
namespace {
void update_size(){CONSOLE_SCREEN_BUFFER_INFO info{};if(!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&info))return;int columns=info.srWindow.Right-info.srWindow.Left+1,rows=info.srWindow.Bottom-info.srWindow.Top+1;if(columns<2||rows<2)return;if(HubResizeState.columns==static_cast<DWORD>(columns)&&HubResizeState.rows==static_cast<DWORD>(rows))return;
 if(!backend_slot)backend_slot=find_backend();uintptr_t object=0,f=0;if(!backend_slot||!read(backend_slot,object)||!backend(object,f)){HubResizeState.error=2;return;}reinterpret_cast<void(*)(void*,int,int)>(f)(reinterpret_cast<void*>(object),columns,rows);HubResizeState.columns=static_cast<DWORD>(columns);HubResizeState.rows=static_cast<DWORD>(rows);++HubResizeState.updates;HubResizeState.error=0;
}
// Preserve UTF-16 surrogate pairs before handing UTF-8 to Plink's byte stream.
BOOL WINAPI hooked_read(HANDLE handle,LPVOID buffer,DWORD capacity,LPDWORD count,LPOVERLAPPED overlapped){
 DWORD mode=0;
 if(overlapped||handle!=GetStdHandle(STD_INPUT_HANDLE)||!GetConsoleMode(handle,&mode)||!capacity||!count)return original_read(handle,buffer,capacity,count,overlapped);
 thread_local char pending[4096]{};thread_local DWORD available=0,offset=0;thread_local wchar_t high=0;
 while(offset==available){
  wchar_t wide[1025]{};DWORD prefix=high?1:0,n=0;if(high)wide[0]=high;high=0;
  if(!ReadConsoleW(handle,wide+prefix,1024-prefix,&n,nullptr)){*count=0;return FALSE;}
  n+=prefix;if(!n){*count=0;return TRUE;}
  if(wide[n-1]>=0xD800&&wide[n-1]<=0xDBFF)high=wide[--n];
  if(!n)continue;
  auto bytes=WideCharToMultiByte(CP_UTF8,0,wide,static_cast<int>(n),pending,sizeof(pending),nullptr,nullptr);
  SecureZeroMemory(wide,sizeof(wide));if(!bytes){*count=0;return FALSE;}available=static_cast<DWORD>(bytes);offset=0;
 }
 *count=std::min(capacity,available-offset);CopyMemory(buffer,pending+offset,*count);SecureZeroMemory(pending+offset,*count);offset+=*count;return TRUE;
}
DWORD WINAPI hooked_wait(DWORD count,const HANDLE*handles,BOOL all,DWORD timeout){if(GetCurrentThreadId()!=main_thread||all||!count)return original_wait(count,handles,all,timeout);const auto start=GetTickCount64();for(;;){update_size();DWORD remaining=timeout;if(timeout!=INFINITE){auto elapsed=GetTickCount64()-start;if(elapsed>=timeout)return original_wait(count,handles,FALSE,0);remaining=timeout-static_cast<DWORD>(elapsed);}auto result=original_wait(count,handles,FALSE,std::min<DWORD>(remaining,100));if(result!=WAIT_TIMEOUT)return result;if(timeout!=INFINITE&&GetTickCount64()-start>=timeout)return WAIT_TIMEOUT;}}
}
extern "C" __declspec(dllexport) DWORD WINAPI HubInitializeResize(void*parameter){if(!SetConsoleCP(CP_UTF8)||!SetConsoleOutputCP(CP_UTF8))return 6;main_thread=static_cast<DWORD>(reinterpret_cast<uintptr_t>(parameter));image=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 1;auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return 2;image_size=nt->OptionalHeader.SizeOfImage;auto imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!imports.VirtualAddress)return 3;for(auto desc=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image+imports.VirtualAddress);desc->Name;++desc){if(!desc->OriginalFirstThunk)continue;auto names=reinterpret_cast<IMAGE_THUNK_DATA64*>(image+desc->OriginalFirstThunk);auto addresses=reinterpret_cast<IMAGE_THUNK_DATA64*>(image+desc->FirstThunk);for(size_t i=0;names[i].u1.AddressOfData;++i){if(IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal))continue;auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(image+names[i].u1.AddressOfData);void* replacement=nullptr;if(lstrcmpA(reinterpret_cast<const char*>(name->Name),"WaitForMultipleObjects")==0){original_wait=reinterpret_cast<WaitFn>(addresses[i].u1.Function);replacement=reinterpret_cast<void*>(&hooked_wait);}else if(lstrcmpA(reinterpret_cast<const char*>(name->Name),"ReadFile")==0){original_read=reinterpret_cast<ReadFn>(addresses[i].u1.Function);replacement=reinterpret_cast<void*>(&hooked_read);}else continue;DWORD old=0;if(!VirtualProtect(&addresses[i].u1.Function,sizeof(uintptr_t),PAGE_READWRITE,&old))return 4;InterlockedExchangePointer(reinterpret_cast<void*volatile*>(&addresses[i].u1.Function),replacement);DWORD ignored=0;VirtualProtect(&addresses[i].u1.Function,sizeof(uintptr_t),old,&ignored);}}return original_wait&&original_read?0:5;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(module);return TRUE;}
