-- The sql of the player bot customization of this fork in one file: the tables that the bots read to be dressed
-- and to learn their talents, and the talent builds that this fork chose. It is one file because it is one
-- feature, and it is applied by hand as every file of this directory is (README.md here). It can be applied again
-- without damaging the database: a table is created with `CREATE TABLE IF NOT EXISTS`, which leaves the rows that a
-- running server has tuned alone, and the steps of one bot talent are deleted and written back as a whole build.
-- `.bot reload` reads both tables again, so retuning a weight or a build needs neither a restart nor compiling.
--
-- The file is read from top to bottom, and that is the order that it needs:
--   1. `playerbot_gear_stat_weights` - the attribute weights that an item is rated by. The file adds no rows:
--      the compiled values of src/game/PlayerBots/BotGearScoring.cpp are used for a pair that has no row, so the
--      table only holds the values that were tuned by hand.
--   2. `playerbot_talent_build` - the build of one talent, one step of a build per row.
--   3. the rows of that table - the builds of the talents that this fork can summon a bot with.
--
-- Nothing that is not a bot is here. The creature that must not aggro and the vendors of the cities are the other
-- files of this directory, and they do not depend on this one.

-- --------------------------------------------------------------------------------------- 1. the gear attributes
-- The compiled values of src/game/PlayerBots/BotGearScoring.cpp are used for the pairs that have no row
-- here, so the table only needs the rows that change a value.
-- The score of an item is the sum of (what the item gives of an attribute) x (the weight of that attribute),
-- where one percent of hit, critical strike, dodge, parry or block counts as one unit of that attribute.
--
-- `spec`, the talent of the bot:
--   0 none                     8 rogue                        16 warlock
--   1 arms warrior             9 discipline priest             17 balance druid
--   2 fury warrior            10 holy priest                  18 feral (cat) druid
--   3 protection warrior      11 shadow priest                19 feral (bear) druid
--   4 holy paladin            12 elemental shaman            20 restoration druid
--   5 protection paladin      13 enhancement shaman
--   6 retribution paladin     14 restoration shaman
--   7 hunter                  15 mage
--
-- `stat`, the attribute that the weight is for:
--   0 strength                  8 ranged attack power        16 any resistance
--   1 agility                   9 spell damage               17 dodge %
--   2 intelligence             10 healing                   18 parry %
--   3 spirit                   11 physical critical strike % 19 block %
--   4 stamina                  12 physical hit %
--   5 fixed mana               13 spell critical strike %
--   6 fixed health             14 spell hit %
--   7 melee attack power       15 mana per 5 sec
CREATE TABLE IF NOT EXISTS `playerbot_gear_stat_weights` (
  `spec` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `stat` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `weight` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`spec`,`stat`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- -------------------------------------------------------------------------------------- 2. the talent build table
-- The talent build that a bot learns, one row per step of a build. A bot spends its free talent points following
-- the steps of its talent in `seq` order, so the rows are written the way a player writes a build in the talent
-- calculator: the primary tree of the talent is filled before another tree is touched, and a bot that has not
-- reached the maximum level has the first steps of the same build. The rows of a talent that has no row here are
-- taken from the premade templates of `player_premade_spell_template`, or learned at random when there is none.
-- `.bot reload` reads the table again, so a build can be retuned without a restart and without compiling; see
-- src/game/PlayerBots/BotTalentBuild.cpp, which resolves every `rank_spell` and says in the log what does not fit.
-- Only the table is added here, and the rows of the builds are written below in this file; a
-- database that has the table and no rows works as it did before, with the fallbacks above.
--
-- `class` and `talent_page` say which talent the build is for. A build is looked up by the class of the bot and by
-- the page of its talent (TalentTabEntry.tabpage), which is what the abbreviations of `.partybot add` give:
--   1 warrior   0 武器战 arms     1 狂暴战 fury       2 防战 protection
--   2 paladin   0 神圣骑士 holy   1 防骑 protection   2 惩戒骑 retribution
--   3 hunter    0 野兽控制 bm     1 射击 mm           2 生存 survival
--   4 rogue      0 刺杀 assass    1 武器 combat       2 敏锐 subtlety
--   5 priest    0 戒律牧 disc    1 神圣牧师 holy     2 暗牧 shadow
--   7 shaman    0 元素萨 elem     1 增强萨 enha       2 恢复萨 resto
--   8 mage      0 奥法 arcane     1 火法 fire          2 冰法 frost
--   9 warlock   0 痛苦术 affl     1 恶魔术 demo       2 毁灭术 destr
--  11 druid     0 平衡德 balance  1 野德 feral        2 恢复德 resto
-- The feral page of a druid is read as the cat by a melee damage role and as the bear by a tank role, so one build
-- of that page is used for both until a build of its own is added for the bear.
--
-- `seq` is the order of the steps of the build, starting at 1; a step takes `ranks` ranks of one talent, from the
-- lowest rank that the bot does not have yet. A build comes back to a talent when its last rank is a late point,
-- so a talent can have more than one step.
-- `rank_spell` is the id of any rank of the talent in Spell.dbc, of any tree of the class; the talent that has it
-- is looked up, and a row whose spell is not a talent of the class that it is declared for is skipped with a word
-- in the log. The steps of one build are all keyed by the class and the `talent_page` of the talent that the build
-- belongs to, so the steps of the second tree of a build are keyed by that talent too.
-- `name` is the name of the talent, to read the table and to find a wrong `rank_spell` in the log.
CREATE TABLE IF NOT EXISTS `playerbot_talent_build` (
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `talent_page` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `seq` smallint(5) unsigned NOT NULL DEFAULT '0',
  `rank_spell` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `ranks` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `name` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`class`,`talent_page`,`seq`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- ----------------------------------------------------------------------------------------- 3. the talent builds
--
-- Every step of a build is keyed by the class and the `talent_page` of the talent that the build belongs to, even
-- when the step is a talent of another tree of that class, so the rows of one talent are read as one build.
--
-- A build is the 51 points of a level 60 character: 31 points in the primary tree, which is what the last talent
-- of a tree asks for, and the 20 points that are left in a second tree. The rows of a build are ordered the way
-- the points are spent, the primary tree from its first row down and then the second tree, so a bot that has not
-- reached the maximum level learns the first rows of the build of its talent and has the primary tree of that
-- talent filled before another tree is touched. The order of the rows also keeps the rule that a talent can not be
-- taken before the rows above it have enough points; the steps that the level of a bot can not reach yet are
-- simply not learned, which is what the points of that level add up to.
--
-- The ranks of one talent may be written in more than one step when a player of that talent takes some of them
-- early and the rest at the last levels: `LearnTalentsFromBuild()` counts the ranks that the bot already has, so a
-- later step of a talent that an earlier step took goes on from the rank that the bot stopped at. 狂暴战 below is
-- written that way, since the last ranks of 双武器专精 are what a fury warrior leaves for its last levels, and one
-- step for that talent would spend the points of thirty and odd levels on them too early.
--
-- The talent trees, the ranks of every talent and the spell ids of those ranks are the data of the talent
-- calculator of https://classicdb.ch (a Wowhead engine; wowhead.com itself answers 403 to a plain request, and its
-- calculator is the same data). The choice of which ranks to take follows the usual classic build of that talent:
-- the talents that a bot has nothing to use in a dungeon or a battleground (the weapon skill of a weapon that the
-- gear rating does not give it, the crowd control that only matters against players) are the ranks that are left
-- out when the 31 points are counted.
--
-- A rogue is offered one build, the one of the combat talent: a rogue is rated by its class alone
-- (`GEAR_SPEC_ROGUE`), so `.partybot add dz` names the talent of the bot without a second talent to choose
-- between. Its steps are all keyed to talent page 1, the page of the combat talent, whatever tree the talent of a
-- step is in, and its points go 31 into the combat tree and 20 into the assassination tree like the second tree of
-- any other build.
--
-- The feral page of a druid is read as the cat by a melee damage role and as the bear by a tank role, so the two
-- share one build until a build of its own is added for the bear.

-- arms (武器战), class 1 warrior talent page 0: 51 points, primary tree Arms
-- The steps are the order in which a 武器战 spends its points from level 10 to level 60, one rank per level: the
-- 31 points of Arms up to 致死打击 (Mortal Strike) at level 40, then the 20 points of Fury (talent page 1) of the
-- levels above it. The three weapon skill talents of the tree are in that order as the player of the talent takes
-- them: 双手武器专精 (12163, of which the build of the talent takes three of the five ranks that the
-- talent has), 斧专精 (12785) and 剑类武器专精 (12281).
DELETE FROM `playerbot_talent_build` WHERE `class`=1 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 1, 16462, 2, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 2, 12286, 3, 'Improved Rend');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 3, 12295, 5, 'Tactical Mastery');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 4, 12834, 3, 'Deep Wounds');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 5, 12290, 2, 'Improved Overpower');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 6, 16493, 2, 'Impale');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 7, 12163, 3, 'Two-Handed Weapon Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 8, 12292, 1, 'Sweeping Strikes');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 9, 12785, 5, 'Axe Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 10, 12281, 4, 'Sword Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 11, 12294, 1, 'Mortal Strike');
--   then 20 points of Fury (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 12, 12320, 5, 'Cruelty');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 13, 12322, 5, 'Unbridled Wrath');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 14, 12318, 5, 'Improved Battle Shout');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 15, 20502, 2, 'Improved Execute');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 16, 12317, 3, 'Enrage');

