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
2. No pick up bound epic gear at all. Pick up bound gear of **green** or **blue** quality that comes from a quest
   or an instance is allowed only when its **item level** is at or below the level of the bot - the point is to
   keep a bot from wearing over tuned gear, so `RequiredLevel` is deliberately not the test. Gear bound when
   equipped is limited by its required level only.
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
   Warriors of any talent and the combat rogue wear no dagger and no fist weapon in either hand;
   `allowDaggersAndFistWeapons` of `BotGearWeaponRules` says it, and the items of those kinds never reach the
   candidate pool of such a bot, so no slot of it can hold one.
5. The random property of an item is read from its `item_enchantment_template` group, scored together with the
   item, and then put on the item before the item is equipped. Equipping is what applies the mods of an item and
   what writes the visible fields of the equipment of the player, which is all that another player sees of what a
   bot wears, its suffix and the attributes of that suffix included; a property set after the equip leaves an item
   that shows no attributes to the one who looks at the bot. That is why `Player::StoreNewItemInSlot()` exists:
   `Player::StoreNewItemInBestSlots()` rolls another property, so the bot would wear something other than what
   was rated. Naming 野熊 / 雄鹰 / 灵猴 / 猛虎 per role is not needed - the scoring picks the best of the group of
   that item where the group has one of them. An item of a group always wears one of its properties: when none of
   them gives an attribute that the talent values, the property that the creation of the item for a player would
   roll is taken, because an item without its suffix is an item that does not exist.
6. The whole set wears at most one copy of any item entry, whether the item is flagged unique or not, because
   quest rewards that are not flagged unique would otherwise be worn twice.
7. The stat weights (the "1% of hit or critical strike is worth 20 points" table in `defaultGearStatWeights`) are
   a starting point that the maintainer tunes by playing. A weight that looks wrong is data to report, not a bug
   to fix silently.
8. `PartyBot.SpiritResurrection`, off by default, says how a party bot that died outside a battleground comes
   back. Off, the bot raises itself as soon as a player of the group stands by its corpse, which is what upstream
   does. On, the bot stays by its unreleased body and waits to be raised while a living member of the group is to
   be counted on to raise it: one that knows a spell with the resurrect effect, which is the test that
   `PopulateSpellData()` uses too, Rebirth excepted - a druid keeps Rebirth for an emergency, and it is the one
   resurrection that the core marks with `SPELL_ATTR_EX3_NO_RES_TIMER`, which is how it is recognised here rather
   than by the ids of its five ranks. A group whose only resurrection is a Rebirth does not hold the bot either:
   the bot releases its body, and a resurrection wants a body that is still there, so the druid that knows nothing
   else than Rebirth - a bot druid, whose `m_resurrectionSpell` that is - no longer has the bot as a target. That
   is decided and accepted, Rebirth stays for the emergency it is meant for. Where nobody is to be counted on the
   body is released at once; outside an instance the release timer that the core gives a player releases it after
   six minutes, which is wanted - that is what a player who is left waiting does. The spirit then walks back to its
   bones and takes the body back under the rules of `WorldSession::HandleReclaimCorpseOpcode`; a spirit whose
   bones are in another copy of the map, or that no path reaches, is raised by the spirit healer through
   `WorldSession::SendSpiritResurrect`. Neither of those paths applies the 2479 that the self-raising one does.
   The soulstone, the Twisting Nether and the Reincarnation of a bot stay with the core's own
   `Player::SelectResurrectionSpellId()`.
9. The recovery of a bot that is out of combat follows a player. A bot sits down to eat and drink only once its
   health has gone below 80% or its mana below 70%; a bot under 50% of its health puts a bandage on before it
   thinks of food, and it goes on with the food and the water when its bandage is not ready. The food and the water
   are cast as spells rather than taken out of the bags, but of the rank of the level of the bot: the ranks of the
   generic "Food" and "Drink" spells are the tables `foodTiers` and `drinkTiers` of `CombatBotBaseAI.cpp`, keyed by
   the level that the victuals carrying each rank ask for, which is what the vendors of a region of that level sell.
   A rank whose spell the Spell.dbc of the running patch does not have is passed over, so a patch of a low maximum
   level keeps the ranks that it has. The core sits down the one that eats and takes the buff away from a character
   that stands up again (`AURA_INTERRUPT_STANDING_CANCELS`), so the update does not stand a bot up while it still
   needs its meal, and a bot called into combat stops eating. The bandage is a real item, and its stack is put in
   the bags of the bot when the bags have none, as the ammo of a hunter is put in the quiver. The bandage of a bot
   is the tier whose first aid rank (`required_skill_rank` of the item) the first aid of a player of that level has
   reached; those levels are the table `bandageTiers` of `CombatBotBaseAI.cpp`. The three tables are some of the few
   places of this subsystem where item and spell ids are written in C++ - to tune the tiers means moving them into
   `sql/custom/repack/`, not keeping a second list. A bot does not learn the first aid skill: the core checks the
   `RequiredSkill` of an item when it loads the prototypes and never when the item is used.
