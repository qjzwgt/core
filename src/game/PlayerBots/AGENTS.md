# AGENTS.md - src/game/PlayerBots

The player bot subsystem: party bots summoned by a player (`PartyBotAI`), battle bots (`BattleBotAI`), the shared
behaviour of both (`CombatBotBaseAI`), and the manager and the loader of the saved characters (`PlayerBotMgr`).

## Start with the design document
`src/game/PlayerBots/BotGearScoring.md` explains how an empty equipment set is filled: which items a bot may get
at all, how much an item is worth for the talent of that bot, which weapon each talent wears, and how the random
property (词缀) of an item is chosen and fixed. The rating itself is in `BotGearScoring.{h,cpp}`; the decisions
about which slot to fill and which item wins stay in `CombatBotBaseAI::EquipRandomGearInEmptySlots()`. Keep the
document and the code in step - it is what the maintainer reads to tune the values.

## Words that matter here
- Quality names: 蓝色 / 精良 = `ITEM_QUALITY_RARE`, 绿色 / 优秀 = `ITEM_QUALITY_UNCOMMON`, 紫色 / 史诗 =
  `ITEM_QUALITY_EPIC`. The rules and the documents say "blue gear" and "pick up bound epic" in that sense; do not
  "correct" them to the English labels of the client, and do not confuse `RARE` with the epic quality.
- "Talent" and "spec" mean `BotGearSpec`. A bot learns its spells in one talent tab, so the tab that holds the
  learned spells says the talent; when nothing is learned, the role of the bot says it.

## Decisions of the maintainer (already made - ask before changing them)
1. The pool is what a player reaches while adventuring: `ItemPrototype::Discovered` ∪ world loot ∪ crafted gear,
   which is what makes the items with random properties reachable. Excluded: anything with a `RequiredSkill`
   (engineering, fishing, tailoring, ...) because bots never learn trade skills, gear gated by a reputation
   standing, an honor rank, a pvp reward, a world event or a holiday, the gear of the other faction, trinkets,
   relics, tabards and shirts.
2. No pick up bound epic gear at all. Pick up bound **blue** gear that comes from a quest or an instance is
   allowed only when its **item level** is at or below the level of the bot - the point is to keep a bot from
   wearing over tuned gear, so `RequiredLevel` is deliberately not the test. Gear bound when equipped is limited
   by its required level only.
3. Dual wield and two handed capability are settled before the gear is chosen, by `PrepareDualWield()`: spell 674
   is taught at the level where a player learns it from a trainer (rogue at 10, warrior and hunter at 20), and it
   is called again for the bots loaded from the database (`PartyBotAI`). An enhancement shaman goes to two handed
   weapons once its talents give `SKILL_2H_AXES` / `SKILL_2H_MACES`. Do not lean on the side effects of
   `Player::SatisfyItemRequirements()` for this: besides teaching 674 for any `INVTYPE_WEAPONOFFHAND` item, it
   raises the level of the bot to the required level of the item and sets its pvp rank, reputations and spells.
4. Weapon slots. Panel damage decides for arms warrior, retribution paladin, fury warrior, rogue, enhancement
   shaman, and for the ranged slot of a hunter - the attributes of those weapons are ignored completely and equal
   damage is resolved randomly. Tanks take a one handed weapon by damage plus the shield with the most armor, and
   the armor of a shield is never added to the score of an option. The casting talents, the feral druid and the
   melee weapons of a hunter are rated by attributes, comparing {two handed weapon or staff} against {one handed
   weapon + off hand item}. Wands are rated by attributes; relics are not worn; the damage that a random property
   adds to a weapon is not part of the damage used for ranking.
5. The random property of an item is read from its `item_enchantment_template` group, scored together with the
   item, and then fixed on the created item. That is why `Player::StoreNewItemInSlot()` exists:
   `Player::StoreNewItemInBestSlots()` rolls another property, so the bot would wear something other than what
   was rated. Naming 野熊 / 雄鹰 / 灵猴 / 猛虎 per role is not needed - the scoring produces them.
6. The whole set wears at most one copy of any item entry, whether the item is flagged unique or not, because
   quest rewards that are not flagged unique would otherwise be worn twice.
