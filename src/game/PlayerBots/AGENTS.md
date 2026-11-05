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
  learned spells says the talent; when nothing is learned, the role of the bot says it. A talent page asked for by
  `.partybot add <abbreviation>` is authoritative for the gear of that bot, since a tank build can spend its first
  points in another tree before the tree of its talent opens.
- Warrior ability names: 横扫 = `Sweeping Strikes` (12292), the ability that copies the hits of the next seconds onto
  the mobs about the warrior, and 顺劈斩 = `Cleave`, a cheap hit over the front of the pack that no bot rule asks for.
  The two names are one letter apart in Chinese and nothing like each other in the spell data, and the rage that a
  damage warrior banks is the rage of the first of them.

## Decisions of the maintainer (already made - ask before changing them)
1. The pool is what a player reaches while adventuring: `ItemPrototype::Discovered` ∪ world loot ∪ crafted gear,
   which is what makes the items with random properties reachable. Excluded: anything with a `RequiredSkill`
   (engineering, fishing, tailoring, ...) because bots never learn trade skills, gear gated by a reputation
   standing, an honor rank, a pvp reward, a world event or a holiday, the gear of the other faction, trinkets,
   relics, tabards and shirts. An item with any negative attribute, including a negative attribute from the random
   property that the bot would put on it, is never a candidate.
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
   shaman, and for the ranged slot of a hunter, warrior or rogue - the attributes of those weapons are ignored
   completely and equal damage is resolved randomly. Tanks take a one handed weapon by damage plus the shield with
   the most armor, and the armor of a shield is never added to the score of an option. The casting talents, the
   feral druid and the melee weapons of a hunter are rated by attributes, comparing {two handed weapon or staff}
   against {one handed weapon + off hand item}. Wands are rated by attributes; relics are not worn; the damage that
   a random property adds to a weapon is not part of the damage used for ranking.
   Warriors of any talent and the combat rogue wear no dagger and no fist weapon in either hand;
   `allowDaggersAndFistWeapons` of `BotGearWeaponRules` says it, and the items of those kinds never reach the
   candidate pool of such a bot. The arms warrior wears only a two handed axe or sword, and the combat rogue only a
   one handed sword; `allowedMeleeWeaponSubclasses` says it. These melee restrictions do not reject a ranged weapon.
5. The random property of an item is read from its `item_enchantment_template` group, scored together with the
   item, and then put on the item before the item is equipped. Equipping is what applies the mods of an item and
   what writes the visible fields of the equipment of the player, which is all that another player sees of what a
   bot wears, its suffix and the attributes of that suffix included; a property set after the equip leaves an item
   that shows no attributes to the one who looks at the bot. Those fields of one slot are what
   `Player::SetVisibleItemSlot()` writes, and they have to say everything that the item says: the entry of it, the
   identifier of every one of its enchantments - the attributes that a random property gives are the spells of the
   PROP slots of the item, so a block that carries only the two enchantments that the name says are inspected
   shows an affix whose attributes nobody can see - and the property with the factor of its suffix. That is why
   `Player::StoreNewItemInSlot()` exists:
   `Player::StoreNewItemInBestSlots()` rolls another property, so the bot would wear something other than what
   was rated; that helper keeps the same order itself now, which is what puts the suffix on the gear that a bot
   copies from a player and on the gear of a premade template. `Item::SetItemRandomProperties()` writes that slot
   of the equipment again when a property goes on an item that is worn already, asking the owner for the item of its
   own slot - the slot of a created item is 0, the head slot, and `Item::IsEquipped` calls such an item worn, so an
   item that nobody wears would else be drawn on the head of its owner. The mods of a worn item are not touched by a
   property, so the order stays the thing to get right. An item that carries no property although its entry names a
   group was dressed while that group had no usable row of `item_enchantment_template` for the realm, and
   `EquipRandomGearInEmptySlots()` takes such an item off before it fills the slots. The gear comes back with the
   character from the database, so `RepairRandomGear()` asks for the same removal when a bot is loaded instead of
   summoned; an old bot would else keep wearing gear that no player owns. Naming 野熊 / 雄鹰 / 灵猴 /
   猛虎 per role is not needed - the scoring picks the best of the group of that item where the group has one of
   them. An item of a group always wears one of its properties: when none of them gives an attribute that the
   talent values, the property that the creation of the item for a player would roll is taken, because an item
   without its suffix is an item that does not exist.
6. The whole set wears at most one copy of any item entry, whether the item is flagged unique or not, because
   quest rewards that are not flagged unique would otherwise be worn twice.
7. The stat weights (the "1% of hit or critical strike is worth 20 points" table in `defaultGearStatWeights`) are
   a starting point that the maintainer tunes by playing. A weight that looks wrong is data to report, not a bug
   to fix silently.
