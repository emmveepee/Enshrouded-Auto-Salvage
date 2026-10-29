# Auto Delete extension — design (Eric's ask, Oct 29, 2026)

Use the base mod's per-item rule design (emmveepee's exact
item type + rarity + level rules) to **auto-delete** bag items —
the junk the looter (Auto Loot Plus) is set to ignore. The looter's
ignore list only stops auto-pickup; anything that lands in the bag
another way (manual grab, area loot, chest transfer, rewards) stays.
A bag-side rule, managed like the salvage rules, catches it however
it got in.

**Confirmed examples (Eric, Oct 29, 2026): torches, campfires,
waterskins.** Non-salvageable utility items that keep landing in the
bag despite being excluded from loot. The inflow routes the looter's
ignore list cannot guard: crafting output (crafted items go straight
to the bag), quest rewards, the game's own area-loot and manual
pickups (the filter only governs the looter's pickup path), and
chest/container transfers. All three are plain items — Common,
fixed level — so one exact-match rule per type covers every instance.

**Keep-count — SETTLED (Eric, Oct 29, 2026): yes, with split-then-trash.**
A Delete rule carries a keep-count K (default 0 = delete all). The
keep total counts **everything he carries** — backpack plus both
action bar pages (the same SlotSelection pair the backpack filter
already reads). The one hard invariant: **the trash event only ever
fires on backpack slots.** The bar is trimmed by transfer, never by
trash. Enforcement runs in two phases per scan:
- **Phase 1 — bag.** Bar copies satisfy the keep count first, so the
  bag preserves only the remainder (K_bag = K − bar count, floored
  at 0). Excess in the bag (E = T_bag − K_bag): trash whole stacks
  whose count ≤ E; when the remaining excess is smaller than the
  next stack, **split that stack first** — the excess moves to a
  free backpack slot as its own stack, then that slot is trashed,
  leaving a preserved stack of exactly the kept amount.
- **Phase 2 — bar** (Eric's correction, Oct 29, 2026): bar stacks
  stack too, so exemption would leave them over the count forever.
  If the bar alone exceeds K, split the bar stack down — the excess
  portion transfers to a free backpack slot (phase 1 has usually
  just freed one) and is trashed there by the normal bag path. The
  bar slot keeps its position with exactly its kept share.

If no free backpack slot exists when a split needs one, defer to
the next scan — never force it.

Both halves are game-native events (types, build 1273043):
`keen::InventorySlotId` (8 B) is just {page entity @0, slot index @4} —
exactly what the mod's Candidate already tracks.
- **Split**: `keen::ecs::TransferItemToInventoryEvent` (40 B) —
  playerEntityId, target/source inventory + slot ids, an
  `InventoryActionType` (One / Half / All / **CustomAmount** /
  Destroy) and an explicit `amount: uint16`. A CustomAmount transfer
  to an empty slot *is* the game's stack split.
- **Trash**: `keen::ecs::TrashItemEvent` {playerEntityId, slotId}, with
  a sibling action model `keen::ecs::ItemAction` whose
  `ItemActionType` includes **Trash = 2**. (The transfer event's own
  Destroy action type is a third candidate path — the RE hunt picks
  whichever the UI actually fires.)

## Rule model — one rule per item, with an action

- The existing rule `Key{item, rarity, level}` gains an **action**:
  `Salvage` (today's behavior) or `Delete`. One rule per key — creating
  an Auto Delete rule for an item that already has an Auto Salvage rule
  *switches* its action instead of stacking a conflicting second rule.
- Rules file bumps to `QuickSalvageRules 3`; v2 files load with every
  rule defaulted to Salvage. Same transactional load / atomic save.
- Right-click menu gains an **"Auto Delete"** row next to "Auto Salvage"
  (same drawn-row + hit-test mechanism). The rules panel shows the
  action per rule; Remove works as today.

## Matching and safety — precedence, checked in this order

1. **Keep-list guard first**: a protected rarity is never salvaged and
   never deleted, even with an exact rule (same hard guard as salvage —
   proven pattern: the Legendary dagger test).
2. **Exact Delete rule** → delete.
3. **Exact Salvage rule / Auto keep-list match** → salvage (unchanged).

- Delete inherits the fork's **backpack-only** filter (SlotSelection bar
  pages excluded) and the owned / not-equipped eligibility checks —
  but **not** `canSalvage`. That is the point: its niche is exactly the
  non-salvageable junk the salvage path can never touch.
- Whole stack in the slot, matching how the game's own trash works.

## Execution — reuse the machinery

- Same `Pump()` scan (~200 ms), same Pending ledger (with the fork's
  10 s retry expiry), same UI-thread commit with fresh revalidation;
  only the final call differs (native trash instead of native salvage).
- The **Salvage Bag sweep** also processes Delete rules — one clean bag.
- Config: `[Fork] AutoDelete=1` default ON for exact rules (parity with
  how exact salvage rules behave), settable to 0 to disable the whole
  delete path without removing rules.
- Traces mirror the salvage ones (`AUTO_DELETE_QUEUED`,
  `AUTO_DELETE_COMMITTED` with item/rarity/level): deletion is
  irreversible, so every deletion is audit-logged.

## The actuator — the one real piece of work

The base mod has **no** delete path (searched: nothing). But the game
has its own trash flow, visible in the build 1273043 types:

- **`keen::ecs::TrashItemEvent`** (24 B, component hash `0x248b58ac`):
  `playerEntityId` @8, `slotId: keen::InventorySlotId` @12. The UI's
  trash action fires this event naming the player and the inventory
  slot; a game system destroys the stack. That handler/raiser pair is
  the native path to find.
- **`keen::ecs::AdminDestroyItemAction`** (12 B, hash `0x176da66d`):
  carries just an `inventorySlotId` — a debug/admin destroy path;
  secondary lead.

RE plan, same discipline as the salvage function: xref the events'
type hashes in the binary to find where they are raised (the
inventory UI's item-action code — salvage, drag/split, and trash
handlers cluster together) and signature-check the chosen call,
never hardcode it. Going through the game's own paths matters: they
handle replication/cleanup, so co-op stays in sync. Directly clearing
the ItemStack slot in memory is the fallback only — it bypasses all
of that and would need save/reload and co-op desync testing before
it could be trusted.

## Test plan (same discipline as the salvage tests)

- Delete rule on a non-salvageable junk item → move it into the bag
  from a chest → deleted, trace shows it, nothing else touched.
- Guard test: delete rule on a keep-listed (Legendary) item → survives.
- Bar test: delete-ruled item placed on an action bar page → survives.
- Keep-count test: rule with K=2 on a stack of 5 → a stack of exactly 2
  remains, 3 deleted via the split-off stack; stacks of 4+4 with K=3 →
  one stack of 3 remains. Full-bag edge: no free slot → nothing forced,
  deletion resumes once a slot frees.
- Bar-counting test: 2 waterskins on the action bar, K=2, bag stack of
  5 → all 5 deleted, bar untouched. With 1 on the bar and K=2 → the
  bag keeps a stack of exactly 1.
- Bar-trim test: bar stack of 8 torches, K=3, bag empty → the bar
  stack is split down to 3 in place, the excess 5 lands in the bag
  and is trashed. Combined: bag 4 + bar 8 with K=3 → bag emptied
  first, then the bar split to 3.
- Reload test: rules persist as v3; an old v2 rules file loads as
  all-Salvage.