-- fury (狂暴战), class 1 warrior talent page 1: 51 points, primary tree Fury
-- The steps are the order in which a fury warrior spends its points from level 10 to level 60, one rank per
-- level: the 31 points of Fury up to 嗜血 (Bloodthirst) at level 40, the 17 points of Arms that the levels above
-- it add, and the three ranks of 双武器专精 (Dual Wield Specialization) that are left at levels 58 to 60. The two
-- ranks of that talent at levels 33 and 34 are what those levels ask for; its last three come with its last
-- levels, so that a bot of forty has 乱舞 (Flurry) and 嗜血 rather than the last ranks of a two weapon talent.
DELETE FROM `playerbot_talent_build` WHERE `class`=1 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 1, 12320, 5, 'Cruelty');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 2, 12322, 5, 'Unbridled Wrath');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 3, 12318, 5, 'Improved Battle Shout');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 4, 20502, 2, 'Improved Execute');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 5, 12317, 5, 'Enrage');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 6, 12328, 1, 'Death Wish');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 7, 23584, 2, 'Dual Wield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 8, 12319, 5, 'Flurry');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 9, 23881, 1, 'Bloodthirst');
--   then 17 points of Arms (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 10, 12286, 3, 'Improved Rend');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 11, 16462, 2, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 12, 12295, 5, 'Tactical Mastery');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 13, 12834, 3, 'Deep Wounds');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 14, 12290, 2, 'Improved Overpower');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 15, 16493, 2, 'Impale');
--   and the three ranks of Dual Wield Specialization that are left (talent page 1 again)
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 16, 23584, 3, 'Dual Wield Specialization');