8. `PartyBot.SpiritResurrection`, on by default, says how a party bot that died outside a battleground comes back.
   The default of on is a deliberate exception to the guideline of `README.md` that custom behaviour is off by
   default: a bot that comes back the way a player does is wanted without a line of `mangosd.conf` written first,
   and the option is there for a server that wants the behaviour of upstream instead. Off, the bot raises itself as
   soon as a player of the group stands by its corpse, which is what upstream does. On, the bot stays by its
   unreleased body and waits to be raised while a living member of the group is to
   be counted on to raise it: one that knows a spell with the resurrect effect, which is the test that
   `PopulateSpellData()` uses too, Rebirth excepted - a druid keeps Rebirth for an emergency, and it is the one
   resurrection that the core marks with `SPELL_ATTR_EX3_NO_RES_TIMER`, which is how it is recognised here rather
   than by the ids of its five ranks. A group whose only resurrection is a Rebirth does not hold the bot either:
   the bot releases its body, and a resurrection wants a body that is still there, so the druid that knows nothing
   else than Rebirth - a bot druid, whose `m_resurrectionSpell` that is - no longer has the bot as a target. That
   is decided and accepted, Rebirth stays for the emergency it is meant for. Where nobody is to be counted on the
   body is released at once; outside an instance the release timer that the core gives a player releases it after
   six minutes, which is wanted - that is what a player who is left waiting does. The spirit then walks back to its
   bones - to a place beside them rather than onto them, because a bot that is raised on its own corpse is raised in
   the middle of the pack that killed it. The sides of the bones are tried from the one the spirit came to, and a
   side is left when a creature stands there that would take the bot for an enemy from that place, which is the
   aggro radius of `Creature::GetAttackDistance` with the hostility, the line of sight and the reachable ground that
   `BasicAI::MoveInLineOfSight` asks for. The body is taken under the rules of `WorldSession::HandleReclaimCorpseOpcode`
   from the first of those places that is free, and after all of them where the bot stands; the 25 yards of
   `PB_RECLAIM_STAND_DISTANCE` are inside the 39 of `CORPSE_RECLAIM_RADIUS`, so the place is one the core allows.
   A spirit whose bones are in another copy of the map, or that has asked for PB_SPIRIT_WALK_LEGS ways without the
   walking coming to an end, is raised by the spirit healer through `WorldSession::SendSpiritResurrect` - not at the
   first path that fails, which is how a bot came back at the graveyard the moment its release was made. Neither of
   those paths applies the 2479 that the self-raising one does.
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
   needs its meal, and a bot called into combat stops eating. When another member of the group enters combat, has an
   attack target, or is being attacked, an out-of-combat bot keeps recovering only while its health or mana is at or
   below thirty per cent. Once both are above thirty per cent it stands up and joins the fight. The bandage is a real
   item, and its stack is put in the bags of the bot when the bags have none, as the ammo of a hunter is put in the
   quiver. The bandage of a bot is the tier whose first aid rank (`required_skill_rank` of the item) the first aid of
   a player of that level has reached; those levels are the table `bandageTiers` of `CombatBotBaseAI.cpp`. The three
   tables are some of the few places of this subsystem where item and spell ids are written in C++ - to tune the
   tiers means moving them into `sql/custom/repack/`, not keeping a second list. A bot does not learn the first aid
   skill: the core checks the `RequiredSkill` of an item when it loads the prototypes and never when the item is used.
   A mounted bot neither eats, drinks, bandages, nor runs the out-of-combat spell routine; it has to dismount first.
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
   ride stays on foot. A mounted group is still travelling: a combat flag or an attacker on a mounted member does not
   make its bots join, and only a group member who has dismounted and is fighting does.
11. A party that goes down over a ledge goes down together, because the leader of a party is a player that jumps.
   `PartyBotAI::FollowLeaderDownTheDrop()` answers two shapes of that. One is the bot that the pathfinder of the
   follow generator has stopped at the edge while its leader waits below it, which is the case the rule was first
   written for; the other is a bot that is walking a way which its generator found the long way round the edge.
   The ledge is the height difference and the distance of `PB_DROP_MIN_Z_DIFF` and `PB_DROP_MAX_DIST` in
   `PartyBotAI.cpp`: what is lower than the first is a slope that the
   follow generator walks down as it always does, and what is deeper than the second is a drop that the bot had
   better look for a way around for, as the follow generator does on its own. The place to land is looked for along
   the line from the bot to its leader and the one nearest the leader is taken, of the places where the ground really
   is lower and where the way over the edge is free, so an overhang sends the bot around rather than into the rock.
   The fall itself is a `MovePoint()` with `MOVE_FALLING` and without pathfinding, which is the movement that the
   gravity of the client moves; there is no path over an edge to be found, and a path would lead the bot back up to
   the edge. A bot that is walking is not a bot that the pathfinder stopped, and it loses the jump the other way:
   its generator has found a way that goes a long way round the edge, and the bot walks it while its leader waits
   below. `PartyBotAI::IsFollowWayRoundTheLedge()` asks the pathfinder for that same way and reads its length
   against the line from the bot to its leader; a way longer than that line by `PB_DROP_DETOUR_FACTOR` and by
   `PB_DROP_DETOUR_EXTRA_DIST` of ground of its own is a way round an edge, and for that bot the ledge floor is the
   lower `PB_DROP_DETOUR_MIN_Z_DIFF`. A slope that the navigation of the place can be walked down answers with a way
   that is the line itself, and is walked as it always was; a place with no navigation to ask, and a way that does
   not reach the leader at all, are left to the follow generator. The way is asked for only while the movement
   generator of the bot is the follow one, and only after the ground to land on has been found, which is the
   question that says whether there is an edge here at all; those two numbers of the detour are for the maintainer
   to tune. The pet of the bot is dismissed for the drop, because a pet that is called at the top of a ledge stands
   there until its master walks back for it: the bot calls it back with `Player::ResummonPetTemporaryUnSummonedIfAny()`
   once it stands on the ground that it aimed for, and while it waits above the ledge the updates of the hunter and of
   the warlock are held from summoning it again, since a pet is called to where its master stands. A bot that must
   fight calls its pet where it is. A bot that rides, a leader that rides, and a pair that stands on different
   transports are left out of all of this.

