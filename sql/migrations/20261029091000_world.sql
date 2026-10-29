DROP PROCEDURE IF EXISTS add_migration;
DELIMITER ??
CREATE PROCEDURE `add_migration`()
BEGIN
DECLARE v INT DEFAULT 1;
SET v = (SELECT COUNT(*) FROM `migrations` WHERE `id`='20261029091000');
IF v = 0 THEN
INSERT INTO `migrations` VALUES ('20261029091000');
-- Add your query below.

-- The value that each attribute of an item has for the talent of a bot, used to choose the gear that the bots
-- are dressed with. The compiled values of src/game/PlayerBots/BotGearScoring.cpp are used for the pairs that
-- have no row here, so the table only needs the rows that change a value, and `.bot reload` reads it again.
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

-- End of migration.
END IF;
END??
DELIMITER ;
CALL add_migration();
DROP PROCEDURE IF EXISTS add_migration;