-- protection (防战), class 1 warrior talent page 2: 51 points, primary tree Protection
-- The steps are the order in which a 防战 spends its points from level 10 to level 60, one rank per level: the
-- 偏斜 (Deflection) and 战术掌握 (Tactical Mastery) of Arms and the 残忍 (Cruelty) of Fury first, since those
-- are the ranks that its low levels ask for, then the Protection tree from 盾牌专精 (Shield Specialization) down
-- to 盾牌猛击 (Shield Slam), and the five ranks of 强化挫志怒吼 (Improved Demoralizing Shout) of Fury last.
-- Unlike the two builds above it, this one does not put 31 points in one tree: it is the order of a tank
-- who plays with a shield, in which the ranks that a tank wants in a dungeon (挑衅 Defiance, 强化破甲攻击
-- Improved Sunder Armor, 强化盾墙 Improved Shield Wall) are what the levels add and the deep talents of the
-- tree come with the levels above them. 强化破甲攻击 is asked for at level 44 and again at levels 46 and 47,
-- because a tank of that level wants its first rank and leaves the two others; `LearnTalentsFromBuild()`
-- counts the ranks that the bot already has, so the second step goes on from the rank that the first one
-- stopped at.
-- A step of a deep talent of the tree comes between the ranks that the levels ask for, and the row that the
-- talent stands in is not open yet at that moment. The walk over the steps asks for such a step again once the
-- points that follow it are spent, so the bot learns it as soon as its tree reaches that row; the steps of a
-- talent of another tree that the build never has the points of are the steps that it leaves out.
DELETE FROM `playerbot_talent_build` WHERE `class`=1 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 1, 16462, 5, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 2, 12295, 5, 'Tactical Mastery');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 3, 12320, 5, 'Cruelty');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 4, 12298, 5, 'Shield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 5, 12301, 2, 'Improved Bloodrage');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 6, 12299, 3, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 7, 12975, 1, 'Last Stand');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 8, 12307, 1, 'Improved Shield Block');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 9, 12303, 5, 'Defiance');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 10, 12302, 2, 'Improved Taunt');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 11, 12308, 1, 'Improved Sunder Armor');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 12, 12809, 1, 'Concussion Blow');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 13, 12308, 2, 'Improved Sunder Armor');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 14, 12312, 2, 'Improved Shield Wall');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 15, 16538, 5, 'One-Handed Weapon Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 16, 23922, 1, 'Shield Slam');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 17, 12324, 5, 'Improved Demoralizing Shout');

-- holy (神圣骑士), class 2 paladin talent page 0: 51 points, primary tree Holy
-- The steps are the order in which a 神圣骑士 spends its points from level 10 to level 60, one rank per level. The
-- Holy tree goes 神圣智慧 (Divine Intellect), 精神集中 (Spiritual Focus), 治疗之光 (Healing Light), 强化圣疗术
-- (Improved Lay on Hands), 启发 (Illumination), 神恩术 (Divine Favor), 强化智慧祝福 (Improved Blessing of Wisdom),
-- 不灭信仰 (Unyielding Faith), 神圣强化 (Holy Power) and, at the level that fills the tree, 神圣震击 (Holy Shock).
-- The levels above it ask for the ranks of the other two trees that a healing paladin wants: 强化虔诚光环
-- (Improved Devotion Aura), 守护者的宠爱 (Guardian's Favor) and 坚韧 (Toughness), and the ranks of 偏斜
-- (Deflection), which is the first row of the retribution tree and asks for no point in that tree.
DELETE FROM `playerbot_talent_build` WHERE `class`=2 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 1, 20257, 5, 'Divine Intellect');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 2, 20205, 5, 'Spiritual Focus');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 3, 20237, 3, 'Healing Light');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 4, 20234, 2, 'Improved Lay on Hands');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 5, 20210, 5, 'Illumination');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 6, 20216, 1, 'Divine Favor');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 7, 20244, 2, 'Improved Blessing of Wisdom');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 8, 9453, 2, 'Unyielding Faith');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 9, 5923, 5, 'Holy Power');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 10, 20473, 1, 'Holy Shock');
--   then the ranks of Protection (talent page 1) and of the first row of Retribution (talent page 2)
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 11, 20138, 5, 'Improved Devotion Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 12, 20174, 2, 'Guardian\'s Favor');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 13, 20143, 3, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 14, 20217, 1, 'Blessing of Kings');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 15, 20042, 5, 'Improved Blessing of Might');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 16, 20060, 4, 'Deflection');

