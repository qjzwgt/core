# The sql of this fork

Every sql change made in this fork is a file in this directory, and nowhere else. Nothing is added to
`sql/migrations/`: those files are the upstream ones, and `migrations_list.h` is generated from that folder at
configure time (`cmake/generators/migrations_list.h.cmake`), which is what the world server checks at startup and
refuses to start when one of them is missing. A change of this fork is not part of that check, so it is applied by
hand, after the database is otherwise up to date.

Every file here is for the `world` database, and every one of them must be safe to apply again:

- a table is added with `CREATE TABLE IF NOT EXISTS`, so the rows that a running server has tuned survive;
- the rows of one object are deleted and written back as a whole (one spell, one item, one bot talent), so a set
  that was retuned by hand is replaced whole and no half of it is left behind;
- a flag is set with `SET x = x | flag` rather than with a value that was read back;
- there is no `add_migration` wrapper, and the `migrations` table is not touched.

The name says what the file does: `Custom-ADD_...`, `Custom-REMOVE_...`, `Custom-RESET_...`,
`Item-<entry>_<name>`, `AQ-SET_GATES_...`.

## The order

The files do not depend on each other, except for one pair, and only because one holds the rows of the table that
the other creates:

1. `Custom-ADD_PLAYERBOT_TALENT_BUILD.sql` - the table `playerbot_talent_build`.
2. `Custom-PLAYERBOT_TALENT_BUILDS.sql` - the rows of that table, the builds of the talents of this fork.

`Custom-ADD_PLAYERBOT_GEAR_STAT_WEIGHTS.sql` creates a table without rows and is not read by any other file, so it
can go anywhere. Everything else changes tables that already exist. Applying the directory in name order therefore
works, and it is what a shell glob gives.

What a running server needs to see the change:

- the two `playerbot_*` tables: `.bot reload`, no restart;
- `creature_template`, `creature_linking`, `npc_vendor`, `gossip_*`, `item_template`, `spell_template`, `variables`,
  `playercreateinfo`: a restart, or the matching `.reload` sub command.

## Databases that already have today's migrations

`20261029022326`, `20261029091000` and `20261029130100` were moved out of `sql/migrations/` into
`Custom-REXXAR_NO_AGGRO.sql`, `Custom-ADD_PLAYERBOT_GEAR_STAT_WEIGHTS.sql` and
`Custom-ADD_PLAYERBOT_TALENT_BUILD.sql`. A database that recorded those ids keeps them: the world server prints a
"extra migrations" warning for them, which is only a warning, and the rows may be left in `migrations` or deleted.
A fresh database never sees them at all and only needs the files here.
