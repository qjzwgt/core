# The gear that bots are dressed with

A summoned bot that has empty equipment slots is dressed by `CombatBotBaseAI::EquipRandomGearInEmptySlots()`
(`src/game/PlayerBots/CombatBotBaseAI.cpp`), which is what `PartyBot.AutoEquip = 1` and `BattleBot.AutoEquip = 1`
select. The goal is the equipment that a player of the talent and the level of that bot could realistically have
got while adventuring, not an item list and not the best item of the whole database.

`src/game/PlayerBots/BotGearScoring.{h,cpp}` holds the rating of one item for one talent; the bot code keeps the
decisions about which slots to fill and which item wins.

## The talent of the bot

`GetBotGearSpec()` reads which talent tab the bot has learned spells in (`TalentEntry` + `TalentTabEntry.tabpage`),
which is reliable because a bot learns its talents in one tab. When it has learned none, the role of the bot
(`AutoAssignRole()`) decides. The role and the learning of the talents are not changed by this code; a bot that is
given an intended talent tree, which `.partybot add <abbreviation>` does, is rated correctly without touching
anything here.

## Summoning a bot of a chosen talent

`.partybot add <abbreviation> [level]` creates a bot of one talent: the abbreviation says the class, the role, and
the talent page to learn (`gearSpecAliases` in `BotGearScoring.cpp`, matched by `GetBotGearSpecAlias()`).
`CombatBotBaseAI::LearnPremadeSpecForClass()` learns the build of that talent instead of looking for a premade
template - those are chosen by role and not by talent - and then learns the spells of the trainers and of the items,
as it does when there is no template. Nothing else is needed for the gear: the rating reads the talent back from the
tree the bot has and from its role, which is also what tells the cat from the bear on the feral page of the druid.

## The talents that a bot learns

The talents come from `playerbot_talent_build` (world), which holds one row per step of a build: `class`, the page of
the talent that the build is for, the order of the step, a spell id of the talent and the number of ranks to take.
The table is created by `sql/migrations/20261029130100_world.sql` and by
`sql/custom/repack/Custom-ADD_PLAYERBOT_TALENT_BUILD.sql`; the rows are hand made content and live in
`sql/custom/PLAYERBOT_TALENT_BUILDS.sql`. The steps of a build are all keyed by the class and the asked page, even
the steps that are talents of another tree of that class. `src/game/PlayerBots/BotTalentBuild.{h,cpp}` reads the
table, resolves every spell id to the talent of Talent.dbc that has it, and skips a row that is not a talent of the
class it is declared for with a word in the log; `.bot reload` reads it again, so a build can be retuned without a
restart.

`CombatBotBaseAI::LearnTalentsFromBuild()` spends the free talent points of the bot by walking the steps in order.
Every build is written as the 51 points of a level 60 character of that talent - 31 in the primary tree, which is
what the last talent of a tree asks for, and 20 in a second tree - and the steps of a tree are ordered by row, so:

- a bot of any level has the first rows of the build of its talent, which is a build that a player would level with;
- the primary tree of the talent is the tree that is filled, and another tree is touched only after it;
- the rule that a row can not be taken before the rows above it have enough points is kept by the order of the rows.

The build of 战斗 puts all 51 points in one tree, because that tree is the talent: a combat rogue has no second tree
to spend the extra points in. Its steps are ordered by row like the others, and the last talent of the tree is still
the 31st point, so a bot of level 40 or above has that spell and the points that its level adds go to the rows that
are left.

A step that can not be taken at the moment it comes (the spell that the talent asks for is learned later, the tree
does not have the points yet) is asked for again after the following points are spent, and the ranks that the bot
already knows are counted out first, so a bot that levels up carries on with the build it had.

A bot that was not summoned with an abbreviation gets the talent that its role has for its class, taken from the same
`gearSpecAliases` list by `GetSpecTalentPageForRole()`; the first abbreviation of the class and role wins, so two
bots summoned the same way learn the same build. A class or a role that has no entry there, 熊德 or 戒律牧师 for
instance, and a talent that has no rows in the table keep the old path: `LearnTalentsInTab()` for an asked page, and
the premade templates of `player_premade_spell_template` otherwise.

The level of the bot follows the player that asked for it: the same level while the player is level 10 or below, a
level or two either way from 11 to 59, and always the maximum (`MaxPlayerLevel`) once the player has reached it. The
optional level is read only from GM accounts and from accounts with `PartyBot.SkipChecks`, and is clamped to the same
maximum.

| Abbreviation | Talent | Page of the class |
| --- | --- | --- |
| `wqz` `kbz` `fz` | 武器战, 狂暴战, 防战 | arms 0, fury 1, protection 2 |
| `nq` `fq` `cjq` | 神圣骑士, 防骑, 惩戒骑 (Alliance) | holy 0, protection 1, retribution 2 |
| `ms` `am` | 治疗牧师, 暗牧 | holy 1, shadow 2 |
| `yss` `zqs` `ns` | 元素萨, 增强萨, 恢复萨 (Horde) | elemental 0, enhancement 1, restoration 2 |
| `phd` `yd` `nd` | 平衡德, 野德, 奶德 | balance 0, feral 1, restoration 2 |
| `fs` `ss` `lr` | 法师, 术士, 猎人 | frost 2, affliction 0, marksmanship 1 |
| `dz` | 盗贼 (战斗) | combat 1 |

The classes of the last two rows are rated by one talent (`GEAR_SPEC_ROGUE`, `GEAR_SPEC_MAGE`, `GEAR_SPEC_WARLOCK`,
`GEAR_SPEC_HUNTER`), so the rating does not care which of their trees is learned; the page is still asked for,
because the page is what makes the bot learn the build of one talent instead of a build chosen by its role. `yd` is
the cat: a bear is the same page with `ROLE_TANK`, and 熊德, 戒律牧师 and the other two rogue talents have no
abbreviation yet.

