-- The talent build that a bot of one talent learns: one step of a build per row, in the order in which the points
-- are spent. The table comes from Custom-ADD_PLAYERBOT_TALENT_BUILD.sql of this directory, which is applied
-- before this file (README.md here gives the order); the content of the table lives here, because the builds are
-- chosen by hand for this fork. The file can be applied again: the steps of one
-- talent are deleted and written back as a whole build, so a build that was tuned by hand is replaced whole and
-- no half of it is left behind. `.bot reload` reads the table again without a restart; see
-- src/game/PlayerBots/BotTalentBuild.cpp, which resolves every `rank_spell` and says in the log what does not fit
-- the class that it is declared for.
--
-- Every step of a build is keyed by the class and the `talent_page` of the talent that the build belongs to, even
-- when the step is a talent of another tree of that class, so the rows of one talent are read as one build.
--
-- A build is the 51 points of a level 60 character: 31 points in the primary tree, which is what the last talent
-- of a tree asks for, and 20 points in a second tree. The rows of a build are ordered the way the points are
-- spent, the primary tree from its first row down and then the second tree, so a bot that has not reached the
-- maximum level learns the first rows of the build of its talent and has the primary tree of that talent filled
-- before another tree is touched. The order of the rows also keeps the rule that a talent can not be taken before
-- the rows above it have enough points; the steps that the level of a bot can not reach yet are simply not learned,
-- which is what the points of that level add up to.
--
-- The talent trees, the ranks of every talent and the spell ids of those ranks are the data of the talent
-- calculator of https://classicdb.ch (a Wowhead engine; wowhead.com itself answers 403 to a plain request, and its
-- calculator is the same data). The choice of which ranks to take follows the usual classic build of that talent:
-- the talents that a bot has nothing to use in a dungeon or a battleground (the weapon skill of a weapon that the
-- gear rating does not give it, the crowd control that only matters against players) are the ranks that are left
-- out when the 31 points are counted.
--
-- The build of a combat rogue puts all 51 points in one tree, because that is the talent that the tree teaches:
-- a rogue is rated by its class alone (`GEAR_SPEC_ROGUE`), so `.partybot add dz` only asks which tree to learn
-- and there is no second tree to give the extra points to. The steps of that build are ordered by row like the
-- others, and the last talent of its tree is taken as the 31st point as in the others, so a bot that has not
-- reached the maximum level has that talent too and spends the points that its level adds on the rows that are
-- left.
--
-- The feral page of a druid is read as the cat by a melee damage role and as the bear by a tank role, so the two
-- share one build until a build of its own is added for the bear.

-- arms (武器战), class 1 warrior talent page 0: 51 points, primary tree Arms
DELETE FROM `playerbot_talent_build` WHERE `class`=1 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 1, 12282, 3, 'Improved Heroic Strike');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 2, 16462, 5, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 3, 12286, 3, 'Improved Rend');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 4, 12285, 2, 'Improved Charge');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 5, 12295, 5, 'Tactical Mastery');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 6, 12287, 3, 'Improved Thunder Clap');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 7, 12290, 2, 'Improved Overpower');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 8, 12296, 1, 'Anger Management');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 9, 12834, 3, 'Deep Wounds');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 10, 12163, 2, 'Two-Handed Weapon Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 11, 12292, 1, 'Sweeping Strikes');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 12, 12294, 1, 'Mortal Strike');
--   then 20 points of Fury (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 13, 12321, 5, 'Booming Voice');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 14, 12320, 5, 'Cruelty');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 15, 12322, 5, 'Unbridled Wrath');
INSERT INTO `playerbot_talent_build` VALUES (1, 0, 16, 12318, 5, 'Improved Battle Shout');

-- fury (狂暴战), class 1 warrior talent page 1: 51 points, primary tree Fury
DELETE FROM `playerbot_talent_build` WHERE `class`=1 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 1, 12320, 5, 'Cruelty');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 2, 12324, 2, 'Improved Demoralizing Shout');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 3, 12322, 5, 'Unbridled Wrath');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 4, 12329, 3, 'Improved Cleave');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 5, 23584, 5, 'Dual Wield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 6, 12317, 5, 'Enrage');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 7, 12862, 5, 'Improved Slam');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 8, 23881, 1, 'Bloodthirst');
--   then 20 points of Arms (talent page 0)
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 9, 16462, 5, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 10, 12286, 3, 'Improved Rend');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 11, 12285, 2, 'Improved Charge');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 12, 12295, 5, 'Tactical Mastery');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 13, 12834, 3, 'Deep Wounds');
INSERT INTO `playerbot_talent_build` VALUES (1, 1, 14, 16493, 2, 'Impale');