-- protection (防护骑士), class 2 paladin talent page 1: 51 points, primary tree Protection
-- The steps are the order in which a 防护骑士 spends its points from level 10 to level 60, one rank per level: the
-- protection tree goes 盾牌壁垒 (Redoubt), 精确 (Precision), 守护者的宠爱 (Guardian's Favor), 强化正义之怒
-- (Improved Righteous Fury), 王者祝福 (Blessing of Kings), 盾牌专精 (Shield Specialization), 强化制裁之锤
-- (Improved Hammer of Justice), 庇护祝福 (Blessing of Sanctuary), 清算 (Reckoning), 单手武器专精 (One-Handed
-- Weapon Specialization) and, at the level that fills the tree, 神圣之盾 (Holy Shield). The levels above it take
-- 神圣之力 (Divine Strength), 强化正义圣印 (Improved Seal of Righteousness) and 奉献 (Consecration), and then
-- 祈福 (Benediction) and 偏斜 (Deflection), which are the first row of the retribution tree and ask for no point
-- in it. `LearnTalentsFromBuild()` matches a step by the ranks that the entry of Talent.dbc carries, so the one
-- line that the maintainer may have to move is 奉献: the rank of that name in the data of this client is 20116.
DELETE FROM `playerbot_talent_build` WHERE `class`=2 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 1, 20127, 5, 'Redoubt');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 2, 20189, 3, 'Precision');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 3, 20174, 2, 'Guardian\'s Favor');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 4, 20468, 3, 'Improved Righteous Fury');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 5, 20217, 1, 'Blessing of Kings');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 6, 20148, 3, 'Shield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 7, 20487, 3, 'Improved Hammer of Justice');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 8, 20911, 1, 'Blessing of Sanctuary');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 9, 20177, 4, 'Reckoning');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 10, 20196, 5, 'One-Handed Weapon Specialization');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 11, 20925, 1, 'Holy Shield');
--   then the ranks of the other two trees of the levels above it
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 12, 20262, 5, 'Divine Strength');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 13, 20224, 5, 'Improved Seal of Righteousness');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 14, 20116, 1, 'Consecration');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 15, 20101, 5, 'Benediction');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 16, 20060, 4, 'Deflection');

-- retribution (惩戒骑), class 2 paladin talent page 2: 51 points, primary tree Retribution
-- The steps are the order in which a 惩戒骑 spends its points from level 10 to level 60, one rank per level: the
-- tree starts at 祈福 (Benediction), 强化审判 (Improved Judgement) and the defensive ranks of 偏斜 (Deflection),
-- takes 命令圣印 (Seal of Command) at level 20, goes on over 定罪 (Conviction), 正义追击 (Pursuit of Justice),
-- the 辩护 (Vindication) and 双手武器专精 (Two-Handed Weapon Specialization) that the middle levels ask for, and
-- ends the tree with 复仇 (Vengeance) and 忏悔 (Repentance). The levels above it add the ranks of the other two
-- trees that a ret paladin wants when its own tree is done: 强化虔诚光环 (Improved Devotion Aura), 精确
-- (Precision), 守护者的宠爱 (Guardian's Favor), 王者祝福 (Blessing of Kings) and the ranks of 神圣之力 (Divine
-- Strength) and 神圣智慧 (Divine Intellect). Two talents are written in two steps because the order of the build
-- leaves their last ranks for the levels in the middle of the tree; `LearnTalentsFromBuild()` counts the ranks
-- that the bot already has, so such a step goes on from the rank that the step before it stopped at.
DELETE FROM `playerbot_talent_build` WHERE `class`=2 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 1, 20101, 5, 'Benediction');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 2, 25956, 2, 'Improved Judgement');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 3, 20060, 3, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 4, 20375, 1, 'Seal of Command');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 5, 20117, 5, 'Conviction');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 6, 26022, 2, 'Pursuit of Justice');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 7, 9452, 2, 'Vindication');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 8, 20111, 3, 'Two-Handed Weapon Specialization');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 9, 9452, 1, 'Vindication');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 10, 20060, 1, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 11, 20049, 5, 'Vengeance');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 12, 20066, 1, 'Repentance');
--   then the ranks of Holy (talent page 0) and Protection (talent page 1) of the levels above it
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 13, 20138, 5, 'Improved Devotion Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 14, 20189, 3, 'Precision');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 15, 20174, 2, 'Guardian\'s Favor');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 16, 20217, 1, 'Blessing of Kings');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 17, 20262, 5, 'Divine Strength');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 18, 20257, 4, 'Divine Intellect');

