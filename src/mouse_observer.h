#include <atomic>
namespace mouse_observer {
static WNDPROC previous=nullptr;static HWND window=nullptr;
static std::atomic<uint64_t> ctrlUntil{0};
static bool rightHeld=false,ctrlAtDown=false;
static void Event(const char*,unsigned,bool down){
 auto now=GetTickCount64();
 if(down&&!rightHeld){ctrlAtDown=(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000);rightHeld=true;}
 if(!down)rightHeld=false;
 ctrlUntil=ctrlAtDown?now+(down?5000:300):0;
}
static LRESULT CALLBACK Window(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_INPUT){RAWINPUT input{};UINT size=sizeof input;auto got=GetRawInputData((HRAWINPUT)lp,RID_INPUT,&input,&size,sizeof(RAWINPUTHEADER));if(got!=UINT(-1)&&input.header.dwType==RIM_TYPEMOUSE&&(input.data.mouse.usButtonFlags&(RI_MOUSE_RIGHT_BUTTON_DOWN|RI_MOUSE_RIGHT_BUTTON_UP)))Event("raw",input.data.mouse.usButtonFlags,(input.data.mouse.usButtonFlags&RI_MOUSE_RIGHT_BUTTON_DOWN)!=0);}
 else if(message==WM_RBUTTONDOWN)Event("window_down",unsigned(wp),true);
 else if(message==WM_RBUTTONUP)Event("window_up",unsigned(wp),false);
 else if(message==WM_KILLFOCUS){ctrlUntil=0;rightHeld=false;ctrlAtDown=false;}
 return previous?CallWindowProcW(previous,hwnd,message,wp,lp):DefWindowProcW(hwnd,message,wp,lp);
}
static BOOL CALLBACK Find(HWND hwnd,LPARAM out){DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);if(pid==GetCurrentProcessId()&&IsWindowVisible(hwnd)&&!GetWindow(hwnd,GW_OWNER)){*(HWND*)out=hwnd;return FALSE;}return TRUE;}
static void Install(){if(window)return;HWND hwnd=nullptr;EnumWindows(Find,(LPARAM)&hwnd);if(!hwnd)return;previous=(WNDPROC)GetWindowLongPtrW(hwnd,GWLP_WNDPROC);SetLastError(0);auto prior=SetWindowLongPtrW(hwnd,GWLP_WNDPROC,(LONG_PTR)&Window);if(!prior&&GetLastError()){previous=nullptr;return;}if(prior)previous=(WNDPROC)prior;window=hwnd;Trace("CTRL_GESTURE_OBSERVER_READY event_only=1");}
// Fork: the one-shot install at startup races the game window (a launch
// can miss it, or subclass a splash window that is then destroyed) and
// fails silently, leaving the Ctrl gesture dead for the whole session.
// Ensure() is called repeatedly from the per-draw shortcut hook: it keeps
// the observer when the window is alive, and otherwise retries at most
// once a second until the install lands.
static void Ensure(){
 if(window&&IsWindow(window))return;
 auto now=GetTickCount64();static uint64_t lastAttempt=0;if(now-lastAttempt<1000)return;lastAttempt=now;
 window=nullptr;previous=nullptr;ctrlUntil=0;rightHeld=false;ctrlAtDown=false;
 Install();
}
static void Restore(){if(window&&(WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC)==Window)SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)previous);}
}
