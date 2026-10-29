namespace native_runtime {
static salvage::Pending pending;
static salvage::Candidate queuedItem;
// Publish exactly one request to the native UI thread. State2 owns the copy;
// completion/cancellation is consumed by the scanner before another publish.
static std::atomic<unsigned> queuedState{0};
static void CommitFromUi(){
 unsigned ready=1;if(!queuedState.compare_exchange_strong(ready,2))return;
 bool sent=false;auto keepList=native_salvage::LoadKeepList();auto keepAuto=native_salvage::KeepListAuto();__try{
  auto c=queuedItem;uint8_t view[72]{};memcpy(view,&c.page,4);view[4]=c.slot;memcpy(view+8,&c.key.item,4);memcpy(view+16,&c.count,4);memcpy(view+20,&c.pide,4);view[44]=uint8_t(c.key.rarity);view[45]=uint8_t(c.key.level);
  if(native_salvage::Enabled()&&c.session==uintptr_t(ecs)&&salvage::AutoSalvageMatch(c.key,native_menu::store.rules,keepList,keepAuto)&&native_salvage::appSlot&&*native_salvage::appSlot&&native_items::canSalvage){
   auto fresh=native_salvage::Candidate(view,native_items::canSalvage(view));
   // Level may change after the scanner published this request (e.g. upgrade).
   // Candidate uses the live UI level, so re-check its exact rule at commit.
   if(salvage::Eligible(fresh)&&fresh.key==c.key&&salvage::AutoSalvageMatch(fresh.key,native_menu::store.rules,keepList,keepAuto))sent=native_salvage::queue(*native_salvage::appSlot,uint64_t(c.page)|(uint64_t(c.slot)<<32));
  }
  if(sent){char line[180];sprintf_s(line,"AUTO_SALVAGE_QUEUED item=%u pide=%u rarity=%u level=%u native_ui_context=1",c.key.item,c.pide,c.key.rarity,c.key.level);Trace(line);}
 }__except(EXCEPTION_EXECUTE_HANDLER){Trace("AUTO_SALVAGE_UI_REQUEST_CANCELLED no_retry_spam=1");}
 queuedState.store(sent?3:4);
}
static void Pump(void* app){
 // Frame encloses this traversal in SEH. Querying the entire multi-megabyte
 // native allocation every draw costs tens of milliseconds on this process.
 if(!app){ecs=nullptr;localPlayer=0;return;}Stage(0);
 auto ui=Read<void*>(app,appUi);Stage(1);if(!ui){ecs=nullptr;localPlayer=0;return;}
 auto client=Read<void*>(ui,uiClient);
 Stage(2);if(!client){ecs=nullptr;localPlayer=0;return;}
 auto wrapper=Read<void*>(client,clientWorld);Stage(3);if(!wrapper){ecs=nullptr;localPlayer=0;return;}
 ecs=unwrap(wrapper);Stage(4);localPlayer=playerId((uint8_t*)client+clientPlayer);Stage(5);
 auto ruleCount=native_menu::store.rules.Size();Stage(6);if(ruleCount==0&&!native_salvage::KeepListAuto())return;
 auto state=queuedState.load();if(state==1||state==2)return;if(state==4)pending.Cancel(queuedItem);if(state)queuedState.store(0);
 static uint64_t last=0;auto now=GetTickCount64();if(now-last<200)return;last=now;
 if(!native_salvage::Enabled()||!ecs||!localPlayer||!native_items::lookup||!native_salvage::queue)return;
 auto keepList=native_salvage::LoadKeepList();auto keepAuto=native_salvage::KeepListAuto();
 auto pages=native_salvage::OwnedPages();
 std::vector<salvage::Candidate> inventory;inventory.reserve(128);
 for(auto page:pages){
  uint8_t contents[96]{};if(!native_salvage::Snapshot(contents,sizeof contents,0xb19528a9,page))continue;
  for(unsigned slot=0;slot<8;++slot){
   auto stack=contents+slot*12;auto item=Read<uint32_t>(stack),count=Read<uint32_t>(stack,4),pide=Read<uint32_t>(stack,8);if(!item||!count||!pide)continue;
   salvage::Candidate c;c.session=uintptr_t(ecs);c.page=page;c.slot=uint8_t(slot);c.pide=pide;c.count=count;c.key.item=item;
   uint8_t meta[20]{};
   if(native_salvage::Snapshot(meta,sizeof meta,0x3a2fac75,pide)){
    c.key.rarity=meta[16];c.key.level=meta[17];c.owned=true;c.equipped=(Read<uint16_t>(meta,14)&8)!=0;
    c.metadataValid=Read<uint32_t>(meta,8)==item&&Read<uint32_t>(meta)==page&&meta[4]==slot;
    if(salvage::AutoSalvageMatch(c.key,native_menu::store.rules,keepList,keepAuto)&&c.metadataValid&&!c.equipped)c.canSalvage=native_items::Allowed(item,pide);
   }
   inventory.push_back(c);
  }
 }
 pending.Observe(inventory);
 for(const auto& c:inventory){
  if(!salvage::AutoSalvageMatch(c.key,native_menu::store.rules,keepList,keepAuto)||!salvage::Eligible(c))continue;
  // Development restriction also covers the automatic path.
  const wchar_t* config=configPath.c_str();
  auto only=GetPrivateProfileIntW(L"QuickSalvage",L"TestOnlyItem",0,config),pide=GetPrivateProfileIntW(L"QuickSalvage",L"TestOnlyPide",0,config);
  if((only&&only!=c.key.item)||(pide&&pide!=c.pide)||!pending.Claim(c))continue;
  queuedItem=c;queuedState.store(1);
  return; // At most one native request per scan; no unchanged-item spam.
 }
}
}
