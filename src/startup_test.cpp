#include <windows.h>
#include <cstdio>

static DWORD WINAPI Worker(void* count){
 InterlockedIncrement(static_cast<volatile LONG*>(count));return 0;
}

int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc!=2)return 2;
 // A .dll filename suppresses the ASI-only game hook worker, while exercising
 // the production DllMain and static CRT with real thread notifications.
 auto module=LoadLibraryW(argv[1]);
 if(!module){std::printf("LoadLibrary error %lu\n",GetLastError());return 3;}
 if(!GetProcAddress(module,"QuickSalvageInstall"))return 4;
 volatile LONG count=0;
 for(unsigned i=0;i<32;++i){
  auto thread=CreateThread(nullptr,32*1024,Worker,const_cast<LONG*>(&count),STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
  if(!thread)return 5;
  auto waited=WaitForSingleObject(thread,10000);DWORD code=0;
  if(waited!=WAIT_OBJECT_0||!GetExitCodeThread(thread,&code)||code!=0)return 6;
  CloseHandle(thread);
 }
 if(count!=32||!FreeLibrary(module))return 7;
 std::printf("PASS: 32 small-stack thread attach/execute/detach cycles and DLL unload\n");
 return 0;
}
