// Native inventory integration. Destructive requests default to disabled.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <atomic>
static HMODULE ownModule=nullptr;
static std::wstring folder,configPath,rulePath,logPath;
static void InitializePaths(){
 if(!configPath.empty())return;
 wchar_t path[32768]{};auto length=GetModuleFileNameW(ownModule,path,32768);if(!length||length>=32768)return;
 std::wstring full(path,length);auto slash=full.find_last_of(L"\\/");if(slash==std::wstring::npos)return;
 auto file=full.substr(slash+1);folder=full.substr(0,slash);
 bool development=file.find(L"QuickSalvageTrace")==0;
 if(development&&folder.size()>6&&!_wcsicmp(folder.c_str()+folder.size()-6,L"\\build"))folder.resize(folder.size()-6);
 configPath=folder+L"\\QuickSalvage.ini";rulePath=folder+L"\\AutoSalvage.rules";
 logPath=folder+(development?L"\\analysis\\quick-salvage.log":L"\\QuickSalvage.log");
}
using GetFn=void*(__fastcall*)(void*,void*,uint32_t,uint32_t);
static GetFn reader=nullptr;static void* ecs=nullptr;static uint32_t localPlayer=0;
extern "C" __declspec(dllexport) void QuickSalvageStep(void* world,uint32_t player,GetFn get){ecs=world;localPlayer=player;reader=get;}
template<class T> static T Read(const void* p,size_t o=0){T v{};memcpy(&v,(const uint8_t*)p+o,sizeof v);return v;}
static bool Readable(const void* p,size_t n){MEMORY_BASIC_INFORMATION m{};return p&&VirtualQuery(p,&m,sizeof m)&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&uintptr_t(p)+n>=uintptr_t(p)&&uintptr_t(p)+n<=uintptr_t(m.BaseAddress)+m.RegionSize;}
static void Trace(const char* message){
 // UI-thread diagnostics only. Even an unexpected repeated event cannot
 // produce unbounded file opens or output during gameplay.
 static uint64_t window=0;static unsigned count=0,total=0;auto now=GetTickCount64();
 if(now-window>=1000){window=now;count=0;}if(count>=8||total>=1000)return;++count;++total;
 FILE* f=nullptr;if(!_wfopen_s(&f,logPath.c_str(),L"a")&&f){SYSTEMTIME t;GetLocalTime(&t);fprintf(f,"[%02u:%02u:%02u] %s\n",t.wHour,t.wMinute,t.wSecond,message);fclose(f);}
}
static uint8_t* Unique(uint8_t* start,size_t n,const char* pattern){std::vector<int> b;for(auto p=pattern;*p;){if(*p==' '){++p;continue;}if(*p=='?'){b.push_back(-1);while(*p=='?')++p;}else{char h[3]={p[0],p[1],0};b.push_back(strtoul(h,nullptr,16));p+=2;}}uint8_t* found=nullptr;for(size_t i=0;i+b.size()<=n;++i){bool ok=true;for(size_t j=0;j<b.size();++j)if(b[j]>=0&&start[i+j]!=b[j]){ok=false;break;}if(ok){if(found)return nullptr;found=start+i;}}return found;}
static void ObserveMenu(void* state,uint8_t* view);
static std::string ItemName(uint32_t item);
static void CaptureItemContext();
static bool OpenRulesPanel();
#include "native_actions.h"
#include "native_menu.h"
#include "native_items.h"
#include "native_runtime.h"
#include "native_automatic.h"
#include "native_rules_panel.h"
static bool OpenRulesPanel(){return native_rules_panel::Open();}
static std::string ItemName(uint32_t item){return native_items::Name(item);}
static void CaptureItemContext(){native_items::Capture();}
#include "mouse_observer.h"
#include "native_shortcut.h"
extern "C" __declspec(dllexport) void QuickSalvageInstall(){
 static bool attempted=false;if(attempted)return;attempted=true;InitializePaths();native_menu::store=salvage::RuleStore(rulePath);
 for(int n=255;n>=2;--n){wchar_t name[80];swprintf_s(name,L"QuickSalvageTrace%d.dll",n);auto prior=GetModuleHandleW(name);auto restore=prior?(void(*)())GetProcAddress(prior,"QuickSalvageRestore"):nullptr;if(restore)restore();}
 auto base=(uint8_t*)GetModuleHandleW(nullptr);auto nt=(IMAGE_NT_HEADERS64*)(base+((IMAGE_DOS_HEADER*)base)->e_lfanew);auto s=IMAGE_FIRST_SECTION(nt);uint8_t* text=nullptr;size_t size=0;for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i,++s)if(!memcmp(s->Name,".text",5)){text=base+s->VirtualAddress;size=s->Misc.VirtualSize;}
 if(!text)return;
 native_salvage::Install(text,size);
 if(GetPrivateProfileIntW(L"QuickSalvage",L"StandaloneContext",0,configPath.c_str())){
  if(native_items::Install(text,size)&&native_menu::store.Load())native_runtime::Install(text,size);
 }
 native_menu::Install(text,size);
 native_rules_panel::Install(text,size);
 if(GetPrivateProfileIntW(L"QuickSalvage",L"Shortcut",1,configPath.c_str())){native_shortcut::Install(text,size);mouse_observer::Install();}
 Trace("QUICK_SALVAGE_READY per_item_trace=absent");
}
extern "C" __declspec(dllexport) void QuickSalvageRestore(){native_runtime::Restore();mouse_observer::Restore();native_shortcut::Restore();native_rules_panel::Restore();native_menu::Restore();native_salvage::Restore();}
static DWORD WINAPI Startup(void*){
 // ASI loaders run before the game creates its main window. Wait for it so
 // the Ctrl gesture observer can attach instead of silently missing startup.
 for(unsigned n=0;n<240;++n){HWND hwnd=nullptr;EnumWindows(mouse_observer::Find,(LPARAM)&hwnd);if(hwnd){QuickSalvageInstall();return 0;}Sleep(250);}
 InitializePaths();Trace("QUICK_SALVAGE_STARTUP_NO_GAME_WINDOW hooks_not_installed=1");return 0;
}
// Keep the 64 KiB path buffer out of DllMain's frame. Even a no-op thread
// notification can arrive on a small stack; noinline prevents /O2 from
// moving this process-attach-only allocation back into the callback.
static __declspec(noinline) void StartStandalone(HINSTANCE module){
 wchar_t path[32768]{};auto n=GetModuleFileNameW(module,path,32768);
 if(n>4&&!_wcsicmp(path+n-4,L".asi"))CloseHandle(CreateThread(nullptr,0,Startup,nullptr,0,nullptr));
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){
  ownModule=module;DisableThreadLibraryCalls(module);
  StartStandalone(module);
 }
 return TRUE;
}