12. A ranged party bot walks away from an enemy that came close only when the ground it wins is ground that it keeps.
   `PartyBotAI::CanRunAwayFrom()` is the one answer, and it is asked before any `RunAwayFromTarget()`: the enemy must
   be held from following - a root, a stun or a confusion - and the bot must have its own movement, which the
   `UNIT_STATE_NO_FREE_MOVE` of the bot and its root, stun, confusion and fear auras say. What the enemy is itself
   busy with is not asked for, because an enemy that is held and can no longer reach the bot turns to the one that
   it can reach and the bot is to walk it around all the same.
   A fear of the enemy is not counted, since a feared enemy is running away from the bot already. Without all of that
   the bot stands and takes the attacks and goes on with what its talent does. The chase generator keeps a distance
   for a ranged role all by itself, so `UpdateInCombatAI()` takes it back to `SetCasterChaseDistance(0.0f)` when the
   enemy next to the bot is not held; that is what keeps a healing priest and a warlock casting instead of walking
   backwards. A shadow priest has the same eight-yard step as a mage when its attacker is held. Every healing talent
   uses that step as well, and `PartyBotAI::SelectNearestMeleeAttacker()` finds the attacker even when the healer has no
   offensive victim of its own.
   The distance of that kiting is `PB_RUN_AWAY_DISTANCE`, eight yards on every kind of ground. It is what
   `RunAwayFromTarget()` steps back by. After that step the mage's chase generator returns to the casting range of
   Frostbolt, or of Fireball when Frostbolt is not learned, so a distant target does not pull the mage back towards
   eight yards. A bot that already has those eight yards does not run again, so a mage whose Frost Nova holds the
   enemy casts from that distance instead of starting another distance movement. The mage casts Frost Nova only when
   the target is closer than eight yards, and asks for that move before its Frostbolt filler so neither a Frost Nova
   nor a Frostbite root leaves the mage standing in melee to cast. A bot in combat follows neither the party leader
   nor a tank: every run away from the enemy is a step directly away from that enemy. Once the mage reaches eight
   yards from a held target, it stops and clears the distance or chase generator before beginning the Frostbolt cast.
   Finishing a distance move calls the player AI immediately, so the mage does not wait for the next one-second
   party-bot update.
   `PartyBotAI::GetChaseDistance()` remains the ordinary distance for chasing and casting: ten yards in a dungeon
   or raid, twenty in the open world and thirty in a battleground, the battleground asked for first because it is
   an instance too. A mage uses the maximum range of its learned Frostbolt, or Fireball when Frostbolt is absent,
   including `SPELLMOD_RANGE` from talents; it does not chase while the target is inside that range. A shadow priest
   uses the modified range of its learned Mind Flay in the same way. Other ranged damage bots use thirty yards on
   every kind of ground instead, so they approach only when the target is outside the range of their longest common
   attack spell and never walk closer for a short-range spell such as Fire Blast.
   `CombatBotBaseAI::GetChaseDistance()` stays at twenty-five for a battle bot, which fights on the same ground in
   every battleground. A hunter swings the melee weapon that it wears
   instead of running - the auto attack of the core does the swinging, `UpdateAI` has taken the auto shot away from
   it below 8 yards, and its raptor strike and mongoose bite are in the `CanReachWithMeleeAutoAttack()` branch. The
   places that ask are the hunter, the mage, the shadow priest, and the moonkin and `FORM_NONE` branch of a druid;
   that druid casts Entangling Roots first, which is the right answer for an enemy that is not held yet. The escape
   of a rogue that has vanished at low health is not kiting and is not part of this. A mage casts Mana Shield only
   below 30 per cent health. On every update, in combat and out of it, a mage that is not mounted checks for Ice
   Barrier and casts it when the buff is absent and the spell is usable. `AttackStart()` keeps a mage out of the
   melee attacking state, so it never swings at an enemy that closes while it is between spells, but it enters
   combat explicitly when it starts attacking, since a spell-only attack has no melee swing to do that later. On
   entering its class combat routine, a mage interrupts the current cast of its target with Counterspell and casts
   Fire Blast whenever it is ready in combat, but never against a frozen target. It otherwise uses Frostbolt; without
   Frostbolt it falls back to Fireball, as it also does when the target is immune to Frostbolt. A non-elite target
   below two per cent health is finished with the wand when Fire Blast is not available. At less than twenty per cent
   mana the mage casts
   Evocation whenever no attacker can reach it with a melee attack. Cone of Cold is used under the same multi-target
   condition as Arcane Explosion. The movement slow that Frostbolt leaves on the target does not hold the next
   Frostbolt back: a bot skips a spell for its existing aura only when that aura is not the by-product of a direct
   damage spell, or when the aura itself is periodic. A party mage never Blinks or uses Ice Block to leave a control
   effect; Ice Block stays for the below-ten-per-cent health branch. Scorch is never cast. The generic
   crowd-control, dispel, movement and marked-target work that runs before the class routine still comes first.
13. A party rogue opens only with Cheap Shot, falling back to Garrote only when Cheap Shot is not learned. With
   neither opener it does not hold an automatic attack back for stealth. While a rogue with stealth and an opener is
   approaching, `PartyBotAI::AttackStart()` only records the target and uses follow movement; neither `Unit::Attack()`
   nor `ChaseMovementGenerator` may start auto attacks before the opener has been cast. A party member that starts
   combat first does not stop that approach: the pending target stays with the rogue routine until the opener or a
   normal attack replaces it. `PartyBotAI::PrepareStealthOpener()` starts stealth or Prowl for a rogue or cat druid
   when it is about to attack and is still out of combat, so the group's combat state does not skip the opening
   approach. A rogue that already has stealth does not cancel it merely because no target has been selected yet or
   because the leader leaves stealth; combat and the rules of the spell still break it normally. At
   two combo points it keeps Slice and Dice on a target above ten per cent health; below ten per cent it Eviscerates
   from three points; at five points it refreshes Slice and Dice below ten seconds and otherwise Eviscerates. Riposte
   precedes Sinister Strike, Kick is the only interrupt, Blade Flurry needs two nearby enemies that are not
   control-marked, and Adrenaline Rush is for an elite at least two levels above the rogue with less than twenty
   energy. Evasion needs health below eighty per cent and two non-ranged attackers within five yards. Vanish at less
   than fifteen per cent health keeps the rogue in stealth away from the fight until health reaches forty per cent.
   Sprint is used only in combat with the target over twenty yards away. Blind remains the control-marked spell and
   the reaction to another attacker on the party.

   New party and battle bots, party bots loaded from the database, and party bots that level up synchronise every
   source of a class ability for their current level: the ordinary trainer spells, the build that spends their
   talent points, the reward spells of the quests of their class and race, and the class spells that usable items
   teach. `LearnSpellsForLevel()` owns the order. The trainer and quest spells come first because a talent can
   depend on one, the talent build comes next, the items follow because an item can require a talent spell, and a
   second talent pass takes any point that an item has just unlocked and then puts a point that the build can not
   use into the tree of the talent. Each source runs once per level.
   Some abilities of a class are taught by a quest and not by a trainer. Warrior stances are not one source at one
   level: the final quests of defensive stance (1498, 1665, 1678, 1683 and 1819) have `MinLevel` 10, while
   `The Affray` (1719), which rewards berserker stance, has `MinLevel` 30; `20171128233629_world.sql` is the
   migration that sets those levels. A bot has no quest log, so `LearnQuestRewardSpellsForLevel()` reads
   `quest_template` directly: a quest teaches the bot when its `RequiredClasses` and `RequiredRaces` include the
   bot, when its own `MinLevel` is reached, and when the reward belongs to the class of the bot (the spell family of
   the reward, or the quest of one class with a reward that is not a spell of another class). The code does not
   assign a stance a guessed level. `MaxLevel` of a quest is not asked for: it says who may start such a quest, not
   who may know its reward, and a bot created at the top level must still have the abilities of its class. A spell
   that a talent brings and a higher rank whose rank below it is unknown are left out, as are spells that
   `SkillLineAbility.dbc` teaches at a higher level than the bot has.
   `LearnItemSpellsForLevel()` uses the same all-items pass as the GM command. An item is skipped when it is not
   obtainable, when the bot can not use it at its class, race, level or proficiency, when its on-use spell is not a
   learn spell, or when the taught spell has no `SkillLineAbility` row. A bot has no profession, so profession
   recipes stay out, while class skill books enter at the level of their item.
   A rogue bot puts its poisons on out of combat, as a player puts them on between two fights, and it asks for the
   enchantment that a poison puts on a weapon as a spell of its own (`CombatBotBaseAI::ApplyWeaponPoisons()`, next
   to `CastWeaponBuff()`): a bot carries neither the poison items of a vendor nor the Poisons skill of the quest of
   its class, and a player reaches that same enchantment through either of them. Which of the two families it wears
   is one rule rather than a choice of the bot - `Crippling Poison` inside a battleground, where holding a target
   out of reach of the pack is worth more than the damage, and `Instant Poison` outside one - and one weapon is
   done per update, the main hand first. The spellbook is not asked about a poison and the other three families are
   not read at all: no rule of a bot wants their effect.

