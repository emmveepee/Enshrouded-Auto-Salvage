# Changelog

## Unreleased

- Fix startup stack overflow on small-stack thread notifications by isolating the large path buffer in a non-inlined, process-attach-only helper. Add a native DLL lifecycle regression.

## v0.1.0-alpha.1

Initial standalone source publication and ASI prerelease, based on development revision 63.

- Quick salvage with Ctrl-right-click and native rewards.
- Exact automatic rules by item type, rarity and displayed level.
- Persistent rules, removal, scrolling and date/name/rarity/level sorting.
- Rules-panel revision tracking to refresh after saved rule changes.
- Portable build/test scripts and seven native mock test programs.

See README for gameplay validation limits.
