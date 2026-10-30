namespace native_runtime {
static salvage::Pending pending;
static salvage::Candidate queuedItem;
// Fork step 3: sweep state. sweepActive spans as many scans as the bag
// needs (one salvage per scan, same cadence as the automatic path);
// queuedFromSweep records how the in-flight request was matched so the
// UI-thread commit re-checks it under the same matcher.
static bool sweepActive=false,queuedFromSweep=false;static unsigned sweepClaimed=0;
// Publish exactly one request to the native UI thread. State2 owns the copy;
// completion/cancellation is consumed by the scanner before another publish.
static std::atomic<unsigned> queuedState{0};
static void CommitFromUi(){
 unsigned ready=1;if(!queuedState.compare_exchange_strong(ready,2))return;
 bool sent=false;auto keepList=native_salvage::LoadKeepList();auto keepAuto=native_salvage::KeepListAuto();__try{
  auto c=queuedItem;uint8_t view[72]{};memcpy(view,&c.page,4);view[4]=c.slot;memcpy(view+8,&c.key.item,4);memcpy(view+16,&c.count,4);memcpy(view+20,&c.pide,4);view[44]=uint8_t(c.key.rarity);view[45]=uint8_t(c.key.level);
  if(native_salvage::Enabled()&&c.session==uintptr_t(ecs)&&salvage::AutoSalvageMatch(c.key,native_menu::store.rules,keepList,keepAuto||queuedFromSweep)&&native_salvage::appSlot&&*native_salvage::appSlot&&native_items::canSalvage){
   auto fresh=native_salvage::Candidate(view,native_items::canSalvage(view));
   // Fork step 2: refuse the commit if the item now sits on an action-bar
   // page — it may have been dragged there after the scanner queued it.
   bool barBlocked=native_salvage::BackpackOnly()&&native_salvage::IsActionbarPage(fresh.page,native_salvage::ReadActionbarPages());
   // Level may change after the scanner published this request (e.g. upgrade).
   // Candidate uses the live UI level, so re-check its exact rule at commit.
   if(!barBlocked&&salvage::Eligible(fresh)&&fresh.key==c.key&&salvage::AutoSalvageMatch(fresh.key,native_menu::store.rules,keepList,keepAuto||queuedFromSweep))sent=native_salvage::queue(*native_salvage::appSlot,uint64_t(c.page)|(uint64_t(c.slot)<<32));
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
 // Fork step 3: latch a panel-requested sweep (native_rules_panel button).
 if(native_salvage::sweepRequested.exchange(false)&&!sweepActive){sweepActive=true;sweepClaimed=0;Trace("SWEEP_STARTED source=panel");}
 auto ruleCount=native_menu::store.rules.Size();Stage(6);if(ruleCount==0&&!native_salvage::KeepListAuto()&&!sweepActive)return;
 auto state=queuedState.load();if(state==1||state==2)return;if(state==4)pending.Cancel(queuedItem);if(state)queuedState.store(0);
 static uint64_t last=0;auto now=GetTickCount64();if(now-last<200)return;last=now;
 if(!native_salvage::Enabled()||!ecs||!localPlayer||!native_items::lookup||!native_salvage::queue)return;
 auto keepList=native_salvage::LoadKeepList();auto keepAuto=native_salvage::KeepListAuto();
 // Fork step 2: with BackpackOnly on, the automatic path never scans the
 // action-bar pages (named by SlotSelection). Manual paths are untouched.
 // If the SlotSelection read fails, barPages is invalid and nothing is
 // skipped — the filter degrades to stock behavior, never to eating bars.
 auto backpackOnly=native_salvage::BackpackOnly();auto barPages=backpackOnly?native_salvage::ReadActionbarPages():native_salvage::ActionbarPages{};
 auto pages=native_salvage::OwnedPages();
 std::vector<salvage::Candidate> inventory;inventory.reserve(128);
 for(auto page:pages){
  if(backpackOnly&&native_salvage::IsActionbarPage(page,barPages))continue;
  uint8_t contents[96]{};if(!native_salvage::Snapshot(contents,sizeof contents,0xb19528a9,page))continue;
  for(unsigned slot=0;slot<8;++slot){
   auto stack=contents+slot*12;auto item=Read<uint32_t>(stack),count=Read<uint32_t>(stack,4),pide=Read<uint32_t>(stack,8);if(!item||!count||!pide)continue;
   salvage::Candidate c;c.session=uintptr_t(ecs);c.page=page;c.slot=uint8_t(slot);c.pide=pide;c.count=count;c.key.item=item;
   uint8_t meta[20]{};
   if(native_salvage::Snapshot(meta,sizeof meta,0x3a2fac75,pide)){
    c.key.rarity=meta[16];c.key.level=meta[17];c.owned=true;c.equipped=(Read<uint16_t>(meta,14)&8)!=0;
    c.metadataValid=Read<uint32_t>(meta,8)==item&&Read<uint32_t>(meta)==page&&meta[4]==slot;
    if(salvage::AutoSalvageMatch(c.key,native_menu::store.rules,keepList,keepAuto||sweepActive)&&c.metadataValid&&!c.equipped)c.canSalvage=native_items::Allowed(item,pide);
   }
   inventory.push_back(c);
  }
 }
 pending.Observe(inventory);
 // Fork page probe ([Fork] PageProbe=1): when the owned-page layout or
 // contents change, log one PAGE_PROBE line per page with its
 // InventoryCategory, linking parent entity, link depth, action-bar
 // flag (from SlotSelection) and item ids. Lines drip one per scan so
 // a full layout stays under the trace rate budget.
 if(native_salvage::PageProbe()){
  static std::vector<std::string> pendingLines;static std::string lastSignature;
  auto detailed=native_salvage::OwnedPagesDetailed();auto probeBars=native_salvage::ReadActionbarPages();std::string signature;std::vector<std::string> lines;
  for(auto& info:detailed){
   uint8_t contents[96]{};std::string items;
   if(native_salvage::Snapshot(contents,sizeof contents,0xb19528a9,info.page)){
    for(unsigned slot=0;slot<8;++slot){auto item=Read<uint32_t>(contents+slot*12);if(item){if(!items.empty())items+=",";items+=std::to_string(item);}}
   }
   unsigned bar=native_salvage::IsActionbarPage(info.page,probeBars)?1:0;
   signature+=std::to_string(info.page)+":"+std::to_string(info.category)+":"+std::to_string(info.parent)+":"+std::to_string(info.depth)+":"+std::to_string(bar)+":"+items+";";
   char line[512];sprintf_s(line,"PAGE_PROBE page=%u category=%u parent=%u depth=%u bar=%u items=%s",info.page,info.category,info.parent,info.depth,bar,items.empty()?"-":items.c_str());lines.emplace_back(line);
  }
  if(signature!=lastSignature){lastSignature=signature;pendingLines=lines;}
  if(!pendingLines.empty()){Trace(pendingLines.front().c_str());pendingLines.erase(pendingLines.begin());}
 }
 for(const auto& c:inventory){
  if(!salvage::AutoSalvageMatch(c.key,native_menu::store.rules,keepList,keepAuto||sweepActive)||!salvage::Eligible(c))continue;
  // Development restriction also covers the automatic path.
  const wchar_t* config=configPath.c_str();
  auto only=GetPrivateProfileIntW(L"QuickSalvage",L"TestOnlyItem",0,config),pide=GetPrivateProfileIntW(L"QuickSalvage",L"TestOnlyPide",0,config);
  if((only&&only!=c.key.item)||(pide&&pide!=c.pide)||!pending.Claim(c))continue;
  queuedItem=c;queuedFromSweep=sweepActive;if(sweepActive)++sweepClaimed;queuedState.store(1);
  return; // At most one native request per scan; no unchanged-item spam.
 }
 // Fork step 3: a full scan claimed nothing, so an active sweep is done.
 if(sweepActive){sweepActive=false;char line[120];sprintf_s(line,"SWEEP_COMPLETE claimed=%u",sweepClaimed);Trace(line);}
}
}
