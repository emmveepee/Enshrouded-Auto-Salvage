// Native inventory details-panel replacement, scoped to its verified caller.
#include <intrin.h>
#include <cmath>
#include "panel_text_style.h"
namespace native_rules_panel {
using DetailFn=void(__fastcall*)(uint8_t*,bool);
using PairFn=void(__fastcall*)(void*,uint64_t);
using FloatFn=void(__fastcall*)(void*,float);
using PointerFn=void(__fastcall*)(void*,const void*);
using RectFn=const float*(__fastcall*)(void*);
using TextFn=void*(__fastcall*)(void*,const void*,const void*,bool);
using FinishFn=void(__fastcall*)(void*);
using ImageFn=void(__fastcall*)(void*,const float*,const uint32_t*,const void*);
using IconFn=uint32_t(__fastcall*)(void*,uint32_t,uint16_t,const void*);
using SliceFn=void(__fastcall*)(void*,const float*,const void*,const void*,uint64_t);
using ResolveImageFn=void*(__fastcall*)(void*,void*,const void*);
using ContextFn=void*(__fastcall*)();
using ItemIconFn=bool(__fastcall*)(void*,const float*,const void*,uint64_t);
using SlotBackgroundFn=void(__fastcall*)(void*,uint64_t);
using LayerFn=void(__fastcall*)(void*,uint8_t);
using OverlayFn=void(__fastcall*)(void*,bool,bool);
using BaseGroupFn=void*(__fastcall*)(void*,const void*);
static OverlayFn overlayLayout=nullptr;static BaseGroupFn baseGroup=nullptr;static PairFn position=nullptr;
static DetailFn previous=nullptr;static uint8_t* entry=nullptr;static uint8_t saved[19];
static PairFn stretch=nullptr,sizePair=nullptr;static FloatFn height=nullptr;
static PointerFn padding=nullptr,frame=nullptr;static RectFn rectangle=nullptr;
static TextFn label=nullptr;static FinishFn finishText=nullptr;static ImageFn image=nullptr;static IconFn icon=nullptr;
static SliceFn slice=nullptr;static ResolveImageFn resolveImage=nullptr;static uint64_t iconProbeAt=0;
static ContextFn imageContext=nullptr;
static ItemIconFn itemIcon=nullptr;static FinishFn scrollbarDefaults=nullptr;
static SlotBackgroundFn slotBackground=nullptr;static PointerFn alignment=nullptr;
static LayerFn layer=nullptr;
static float pixelScale=1;
static void* inventoryCaller=nullptr;static bool ready=false,opened=false;static uint64_t lastDraw=0;
using ScrollFn=void*(__fastcall*)(void*,uint8_t,float*,void*,const void*);
static ScrollFn scrollingLayout=nullptr;static FinishFn scrollDefaults=nullptr,endScrollingLayout=nullptr;
static float scrollOffset=0;
static uint64_t lastButton=0;
static salvage::RuleSort sortBy=salvage::RuleSort::Date;static bool sortExpanded=false,sortReversed=false;
static bool drawingSort=false,mouseWasDown=false;
static uint64_t profileUntil=0,profileFrames=0;static LONGLONG profileTicks=0,profileMax=0,frequency=0;
struct Entry {salvage::Rule rule;std::string id,detail;bool preview=false;};static std::vector<Entry> entries;
static uint64_t displayedRevision=0;
static void SortEntries(){std::stable_sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return salvage::RuleBefore(a.rule,b.rule,sortBy,sortReversed);});}
static uint64_t Pair(float x,float y){float values[2]={x,y};return Read<uint64_t>(values);}
static void Refresh(){
 entries.clear();static const char* rarities[]={"Common","Uncommon","Rare","Epic","Legendary"};
 for(const auto& r:native_menu::store.rules.List())entries.push_back({r,"salvage.rule."+std::to_string(r.key.item)+"."+std::to_string(r.key.rarity)+"."+std::to_string(r.key.level),std::string(rarities[r.key.rarity])+" | Level "+std::to_string(r.key.level)});
 SortEntries();
 displayedRevision=native_menu::store.Revision();
}
static bool Open(){
 if(!ready)return false;Refresh();
 auto primitive=imageContext();auto display=primitive?Read<void*>(primitive,0x10):nullptr;
 auto canvasWidth=display?Read<float>(display,0x20):0;RECT client{};DWORD pid=0;auto window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
 pixelScale=1;if(pid==GetCurrentProcessId()&&GetClientRect(window,&client)&&canvasWidth>100&&canvasWidth<20000)pixelScale=float(client.right-client.left)/canvasWidth;
 if(pixelScale<0.5f||pixelScale>4)pixelScale=1;
 // Opt-in development example, never saved as an active salvage rule.
 auto example=GetPrivateProfileIntW(L"QuickSalvage",L"RulesPanelExampleItem",0,configPath.c_str());
 if(entries.empty()&&example&&!native_salvage::Enabled()){
  auto name=ItemName(example);auto count=(std::min)(100u,(std::max)(1u,GetPrivateProfileIntW(L"QuickSalvage",L"RulesPanelExampleCount",1,configPath.c_str())));
  bool colors=GetPrivateProfileIntW(L"QuickSalvage",L"RulesPanelExampleRarities",0,configPath.c_str())!=0;
  for(unsigned i=0;i<count;++i)entries.push_back({{{example,uint8_t(colors?i%5:0),uint16_t(i+1)},name,uint64_t(i+1)},"rule.preview."+std::to_string(i),"Preview only",true});
 }
 SortEntries();sortExpanded=false;
 scrollOffset=0;opened=true;lastDraw=GetTickCount64();iconProbeAt=lastDraw+1000;profileUntil=lastDraw+3000;profileFrames=0;profileTicks=profileMax=0;LARGE_INTEGER f;QueryPerformanceFrequency(&f);frequency=f.QuadPart;return true;
}
static void Group(void* storage,const char* name,float gap,bool horizontal=false){auto text=native_menu::Text(name);native_menu::group(storage,&text,gap);if(horizontal){const float spacing[2]={gap,0};native_menu::setLayout(Read<void*>(storage,8),spacing,false);}}
static void Overlay(void* storage,const char* name){auto text=native_menu::Text(name);baseGroup(storage,&text);overlayLayout(Read<void*>(storage,8),false,false);}
static void Text(const char* value,const void* theme,bool heading=false){
 // Native constructor writes0x144 bytes;512 leaves explicit headroom.
 alignas(16) uint8_t model[512]{};auto text=native_menu::Text(value);label(model,&text,(const uint8_t*)theme+(heading?0x10a0c:0xf834),true);
 stretch(Read<void*>(model,8),Pair(1,0));finishText(model);
}
static void ItemText(const char* value,const void* theme,unsigned kind,uint64_t color,unsigned rarity){
 // Native named typography, rather than the yellow menu-button label style.
 // Stable storage and self-contained unique BlobString identifiers are required:
 // a raw stack copy preserves an offset into the original theme and crashes.
 static panel_text_style::Style styles[5][3];static bool initialized[5][3]{};
 if(kind>2||rarity>4)return;auto& style=styles[rarity][kind];
 if(!initialized[rarity][kind]){panel_text_style::Initialize(style,(const uint8_t*)theme+68108+(kind==0?792:0),kind,rarity,color);initialized[rarity][kind]=true;}
 alignas(16) uint8_t model[512]{};auto text=native_menu::Text(value);label(model,&text,style.data,true);stretch(Read<void*>(model,8),Pair(1,0));finishText(model);
}
static bool Button(const char* text,float width=100,uint32_t style=4,float h=44){
 if(sortExpanded&&!drawingSort&&strcmp(text,"Sort")){
  // Do not instantiate the native interactive row at all: its input registration
  // claims the pointer before the later popup gets to process it. Draw the same
  // idle compact-button assets and label through noninteractive primitives.
  alignas(16) uint8_t visual[256]{};auto id=std::string("modalBlocked.")+text;Overlay(visual,id.c_str());auto visualNode=Read<void*>(visual,8);
  if(width>0)sizePair(visualNode,Pair(width,h));else{stretch(visualNode,Pair(1,0));height(visualNode,h);}
  auto theme=(const uint8_t*)Read<void*>(*native_salvage::appSlot,0xc8);auto textures=theme+59488+3140+512;
  const float crop[4]={0,0,0,0};uint64_t white=0x3f800000ffffffffull;
  slice(visual,rectangle(visualNode),textures,crop,white);
  slice(visual,rectangle(visualNode),textures+96,crop,Read<uint64_t>(theme+59488+3140+1000));
  Text(text,theme);native_menu::endGroup(visual);return false;
 }
 uint8_t descriptor[72]{};descriptor[0]=91;descriptor[1]=1;descriptor[0x1a]=2;native_menu::SetText(descriptor,text);
 uint8_t parameters[64]{};memcpy(parameters,&style,4);memcpy(parameters+4,&width,4);memcpy(parameters+8,&h,4);uint64_t none=~0ull;memcpy(parameters+0x30,&none,8);memcpy(parameters+0x38,&none,8);
 bool hit=native_menu::previousRow(descriptor,parameters);auto now=GetTickCount64();if(!hit||now-lastButton<150)return false;lastButton=now;return true;
}
static void Draw(){
 // Changes are published only after a successful save/load. Rebuild the
 // snapshot once per change; ordinary frames do one integer comparison.
 // Refresh retains scrollOffset, sortBy, direction and popup state.
 if(displayedRevision!=native_menu::store.Revision())Refresh();
 auto root=*native_salvage::appSlot;auto theme=Read<void*>(root,0xc8);auto uiImages=imageContext();
 alignas(16) uint8_t surface[256]{};Overlay(surface,"autoSalvageSurface");stretch(Read<void*>(surface,8),Pair(1,1));
 alignas(16) uint8_t panel[256]{};Group(panel,"autoSalvageRulesPanel",12);auto node=Read<void*>(panel,8);stretch(node,Pair(1,1));
 // Same ornamental frame routine as inventory details. Largest parameter
 // read is offset180;256 bytes covers the actual native frame model.
 alignas(16) uint8_t background[256]{};uint32_t style=1;memcpy(background,&style,4);float opacity=1;memcpy(background+4,&opacity,4);uint64_t white=0x3f800000ffffffffull;memcpy(background+16,&white,8);frame(panel,background);
 const float inset[4]={24,24,24,24};padding(node,inset);
 const float chromeInset[4]={4,0,48,0};
 alignas(16) uint8_t heading[256]{};Overlay(heading,"rulesHeading");auto headingNode=Read<void*>(heading,8);stretch(headingNode,Pair(1,0));height(headingNode,44);const float titleInset[4]={4,0,4,0};padding(headingNode,titleInset);Text("Auto Salvage Rules",theme,true);
 alignas(16) uint8_t sorting[256]{};Group(sorting,"rulesSort",0);auto sortNode=Read<void*>(sorting,8);sizePair(sortNode,Pair(104,44));const uint8_t rightCenter[2]={3,2};alignment(sortNode,rightCenter);position(sortNode,Pair(-44,0));
 if(Button("Sort",104))sortExpanded=!sortExpanded;
 native_menu::endGroup(sorting);native_menu::endGroup(heading);
 // Only this middle child consumes the remaining height. Native scroll layout
 // clips its children and keeps the title and footer outside the scroll region.
 // Full GameUiScrollingLayout, rather than the low-level clipping scope:
 // it owns wheel navigation, offset clamping, clipping and scrollbar drawing.
 // Actual constructor writes through0x468;2048 bytes covers the native model.
 alignas(16) uint8_t scrolling[2048]{},scrollParameters[272]{};
 scrollDefaults(scrollParameters);float gap=12;memcpy(scrollParameters+60,&gap,4);
 scrollParameters[43]=1;scrollParameters[46]=1;scrollParameters[52]=1;scrollParameters[53]=0;
 // FbUiBundle.menu.scrollbar: apply the same nine-slice assets used by
 // native menus. Keep a gutter between content cards and the scroll track.
 const auto scrollbar=(const uint8_t*)theme+0xf8d4;auto bar=scrollParameters+68;scrollbarDefaults(bar);
 // Request resources before the native render-only widgets query them.
 // a3ea10 passes loadIfMissing=1; their a3eef0 helper only probes the cache.
 for(auto texture:{scrollbar,scrollbar+64,(const uint8_t*)theme+53840+448}){alignas(16) uint8_t loaded[64]{};resolveImage(uiImages,loaded,texture);}
 memcpy(bar+36,scrollbar+64,32);memcpy(bar+68,scrollbar,32);memcpy(bar+4,scrollbar+128,8);bar[124]=1;
 float barWidth=16;memcpy(bar+136,&barWidth,4);float contentPadding[4]={4,4,28,4};memcpy(scrollParameters+24,contentPadding,16);
 scrollingLayout(scrolling,1,&scrollOffset,nullptr,scrollParameters);
 stretch(Read<void*>(scrolling,8),Pair(1,1));
 if(entries.empty())Text("No Auto Salvage Rules",theme);
 size_t remove=SIZE_MAX;
 for(size_t i=0;i<entries.size();++i){
  const auto& e=entries[i];alignas(16) uint8_t row[256]{},picture[256]{},column[256]{};Group(row,e.id.c_str(),16,true);auto rowNode=Read<void*>(row,8);stretch(rowNode,Pair(1,0));height(rowNode,128);
  const float cardInset[4]={12,12,12,12};padding(rowNode,cardInset);
  // Native simple FancyBox: patterned food/status-effect panel, with restrained
  // gold top and bottom edges rather than the ornamental full-panel corners.
  alignas(16) uint8_t cardFrame[256];memcpy(cardFrame,background,sizeof cardFrame);uint32_t simple=10;memcpy(cardFrame,&simple,4);frame(row,cardFrame);
  const uint8_t centered[2]={0,2};
  Group(picture,"ruleIcon",0);auto pictureNode=Read<void*>(picture,8);sizePair(pictureNode,Pair(80,80));alignment(pictureNode,centered);
  // Prefer ItemInfo.iconImage, the native inventory's authored 2D asset.
  // The separate model-preview cache can legitimately have no render target.
  auto itemInfo=native_items::Info(e.rule.key.item);auto rect=rectangle(pictureNode);
  alignas(16) uint8_t slotStyle[256]{};memcpy(slotStyle,(const uint8_t*)theme+67680,48);memcpy(slotStyle+0x30,(const uint8_t*)theme+67840,8);memcpy(slotStyle+0x88,&opacity,4);
  auto rarityColor=Read<uint64_t>((const uint8_t*)theme+54380+8*(e.rule.key.rarity+1));slotBackground(slotStyle,rarityColor);
  // Same item icon renderer called by the native inventory. It uses objectId
  // and handles authored images/generated model icons internally.
  if(itemInfo)itemIcon(root,rect,itemInfo,white);
  alignas(16) uint8_t slotImage[64]{};auto slotTexture=resolveImage(uiImages,slotImage,slotStyle);if(slotTexture){auto texture=Read<uint32_t>(slotTexture);layer(picture,3);image(picture,rect,&texture,&white);layer(picture,0);}
  if(i==0&&iconProbeAt&&GetTickCount64()>=iconProbeAt){iconProbeAt=0;char message[180];sprintf_s(message,"RULE_ICON_PROBE native_item_renderer=1 metadata=%u rect=%.1f,%.1f,%.1f,%.1f",itemInfo!=nullptr,rect[0],rect[1],rect[2],rect[3]);Trace(message);}
  native_menu::endGroup(picture);
  Group(column,"ruleDescription",3);auto columnNode=Read<void*>(column,8);stretch(columnNode,Pair(1,0));alignment(columnNode,centered);ItemText(e.rule.label.c_str(),theme,0,white,e.rule.key.rarity);
  auto level="Level "+std::to_string(e.rule.key.level);ItemText(level.c_str(),theme,1,white,e.rule.key.rarity);
  static const char* rarities[]={"Common","Uncommon","Rare","Epic","Legendary"};ItemText(rarities[e.rule.key.rarity],theme,2,rarityColor,e.rule.key.rarity);
  alignas(16) uint8_t divider[256]{},lineImage[64]{};Group(divider,"ruleRarityLine",0);auto dividerNode=Read<void*>(divider,8);stretch(dividerNode,Pair(1,0));height(dividerNode,3);
  auto lineTexture=resolveImage(uiImages,lineImage,(const uint8_t*)theme+53840+112);if(lineTexture){auto texture=Read<uint32_t>(lineTexture);image(divider,rectangle(dividerNode),&texture,&rarityColor);}native_menu::endGroup(divider);native_menu::endGroup(column);
  alignas(16) uint8_t action[256]{};Group(action,"ruleRemove",0);sizePair(Read<void*>(action,8),Pair(88,44));alignment(Read<void*>(action,8),centered);if(Button("Remove",88))remove=i;native_menu::endGroup(action);native_menu::endGroup(row);
 }
 if(!entries.empty()){alignas(16) uint8_t bottomSpace[256]{};Group(bottomSpace,"rulesBottomSpace",0);height(Read<void*>(bottomSpace,8),8);native_menu::endGroup(bottomSpace);}
 endScrollingLayout(scrolling);
 alignas(16) uint8_t footer[256]{};Group(footer,"rulesFooter",0);stretch(Read<void*>(footer,8),Pair(1,0));padding(Read<void*>(footer,8),chromeInset);
 // Fork: keep-list checkboxes. Each click flips one rarity and writes
 // the whole list back to the ini (SetKeepList); Pump and the commit
 // path reload it on every scan, and these labels re-read it every
 // frame, so the boxes always show the live state. Panel stays open.
 Text("Keep Rarities (never auto-salvaged)",theme);
 // Rows are opened, filled and closed one at a time (the author's code
 // never opens two groups back to back) and sized explicitly, with the
 // alignment the Sort button uses to place its group — {1,2} centers a
 // fixed-size row in the footer instead of packing it left.
 const uint8_t rowCenter[2]={1,2};
 {auto keepList=native_salvage::LoadKeepList();static const char* rarityNames[5]={"Common","Uncommon","Rare","Epic","Legendary"};
  auto keepBox=[&](unsigned r){char boxLabel[40];sprintf_s(boxLabel,"%s %s",keepList.keep[r]?"[x]":"[ ]",rarityNames[r]);if(Button(boxLabel,200,4,36)){auto next=keepList;next.keep[r]=!next.keep[r];native_salvage::SetKeepList(next);}};
  alignas(16) uint8_t keepRow1[256]{};Group(keepRow1,"rulesKeepRow1",8,true);sizePair(Read<void*>(keepRow1,8),Pair(616,36));alignment(Read<void*>(keepRow1,8),rowCenter);
  keepBox(0);keepBox(1);keepBox(2);native_menu::endGroup(keepRow1);
  alignas(16) uint8_t keepRow2[256]{};Group(keepRow2,"rulesKeepRow2",8,true);sizePair(Read<void*>(keepRow2,8),Pair(408,36));alignment(Read<void*>(keepRow2,8),rowCenter);
  keepBox(3);keepBox(4);native_menu::endGroup(keepRow2);}
 // Fork: Auto toggle and the step-3 Salvage Bag sweep share one row.
 // The toggle writes the same [Fork] AutoSalvage ini key the automatic
 // path reads live (panel stays open); the sweep requests the drain
 // through the automatic path and closes the panel so it is visible.
 alignas(16) uint8_t actionRow[256]{};Group(actionRow,"rulesActionRow",8,true);sizePair(Read<void*>(actionRow,8),Pair(448,40));alignment(Read<void*>(actionRow,8),rowCenter);
 if(Button(native_salvage::KeepListAuto()?"Auto Salvage: On":"Auto Salvage: Off",250,4,40))native_salvage::SetAutoSalvage(!native_salvage::KeepListAuto());
 if(Button("Salvage Bag",190,4,40)){native_salvage::RequestSweep();opened=false;}
 native_menu::endGroup(actionRow);
 if(Button("Back",0))opened=false;native_menu::endGroup(footer);
 native_menu::endGroup(panel);
 // Sibling of the full-size content inside a stack layout: it cannot consume
 // list height. Draw last at the native context-menu layer, over the cards.
 bool mouseDown=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
 if(sortExpanded){
  alignas(16) uint8_t popup[256]{};Group(popup,"rulesSortPopup",8);auto popupNode=Read<void*>(popup,8);sizePair(popupNode,Pair(312,424));const uint8_t rightTop[2]={3,1};alignment(popupNode,rightTop);position(popupNode,Pair(-72,76));layer(popup,4);
  // Native ContextMenuWindow b0d712 chooses FancyBox style10. Style1 is
  // the inventory panel's ornamental corners and is wrong for this submenu.
  alignas(16) uint8_t popupFrame[256];memcpy(popupFrame,background,sizeof popupFrame);uint32_t popupStyle=10;memcpy(popupFrame,&popupStyle,4);frame(popup,popupFrame);const float popupInset[4]={24,24,24,24};padding(popupNode,popupInset);
  static const char* sortNames[]={"Date","Name","Rarity","Level"};drawingSort=true;int selected=-1;
  // Full context-button end caps need their native vertical room. The compact
  // 60px height made the top/bottom nine-slice artwork overlap in the middle.
  for(unsigned s=0;s<4;++s)if(Button(sortNames[s],264,0,88))selected=int(s);
  drawingSort=false;const auto popupRect=rectangle(popupNode);float bounds[4];memcpy(bounds,popupRect,sizeof bounds);layer(popup,0);native_menu::endGroup(popup);
  if(selected>=0){salvage::SelectRuleSort(sortBy,sortReversed,salvage::RuleSort(selected));sortExpanded=false;SortEntries();scrollOffset=0;remove=SIZE_MAX;}
  else if(mouseDown&&!mouseWasDown&&GetTickCount64()-lastButton>150){POINT cursor{};if(GetCursorPos(&cursor)&&ScreenToClient(GetForegroundWindow(),&cursor)){float x=cursor.x/pixelScale,y=cursor.y/pixelScale;if(x<bounds[0]||y<bounds[1]||x>bounds[0]+bounds[2]||y>bounds[1]+bounds[3])sortExpanded=false;}}
 }
 mouseWasDown=mouseDown;native_menu::endGroup(surface);
 if(remove<entries.size()){
  if(entries[remove].preview){entries.erase(entries.begin()+remove);return;}
  bool ruleSaved=native_menu::store.Remove(entries[remove].rule.key);Trace(ruleSaved?"AUTO_SALVAGE_RULE_REMOVED panel=inventory_details":"AUTO_SALVAGE_RULE_REMOVE_FAILED rules_unchanged=1");if(ruleSaved)Refresh();
 }
}
static void __fastcall Detail(uint8_t* model,bool transient){
 if(!opened||_ReturnAddress()!=inventoryCaller){previous(model,transient);return;}
 auto now=GetTickCount64();if(now-lastDraw>1500){opened=false;previous(model,transient);return;}lastDraw=now;
 LARGE_INTEGER begin{},end{};bool measure=profileUntil!=0;if(measure)QueryPerformanceCounter(&begin);Draw();
 if(measure){QueryPerformanceCounter(&end);auto duration=end.QuadPart-begin.QuadPart;++profileFrames;profileTicks+=duration;profileMax=(std::max)(profileMax,duration);
  bool slow=duration>frequency/20;if(GetTickCount64()>=profileUntil||!opened||slow){profileUntil=0;if(slow)opened=false;char line[220];sprintf_s(line,"RULES_PANEL_PROFILE frames=%llu average_us=%.2f maximum_us=%.2f slow_panel_closed=%u",profileFrames,profileTicks*1000000.0/frequency/profileFrames,profileMax*1000000.0/frequency,slow);Trace(line);}
 }
}
static bool Install(uint8_t* text,size_t size){
 if(!native_menu::Preview())return false;
 auto function=Unique(text,size,"48 89 5C 24 08 48 89 7C 24 10 55 48 8D AC 24 00 FF FF FF 48 81 EC 00 02 00 00 0F B6 DA");
 auto caller=Unique(text,size,"C7 85 10 04 00 00 00 00 80 3F E8 ?? ?? ?? ?? 48 8D 4D A0 E8 ?? ?? ?? ?? 41 0F 28 C5");
 stretch=(PairFn)Unique(text,size,"48 89 91 90 00 00 00 C3 CC CC CC CC CC CC CC CC 88 51 6D C3");
 sizePair=(PairFn)Unique(text,size,"48 89 54 24 08 0F 57 C0 F3 0F 10 54 24 08 33 C0 F3 0F 10 4C 24 0C F3 0F 5F D0");
 height=(FloatFn)Unique(text,size,"0F 57 C0 C7 81 94 00 00 00 00 00 00 00 F3 0F 5F C8 F3 0F 11 49 7C");
 padding=(PointerFn)Unique(text,size,"0F 10 02 0F 11 81 A8 00 00 00 C3");
 rectangle=(RectFn)Unique(text,size,"48 8D 81 14 01 00 00 C3");
 frame=(PointerFn)Unique(text,size,"48 8B C4 55 41 56 48 8D A8 B8 FE FF FF 48 81 EC 38 02 00 00 48 89 58 20 0F 57 C9");
 label=(TextFn)Unique(text,size,"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 48 8B DA 41 0F B6 F9");
 finishText=(FinishFn)Unique(text,size,"40 55 56 57 48 8D 6C 24 B9 48 81 EC F0 00 00 00 0F 29 BC 24 B0 00 00 00");
 image=(ImageFn)Unique(text,size,"4D 8B 09 45 8B 00 E9 ?? ?? ?? ?? CC CC CC CC CC 48 83 EC 38 F3 0F 10 0D");
 icon=(IconFn)Unique(text,size,"48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 70 48 8B 81 D0 00 00 00 0F 57 C0 41 0F 10 49 10");
 slice=(SliceFn)Unique(text,size,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 20 57 48 83 EC 30 48 8B F2 48 8B E9");
 imageContext=(ContextFn)Unique(text,size,"48 8B 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 4C 8B 41 28 C6 42 04 00");
 itemIcon=(ItemIconFn)Unique(text,size,"48 89 5C 24 10 48 89 6C 24 20 57 48 83 EC 50 49 8B D9 49 8B F8 48 8B E9 4D 85 C0");
 scrollbarDefaults=(FinishFn)Unique(text,size,"C7 44 24 08 FF FF FF FF 0F 57 C0 C7 44 24 0C 00 00 80 3F 0F 57 C9");
 alignment=(PointerFn)Unique(text,size,"0F B7 02 66 89 41 6C C3");
 layer=(LayerFn)Unique(text,size,"48 8B 41 08 48 8B 48 50 4C 8B 41 18 4D 85 C0 74 5E 41 83 78 08 04");
 baseGroup=(BaseGroupFn)Unique(text,size,"48 89 5C 24 08 57 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 48 8B F9 48 8B DA 45 33 C0 48 8B 52 10");
 overlayLayout=(OverlayFn)Unique(text,size,"33 C0 4C 8B D1 0F 57 C0 0F 11 81 B8 00 00 00 89 81 C8 00 00 00");
 position=(PairFn)Unique(text,size,"48 89 51 70 C3 CC CC CC CC CC CC CC CC CC CC CC 0F 10 02");
 slotBackground=(SlotBackgroundFn)Unique(text,size,"48 89 5C 24 08 55 56 57 48 8B EC 48 81 EC 80 00 00 00 48 8D 05 ?? ?? ?? ?? 48 C7 45 C8 18 00 00 00");
 resolveImage=(ResolveImageFn)Unique(text,size,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 60 41 0F 10 10 33 C0 C7 44 24 30 97 69 E8 F9 48 89 44 24 34 48 8B D9 49 8B 48 08 48 8B FA 89 44 24 3C 66 48 0F 7E D0 48 0B C8 0F 10 44 24 30 0F 11 54 24 40 0F 11 44 24 50 75 06 48 83 C3 10 EB 50 48 8B 43 10 48 8D 54 24 40 48 83 C3 10 41 B0 01");
 scrollingLayout=(ScrollFn)Unique(text,size,"48 8B C4 55 57 41 54 41 55 41 57 48 8D A8 C8 FD FF FF 48 81 EC 10 03 00 00");
 scrollDefaults=(FinishFn)Unique(text,size,"48 89 5C 24 10 55 48 8D 6C 24 A9 48 81 EC E0 00 00 00 48 8B D9 E8");
 endScrollingLayout=(FinishFn)Unique(text,size,"48 8B C4 55 53 56 57 41 56 41 57 48 8D A8 D8 FC FF FF 48 81 EC F8 03 00 00");
 if(!function||!caller||!stretch||!sizePair||!height||!padding||!rectangle||!frame||!label||!finishText||!image||!icon||!slice||!resolveImage||!imageContext||!itemIcon||!scrollbarDefaults||!alignment||!layer||!baseGroup||!overlayLayout||!position||!slotBackground||!scrollingLayout||!scrollDefaults||!endScrollingLayout){Trace("RULES_PANEL_UNSUPPORTED original_panel_retained=1");return false;}
 inventoryCaller=caller+15;
 if(!native_menu::Hook(function,19,(void*)&Detail,saved,(void**)&previous))return false;entry=function;ready=true;Trace("RULES_PANEL_READY native_icons_and_frame=1");return true;
}
static void Restore(){opened=false;ready=false;native_menu::Unhook(entry,19,(void*)&Detail,saved);}
}
