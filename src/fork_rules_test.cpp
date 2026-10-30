// Proof test for the fork additions in rules.h (KeepList + AutoSalvageMatch).
// Build: g++ -std=c++17 -I<dir-of-rules.h> fork_rules_test.cpp -o fork_rules_test
#include "rules.h"
#include <cstdio>
using namespace salvage;
static int failures = 0;
static void Check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}
int main() {
    // --- KeepList::Parse ---
    auto def = KeepList::Parse("");
    Check(def.Protects(4) && !def.Protects(3) && !def.Protects(0), "empty string -> default protects Legendary only");
    auto junk = KeepList::Parse("abc");
    Check(junk.Protects(4) && !junk.Protects(3), "garbage string -> default kept (typo-safe)");
    auto leg = KeepList::Parse("4");
    Check(leg.Protects(4) && !leg.Protects(3), "\"4\" protects Legendary only");
    auto epic = KeepList::Parse("3,4");
    Check(epic.Protects(3) && epic.Protects(4) && !epic.Protects(2), "\"3,4\" protects Epic + Legendary");
    auto loose = KeepList::Parse(" 0 ; 2 ");
    Check(loose.Protects(0) && loose.Protects(2) && !loose.Protects(4), "\" 0 ; 2 \" protects Common + Rare only");
    auto outOfRange = KeepList::Parse("7");
    Check(outOfRange.Protects(4) && !outOfRange.Protects(3), "\"7\" (no valid token) -> default kept");

    // --- AutoSalvageMatch ---
    Rules rules;
    Key commonSword{1001, 0, 20}, rareSword{1002, 2, 20}, legendarySword{1003, 4, 20};
    rules.Add(rareSword, "Rare Sword");
    rules.Add(legendarySword, "Legendary Sword");
    auto keepLeg = KeepList::Parse("4");

    Check(AutoSalvageMatch(rareSword, rules, keepLeg, false), "exact rule matches with auto off -> salvage");
    Check(!AutoSalvageMatch(commonSword, rules, keepLeg, false), "no rule, auto off -> keep");
    Check(AutoSalvageMatch(commonSword, rules, keepLeg, true), "no rule, keep-list auto on -> salvage");
    Check(!AutoSalvageMatch(legendarySword, rules, keepLeg, false), "HARD GUARD: Legendary rule cannot override keep-list (auto off)");
    Check(!AutoSalvageMatch(legendarySword, rules, keepLeg, true), "HARD GUARD: Legendary protected with auto on");
    Check(!AutoSalvageMatch(Key{0, 0, 1}, rules, keepLeg, true), "invalid key (item 0) never matches");
    Check(!AutoSalvageMatch(Key{1001, 9, 20}, rules, keepLeg, true), "invalid key (rarity 9) never matches");

    auto keepEpicLeg = KeepList::Parse("3,4");
    Key epicSword{1004, 3, 20};
    Check(!AutoSalvageMatch(epicSword, rules, keepEpicLeg, true), "Epic protected when 3,4 configured, even with auto on");
    Check(AutoSalvageMatch(rareSword, rules, keepEpicLeg, true), "Rare still salvages when 3,4 configured");

    std::printf(failures ? "RESULT: %d FAILURE(S)\n" : "RESULT: ALL PASS\n", failures);
    return failures ? 1 : 0;
}
