#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <tuple>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cctype>
namespace salvage {
struct Key {
 uint32_t item=0; unsigned rarity=0,level=0;
 bool operator<(const Key& b)const {if(item!=b.item)return item<b.item;if(rarity!=b.rarity)return rarity<b.rarity;return level<b.level;}
 bool operator==(const Key& b)const{return item==b.item&&rarity==b.rarity&&level==b.level;}
 bool Valid()const{return item!=0&&rarity<=4&&level>=1&&level<=65535;}
};
struct Rule {Key key;std::string label;uint64_t created=0;};
enum class RuleSort {Date,Name,Rarity,Level};
inline bool RuleBefore(const Rule& a,const Rule& b,RuleSort sort){
 if(sort==RuleSort::Name){auto lower=[](std::string s){for(char& c:s)c=char(std::tolower((unsigned char)c));return s;};auto x=lower(a.label),y=lower(b.label);if(x!=y)return x<y;}
 if(sort==RuleSort::Rarity&&a.key.rarity!=b.key.rarity)return a.key.rarity>b.key.rarity;
 if(sort==RuleSort::Level&&a.key.level!=b.key.level)return a.key.level>b.key.level;
 if(a.created!=b.created)return a.created>b.created;
 return a.key<b.key;
}
inline bool RuleBefore(const Rule& a,const Rule& b,RuleSort sort,bool reversed){return reversed?RuleBefore(b,a,sort):RuleBefore(a,b,sort);}
inline void SelectRuleSort(RuleSort& current,bool& reversed,RuleSort selected){if(current==selected)reversed=!reversed;else{current=selected;reversed=false;}}
class Rules {
 std::map<Key,Rule> entries;
 static bool SafeLabel(const std::string& s){return s.size()<=1024&&s.find_first_of("\r\n\t") == std::string::npos;}
 public:
 bool Add(const Key& k,const std::string& label){
  if(!k.Valid()||!SafeLabel(label))return false;
  uint64_t now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  for(auto& e:entries)if(e.second.created>=now)now=e.second.created+1;
  return entries.emplace(k,Rule{k,label,now}).second;
 }
 bool Remove(const Key& k){return entries.erase(k)!=0;}
 bool Contains(const Key& k)const{return k.Valid()&&entries.count(k)!=0;}
 size_t Size()const{return entries.size();}
 std::vector<Rule> List()const{std::vector<Rule> v;for(auto& e:entries)v.push_back(e.second);return v;}
 std::string Encode()const{std::ostringstream s;s<<"QuickSalvageRules 2\n";for(auto& e:entries)s<<e.first.item<<'\t'<<e.first.rarity<<'\t'<<e.first.level<<'\t'<<e.second.created<<'\t'<<e.second.label<<'\n';return s.str();}
 // Parse transactionally. Corrupt/unsupported files never partially activate
 // an unintended list, and a failed reload leaves the current rules intact.
 bool Decode(const std::string& data){
  if(data.size()>1024*1024)return false;std::istringstream stream(data);std::string line;if(!std::getline(stream,line))return false;
  bool dates=line=="QuickSalvageRules 2";if(!dates&&line!="QuickSalvageRules 1")return false;
  Rules next;while(std::getline(stream,line)){if(line.empty())continue;std::istringstream row(line);std::string a,b,c,label,stamp;if(!std::getline(row,a,'\t')||!std::getline(row,b,'\t')||!std::getline(row,c,'\t'))return false;
   uint64_t created=0;if(dates){if(!std::getline(row,stamp,'\t')||stamp.empty()||stamp.size()>19)return false;for(char ch:stamp){if(ch<'0'||ch>'9'||created>(UINT64_MAX-unsigned(ch-'0'))/10)return false;created=created*10+unsigned(ch-'0');}}
   if(!std::getline(row,label))return false;
   auto number=[](const std::string& value,uint32_t& result){if(value.empty()||value.size()>10)return false;uint64_t n=0;for(char ch:value){if(ch<'0'||ch>'9')return false;n=n*10+unsigned(ch-'0');if(n>UINT32_MAX)return false;}result=uint32_t(n);return true;};
   uint32_t item,rarity,level;if(!number(a,item)||!number(b,rarity)||!number(c,level))return false;Key key{item,rarity,level};
   if(!key.Valid()||!SafeLabel(label)||!next.entries.emplace(key,Rule{key,label,created}).second)return false;
  }entries.swap(next.entries);return true;
 }
};
// Fork: rarity keep-list. A protected rarity is never salvaged by the
// keep-list paths and acts as a hard guard over exact rules too.
struct KeepList {
 bool keep[5]={false,false,false,false,true}; // Default: Legendary only.
 bool Protects(unsigned rarity)const{return rarity<=4&&keep[rarity];}
 // Parses a list like "3,4" (spaces/semicolons also accepted). Tokens that
 // are not rarities 0..4 are ignored; if no valid token is found the
 // default stays, so a typo in the ini can never unprotect everything.
 static KeepList Parse(const std::string& text){
  KeepList out;bool flags[5]={false,false,false,false,false};bool any=false;size_t i=0;
  while(i<text.size()){
   if(text[i]<'0'||text[i]>'9'){++i;continue;}
   unsigned value=0;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){value=value*10+unsigned(text[i]-'0');++i;}
   if(value<=4){flags[value]=true;any=true;}
  }
  if(any)for(unsigned r=0;r<5;++r)out.keep[r]=flags[r];
  return out;
 }
};
// Fork gate for the automatic path (and later the bag sweep): hard guard
// first, then either an exact rule match or, with keep-list auto enabled,
// any valid item. Eligibility is still checked separately by the caller.
inline bool AutoSalvageMatch(const Key& key,const Rules& rules,const KeepList& keep,bool keepListAuto){
 if(!key.Valid()||keep.Protects(key.rarity))return false;
 return keepListAuto||rules.Contains(key);
}
// The caller derives these flags from native owned inventory and native item
// action availability. Unknown item metadata is never treated as a match.
struct Candidate {Key key;uint64_t session=0;uint32_t page=0,pide=0,count=0;uint8_t slot=0;bool owned=false,equipped=false,canSalvage=false,metadataValid=false;};
inline bool Eligible(const Candidate& c){return c.key.Valid()&&c.session&&c.page&&c.pide&&c.slot<8&&c.count&&c.owned&&!c.equipped&&c.canSalvage&&c.metadataValid;}
class Pending {
 struct Fingerprint {uint64_t session;uint32_t pide,item;bool operator<(const Fingerprint& b)const {return std::tie(session,pide,item)<std::tie(b.session,b.pide,b.item);}};
 std::set<Fingerprint> sent;
 uint64_t session=0;
 public:
 bool Claim(const Candidate& c){if(!Eligible(c))return false;if(c.session!=session){sent.clear();session=c.session;}return sent.insert({c.session,c.pide,c.key.item}).second;}
 void Cancel(const Candidate& c){sent.erase({c.session,c.pide,c.key.item});}
 // Retain only requests for items that still exist. A removed and subsequently
 // acquired item can be processed again; an unchanged failed item is not spammed.
 void Observe(const std::vector<Candidate>& inventory){std::set<Fingerprint> live;for(auto& c:inventory)if(c.session==session)live.insert({c.session,c.pide,c.key.item});for(auto i=sent.begin();i!=sent.end();)if(!live.count(*i))i=sent.erase(i);else ++i;}
};
class Gesture {
 bool wasDown=false,latched=false;
 public:
 void Update(bool rightDown,bool ctrl,bool focused){if(!rightDown){wasDown=false;latched=false;return;}if(!wasDown){latched=ctrl&&focused;wasDown=true;}if(!focused)latched=false;}
 bool Consume(){if(!latched)return false;latched=false;return true;}
};
}