-- beast mastery (野兽控制猎人), class 3 hunter talent page 0: 51 points, primary tree Beast Mastery
DELETE FROM `playerbot_talent_build` WHERE `class`=3 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 1, 19552, 5, 'Improved Aspect of the Hawk');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 2, 19583, 5, 'Endurance Training');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 3, 19557, 2, 'Improved Eyes of the Beast');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 4, 19549, 5, 'Improved Aspect of the Monkey');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 5, 19609, 3, 'Thick Hide');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 6, 24443, 2, 'Improved Revive Pet');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 7, 19559, 2, 'Pathfinding');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 8, 19596, 1, 'Bestial Swiftness');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 9, 19616, 5, 'Unleashed Fury');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 10, 19574, 1, 'Bestial Wrath');
--   then 20 points of Marksmanship (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 11, 19407, 5, 'Improved Concussive Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 12, 19416, 5, 'Efficiency');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 13, 19434, 1, 'Aimed Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 14, 19454, 5, 'Improved Arcane Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 0, 15, 19464, 4, 'Improved Serpent Sting');

-- marksmanship (射击猎人), class 3 hunter talent page 1: 51 points, primary tree Marksmanship
DELETE FROM `playerbot_talent_build` WHERE `class`=3 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 1, 19407, 5, 'Improved Concussive Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 2, 19416, 5, 'Efficiency');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 3, 19426, 5, 'Lethal Shots');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 4, 19434, 1, 'Aimed Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 5, 19454, 5, 'Improved Arcane Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 6, 19498, 3, 'Hawk Eye');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 7, 19464, 1, 'Improved Serpent Sting');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 8, 19485, 5, 'Mortal Shots');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 9, 19506, 1, 'Trueshot Aura');
--   then 20 points of Beast Mastery (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 10, 19552, 5, 'Improved Aspect of the Hawk');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 11, 19583, 5, 'Endurance Training');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 12, 19609, 3, 'Thick Hide');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 13, 24443, 2, 'Improved Revive Pet');
INSERT INTO `playerbot_talent_build` VALUES (3, 1, 14, 19616, 5, 'Unleashed Fury');

-- survival (生存猎人), class 3 hunter talent page 2: 51 points, primary tree Survival
DELETE FROM `playerbot_talent_build` WHERE `class`=3 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 1, 24293, 3, 'Monster Slaying');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 2, 19151, 3, 'Humanoid Slaying');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 3, 19295, 5, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 4, 19184, 5, 'Entrapment');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 5, 19159, 2, 'Savage Strikes');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 6, 19228, 5, 'Improved Wing Clip');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 7, 19239, 2, 'Clever Traps');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 8, 19255, 5, 'Survivalist');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 9, 19386, 1, 'Wyvern Sting');
--   then 20 points of Marksmanship (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 10, 19407, 5, 'Improved Concussive Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 11, 19416, 5, 'Efficiency');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 12, 19434, 1, 'Aimed Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 13, 19454, 5, 'Improved Arcane Shot');
INSERT INTO `playerbot_talent_build` VALUES (3, 2, 14, 19464, 4, 'Improved Serpent Sting');

-- combat (战斗贼), class 4 rogue talent page 1: 51 points, the only rogue build
-- The steps are the order in which a 战斗贼 spends its points from level 10 to level 60, one rank per level: the
-- combat tree goes 强化邪恶攻击 (Improved Sinister Strike), 闪电反射 (Lightning Reflexes), 精确 (Precision),
-- 偏斜 (Deflection), 还击 (Riposte), 双武器专精 (Dual Wield Specialization), 剑刃乱舞 (Blade Flurry),
-- 剑类武器专精 (Sword Specialization), 侵略 (Aggression), 武器专家 (Weapon Expertise) and, as the 31st point,
-- 冲动 (Adrenaline Rush). The levels above it take the 20 points of the assassination tree: 恶意 (Malice),
-- 强化切割 (Improved Slice and Dice), 无情 (Ruthlessness), 致命偷袭 (Lethality), 无情打击 (Relentless Strikes
-- in the data named above), the rank of 无情 that the order leaves for level 57, and 强化刺骨 (Improved
-- Eviscerate). A step of a talent whose ranks an earlier step has taken goes on from the rank that the bot has.
DELETE FROM `playerbot_talent_build` WHERE `class`=4 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 1, 13732, 2, 'Improved Sinister Strike');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 2, 13712, 3, 'Lightning Reflexes');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 3, 13705, 5, 'Precision');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 4, 13713, 5, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 5, 14251, 1, 'Riposte');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 6, 13715, 5, 'Dual Wield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 7, 13877, 1, 'Blade Flurry');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 8, 13960, 3, 'Sword Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 9, 18427, 3, 'Aggression');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 10, 30919, 2, 'Weapon Expertise');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 11, 13750, 1, 'Adrenaline Rush');
--   then the 20 points of the assassination tree, still keyed to the page of the combat talent
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 12, 14138, 5, 'Malice');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 13, 14165, 3, 'Improved Slice and Dice');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 14, 14156, 2, 'Ruthlessness');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 15, 14128, 5, 'Lethality');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 16, 14179, 1, 'Relentless Strikes');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 17, 14156, 1, 'Ruthlessness');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 18, 14162, 3, 'Improved Eviscerate');

