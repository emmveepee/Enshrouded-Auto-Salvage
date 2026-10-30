# Enshrouded Auto Salvage

A Windows client mod for quick salvaging and exact automatic-salvage rules. It uses the game's native salvage action and rewards.

> **Fork of [emmveepee/Enshrouded-Auto-Salvage](https://github.com/emmveepee/Enshrouded-Auto-Salvage).**
> This fork adds a rarity keep-list, a backpack-only filter, an Auto mode,
> and in-panel controls (rarity checkboxes, an Auto toggle, and a Salvage
> Bag sweep button). See **Fork additions** below. Everything from the
> original mod still works as described.

Ctrl + right-click a salvageable backpack or action-bar item to salvage it immediately.
Right-click > Auto Salvage to save a rule for that exact item type, rarity, and level.
Open the adjacent ... button to view, remove, and sort rules by date, name, rarity, or level.
Equipped items are excluded. Different rarities or levels do not match a rule.
Adding a rule also applies it to matching items already in your inventory. Start with an unwanted item and check the rule before keeping it enabled.

## Fork additions

**Rarity keep-list.** Rarities on the keep-list are *never* salvaged by Auto
mode or the bag sweep — even if an exact rule matches them. The keep-list
always wins. Configure it in the `[Fork]` section of QuickSalvage.ini, or
in game from the rules panel's checkboxes (Common, Uncommon, Rare, Epic,
Legendary). The value is a list like `3,4` (Epic and Legendary), or `none`
to protect nothing. If the value is missing or unparsable, only Legendary
is protected, so a typo can never unprotect everything.

**Backpack only.** Auto mode and the sweep only ever touch the backpack.
The two action-bar pages are identified from the game's own SlotSelection
data and skipped, and a queued item that is dragged onto the bar before
its salvage commits is refused at commit time. Equipped items are never
salvaged, as in the original.

**Auto mode.** With `[Fork] AutoSalvage=1` (or the panel's Auto Salvage
toggle), any unprotected salvageable item is salvaged as soon as it is in
the backpack — the current bag included, not just new pickups. Intended
for farming low-level areas. **Off by default.**

**Salvage Bag button.** The rules panel has a Salvage Bag button that
sweeps the backpack once, through the same guarded path as Auto mode.
The panel also hosts the rarity checkboxes and the Auto Salvage on/off
toggle; both rewrite the ini in game and take effect immediately.

**Fixes over the base mod.**

- A salvage request the game silently drops no longer blocks its item
  for the rest of the session: pending claims expire after 10 seconds
  and are retried.
- The Ctrl+right-click mouse observer retries installation until it
  lands, instead of failing silently when it loses a startup race.

### [Fork] configuration

```ini
[Fork]
KeepRarities=4   ; rarities never auto-salvaged: 0 Common, 1 Uncommon,
                 ; 2 Rare, 3 Epic, 4 Legendary; comma list, or "none"
AutoSalvage=0    ; 1 = salvage unprotected items on pickup (and in bag)
BackpackOnly=1   ; 1 = auto/sweep never touch the action bar
PageProbe=0      ; 1 = developer diagnostic logging of inventory pages
```

All four are read live; the panel's checkboxes and toggle write
`KeepRarities` and `AutoSalvage` back to the ini.

## Install

Download the ZIP from Releases, extract it, and read INSTALL.txt.

Close Enshrouded.
Install a Windows x64 ASI loader, such as Ultimate ASI Loader. A copy is included in the release's OptionalLoader folder. If you already use an ASI loader, keep it; do not overwrite another mod's dinput8.dll blindly.
If dinput8.dll is already in use by another mod, you do not need Ultimate ASI Loader at all: a chaining loader such as Mod Loader Proxy can load the ASI directly. Add QuickSalvage.asi to the loader's chain.ini, keep QuickSalvage.ini beside enshrouded.exe, and start the game normally.
Copy QuickSalvage.asi and QuickSalvage.ini beside enshrouded.exe. Steam: Manage > Browse local files.
Start the game normally. No server installation or Auto Loot mod is required.
Rules start empty. They are stored in AutoSalvage.rules beside the mod. Preserve that file and your settings when updating.

If you already installed the combined Auto Loot / Auto Salvage friend package, do not also load this standalone ASI: that would run two salvage instances. Have your installer or agent disable the older salvage component first.

Set [QuickSalvage] Enabled=0 in QuickSalvage.ini to pause quick and automatic salvaging. Remove QuickSalvage.asi with the game closed to uninstall; preserve rules if wanted. Keep the ASI loader if other mods use it.

## Build

Requires Windows x64, Visual Studio 2022 C++ Build Tools and a Windows SDK. No game SDK or downloaded dependencies are needed.

From an x64 Native Tools Command Prompt:

```
build.cmd
test.cmd
```

Alternatively set VCVARS64 to your existing vcvars64.bat. Output: dist/QuickSalvage.asi and dist/QuickSalvage.ini. The CRT is statically linked (/MT).

## Status and compatibility

This is a prerelease based on development revision 63, with a startup stack-overflow fix in v0.1.0-alpha.2 contributed by BenWare144 in PR #1. Update from alpha.1 if the mod crashes during startup. Runtime function addresses are located with checked signatures; game updates can require a new build. The release records the reference client executable hash in VALIDATION.txt.

Observed in real Windows gameplay: Ctrl-right-click salvage with rewards, saved-rule automatic salvage, rule removal, and the rules interface. Seven native mock test programs cover matching, persistence, eligibility, request handling, aggregate-result storage, ordinary draw passthrough, and text-style relocation. An additional DLL startup regression exercises 32 small-stack thread attach/detach cycles and unload against the production callback, without installing game hooks.

Fork verification (October 29, 2026, client build 1273043, real gameplay):

- Automatic salvage of newly acquired items during ordinary play — Auto mode drained a full backpack and salvaged fresh pickups while farming.
- The keep-list guard refusing a saved rule for a Legendary item, and the action-bar filter holding against a rule for an item seated on the bar.
- The Salvage Bag sweep draining the backpack through the same guarded path.
- The rules panel's rarity checkboxes and Auto toggle writing QuickSalvage.ini live, with the open panel reflecting changes (including rule removal) without reopening.
- The fork was rebuilt and relaunched repeatedly across the test sessions and loaded on every launch; saved rules persisted across restarts.

Still unverified for this fork: a full unload/reload regression run, and the pending-claim retry under a live dropped request — the dropped-request failure it fixes was observed on the base logic, and the expiry itself is covered by standalone tests. Proton is untested.

QuickSalvage.log records bounded startup and action diagnostics. No game executable or Steam API DLL is distributed or replaced. The repository excludes personal rules, private logs, saves, and the development Auto Loot adapters.

## License

MIT. The optional ASI loader in the downloadable release has its own MIT notice.