14. A party bot that has no target of its own first assists the current target of another group member within fifty
    yards, before it considers the units that are only attacking that member. Having a target is enough here because
    the first melee swing may not have set the member's in-combat flag yet. A breakable-by-damage control effect
    normally keeps a party bot from selecting that unit as its attack target. When the only attackable hostile unit
    within fifty yards is in combat and held by such an effect, the bot instead selects that unit and attacks it,
    which may break the control. A second hostile unit in range returns target selection to the ordinary rule, so a
    bot does not break a sheep or a blind while there is another target to use.

15. A mage casts Arcane Intellect on itself and the mana users of its group outside an instance. In an instance with
    Arcane Brilliance, a group below five receives no Intellect buff, a full group with two or more mana users missing
    the buff receives Arcane Brilliance, and a full group with fewer missing receives Arcane Intellect. A priest waits
    thirty seconds after the last new member joins before it chooses Prayer of Fortitude for three or more members
    without Stamina and Power Word: Fortitude for fewer; it activates Inner Focus before Prayer of Fortitude. Divine
    Spirit is for mana users outside an instance, while Prayer of Spirit is used inside only for a full group with
    three or more mana users missing Spirit. Priests do not cast Shadow Protection.

16. A shadow priest opens with Mind Blast, keeps Shadow Word: Pain and Vampiric Embrace on its target, combines Inner
    Focus with Mind Blast, and uses Mind Flay as its filler before falling back to Mind Blast and the wand. It never
    casts Psychic Scream or Mana Burn. It dispels hostile buffs and dispels magic from the party. Outside combat a
    non-healing Shadowform priest waits twenty seconds when a resurrection or disease removal is needed, then breaks
    the form if the need remains; a shadow priest in combat removes only magic. A healing priest never enters
   Shadowform, leaves an existing one before acting, dispels magic and disease from the party and their pets. A
   friendly dispel does not select a holder whose only negative effect is a movement-speed reduction, even when its
   dispel type matches: it is frequent and small enough that group members can walk it off, so it is not worth the
   mana and global cooldown. A priest of either role casts Fade as soon as a hostile unit has it as a target, at any
   distance. Three injured party members below seventy per cent health within thirty yards call for Prayer of
   Healing. When every group member is at least ninety per cent health and no buff, heal or dispel is due, the healer
   shoots a target that is already in combat with its wand; it never starts a fight with the wand. It does not shield
   a tank-role warrior above sixty per cent health. A shadow priest does not start the wand during the global cooldown
   left by a DoT, and an active wand gives way as soon as Mind Flay is ready and the target is in its range.

