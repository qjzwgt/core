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
`CombatBotBaseAI::LearnPremadeSpecForClass()` learns that tree with `LearnTalentsInTab()` instead of looking for a
premade template - those are chosen by role and not by talent - and then learns the spells of the trainers and of the
items, as it does when there is no template. Nothing else is needed for the gear: the rating reads the talent back
from the tree the bot has and from its role, which is also what tells the cat from the bear on the feral page of the
druid.

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
| `fs` `ss` `lr` | 法师, 术士, 猎人 | none, the class is rated by one talent |

The classes of the last row are rated by one talent (`GEAR_SPEC_MAGE`, `GEAR_SPEC_WARLOCK`, `GEAR_SPEC_HUNTER`), so
no tree is asked for and the bot takes a premade build of its role as before. `yd` is the cat: a bear is the same
page with `ROLE_TANK`, and 熊德, 戒律牧师 and the three rogue talents have no abbreviation yet. The tree itself is
filled in the order the tree lists its talents, so a signature spell can still be missing; the points that the level
of the bot pays for are the only limit.

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