-- discipline (戒律牧), class 5 priest talent page 0: 51 points, primary tree Discipline
DELETE FROM `playerbot_talent_build` WHERE `class`=5 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 1, 14522, 5, 'Unbreakable Will');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 2, 14523, 5, 'Silent Resolve');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 3, 14748, 3, 'Improved Power Word: Shield');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 4, 14751, 1, 'Inner Focus');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 5, 14521, 3, 'Meditation');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 6, 14747, 3, 'Improved Inner Fire');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 7, 14520, 5, 'Mental Agility');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 8, 18551, 5, 'Mental Strength');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 9, 10060, 1, 'Power Infusion');
--   then 20 points of Holy (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 10, 14913, 2, 'Healing Focus');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 11, 14908, 3, 'Improved Renew');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 12, 14889, 5, 'Holy Specialization');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 13, 27900, 5, 'Spell Warding');
INSERT INTO `playerbot_talent_build` VALUES (5, 0, 14, 18530, 5, 'Divine Fury');

-- holy (神圣骑士), class 5 priest talent page 1: 51 points, primary tree Holy
DELETE FROM `playerbot_talent_build` WHERE `class`=5 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 1, 14913, 2, 'Healing Focus');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 2, 14908, 3, 'Improved Renew');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 3, 14889, 5, 'Holy Specialization');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 4, 27900, 5, 'Spell Warding');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 5, 18530, 5, 'Divine Fury');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 6, 15237, 1, 'Holy Nova');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 7, 27811, 3, 'Blessed Recovery');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 8, 14892, 3, 'Inspiration');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 9, 14912, 3, 'Improved Healing');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 10, 724, 1, 'Lightwell');
--   then 20 points of Discipline (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 11, 14522, 5, 'Unbreakable Will');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 12, 14523, 5, 'Silent Resolve');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 13, 14748, 3, 'Improved Power Word: Shield');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 14, 14751, 1, 'Inner Focus');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 15, 14521, 3, 'Meditation');
INSERT INTO `playerbot_talent_build` VALUES (5, 1, 16, 14747, 3, 'Improved Inner Fire');

-- shadow (暗牧), class 5 priest talent page 2: 51 points, primary tree Shadow
DELETE FROM `playerbot_talent_build` WHERE `class`=5 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 1, 15270, 4, 'Spirit Tap');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 2, 15268, 5, 'Blackout');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 3, 15275, 2, 'Improved Shadow Word: Pain');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 4, 15260, 5, 'Shadow Focus');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 5, 15273, 5, 'Improved Mind Blast');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 6, 15407, 1, 'Mind Flay');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 7, 17322, 3, 'Shadow Reach');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 8, 15257, 5, 'Shadow Weaving');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 9, 15473, 1, 'Shadowform');
--   then 20 points of Discipline (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 10, 14522, 5, 'Unbreakable Will');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 11, 14523, 5, 'Silent Resolve');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 12, 14748, 3, 'Improved Power Word: Shield');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 13, 14751, 1, 'Inner Focus');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 14, 14521, 3, 'Meditation');
INSERT INTO `playerbot_talent_build` VALUES (5, 2, 15, 14747, 3, 'Improved Inner Fire');

-- elemental (元素萨), class 7 shaman talent page 0: 51 points, primary tree Elemental
DELETE FROM `playerbot_talent_build` WHERE `class`=7 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 1, 16039, 5, 'Convection');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 2, 16035, 5, 'Concussion');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 3, 28996, 3, 'Elemental Warding');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 4, 16038, 3, 'Call of Flame');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 5, 16164, 1, 'Elemental Focus');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 6, 16040, 5, 'Reverberation');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 7, 16041, 5, 'Call of Thunder');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 8, 30160, 3, 'Elemental Devastation');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 9, 16166, 1, 'Elemental Mastery');
--   then 20 points of Enhancement (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 10, 17485, 5, 'Ancestral Knowledge');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 11, 16253, 5, 'Shield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 12, 16255, 5, 'Thundering Strikes');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 13, 16269, 1, 'Two-Handed Axes and Maces');
INSERT INTO `playerbot_talent_build` VALUES (7, 0, 14, 16256, 4, 'Flurry');

-- enhancement (增强萨), class 7 shaman talent page 1: 51 points, primary tree Enhancement
DELETE FROM `playerbot_talent_build` WHERE `class`=7 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 1, 17485, 5, 'Ancestral Knowledge');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 2, 16253, 5, 'Shield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 3, 16258, 2, 'Guardian Totems');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 4, 16255, 5, 'Thundering Strikes');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 5, 16262, 2, 'Improved Ghost Wolf');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 6, 16261, 3, 'Improved Lightning Shield');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 7, 16269, 1, 'Two-Handed Axes and Maces');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 8, 16256, 5, 'Flurry');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 9, 16252, 2, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 10, 17364, 1, 'Stormstrike');
--   then 20 points of Elemental (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 11, 16039, 5, 'Convection');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 12, 16035, 5, 'Concussion');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 13, 16041, 5, 'Call of Thunder');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 14, 16086, 2, 'Improved Fire Totems');
INSERT INTO `playerbot_talent_build` VALUES (7, 1, 15, 30160, 3, 'Elemental Devastation');

