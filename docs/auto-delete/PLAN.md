# Auto Delete extension — plan board

Started: Oct 29, 2026 (build session tonight, after Eric's shift).
Spec: `SPEC.md (same folder)` (settled design — this board tracks
the work, the spec holds the decisions).
Repo: github.com/rifters/Enshrouded-Auto-Salvage (fork of emmveepee's);
local clone `F:\projects\Enshrouded-Auto-Salvage`. Upstream PR #2 for
the salvage fork is open; this extension is fork work for now.

## Settled invariants (from the spec)

- Keep-list guard outranks everything: a protected rarity is never
  salvaged and never deleted, exact rule or not.
- One rule per item key; the rule carries the action (Salvage | Delete)
  and, for Delete, a keep-count (default 0).
- Delete never requires `canSalvage` — its niche is non-salvageable
  junk (torches, campfires, waterskins).
- Keep-count is counted over everything carried (backpack + both
  action bar pages); bar copies satisfy the count first.
- The trash event only ever fires on backpack slots. The bar is
  trimmed by transfer (split to bag, trash there), never by trash.
- Splits go through the game's own transfer event with an explicit
  amount; no free backpack slot → defer to the next scan, never force.

## Work order

1. **Rules core v3** (Remy pre-drafts before tonight) — `rules.h`:
   action + keep-count on the rule, file header `QuickSalvageRules 3`,
   v2 files load as all-Salvage, matcher precedence (guard → Delete →
   Salvage). Proved in the Linux harness before it reaches Eric.
2. **Menu + panel** — "Auto Delete" row beside "Auto Salvage" in the
   right-click menu (setting a Delete rule switches an existing
   Salvage rule's action); rules panel shows action + keep-count.
3. **Scanner** — delete pass inside the existing Pump/commit flow:
   two-phase keep-count enforcement (bag phase, then bar phase),
   same Pending ledger, `[Fork] AutoDelete` gate, `AUTO_DELETE_*`
   traces. Native trash/transfer calls stubbed until step 4.
4. **RE hunt** (together, on Eric's PC — the gate) — find where the
   game raises `TrashItemEvent` (hash 0x248b58ac) and
   `TransferItemToInventoryEvent`; signature-check both like the
   salvage function, never hardcode. `ItemAction` Trash=2 and the
   transfer event's Destroy=4 are the alternate candidate paths.
5. **Wire + build + test** — replace the stubs, VS2022 x64 build,
   `test.cmd` suites, then the spec's in-game test list: guard test,
   bar tests, keep-count tests (5→2, 4+4→3, bar 8→3), full-bag edge,
   v2 rules file loading as all-Salvage.

## Status

- [x] Design settled + spec written (Oct 29, 2026)
- [x] GitHub connected; fork + PR #2 verified live (Oct 29, 2026)
- [ ] Step 1 — rules core v3 draft + harness proof
- [ ] Step 2 — menu + panel draft
- [ ] Step 3 — scanner draft (stubbed natives)
- [ ] Step 4 — RE hunt (needs Eric at his PC)
- [ ] Step 5 — wire, build, in-game tests

Drafts for this extension land in `auto-delete/src/` as they're
written, same review flow as the salvage work: Remy drafts, Eric
applies and commits.