17. A protection warrior tank treats the leader's selected or focused target as an attack order even while the group
    is out of combat, and an explicit `AttackStart()` keeps that target while a stance is changed. Charge is an
    optional opening only when it is learned, ready, affordable and usable outside a dungeon; if it cannot open the
    fight, the tank still follows the target to melee in Defensive Stance. The same stance is its out-of-combat
    default, and the protection routine switches back to it before Revenge, Shield Block, Shield Slam and Sunder
    Armor. A stance cast that the ordinary bot cast gate rejects uses the same spell through the triggered server
    path, with the global cooldown put back. Only the worn form says that a stance was taken: a routine that is
    told anything else goes on with the skills of the stance that the bot is standing in, because a stance that was
    asked for comes, while a routine that waits for it never runs. `Charge` has one owner for every warrior of the
    party, `AttackStart()`: the opening of a shield tank and the opening of a damage warrior are the same code, and
    the out-of-combat routine of a damage warrior asks no charge of its own - which is also why that warrior no
    longer rests in the Battle Stance to be ready for one. The opening takes Battle Stance with the charge, keeps
    the target through it, and the rotation of the bot brings its own stance back on the update after it. An opening
    is refused in an instance - `Map::IsDungeon()`, which is a dungeon and a raid - and in nothing else: a
    battleground is open ground, as are the fields of the world, and those are the ground a charge is for.
    An opening of a charge is an ask that stands, and not one cast. The stance that the charge needs is asked in
    the update that begins the opening; the core applies a stance when the spell event of the caster runs, and that
    stance puts a global cooldown back and leaves its own spell in progress, either of which makes the core refuse
    the charge of the same update. A target that is inside the charge range from the beginning was the case that
    this lost: its one ask was refused, `TryChargeWarriorTarget()` was read as a plain no, and the tank walked to
    the mob in its own Defensive Stance and never charged. The function now answers what the ask is -
    `PB_CHARGE_CAST`, `PB_CHARGE_APPROACH`, `PB_CHARGE_LATER`, `PB_CHARGE_REFUSED` - and only the last of those
    gives the opening up: the target named when the opening began is kept, `TryContinueWarriorCharge()` asks again
    on the next updates, and it owns those updates, which is what keeps a stance owner from taking the Battle
    Stance back between the ask of an opening and its charge. `PB_WARRIOR_CHARGE_ASK_UPDATES` bounds the waiting
    for the stance and its global cooldown, because a tank that stands at the edge of the charge range while its
    party fights is worse than a tank that walks in; the walk to that edge and the charge of a target that was
    never in the range are bounded by nothing, as before.
    `UpdateProtectionWarriorTankCombat()` and `UpdateOutOfCombatAI_Warrior()` are the only owners of the stance of a
    shield tank, apart from the standing opening of a charge above, which owns the Battle Stance that it asked until
    the charge comes or its bounded ask is given up. The taunt fallback casts `Taunt`, asks for the chase of a
    target that is out of melee reach, and
    answers false so that the rotation still runs. Where a second owner asked for another stance, the Defensive
    Stance was taken away on one update and asked back on the next, both updates stopped at the stance, and a shield
    tank stayed in the stance of its charge without a Taunt or a Sunder Armor. `Mocking Blow` belongs to Battle
    Stance, so that fallback does not cast it either: the spell is a trigger of the rotation, next to the Thunder
    Clap, which makes the stance switch that the spell needs and takes the Defensive Stance back on its own update
    afterwards. The trigger carries the caution that the long cooldown of the spell asks for: `Taunt` must be on its
    own cooldown, the tank must hold the spell and the rage for it, and the mob must really be at the party member -
    `Unit::CanReachWithMeleeAutoAttack()` of either side of that fight says so, and a mob that is still running
    across the room is a mob that the tank walks to and takes with its rotation.
    `Thunder Clap` is asked for in two cases and in no other. The opening of a charge may spend one clap on the pack
    about its target: the charge leaves the tank in the Battle Stance that the clap needs, that pack is at its
    closest in the seconds after the charge, and the area of the clap is the first threat on every mob that is
    coming - `PB_WARRIOR_THUNDER_CLAP_PACK_COUNT` red-name mobs about the target, the rage of the spell and its
    cooldown, one cast, and the ask is over with the Battle Stance of the charge, whether the cast was answered or
    the conditions were not there. The debuff of the spell is not asked for in that case, because the point is the
    threat over the pack and not the debuff of one mob. The other case is an elite `PB_WARRIOR_THUNDER_CLAP_LEVEL_GAP`
    levels above the tank or more, which is the skull that the client shows for a boss: the debuff of the clap takes
    attack power off the mob that hits the tank, so the tank keeps that debuff, asks the Battle Stance when it is
    gone and the rage is there, and asks the Defensive Stance back on the next update. The rank of `Tactical Mastery`
    is no condition of either case, and the rule that came before this one - a pack of three, a target without the
    debuff, and that rank of the talent - asked for a stance change in the middle of a fight that the talent had to
    make worth its rage.
    The opening of a charge is held back while a neutral stands at peace within
    `PB_WARRIOR_THUNDER_CLAP_NEUTRAL_WATCH_DIST` of the tank or of its target, because the debuff of the clap would
    wake such a neutral into a second fight that the tank did not ask for. A neutral that is fighting already is in
    that fight whatever the clap does to it, so it is no reason to hold the clap back - and it is not one of the
    three of the pack either, which is the pack of the mobs that the tank goes to hold. The look is asked at both
    of the points about which the area of the spell may be drawn, because which of the two the core uses is in the
    spell data of the running patch, and `PartyBotAI::HasWakingNeutralNear()` is the one look that the shout of the
    tank and this one share; the reaction of a mob is what its faction and the reputation of the bot say, and a
    fight does not change it, so `Unit::IsInCombat()` is what tells a neutral that is engaged from one at peace.
    A mob that beats another party member is a mob that the tank is to take, not to fight: the tank goes to it, keeps
    it with the ordinary rotation and with its taunts, and goes back to the target it was holding when its hold is
    full - `PB_WARRIOR_SECONDARY_HOLD_MS`, six seconds, a value for the maintainer to tune when the mobs are seen to
    go back to the party member too soon. The hold is a length of time and not the sight of a spell that lands: what
    one cast does to the threat on one mob is not known to the bot, and a cast that lands says nothing about whether
    the mob stays. The time is spent only in an update in which a swing of the bot could really land on the mob -
    `Unit::CanAutoAttackTarget()` is the gate that the core puts in front of its own auto attack, and it covers the
    controls of the bot, its reach and its facing - so a run to a mob that stands away, a stun or a fear of the bot
    and a mob that walks out of reach spend nothing of the hold.
    `Shield Block` is asked of a mob that hurts the party with its body. The block blunts the next physical blows
    landed on the tank, so whether it is for a mob at all is asked of the class that the template of that mob gives
    it - `CombatBotBaseAI::IsPhysicalDamageClass()`, the same reading that the battle bot warrior makes before its
    own `Shield Block`. A mob that reads a bar to hurt the party may well walk in and swing once in a while, and
    that is not what ten of the rage and a minute of a cooldown are spent on: against such a mob `Revenge`,
    `Shield Slam` and `Sunder Armor` do more for the hold than a block that nothing lands on.
    `PartyBotAI::IsWarriorShieldBlockWanted()` asks that as well as the older condition, that some mob is really
    swinging at the tank.
    The survival of that tank is a rule of its own, asked for only in combat and without ending the update: Last
    Stand at thirty per cent of the health of the bot or below, Shield Wall at twenty per cent or below. Neither
    spell costs rage nor belongs to a stance, so a tank that puts one of them up keeps its target, its taunt, its
    rotation and its chase; the global cooldown that the core shares between the spells of one caster is all that
    can move one of them to a later update. Last Stand is asked for first because its threshold is the higher one,
    which keeps the longer cooldown for later.
    `Demoralizing Shout` outranks `Sunder Armor` in that rotation, and the number of mobs around the tank is not a
    condition of it: the debuff of the shout lowers the damage that the party takes from one mob as much as from
    five, so the shout is asked for whenever the mob the tank holds does not carry it. The one thing that stops the
    shout is a neutral that stands at peace within `PB_WARRIOR_SHOUT_NEUTRAL_WATCH_DIST` of the tank, because the
    debuff of the shout wakes such a neutral into a second fight that the tank did not ask for; a neutral that is
    already fighting is in that fight whatever the shout does to it, and is no reason to hold the shout back. Only
    the units that the shout could really reach are looked at, and the shout is let off at the tank itself as
    `Battle Shout` is, because the area of the spell is the tank.
    `Intercept` is the reach that a warrior has without a mount, and the question of whether a target is ground for
    it is one function for both of the warriors of the party, `PartyBotAI::IsWarriorInterceptWanted()`: the bot and
    its target must both be fighting, the target must be out of a swing of the bot and inside the range that the
    data of the spell covers (its minimum range with it, which is the same ground as the swing read a line above),
    the spell must be ready and free of the global cooldown, and the rage must be above
    `PB_WARRIOR_INTERCEPT_RAGE_MIN` points. What the spell itself gates - the rank in the spellbook, its cooldown,
    the global cooldown, the immunity of the target and the line of sight to it - is asked of the spell and never of
    a rule of the bot. A target beyond the far end of that range is not waited for: the chase that the rotation of
    a warrior asks anyway walks the bot through the range, and the jump is asked on the update in which the bot
    stands inside it, because a bot that stopped at the edge of the range to cast would stand twenty yards from the
    mob it is fighting and hit nothing.
    The stance that the jump is allowed in is named by the rule rather than read from the spell, because what decides
    it is the rage of a stance change: a bot that wears the Berserker Stance jumps where it stands, and a bot in the
    Battle Stance or the Defensive Stance is changed only when a rank of `Tactical Mastery` keeps its rage through
    the change. Without that talent the jump is not worth a change that the routine asks back a moment later, which
    is the same reading that `Overpower` and the rest of the rotation carry. The two warriors answer the same answer
    in their own way: `UpdateProtectionWarriorTankCombat()` casts the spell where the stance is worn and otherwise
    switches to the Berserker Stance, which its own rule of the tank's stance takes back to the Defensive Stance on
    the next update, and `CollectWarriorDamageWants()` carries the want into the rotation with the stance it wants
    named, which is what makes the change and the spell paid for together.
    Whether a mob may be fought is not asked of `Unit::IsHostileTo()`, which reads the red name only: it is asked of
    `CombatBotBaseAI::IsValidHostileTarget()`, which is built on `WorldObject::IsValidAttackTarget()` and takes a
    yellow neutral that has joined the fight, as the area of effect of the core takes it. The yellow of a unit is
    `WorldObject::GetReactionTo()` at `REP_NEUTRAL`, the same value that the client colours that name plate.