10. A bot rides the mount of its own race, or the mount of its class for a paladin and a warlock, and it calls the
   summon as a player calls it. The summons are the table `mountTiers` of `CombatBotBaseAI.cpp`: the ground mount of
   each race at the level its vendor teaches it, forty for the one of sixty per cent speed and sixty for the one of a
   hundred per cent, and the same two ranks of the warhorse and the steed that the classes teach their own members.
   `CombatBotBaseAI::LearnRidingAndMounts()` gives a bot the summons of the rows that its race, class and level
   answer to. It is asked for when a bot is created or loaded from the saved character, and again from the out of
   combat update of `PartyBotAI` - a party that plays grows and its bots grow with it, since the group shares its
   experience - and the function keeps the level that it taught at, so that it returns at once while the level of a
   bot has not moved. What a bot already knows is never taken away. The rank of the riding skill is not written in
   the table: `SkillLineAbility.dbc` says which skill and which rank of it a summon asks for, which covers the
   riding of one's own race of the patches before 1.12 and the one riding skill of 1.12 without naming a skill.
   Only when that data says nothing about the mount of a race are the `SKILL_RIDING_*` of `SharedDefines.h` raised
   to the rank of the tier - the one place of this subsystem where a skill is written in C++, and a skill that the
   SkillLine.dbc of the running patch does not have is never given. The two spells of the one riding skill of 1.12
   (33388 and 33391) are learned besides, because they are how `Player::UpdateOldRidingSkillToNew()` teaches that
   skill to a character that reaches the patch. A spell that the Spell.dbc of the running patch does not have is
   passed over, so a patch of a low maximum level keeps the mounts and the skills that it has. `GetMountSpellId()`
   answers with a summon only when the bot has learned it, which is also the measure that the travel form of a druid
   and the ghost wolf of a shaman of a battleground are taken against: they move in their
   own forms where there is no mount to ride. `SummonMount()` puts the summon through the normal pipeline of a spell,
   so that the three seconds that a summon of these mounts takes show on the cast bar of the bot and the mount comes
   under the rules that stop a spell. A party does not ride on the mount of its leader: a bot of `PartyBotAI` gets on
   a mount while the leader is mounted and gets off when the leader gets off, and one that has nothing of its own to
   ride stays on foot.
11. A party that goes down over a ledge goes down together, because the leader of a party is a player that jumps.
   `PartyBotAI::FollowLeaderDownTheDrop()` is for the bot that the pathfinder of the follow generator has stopped at
   the edge while its leader waits below it. The ledge is the height difference and the distance of
   `PB_DROP_MIN_Z_DIFF` and `PB_DROP_MAX_DIST` in `PartyBotAI.cpp`: what is lower than the first is a slope that the
   follow generator walks down as it always does, and what is deeper than the second is a drop that the bot had
   better look for a way around for, as the follow generator does on its own. The place to land is looked for along
   the line from the bot to its leader and the one nearest the leader is taken, of the places where the ground really
   is lower and where the way over the edge is free, so an overhang sends the bot around rather than into the rock.
   The fall itself is a `MovePoint()` with `MOVE_FALLING` and without pathfinding, which is the movement that the
   gravity of the client moves; there is no path over an edge to be found, and a path would lead the bot back up to
   the edge. The pet of the bot is dismissed for the drop, because a pet that is called at the top of a ledge stands
   there until its master walks back for it: the bot calls it back with `Player::ResummonPetTemporaryUnSummonedIfAny()`
   once it stands on the ground that it aimed for, and while it waits above the ledge the updates of the hunter and of
   the warlock are held from summoning it again, since a pet is called to where its master stands. A bot that must
   fight calls its pet where it is. A bot that rides, a leader that rides, and a pair that stands on different
   transports are left out of all of this.

## Tuning without compiling
- `playerbot_gear_stat_weights(spec, stat, weight)` overrides single pairs only; everything without a row keeps
  the compiled value. `.bot reload` reads the table again, so changing a value needs neither a restart nor a
  build. The ids of `spec` and `stat` are documented in
  `sql/custom/repack/Custom-BOT_CUSTOMIZATION.sql` and in the enums of `BotGearScoring.h`.
- `playerbot_talent_build(class, talent_page, seq, rank_spell, ranks, name)` is the build that a bot of one talent
  learns: one step per row, spent in `seq` order, so a bot of any level has the first steps of the build and its
  primary tree is the tree that gets filled. The table comes from
  `sql/custom/repack/Custom-BOT_CUSTOMIZATION.sql`, which creates the table and writes the builds that this fork
  chose into it below, in the same file, and is re-runnable; the order of the sql of the fork is in
  `sql/custom/repack/README.md`. `.bot reload` reads the table again. The steps of a build are keyed by the class
  and the asked page even when they are talents of another tree, so the lookup of a `rank_spell` runs over the
  trees of the class; a row that is not a talent of that class is skipped with a `LOG_DBERROR` line naming the
  step, which is how a wrong spell id is found without a build.
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