7. The stat weights (the "1% of hit or critical strike is worth 20 points" table in `defaultGearStatWeights`) are
   a starting point that the maintainer tunes by playing. A weight that looks wrong is data to report, not a bug
   to fix silently.

## Tuning without compiling
- `playerbot_gear_stat_weights(spec, stat, weight)` overrides single pairs only; everything without a row keeps
  the compiled value. `.bot reload` reads the table again, so changing a value needs neither a restart nor a
  build. The ids of `spec` and `stat` are documented in
  `sql/custom/repack/Custom-ADD_PLAYERBOT_GEAR_STAT_WEIGHTS.sql` and in the enums of `BotGearScoring.h`.
- `playerbot_talent_build(class, talent_page, seq, rank_spell, ranks, name)` is the build that a bot of one talent
  learns: one step per row, spent in `seq` order, so a bot of any level has the first steps of the build and its
  primary tree is the tree that gets filled. The table comes from
  `sql/custom/repack/Custom-ADD_PLAYERBOT_TALENT_BUILD.sql` and the rows are hand made content in
  `sql/custom/repack/Custom-PLAYERBOT_TALENT_BUILDS.sql`, which is applied after it and is re-runnable; the order
  of the sql of the fork is in `sql/custom/repack/README.md`. `.bot reload` reads the table again. The steps of a
  build are keyed by the class and the asked page even when they are talents of another tree, so the lookup of a
  `rank_spell` runs over the trees of the class; a row that is not a talent of that class is skipped with a
  `LOG_DBERROR` line naming the step, which is how a wrong spell id is found without a build.
- `.partybot gearscore <item>` with a bot selected prints, for the talent of that bot, every attribute an item
  gives, the weight used, the partial score, the random property that would be picked and the damage per second of
  the weapon. It is the answer to "why did the bot wear that instead of this".

## Deliberately not modelled
Trinkets (bots wear none), on use and combat procs, set bonuses, the enchantment of an item (the items are
created new), defense rating, weapon skills.

## Known rough edges, on purpose
- `.partybot add <abbreviation>` says which tree a new bot learns: the class, the role and the talent page come from
  `gearSpecAliases` in `BotGearScoring.cpp`, and `LearnPremadeSpecForClass()` fills that page with
  the build of that page from `playerbot_talent_build` instead of applying a premade template, which is chosen by
  role and not by talent; a talent with no rows there falls back to `LearnTalentsInTab()`, which fills the tree in
  row order and can miss a signature spell. A bot summoned without an abbreviation gets the page that its role has
  for its class (`GetSpecTalentPageForRole()`), so `AutoAssignRole()` still can not tell arms from fury or elemental
  from restoration; the rating is not told about the asked talent either, it reads the tree that was learned and the
  role back, which is also what tells the cat from the bear on the feral page of a druid.
- `Player::CanDualWield()` reads `m_canDualWield`, which is false after construction and is never set anywhere
  else, and `SKILL_DUAL_WIELD` has no grant path. Spell 674 is taught directly for that reason; a bot loaded from
  the database would otherwise lose the second weapon.
- Whether pick up bound **green** quest and instance gear should get the same item level gate as blue gear is an
  open question. Only blue is gated today.

## Commands of this subsystem
- The handlers live in `PlayerBotMgr.cpp`; the sub command tables of `.partybot` and `.bot` are in
  `src/game/Chat/Chat.cpp`, which is a CRLF file. Add new commands to those tables with the existing aligned
  column layout and keep the CRLF endings.
- The talents that a bot can be summoned with are arguments of `.partybot add` and not sub commands of
  `src/game/Chat/Chat.cpp`: the list is `gearSpecAliases` in `BotGearScoring.cpp` and what every page means is in
  `BotGearScoring.md`. A new talent is a row in that list, with nothing to change in the command tables.
- The level of a bot summoned by `.partybot add` comes from `PickPartyBotLevel()` in `PlayerBotMgr.cpp` and not
  from the level of the player: the same level up to 10, two levels either way above that, and the maximum player
  level once the player has it. An explicit level is parsed after the abbreviation, only for GM accounts and for
  `PartyBot.SkipChecks`, and is clamped to `MaxPlayerLevel`.
- Everything that reads a table of this subsystem is reached from `PlayerBotMgr::Load()`, which `.bot reload`
  calls again.