18. A warrior is under the shield tank rules when it wears a shield, or when it carries the tank role without a
    shield (`IsProtectionWarriorTank()`): the Defensive Stance, the taunts and the target handoff of a tank must not
    depend on one gear slot being filled, and the role it falls back to was itself given by the shield, by the talent
    build, or by `.partybot add <talent>`. The role of a bot that comes back from the database comes from the same
    shield rule: neither the role nor the asked talent is stored with a saved bot, and a role set with
    `.bot setrole` is kept until the next load. `ReadPlayerRole()`, which `AutoAssignRole()` now only calls, reads
    the shield that the bot wears, and for a warrior without a shield the build of `playerbot_talent_build` whose
    steps its learned talents complete the most (`GetLearnedTalentBuildPage()`).
    One signature spell cannot say the talent of a warrior: the id that was asked for there is Shield Slam, the
    sixteenth of the seventeen steps of the protection build, which a bot knows only when it is deep in that
    build, so every shield tank that was not deep in its build came back as a damage dealer without a Defensive
    Stance, a Taunt or a Sunder Armor. A bot cloned from a player matches no build, and so does a bot with too
    few points spent to tell two builds apart: for both of them only the shield says the tank.
    `.partybot stance` prints which of the two rules a selected warrior is under.

19. A warrior that is not under the shield tank rules is a damage warrior, and its rotation keeps one stance between
    the spells that ask for another: the Berserker Stance while the bot knows Bloodthirst and has not taken Improved
    Overpower, the Battle Stance for every other damage warrior (`GetWarriorHomeStanceSpell()`). Neither the asked
    talent nor the worn stance is saved with a bot, so the rule is read from what the bot knows. Bloodthirst is a rank
    spell of the Fury tree that the spell data of the bot shows; Improved Overpower is a passive of the Arms tree,
    which gives no spell of its own and is to be seen only in the spell map of the character - which is why
    `PopulateSpellData()` collects it next to Tactical Mastery, keeps the best rank of either, and does so before the
    passive spells of that map are skipped a few lines below.
    Keeping a stance is not never changing one. The rotation changes into the stance that a spell of another stance
    asks for, casts there, and asks the stance of the talent back: the ask of that stance comes before a want that
    names no stance, and a rotation that found nothing else to do asks it back too
    (`UpdateWarriorDamageRotation()`). An ask that the core refuses does not end the update, because a routine that
    waits for a stance that is not coming never runs at all - the want is answered in the stance that is worn, and the
    ask is made again on the next update. The ask is made for one want only where that want could really be cast in
    the stance of the talent: a spell that the data of the spell keeps to the other stance would buy a change that
    ends with itself refused, and the next update would change back again, two stance asks a second and no spell
    behind either. Which stance the data of a spell asks for is read from `SpellEntry::Stances`, the field that
    `GetErrorAtShapeshiftedCast()` reads (`GetSpellStanceRequirement()`), so the rotation carries no table of which
    ability belongs to which form; a spell whose data names no stance goes off in the stance that is worn. A want that
    is worth no more than the rage and the global cooldown of a change is collected with `canChangeStance = false`
    and is done in the stance that is worn or not at all: `Overpower` is the case the maintainer named, and
    `Mortal Strike`, `Bloodthirst`, `Rend`, `Heroic Strike`, `Battle Shout`, `Death Wish` and `Berserker Rage` are
    the same reading. `Bloodthirst` names no stance in the data of this world, which is what makes the stance of the
    talent the stance in which the abilities of a talent go off: a bot that keeps the Berserker Stance and a bot that
    has the Battle Stance back because it took Improved Overpower both use it where they stand.
    The order of the rotation is `CollectWarriorDamageWants()`, which writes the wants of the maintainer in their rank
    and nothing else: `Berserker Rage` when a fear holds the bot in the Berserker Stance; the `Intercept` jump at a
    mob that stands beyond a swing of the bot and inside the range of the spell, with the stance that the want names
    and the rank of `Tactical Mastery` of the paragraph above; the bank of a `Sweeping
    Strikes` over a pack; `Overpower` in the Battle Stance; `Execute` below twenty per cent of the health of the mob;
    the interrupt; `Mortal Strike`; `Bloodthirst`; `Whirlwind`; then, for a bot that has neither of the two talent
    abilities and nothing else to spend its rage on, `Rend` and the stacks of `Sunder Armor` without a limit of
    stacks; and `Heroic Strike` of a bot that has no `Sunder Armor`. The shout of attack strength is asked for while
    the fight lasts and not only before it, because a party that has been fighting for a minute is a party that lost
    the shout on the way. What the list of the maintainer does not name is not asked: `Last Stand`, `Concussion Blow`,
    `Intimidating Shout`, `Retaliation`, `Recklessness`, `Disarm`, `Hamstring` and `Cleave` have no rule
    there, and a damage warrior does not take the Defensive Stance because its own health went low. `Death Wish` is
    asked for at the elite that the client marks with a skull, `IsSkullConEliteTarget()`, the same reading the shield
    tank uses for its `Thunder Clap`: an elite frame a few levels above the bot is not what the burst of a warrior is
    spent on.
    Three of the answers of an update are not casts. The rage that a want costs is asked for together with the rage of
    the stance change that the want needs, and where it is not there the answer is `Bloodrage` - the two are asked for
    together because a stance that the bot changed into and stands in with nothing to cast is rage spent on nothing.
    A `Sweeping Strikes` over a pack is a bank: while that spell is ready, the rotation waits for the rage that the
    data of the spell names and spends none of it on the wants below (`keepsRage`), because the pack is at its closest
    in the seconds in which the warrior stands in the middle of it, and the rage spent elsewhere is the sweep that
    never came. The armor debuff is the one spell of the rotation that adds a stack by being cast again while the mob
    still carries it, which is what the ordinary cast gate of the core refuses, so it goes through the forced cast that
    the shield tank uses for the same reason (`TryCastForcedWarriorSunder()`).
    The counts that an area of several mobs asks for are one read of the melee about the bot,
    `ReadWarriorMeleePack()`: the red names of the pack, a neutral at peace that the area would wake into a second
    fight that the party did not ask for, and a mob held by a control that damage breaks, which is the control that
    another party member is keeping. The neutral and the held mob each stop a `Sweeping Strikes` and a `Whirlwind`;
    a neutral that has joined a fight of its own is in that fight whatever the area does to it, and is no reason to
    hold a spell back. `PB_WARRIOR_MULTI_MOB_DIST` is the
    radius of that read, and the red names in it are what a pack is; `PB_WARRIOR_WHIRLWIND_RAGE` is the fifty points
    of rage that a Whirlwind is asked for at rather than the cost of the spell, because half of a Whirlwind is a
    stance change spent on nothing; `PB_WARRIOR_PUMMEL_STANCE_RAGE_MIN` and `PB_WARRIOR_PUMMEL_STANCE_RAGE_MAX` bound
    the window of rage in which a bot that stands in the Battle Stance may leave it for an interrupt. All of those are
    values for the maintainer to tune; `PB_RAGE_PER_POINT` is the ratio between the points that those rules speak of
    and the rage of the core.
    That read counts the mobs that a swing of this bot could really land on as well, and that count is the one a
    `Sweeping Strikes` is asked at: `PB_WARRIOR_SWEEPING_STRIKE_TARGETS` of them, each within the reach of the
    weapon of the bot as `Unit::CanReachWithMeleeAutoAttack()` measures it for its own auto attack, and each in the
    arc that the bot faces, which `IsInFrontOfWarrior()` reads at `PB_WARRIOR_FRONT_HALF_ANGLE`. The core has no
    test for the side that a character stands in because the client decides that, and the hit that a `Sweeping
    Strikes` copies lands on the mobs that the swing itself could reach: the count of the red names about the
    warrior let a bot with one mob in front of it and two behind spend its banked rage on a copy that had nothing to
    hit, which is what the second count is for. A `Whirlwind` draws its area round the bot and is asked at the red
    names of the pack as it was; the two numbers of the swing are for the maintainer to tune.
    The interrupt of a damage warrior stands below the abilities of its rotation rather than above them: the party
    loses less from a spell that the warrior was late to stop than from the hit that the same global cooldown would
    have bought. In the Berserker Stance `Pummel` is there for nothing; a bot that stands in the Battle Stance leaves
    it for the interrupt only where the talent that keeps the rage through a change was taken and the rage is inside
    the window above.