## The pool of items

The sources of every item are indexed once at startup by `ObjectMgr::LoadItemGearSources()`
(`src/game/ObjectMgr.cpp`), which fills `SourceKinds`, `SourceRestrictions` and `SourceTeams` of `ItemPrototype`.
It is run again after the reloads of `item_template`, `quest_template`, `npc_vendor`, `conditions` and the loot
tables.

An item is a candidate only when:

- someone has already obtained it (`Discovered`), or the world drops it, or a profession/spell makes it; the world
  and crafted pools are what a player reaches while adventuring, and they include the items with random properties;
- it is a weapon or a piece of armour, and not a trinket, a relic, a tabard or a shirt;
- it needs no trade skill (`RequiredSkill`: engineering, fishing, ...), because bots do not learn trade skills;
- it does not need a reputation standing, an honor rank or a pvp reward, and is not a reward of a world event or a
  holiday (`SourceRestrictions`);
- it belongs to the faction of the bot, or to both (`SourceTeams`);
- it is a level appropriate for the bot, which also excludes the items that have no required level but an item
  level above it, and the rewards of quests above its level;
- it is not pick up bound epic gear at all, and the pick up bound **blue** gear (`ITEM_QUALITY_RARE`, the "rare"
  quality) of quests and instances has an item level equal to or below the level of the bot, so that a bot can not
  wear gear that is over tuned for its level; gear bound when equipped is only limited by its required level;
- the bot can use it (`Player::CanUseItem`: class, race, level, required spell, weapon proficiency) and knows the
  armour proficiency of the item.

## The score of an item

Every candidate is rated with `EvaluateBotGearItem()`, which collects what the item gives:

- the stats of the item itself;
- the spells that the item applies while it is equipped (attack power, spell damage and healing, critical strike,
  hit, mana per five seconds, resistances, dodge, parry, block, fixed mana and health);
- the best of the random properties that the item can use (see below).

The attributes are multiplied by the weight they have for the talent of the bot and summed up. Percentages count as
one unit per percent. The compiled weights are in `defaultGearStatWeights` and follow the scale "1% of hit or
critical strike is worth 20 points"; the values are meant to be adjusted, they are a starting point.

Not rated: the on use and combat procs of an item, the set bonuses, the enchantments of the items (the items are
created new), the weapon skills and the defense rating (only the dodge, parry and block percentages of an item are
visible). Bots do not wear trinkets, so their effects are not needed.

## Changing the weights

`playerbot_gear_stat_weights(spec, stat, weight)` (see `sql/migrations/20261029091000_world.sql` for the meaning of
every id) overrides single values, and only the pairs that a row gives: the compiled value is used for everything
else. `.bot reload` reads the table again, so no restart and no compiling is needed.

`.partybot gearscore <item>` with a bot selected prints, for the talent of that bot, every attribute that the item
gives, the weight used, the partial score, the random property that would be chosen and the damage per second of the
weapon. It is the way to see why an item wins or loses.

## The weapon slots

The rules of a talent are in `GetBotWeaponRules()`: whether a slot is filled by the damage of the weapon or by the
score of its attributes, and which kinds of weapon are allowed.

| Talent | Main hand | Off hand |
| --- | --- | --- |
| arms warrior, retribution paladin | two handed, highest damage | - |
| fury warrior | one handed, highest damage | one handed, highest damage |
| rogue | one handed, highest damage | one handed, highest damage |
| protection warrior, protection paladin | one handed, highest damage | the shield with the most armor |
| enhancement shaman | same as protection until its talents give the two handed axes and maces, then a two handed weapon with the highest damage | shield |
| hunter | attributes only, the best of {two handed} and {two one handed weapons} | a weapon |
| holy paladin | attributes only | shield, off hand weapon or holdable, by attributes |
| feral druid | attributes only, two handed | - |
| every casting talent that can use a staff | attributes only, the best of {a staff} and {one handed + off hand} | attributes only |

Where the damage decides, the attributes of the weapon are ignored completely, and two weapons with the same damage
are chosen randomly. The damage is the one of the tooltip, without the damage that a random property adds. A shield
is chosen by its armor, and the armor of a shield is not added to the score of the options when a two handed weapon
is compared with a one handed weapon and an off hand item.

A bot learns the dual wield of the trainers (spell 674) at the level where a player learns it: rogues at 10,
warriors and hunters at 20 (`PrepareDualWield()`), because `Player::CanDualWield()` is not saved with the character.
Until then a fury warrior is dressed like a protection warrior, a rogue wears a single one handed weapon, and a
hunter has no choice but a two handed melee weapon.

## Random properties

The `RandomProperty` field of an item is a group of `item_enchantment_template`, and the values of the properties of
the group grow with the level of the item, so the properties are read from the table (`GetItemEnchantMods()`) and
each of them is scored with the item. The item is created with the property that gives the most to that talent,
which is what makes a tank take "of the Bear", a caster "of the Eagle", a hunter "of the Monkey" and a melee damage
dealer "of the Tiger" without naming any of them.

`Player::StoreNewItemInBestSlots()` would roll another property, so the items are created with
`Player::StoreNewItemInSlot()`, which puts the item in one given slot with the chosen property and applies the mods
of that property. This is also why the weapon slots are filled after the armour: the item that goes in one hand
depends on the item that goes in the other.

## Two rules that are enforced while dressing

- no two pieces of the equipment share the same item entry, whether the item is marked unique or not, including the
  two hands of a dual wield;
- a bot that already wears a two handed weapon is given nothing in its other hand.
