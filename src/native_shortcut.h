namespace native_shortcut {
using SlotFn=void*(__fastcall*)(void*,void*,void*,void*);
static SlotFn previous=nullptr;static uint8_t* entry=nullptr;static uint8_t saved[14];
using MouseFn=void*(__fastcall*)(void*,void*,uint8_t,uint8_t,uint8_t);
static MouseFn previousMouse=nullptr;static uint8_t* mouseCall=nullptr;static int32_t oldMouse=0,newMouse=0;
static thread_local bool ctrlQuery=false;
static uint8_t learnedCtrl=0;
static void* __fastcall Mouse(void* widget,void* out,uint8_t button,uint8_t modifiers,uint8_t flags){
 if(!ctrlQuery||button!=2||modifiers!=0)return previousMouse(widget,out,button,modifiers,flags);
 if(learnedCtrl)return previousMouse(widget,out,button,learnedCtrl,flags);
 // Modifier bit ordering is an internal game detail. Ask the native widget
 // for each single modifier, learning from the actual mouse event instead of
 // assuming an ordering from another input library. At most three queries,
 // and only during an observed Ctrl-mouse gesture.
 for(uint8_t value:{uint8_t(1),uint8_t(2),uint8_t(4)}){
  auto result=previousMouse(widget,out,button,value,flags);
  if(Read<uint8_t>(result)){learnedCtrl=value;char line[80];sprintf_s(line,"CTRL_NATIVE_MODIFIER_LEARNED value=%u",value);Trace(line);return result;}
 }
 return out;
}
static uint64_t lastEvent=0,lastSlot=0;
static void* __fastcall Slot(void* out,void* reference,void* actions,void* state){
 mouse_observer::Ensure(); // Fork: retry the Ctrl-gesture observer install until it lands.
 CaptureItemContext();
 native_runtime::CommitFromUi();
 const bool savedQuery=ctrlQuery;
 ctrlQuery=mouse_observer::ctrlUntil.load()!=0&&GetTickCount64()<mouse_observer::ctrlUntil.load();
 const bool clickWithCtrl=ctrlQuery;
 auto result=previous(out,reference,actions,state);ctrlQuery=savedQuery;

 // Native item widget output: rectangle[16], hover byte, primary mouse
 // event[4] at+21, secondary at+25 (verified from its actual output copy).
 // Phase4 is the normal click that selects an item. Reuse that event instead
 // of polling screen coordinates or treating hover as a selection.
 if(native_rules_panel::opened&&Read<uint8_t>(out,21)==4){
  auto view=Read<uint8_t*>(reference);
  if(view&&Read<uint32_t>(view,8))native_rules_panel::opened=false;
 }

 // The native widget writes the secondary-click phase here. The same phase
 // (4) is used by the game immediately before opening its item action menu.
 // Every ordinary draw takes just this byte check: no disk I/O, queries,
 // inventory scans, string formatting, allocation or logging.
 if(Read<uint8_t>(out,25)!=4)return result;
 __try{
  auto view=Readable(reference,8)?Read<uint8_t*>(reference):nullptr;
  if(!Readable(view,72)||!Readable(state,0xbc8)||!Readable(actions,0x481))return result;
  ObserveMenu(state,view);
  DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
  if(pid!=GetCurrentProcessId()||!clickWithCtrl)return result;
  auto count=Read<uint8_t>(actions,0x480);bool allowed=false;
  for(unsigned i=0;i<count&&i<16;++i)if(((uint8_t*)actions)[i*72]==66&&((uint8_t*)actions)[i*72+1])allowed=true;
  auto candidate=native_salvage::Candidate(view,allowed);
  char message[320];sprintf_s(message,"CTRL_RIGHT_CLICK item=%u pide=%u page=%u slot=%u rarity=%u level=%u owned=%u metadata=%u eligible=%u",candidate.key.item,candidate.pide,candidate.page,candidate.slot,candidate.key.rarity,candidate.key.level,candidate.owned,candidate.metadataValid,salvage::Eligible(candidate));Trace(message);
  if(!salvage::Eligible(candidate)||!native_salvage::Enabled())return result;
  const wchar_t* config=configPath.c_str();
  auto onlyItem=GetPrivateProfileIntW(L"QuickSalvage",L"TestOnlyItem",0,config),onlyPide=GetPrivateProfileIntW(L"QuickSalvage",L"TestOnlyPide",0,config);
  if((onlyItem&&candidate.key.item!=onlyItem)||(onlyPide&&candidate.pide!=onlyPide)){Trace("QUICK_SALVAGE_TEST_TARGET_MISMATCH no_request=1");return result;}
  auto slot=uint64_t(candidate.page)|(uint64_t(candidate.slot)<<32);auto now=GetTickCount64();
  // A slot may be rendered in more than one inventory panel in the same frame.
  if(slot==lastSlot&&now-lastEvent<100)return result;
  if(!native_salvage::queue||!Readable(native_salvage::appSlot,8)||!*native_salvage::appSlot)return result;
  if(native_salvage::queue(*native_salvage::appSlot,slot)){
   lastSlot=slot;lastEvent=now;
   if(Read<uint64_t>(state,0xb94)==slot){uint64_t empty=0;memcpy((uint8_t*)state+0xb94,&empty,8);}
   Trace("QUICK_SALVAGE_QUEUED native_command=0x51 confirmation_bypassed=1");
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){Trace("QUICK_SALVAGE_EXCEPTION native_behavior_retained=1");}
 return result;
}
static bool Install(uint8_t* text,size_t size){
 auto target=Unique(text,size,"48 8B C4 4C 89 40 18 48 89 48 08 55 53 56 57 41 55 41 56 41 57 48 8D A8 A8 FD FF FF 48 81 EC 20 03 00 00");
 auto query=Unique(text,size,"41 B0 02 48 8D 4D 90 E8 ?? ?? ?? ?? 44 0F B6 6C 24 65 8B 08 89 4C 24 69");
 if(!target||!query||!native_salvage::queue)return false;
 auto call=query+7;previousMouse=(MouseFn)(call+5+Read<int32_t>(call,1));
 SYSTEM_INFO system{};GetSystemInfo(&system);auto gran=uintptr_t(system.dwAllocationGranularity);auto center=uintptr_t(call)&~(gran-1);uint8_t* stub=nullptr;
 for(uintptr_t offset=gran;offset<0x70000000&&!stub;offset+=gran){if(center>=offset)stub=(uint8_t*)VirtualAlloc((void*)(center-offset),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!stub)stub=(uint8_t*)VirtualAlloc((void*)(center+offset),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);}
 if(!stub)return false;const uint8_t jump[]={0xff,0x25,0,0,0,0};memcpy(stub,jump,6);auto destination=uintptr_t(&Mouse);memcpy(stub+6,&destination,8);auto distance=stub-(call+5);if(distance<INT32_MIN||distance>INT32_MAX)return false;
 DWORD old;if(!VirtualProtect(stub,4096,PAGE_EXECUTE_READ,&old))return false;FlushInstructionCache(GetCurrentProcess(),stub,14);
 if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old))return false;oldMouse=Read<int32_t>(call,1);newMouse=int32_t(distance);memcpy(call+1,&newMouse,4);DWORD ignored;VirtualProtect(call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);mouseCall=call;
 if(!native_menu::Hook(target,14,(void*)&Slot,saved,(void**)&previous))return false;entry=target;Trace("NATIVE_SECONDARY_CLICK_READY per_draw_work=one_byte_check");return true;
}
static void Restore(){native_menu::Unhook(entry,14,(void*)&Slot,saved);if(mouseCall&&Read<int32_t>(mouseCall,1)==newMouse){DWORD old;if(VirtualProtect(mouseCall,5,PAGE_EXECUTE_READWRITE,&old)){memcpy(mouseCall+1,&oldMouse,4);DWORD ignored;VirtualProtect(mouseCall,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),mouseCall,5);}}}
}