20. A paladin stands out of combat and blesses its group one member at a time, and it serves a member with what
    that member does in the group: `Blessing of Kings` for every member of the group that does not carry it,
    `Blessing of Wisdom` for the healers and the casters (a hunter goes with them, for the mana that its shots
    cost), and `Blessing of Might` for the melee and the tank. A retribution paladin or a feral druid that has its
    Might from another paladin is given the Wisdom instead; a warrior and a rogue have no mana bar, so nothing is
    left for them. `Blessing of Light` and `Blessing of Sanctuary` are not read from the spellbook at all now, and
    the one blessing that used to be picked at random for a paladin bot to put on whoever stood without one is
    gone with `pBlessingBuff`.
    What a member does in the group is asked of `GetGroupMemberRole()`: the role of a bot of this subsystem comes
    from its AI, and anybody else is read by `ReadPlayerRole()` - the reading of the signature spells that
    `AutoAssignRole()` makes a bot's role from, which one function owns now. A player that is not a bot is served
    as the talent that its spellbook says rather than as its class; that is the same question, and one function
    answers it. In a dungeon or a raid the bot asks for the greater blessing of a kind when it knows it, and the
    ordinary one outside it - `Map::IsDungeon()` again, as rule 17 asks it, so a battleground is open ground and a
    battle paladin serves its group with the ordinary blessings. The two ranks are kept apart while the spellbook
    is read, `Greater Blessing of ...` before the ordinary name that every one of those contains: the ranks of the
    two families are numbered separately, and the higher rank of one is not the higher buff of the other.
    `UpdatePaladinGroupBlessings()` is the one owner of all of this for the party paladin and the battle paladin;
    `SelectPaladinBlessingTarget()` walks the group in the order the group lists its members and
    `SelectPaladinBlessingSpell()` answers what one member is waiting for. A member that carries a blessing in
    either of its ranks, or a stronger spell of the same kind, is passed over: `SelectPaladinBlessingOfKind()`
    asks `IsValidBuffTarget()` about both of the ranks of a kind, so the greater rank of a dungeon is not put over
    an ordinary blessing that is standing on the member already - the bot waits for that buff to run out, which is
    where its own greater rank comes up. That is what keeps two paladins of one group from putting the same
    blessing on one member twice, and what the wait below is for is the member that both of them would take next.
    A paladin of the group that stands out of combat, on foot, with its global cooldown running is blessing the
    group: the blessings are the only spells that a paladin stands about a group to cast, and it puts them on one
    member after another, which is that cooldown over and over. Such a paladin is left
    `PB_PALADIN_BUFF_CONFLICT_WAIT_S` of twenty seconds to finish before the bot begins its own. The blessings of
    the bot then go in a round, which is not broken off in the middle: it closes when no member of the group is
    missing a blessing, or at `PB_PALADIN_BUFF_ROUND_S`, and another paladin about the group is looked for only
    between the rounds. That is the answer to a paladin that buffs a group with its own hands and to a second
    paladin bot at the same time; the auras above are what stops either of them blessing a member twice.
    In combat the blessings answer what is happening to the group. `SelectPaladinProtectionTarget()` gives
    `Blessing of Protection` to the caster of the group that can not stand a physical hit with armour, a shield
    or a pet of its own and that is the closest to death, once it is under `PB_PALADIN_PROTECTION_HEALTH_PERCENT`;
    a member that stands under an immunity over every school is passed over, which is the Ice Block that the
    frost talent of a mage grants and the Divine Shield of another paladin. `Unit::IsTotalImmune()` is the
    reading of that state, the same one that keeps a bot from attacking or healing an immune unit, so no id of a
    rank of either spell is written in the source and no spell of the member is read: a spell that is only
    learned, or only off its cooldown, protects nobody and answers nothing. `Lay on Hands` is kept for the tank
    of the group at `PB_PALADIN_LAY_ON_HANDS_HEALTH_PERCENT`, which is this bot itself when it plays the tank,
    and `SelectPaladinGroupTank()` answers who that member is. `Blessing of Sacrifice` is neither used nor read
    from
    the spellbook: the life that it moves onto a member is the life that the paladin then lacks for the healing
    of the party. `Blessing of Freedom` answers a member that stands held in place, by a root or by a slow, and
    it goes to the tank of the group while the group has one and to this bot when it has none - a paladin that
    is slowed while its tank stands held keeps the blessing for the tank. It stands below `Cleanse`: the
    battleground routine asks for the freedom after its Cleanse, and a party bot only reaches the routine of its
    class after `CheckForDispelTargets()`. It also waits `PB_PALADIN_FREEDOM_WAIT_S` of three seconds after the
    holding is first seen, which is the time that the Cleanse of the bot and the mobs of the party have for a
    hold that needs no blessing of its own. These three answers are owned by `SelectPaladinProtectionTarget()`,
    `SelectPaladinLayOnHandsTarget()` and `SelectPaladinFreedomTarget()` for the party paladin and the battle
    paladin alike; the battleground adds its own reason to leave the carrier of its flag out of the Protection.

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
- The rank of the poison that a rogue bot wears is the highest rank of that family whose `SpellLevel` in
  `Spell.dbc` the level of the bot has reached (`GetHighestRankOfPoisonByName()`, read by `PopulateSpellData()`),
  because that column is the only level this world gives for the enchantment of a poison item: the vendor that sells
  such an item to a rogue of that level is not asked, and neither is the Poisons skill of the quest of the class.

