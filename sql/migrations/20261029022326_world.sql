DROP PROCEDURE IF EXISTS add_migration;
DELIMITER ??
CREATE PROCEDURE `add_migration`()
BEGIN
DECLARE v INT DEFAULT 1;
SET v = (SELECT COUNT(*) FROM `migrations` WHERE `id`='20261029022326');
IF v = 0 THEN
INSERT INTO `migrations` VALUES ('20261029022326');
-- Add your query below.

-- Rexxar (10182), Champion of the Horde, standing in Shadowprey Village (Desolace).
-- He keeps faction 83 (Horde Generic), so he stays red named for Alliance players, but he must never
-- attack them on sight, whether they are PvP flagged or not, and he must not join the fights of the
-- other Horde NPCs and players around him.
-- Only Attack PvP Enabling (static_flags1 0x04000000) still aggroes on PvP flagged players, and
-- Ignore Combat (static_flags1 0x02000000) sets the passive react state, which also makes
-- CreatureAI::AttackStart return early, so he would not even defend himself.
-- No Aggro (2) sets the defensive react state: Creature::CanInitiateAttack() fails, so there is no
-- proximity aggro at all, but he still attacks back whoever damages him.
-- No Assist (65536) makes Creature::CanRespondToCallForHelpAgainst() fail, so he never answers the
-- call for help / assistance of another creature.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` | 2 /*No Aggro*/ | 65536 /*No Assist*/ WHERE `entry`=10182;

-- Misha (10204) uses NullCreatureAI, so she does not aggro anyway, but she must not join the fights
-- of other creatures either. She helps Rexxar through the creature_linking below.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` | 65536 /*No Assist*/ WHERE `entry`=10204;

-- Link Misha (300850) to Rexxar (29113) so that they are the only ones who help each other.
-- FLAG_AGGRO_ON_AGGRO (1) makes Misha attack whoever attacks Rexxar, FLAG_TO_AGGRO_ON_AGGRO (2)
-- makes Rexxar attack whoever attacks Misha.
DELETE FROM `creature_linking` WHERE `guid`=300850;
INSERT INTO `creature_linking` (`guid`,`master_guid`,`flag`) VALUES (300850, 29113, 3);

-- End of migration.
END IF;
END??
DELIMITER ;
CALL add_migration();
DROP PROCEDURE IF EXISTS add_migration;
