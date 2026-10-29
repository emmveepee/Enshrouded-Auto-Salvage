// Included after the passive trace utilities. No persistent game RVAs.
#include "rules.h"
namespace native_salvage {
using QueueFn=bool(__fastcall*)(void*,uint64_t);
static QueueFn queue=nullptr;static void** appSlot=nullptr;
static bool Enabled(){return GetPrivateProfileIntW(L"QuickSalvage",L"Enabled",0,configPath.c_str())!=0;}
// Fork settings, read live from the [Fork] ini section, same pattern as
// Enabled() above. Step 1 wires KeepRarities + AutoSalvage; BackpackOnly
// is parsed here and consumed by the step-2 backpack filter.
static salvage::KeepList LoadKeepList(){
 wchar_t text[64]{};GetPrivateProfileStringW(L"Fork",L"KeepRarities",L"4",text,64,configPath.c_str());
 char narrow[64]{};for(unsigned i=0;i<63&&text[i];++i)narrow[i]=(text[i]>=32&&text[i]<127)?char(text[i]):' ';
 return salvage::KeepList::Parse(narrow);
}
static bool KeepListAuto(){return GetPrivateProfileIntW(L"Fork",L"AutoSalvage",0,configPath.c_str())!=0;}
static bool BackpackOnly(){return GetPrivateProfileIntW(L"Fork",L"BackpackOnly",1,configPath.c_str())!=0;}
static void* Component(uint32_t hash,uint32_t id){alignas(16) uint8_t result[32]{};if(reader&&ecs&&id)reader(result,ecs,hash,id);return Read<void*>(result);}
static bool Copy(void* out,const void* value,size_t size){__try{if(!value)return false;memcpy(out,value,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){memset(out,0,size);return false;}}
// Copy only the component bytes actually consumed. Fault containment covers
// both lookup and copy, without querying a huge native allocation each time.
static bool Snapshot(void* out,size_t size,uint32_t hash,uint32_t id){
 __try{auto value=Component(hash,id);if(!value)return false;memcpy(out,value,size);return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){memset(out,0,size);return false;}
}
static std::vector<uint32_t> OwnedPages(){
 std::vector<uint32_t> pages,visited{localPlayer};uint8_t top[84]{};
 if(!Snapshot(top,sizeof top,0xc24df82a,localPlayer)||!(top[80]&1)||!top[83])return pages;
 for(size_t n=0;n<visited.size()&&n<64;++n){
  uint8_t setup[84]{};if(!Snapshot(setup,sizeof setup,0xc24df82a,visited[n])||!setup[83])continue;
  for(unsigned i=0;i<16;++i){auto page=Read<uint32_t>(setup,i*4);if(!page)continue;
   if(std::find(visited.begin(),visited.end(),page)==visited.end()&&visited.size()<64)visited.push_back(page);
   auto category=setup[64+i];if((category==4||category==5)&&std::find(pages.begin(),pages.end(),page)==pages.end())pages.push_back(page);
  }
 }
 return pages;
}
static bool OwnedPage(uint32_t page){auto pages=OwnedPages();return std::find(pages.begin(),pages.end(),page)!=pages.end();}
static salvage::Candidate Candidate(uint8_t* view,bool allowed,bool diagnose=false){
 uint8_t copiedView[72]{};salvage::Candidate c;if(!Copy(copiedView,view,sizeof copiedView))return c;view=copiedView;c.key={Read<uint32_t>(view,8),view[44],view[45]};c.session=uintptr_t(ecs);c.page=Read<uint32_t>(view);c.slot=view[4];c.pide=Read<uint32_t>(view,20);c.count=Read<uint32_t>(view,16);c.owned=OwnedPage(c.page);c.canSalvage=allowed;
 // Confirm displayed metadata and location against the actual replicated item.
 uint8_t meta[20]{};bool valid=Snapshot(meta,sizeof meta,0x3a2fac75,c.pide);
 // ItemState.itemLevelUi is the live displayed level (including upgrades).
 // The slot/menu view can retain the base level, so comparing them rejects
 // legitimate upgraded items. Identity/location/rarity and stack/PIDE remain
 // independently checked; use the authoritative live level for exact rules.
 c.metadataValid=valid&&Read<uint32_t>(meta,8)==c.key.item&&Read<uint32_t>(meta)==c.page&&meta[4]==c.slot&&meta[16]==c.key.rarity;
 if(c.metadataValid){if(diagnose&&c.key.level!=meta[17]){char message[140];sprintf_s(message,"ITEM_LEVEL_NORMALIZED item=%u menu_level=%u live_level=%u",c.key.item,c.key.level,meta[17]);Trace(message);}c.key.level=meta[17];}
 if(diagnose&&!c.metadataValid){char message[300];sprintf_s(message,"ITEM_METADATA_COMPARE valid=%u item=%u/%u page=%u/%u slot=%u/%u rarity=%u/%u level=%u/%u raw=%02x%02x%02x%02x",valid,c.key.item,Read<uint32_t>(meta,8),c.page,Read<uint32_t>(meta),c.slot,meta[4],c.key.rarity,meta[16],c.key.level,meta[17],meta[16],meta[17],meta[18],meta[19]);Trace(message);}
 c.equipped=valid&&(Read<uint16_t>(meta,14)&8)!=0;
 uint8_t inventory[96]{};
 if(!Snapshot(inventory,sizeof inventory,0xb19528a9,c.page)||c.slot>=8){c.metadataValid=false;Trace("CANDIDATE_INVENTORY_UNREADABLE");return c;}
 auto stack=inventory+c.slot*12;
 if(Read<uint32_t>(stack)!=c.key.item||Read<uint32_t>(stack,4)!=c.count||Read<uint32_t>(stack,8)!=c.pide){char line[300];sprintf_s(line,"CANDIDATE_STACK_MISMATCH item=%u count=%u pide=%u actual_item=%u actual_count=%u actual_pide=%u",c.key.item,c.count,c.pide,Read<uint32_t>(stack),Read<uint32_t>(stack,4),Read<uint32_t>(stack,8));Trace(line);c.metadataValid=false;}
 return c;
}
static bool Install(uint8_t* text,size_t size){
 auto confirm=Unique(text,size,"49 8B CD E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 84 C0 74 2A 48 8B 81 C8 00 00 00 48 8D 54 24 40 0F 10 80 00 23 01 00 0F 29 44 24 40 E8 ?? ?? ?? ?? 49 8B 45 00 48 89 86 AC 0B 00 00");
 auto allocation=Unique(text,size,"48 8B 77 30 33 C0 8B CA 48 03 F1 48 89 06 89 46 08 8D 42 0C C6 06 51");
 if(!confirm||!allocation){Trace("NATIVE_ACTION_UNSUPPORTED signature_missing_or_ambiguous");return false;}
 DWORD64 imageBase=0;auto range=RtlLookupFunctionEntry(DWORD64(allocation),&imageBase,nullptr);if(!range)return false;
 auto queueEntry=(uint8_t*)(imageBase+range->BeginAddress);const uint8_t queuePrologue[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};if(memcmp(queueEntry,queuePrologue,sizeof queuePrologue))return false;
 appSlot=(void**)(confirm+15+Read<int32_t>(confirm,11));queue=(QueueFn)queueEntry;
 Trace("NATIVE_SALVAGE_QUEUE_READY command=0x51");return true;
}
static void Restore(){}
}
