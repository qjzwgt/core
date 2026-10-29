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
`Item-<entry>_<name>`, `AQ-SET_GATES_...`. A file may also be named after the feature it is, when the feature is
one thing that is applied at once: `Custom-BOT_CUSTOMIZATION.sql` is the whole of the bot customization, its
tables and its rows together, because a file of tables and a second file of their rows are one change that has to
be taken as a whole.

## The order

Apply the files in name order; a shell glob gives that, and it is right. The only order that matters is inside
`Custom-BOT_CUSTOMIZATION.sql`, which is read from top to bottom: a table comes before the rows written into it.
Nothing else here depends on anything else, so a file can be applied alone.

Two pairs are deliberately not applied together, because they are switches and not steps:
`AQ-SET_GATES_OPEN` / `AQ-SET_GATES_CLOSED`, and `Custom-ADD_GM_ISLAND_VENDORS` /
`Custom-REMOVE_GM_ISLAND_VENDORS`. That is why they are separate files, and why the rest of the directory cannot be
merged the way the bot customization was.

What a running server needs to see the change:

- the two `playerbot_*` tables of `Custom-BOT_CUSTOMIZATION.sql`: `.bot reload`, no restart;
- `creature_template`, `creature_linking`, `npc_vendor`, `gossip_*`, `item_template`, `spell_template`, `variables`,
  `playercreateinfo`: a restart, or the matching `.reload` sub command.

## Databases that already have today's migrations

`20261029022326`, `20261029091000` and `20261029130100` were moved out of `sql/migrations/`: their queries are now
in `Custom-REXXAR_NO_AGGRO.sql` and in `Custom-BOT_CUSTOMIZATION.sql`, which holds the gear weight table and the
talent build table with the rows of both. A database that recorded those ids keeps them: the world server prints an
"extra migrations" warning for them, which is only a warning, and the rows may be left in `migrations` or deleted.
A fresh database never sees them at all and only needs the files here.