-- restoration (?), class 7 shaman talent page 2: 51 points, primary tree Restoration
DELETE FROM `playerbot_talent_build` WHERE `class`=7 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 1, 16182, 5, 'Improved Healing Wave');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 2, 16179, 5, 'Tidal Focus');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 3, 16176, 3, 'Ancestral Healing');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 4, 16173, 5, 'Totemic Focus');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 5, 16181, 5, 'Healing Focus');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 6, 16189, 1, 'Totemic Mastery');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 7, 16187, 5, 'Restorative Totems');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 8, 16194, 1, 'Tidal Mastery');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 9, 16190, 1, 'Mana Tide Totem');
--   then 20 points of Elemental (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 10, 16039, 5, 'Convection');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 11, 16035, 5, 'Concussion');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 12, 16041, 5, 'Call of Thunder');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 13, 16086, 2, 'Improved Fire Totems');
INSERT INTO `playerbot_talent_build` VALUES (7, 2, 14, 30160, 3, 'Elemental Devastation');

-- arcane (奥法), class 8 mage talent page 0: 51 points, primary tree Arcane
DELETE FROM `playerbot_talent_build` WHERE `class`=8 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 1, 11210, 2, 'Arcane Subtlety');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 2, 11222, 5, 'Arcane Focus');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 3, 11237, 5, 'Improved Arcane Missiles');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 4, 29441, 5, 'Magic Absorption');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 5, 11213, 5, 'Arcane Concentration');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 6, 11247, 2, 'Magic Attunement');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 7, 11242, 3, 'Improved Arcane Explosion');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 8, 18462, 3, 'Arcane Meditation');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 9, 12042, 1, 'Arcane Power');
--   then 20 points of Fire (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 10, 11069, 5, 'Improved Fireball');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 11, 11103, 5, 'Impact');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 12, 11119, 5, 'Ignite');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 13, 11100, 2, 'Flame Throwing');
INSERT INTO `playerbot_talent_build` VALUES (8, 0, 14, 11078, 3, 'Improved Fire Blast');

-- fire (火法), class 8 mage talent page 1: 51 points, primary tree Fire
DELETE FROM `playerbot_talent_build` WHERE `class`=8 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 1, 11069, 5, 'Improved Fireball');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 2, 11103, 5, 'Impact');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 3, 11119, 5, 'Ignite');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 4, 11100, 2, 'Flame Throwing');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 5, 11078, 3, 'Improved Fire Blast');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 6, 18459, 2, 'Incinerate');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 7, 11108, 3, 'Improved Flamestrike');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 8, 11366, 1, 'Pyroblast');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 9, 11083, 2, 'Burning Soul');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 10, 29074, 2, 'Master of Elements');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 11, 11129, 1, 'Combustion');
--   then 20 points of Frost (talent page 2)
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 12, 11070, 5, 'Improved Frostbolt');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 13, 29438, 3, 'Elemental Precision');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 14, 11071, 3, 'Frostbite');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 15, 11165, 2, 'Improved Frost Nova');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 16, 11175, 3, 'Permafrost');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 17, 16757, 1, 'Arctic Reach');
INSERT INTO `playerbot_talent_build` VALUES (8, 1, 18, 11160, 3, 'Frost Channeling');

-- frost (冰法), class 8 mage talent page 2: 51 points, primary tree Frost
DELETE FROM `playerbot_talent_build` WHERE `class`=8 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 1, 11189, 2, 'Frost Warding');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 2, 11070, 5, 'Improved Frostbolt');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 3, 29438, 3, 'Elemental Precision');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 4, 11207, 5, 'Ice Shards');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 5, 11071, 3, 'Frostbite');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 6, 11165, 2, 'Improved Frost Nova');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 7, 11175, 3, 'Permafrost');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 8, 11151, 3, 'Piercing Ice');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 9, 12472, 1, 'Cold Snap');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 10, 11185, 3, 'Improved Blizzard');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 11, 11426, 1, 'Ice Barrier');
--   then 20 points of Arcane (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 12, 11210, 2, 'Arcane Subtlety');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 13, 11222, 5, 'Arcane Focus');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 14, 29441, 4, 'Magic Absorption');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 15, 11213, 5, 'Arcane Concentration');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 16, 11247, 2, 'Magic Attunement');
INSERT INTO `playerbot_talent_build` VALUES (8, 2, 17, 11252, 2, 'Improved Mana Shield');

-- affliction (痛苦术), class 9 warlock talent page 0: 51 points, primary tree Affliction
DELETE FROM `playerbot_talent_build` WHERE `class`=9 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 1, 18174, 5, 'Suppression');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 2, 17810, 5, 'Improved Corruption');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 3, 18182, 2, 'Improved Life Tap');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 4, 17804, 5, 'Improved Drain Life');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 5, 18827, 3, 'Improved Curse of Agony');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 6, 17783, 5, 'Fel Concentration');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 7, 18288, 1, 'Amplify Curse');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 8, 18218, 2, 'Grim Reach');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 9, 18094, 2, 'Nightfall');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 10, 18220, 1, 'Dark Pact');
--   then 20 points of Demonology (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 11, 18697, 5, 'Demonic Embrace');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 12, 18731, 5, 'Fel Intellect');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 13, 18748, 5, 'Fel Stamina');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 14, 18709, 2, 'Master Summoner');
INSERT INTO `playerbot_talent_build` VALUES (9, 0, 15, 18769, 3, 'Unholy Power');