-- protection (防战), class 1 warrior talent page 2: 51 points, primary tree Protection
DELETE FROM `playerbot_talent_build` WHERE `class`=1 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 1, 12298, 5, 'Shield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 2, 12297, 5, 'Anticipation');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 3, 12301, 2, 'Improved Bloodrage');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 4, 12299, 5, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 5, 12300, 4, 'Iron Will');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 6, 12308, 3, 'Improved Sunder Armor');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 7, 12302, 2, 'Improved Taunt');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 8, 12312, 2, 'Improved Shield Wall');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 9, 12311, 2, 'Improved Shield Bash');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 10, 23922, 1, 'Shield Slam');
--   then 20 points of Fury (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 11, 12321, 5, 'Booming Voice');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 12, 12320, 5, 'Cruelty');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 13, 12322, 5, 'Unbridled Wrath');
INSERT INTO `playerbot_talent_build` VALUES (1, 2, 14, 12318, 5, 'Improved Battle Shout');

-- holy (神圣骑士), class 2 paladin talent page 0: 51 points, primary tree Holy
DELETE FROM `playerbot_talent_build` WHERE `class`=2 AND `talent_page`=0;
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 1, 20262, 5, 'Divine Strength');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 2, 20257, 5, 'Divine Intellect');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 3, 20205, 5, 'Spiritual Focus');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 4, 20224, 5, 'Improved Seal of Righteousness');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 5, 20237, 3, 'Healing Light');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 6, 9453, 2, 'Unyielding Faith');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 7, 20210, 5, 'Illumination');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 8, 20473, 1, 'Holy Shock');
--   then 20 points of Protection (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 9, 20138, 5, 'Improved Devotion Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 10, 20127, 5, 'Redoubt');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 11, 20143, 5, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (2, 0, 12, 20096, 5, 'Anticipation');

-- protection (防战), class 2 paladin talent page 1: 51 points, primary tree Protection
DELETE FROM `playerbot_talent_build` WHERE `class`=2 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 1, 20138, 5, 'Improved Devotion Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 2, 20127, 5, 'Redoubt');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 3, 20189, 3, 'Precision');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 4, 20143, 5, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 5, 20217, 1, 'Blessing of Kings');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 6, 20468, 3, 'Improved Righteous Fury');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 7, 20148, 3, 'Shield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 8, 20096, 5, 'Anticipation');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 9, 20925, 1, 'Holy Shield');
--   then 20 points of Retribution (talent page 2)
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 10, 20042, 5, 'Improved Blessing of Might');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 11, 20101, 5, 'Benediction');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 12, 25956, 2, 'Improved Judgement');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 13, 20335, 3, 'Improved Seal of the Crusader');
INSERT INTO `playerbot_talent_build` VALUES (2, 1, 14, 20060, 5, 'Deflection');

-- retribution (惩戒骑), class 2 paladin talent page 2: 51 points, primary tree Retribution
DELETE FROM `playerbot_talent_build` WHERE `class`=2 AND `talent_page`=2;
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 1, 20042, 5, 'Improved Blessing of Might');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 2, 20101, 5, 'Benediction');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 3, 25956, 2, 'Improved Judgement');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 4, 20335, 3, 'Improved Seal of the Crusader');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 5, 20060, 3, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 6, 9452, 3, 'Vindication');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 7, 20117, 5, 'Conviction');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 8, 20375, 1, 'Seal of Command');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 9, 20091, 2, 'Improved Retribution Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 10, 20218, 1, 'Sanctity Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 11, 20066, 1, 'Repentance');
--   then 20 points of Protection (talent page 1)
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 12, 20138, 5, 'Improved Devotion Aura');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 13, 20127, 5, 'Redoubt');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 14, 20143, 5, 'Toughness');
INSERT INTO `playerbot_talent_build` VALUES (2, 2, 15, 20096, 5, 'Anticipation');

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

-- combat (战斗贼), class 4 rogue talent page 1: 51 points, the Combat tree only
-- The 31 points of the tree and the 20 that a level 60 character has left are spent in the same tree, so the
-- talent of the tree comes as the 31st step and the rows that are left come after it. Two steps name the dagger
-- skill: its first rank comes before the dual wield talent next to it, as a player takes them, and its four other
-- ranks come with the rest of the tree. The ranks that are not taken are the crowd control that a bot does not use
-- (Improved Gouge), the protection of a cast that a rogue never makes (Endurance), the second rank of an interrupt
-- whose longer lock only matters against players, and the weapon skill of the one weapon kind that the gear of a
-- combat bot is not given (fist weapons).
DELETE FROM `playerbot_talent_build` WHERE `class`=4 AND `talent_page`=1;
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 1, 13732, 2, 'Improved Sinister Strike');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 2, 13712, 5, 'Lightning Reflexes');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 3, 13733, 3, 'Improved Backstab');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 4, 13713, 5, 'Deflection');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 5, 13705, 5, 'Precision');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 6, 14251, 1, 'Riposte');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 7, 13743, 2, 'Improved Sprint');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 8, 13754, 1, 'Improved Kick');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 9, 13706, 1, 'Dagger Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 10, 13715, 5, 'Dual Wield Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 11, 13750, 1, 'Adrenaline Rush');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 12, 13706, 4, 'Dagger Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 13, 13709, 5, 'Mace Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 14, 13877, 1, 'Blade Flurry');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 15, 13960, 5, 'Sword Specialization');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 16, 30919, 2, 'Weapon Expertise');
INSERT INTO `playerbot_talent_build` VALUES (4, 1, 17, 18427, 3, 'Aggression');

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
