# Enshrouded Auto Salvage

A Windows client mod for quick salvaging and exact automatic-salvage rules. It uses the game's native salvage action and rewards.

- **Ctrl + right-click** a salvageable backpack or action-bar item to salvage it immediately.
- **Right-click > Auto Salvage** to save a rule for that exact item type, rarity, and level.
- Open the adjacent **...** button to view, remove, and sort rules by date, name, rarity, or level.
- Equipped items are excluded. Different rarities or levels do not match a rule.

Adding a rule also applies it to matching items already in your inventory. Start with an unwanted item and check the rule before keeping it enabled.

## Install

Download the ZIP from [Releases](../../releases), extract it, and read `INSTALL.txt`.

1. Close Enshrouded.
2. Install a Windows x64 ASI loader, such as [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases). A copy is included in the release's `OptionalLoader` folder. If you already use an ASI loader, keep it; do not overwrite another mod's `dinput8.dll` blindly.
3. Copy `QuickSalvage.asi` and `QuickSalvage.ini` beside `enshrouded.exe`. Steam: **Manage > Browse local files**.
4. Start the game normally. No server installation or Auto Loot mod is required.

Rules start empty. They are stored in `AutoSalvage.rules` beside the mod. Preserve that file and your settings when updating.

If you already installed the combined Auto Loot / Auto Salvage friend package, do not also load this standalone ASI: that would run two salvage instances. Have your installer or agent disable the older salvage component first.

Set `[QuickSalvage] Enabled=0` in `QuickSalvage.ini` to pause quick and automatic salvaging. Remove `QuickSalvage.asi` with the game closed to uninstall; preserve rules if wanted. Keep the ASI loader if other mods use it.

## Build

Requires Windows x64, Visual Studio 2022 C++ Build Tools and a Windows SDK. No game SDK or downloaded dependencies are needed.

From an **x64 Native Tools Command Prompt**:

```bat
build.cmd
test.cmd
```

Alternatively set `VCVARS64` to your existing `vcvars64.bat`. Output: `dist/QuickSalvage.asi` and `dist/QuickSalvage.ini`. The CRT is statically linked (`/MT`).

## Status and compatibility

This is a **prerelease** based on the development revision 63 used during October 2026 testing. Runtime function addresses are located with checked signatures; game updates can require a new build. The release records the reference client executable hash in `VALIDATION.txt`.

Observed in real Windows gameplay: Ctrl-right-click salvage with rewards, saved-rule automatic salvage, rule removal, and the rules interface. Seven native mock test programs cover matching, persistence, eligibility, request handling, aggregate-result storage, ordinary draw passthrough, and text-style relocation. An additional DLL startup regression exercises 32 small-stack thread attach/detach cycles and unload against the production callback, without installing game hooks.

Still awaiting independent verification: this standalone ASI's complete startup/restart path, automatic salvage of newly acquired matching items across ordinary play, and the latest revision's open-panel refresh. Native mocks do not establish those gameplay results. Proton is untested.

`QuickSalvage.log` records bounded startup and action diagnostics. No game executable or Steam API DLL is distributed or replaced. The repository excludes personal rules, private logs, saves, and the development Auto Loot adapters.

## License

[MIT](LICENSE). The optional ASI loader in the downloadable release has its own MIT notice.