## Commands of this subsystem
- The handlers live in `PlayerBotMgr.cpp`; the sub command tables of `.partybot` and `.bot` are in
  `src/game/Chat/Chat.cpp`, which is a CRLF file. Add new commands to those tables with the existing aligned
  column layout and keep the CRLF endings.
- `.partybot stance`, for the selected bot, prints what decides the stance of a warrior: the rules it is under
  (shield tank or damage, from the off hand item), the talent its gear is rated for, the form it wears, which of the
  three stance spells and the `Intercept` jump its spellbook knows, the gates of the stance the rules ask for now,
  and the reason that the last stance ask was refused (`TrySwitchWarriorStance()` records it in
  `m_warriorStanceBlock`). That is how a
  stance that never comes is told apart: a spell the bot has not learned, a gate that never clears, or a rule that
  was never reached.
  The command also says whether an opening of a charge still stands for a target and what it has spent waiting for
  the stance that it asked, which is how a shield tank that stands out of combat in the Battle Stance of its charge
  is told from a tank whose rules have gone wrong: the ask owns the stance while it stands.
  For a damage warrior the command prints the two facts that its stance is made of - whether Bloodthirst was learned
  and whether Improved Overpower was taken - because nothing else about a bot says which of the two stances its rules
  should keep, and a bot between the level at which one is learned and the level at which the other is taken is a bot
  whose stance changes in the middle of its levels.
  A stance or the `Intercept` jump listed as not known is a spell that was never taught, which is a question for the
  spell syncs of rule 13 and not for the stance rules or the rotation, so the command also lists the quests of this
  world that would give such a spell to a character of the class and race of the bot, with the minimum level and the
  class and race masks that their rows of `quest_template` ask for, or the words that no quest gives it. `Intercept`
  is asked for there because it is the other ability of a warrior that this world teaches with a class quest and not
  with a trainer, and because a warrior that knows it at the level of that quest is the first thing that the two
  rules of the jump ask for.
- The talents that a bot can be summoned with are arguments of `.partybot add` and not sub commands of
  `src/game/Chat/Chat.cpp`: the list is `gearSpecAliases` in `BotGearScoring.cpp` and what every page means is in
  `BotGearScoring.md`. A new talent is a row in that list, with nothing to change in the command tables.
- The level of a bot summoned by `.partybot add` comes from `PickPartyBotLevel()` in `PlayerBotMgr.cpp` and not
  from the level of the player: the same level up to 10, two levels either way above that, and the maximum player
  level once the player has it. An explicit level is parsed after the abbreviation, only for GM accounts and for
  `PartyBot.SkipChecks`, and is clamped to `MaxPlayerLevel`.
- Everything that reads a table of this subsystem is reached from `PlayerBotMgr::Load()`, which `.bot reload`
  calls again.
