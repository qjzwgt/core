DROP PROCEDURE IF EXISTS add_migration;
DELIMITER ??
CREATE PROCEDURE `add_migration`()
BEGIN
DECLARE v INT DEFAULT 1;
SET v = (SELECT COUNT(*) FROM `migrations` WHERE `id`='20261029130100');
IF v = 0 THEN
INSERT INTO `migrations` VALUES ('20261029130100');
-- Add your query below.

-- The talent build that a bot learns, one row per step of a build. A bot spends its free talent points following
-- the steps of its talent in `seq` order, so the rows are written the way a player writes a build in the talent
-- calculator: the primary tree of the talent is filled before another tree is touched, and a bot that has not
-- reached the maximum level has the first steps of the same build. The rows of a talent that has no row here are
-- taken from the premade templates of `player_premade_spell_template`, or learned at random when there is none.
-- `.bot reload` reads the table again, so a build can be retuned without a restart and without compiling; see
-- src/game/PlayerBots/BotTalentBuild.cpp, which resolves every `rank_spell` and says in the log what does not fit.
-- Only the table is added here. The rows are the builds that this fork chose, and they live in
-- sql/custom/PLAYERBOT_TALENT_BUILDS.sql, which is applied after the migrations; a database without that file
-- works as it did before, and the fallbacks above.
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

-- End of migration.
END IF;
END??
DELIMITER ;
CALL add_migration();
DROP PROCEDURE IF EXISTS add_migration;