-- demonology (恶魔术), class 9 warlock talent page 1: 51 points, primary tree Demonology
DELETE FROM `playerbot_talent_build` WHERE `class`=9 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 1, 18692, 2, 'Improved Healthstone');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 2, 18694, 3, 'Improved Imp');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 3, 18697, 5, 'Demonic Embrace');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 4, 18703, 2, 'Improved Health Funnel');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 5, 18705, 3, 'Improved Voidwalker');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 6, 18731, 5, 'Fel Intellect');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 7, 18754, 3, 'Improved Succubus');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 8, 18708, 1, 'Fel Domination');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 9, 18748, 5, 'Fel Stamina');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 10, 18709, 1, 'Master Summoner');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 11, 19028, 1, 'Soul Link');
--   then 20 points of Affliction (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 12, 18174, 5, 'Suppression');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 13, 17810, 5, 'Improved Corruption');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 14, 18182, 2, 'Improved Life Tap');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 15, 17804, 5, 'Improved Drain Life');
INSERT INTO `playerbot_talent_build` VALUES (9, 1, 16, 18827, 3, 'Improved Curse of Agony');

-- destruction (毁灭术), class 9 warlock talent page 2: 51 points, primary tree Destruction
DELETE FROM `playerbot_talent_build` WHERE `class`=9 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 1, 17793, 5, 'Improved Shadow Bolt');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 2, 17778, 5, 'Cataclysm');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 3, 17788, 5, 'Bane');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 4, 18119, 5, 'Aftermath');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 5, 18126, 2, 'Improved Firebolt');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 6, 18130, 5, 'Devastation');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 7, 17877, 1, 'Shadowburn');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 8, 18135, 2, 'Intensity');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 9, 17962, 1, 'Conflagrate');
--   then 20 points of Affliction (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 10, 18174, 5, 'Suppression');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 11, 17810, 5, 'Improved Corruption');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 12, 18182, 2, 'Improved Life Tap');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 13, 17804, 5, 'Improved Drain Life');
INSERT INTO `playerbot_talent_build` VALUES (9, 2, 14, 18827, 3, 'Improved Curse of Agony');

-- balance (平衡德), class 11 druid talent page 0: 51 points, primary tree Balance
DELETE FROM `playerbot_talent_build` WHERE `class`=11 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 1, 16814, 5, 'Improved Wrath');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 2, 16918, 3, 'Improved Entangling Roots');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 3, 16821, 5, 'Improved Moonfire');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 4, 16902, 5, 'Natural Weapons');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 5, 16864, 1, 'Omen of Clarity');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 6, 16909, 5, 'Vengeance');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 7, 16850, 5, 'Improved Starfire');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 8, 16845, 1, 'Moonglow');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 9, 24858, 1, 'Moonkin Form');
--   then 20 points of Restoration (talent page 2)
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 10, 17050, 5, 'Improved Mark of the Wild');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 11, 17056, 5, 'Furor');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 12, 17069, 5, 'Improved Healing Touch');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 13, 5570, 1, 'Insect Swarm');
INSERT INTO `playerbot_talent_build` VALUES (11, 0, 14, 17118, 4, 'Subtlety');

-- feral (野德/熊德), class 11 druid talent page 1: 51 points, primary tree Feral Combat
DELETE FROM `playerbot_talent_build` WHERE `class`=11 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 1, 16934, 5, 'Ferocity');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 2, 16858, 5, 'Feral Aggression');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 3, 16947, 5, 'Feral Instinct');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 4, 16940, 2, 'Brutal Impact');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 5, 16929, 5, 'Thick Hide');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 6, 17002, 2, 'Feline Swiftness');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 7, 16979, 1, 'Feral Charge');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 8, 16942, 3, 'Sharpened Claws');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 9, 16966, 2, 'Improved Shred');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 10, 17007, 1, 'Leader of the Pack');
--   then 20 points of Balance (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 11, 16814, 5, 'Improved Wrath');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 12, 16821, 5, 'Improved Moonfire');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 13, 16902, 5, 'Natural Weapons');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 14, 16864, 1, 'Omen of Clarity');
INSERT INTO `playerbot_talent_build` VALUES (11, 1, 15, 16909, 4, 'Vengeance');

-- restoration (?), class 11 druid talent page 2: 51 points, primary tree Restoration
DELETE FROM `playerbot_talent_build` WHERE `class`=11 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 1, 17050, 5, 'Improved Mark of the Wild');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 2, 17056, 5, 'Furor');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 3, 17069, 5, 'Improved Healing Touch');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 4, 17106, 3, 'Reflection');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 5, 5570, 1, 'Insect Swarm');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 6, 17118, 4, 'Subtlety');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 7, 24968, 5, 'Tranquil Spirit');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 8, 17111, 2, 'Improved Rejuvenation');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 9, 18562, 1, 'Swiftmend');
--   then 20 points of Balance (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 10, 16814, 5, 'Improved Wrath');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 11, 16821, 5, 'Improved Moonfire');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 12, 16902, 5, 'Natural Weapons');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 13, 16864, 1, 'Omen of Clarity');
INSERT INTO `playerbot_talent_build` VALUES (11, 2, 14, 16909, 4, 'Vengeance');
