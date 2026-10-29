-- Rexxar (10182), Champion of the Horde, standing in Shadowprey Village (Desolace), and his beast
-- Misha (10204), standing next to him. Both keep faction 83 (Horde Generic), so they stay red named for
-- Alliance players, but neither of them may attack an Alliance player on sight; both still defend
-- themselves and each other.
--
-- Only a creature in the react state REACT_AGGRESSIVE may start a fight by itself. No Aggro (2) makes
-- Creature::InitializeReactState() choose REACT_DEFENSIVE, and BasicAI::MoveInLineOfSight() then fails the
-- Creature::CanInitiateAttack() test, so there is no aggro on sight at all. The two still fight back:
-- CreatureAI::AttackStart() only refuses REACT_PASSIVE, and the linking written below calls it directly
-- (CreatureLinkingHolder::ProcessSlave) or through Creature::EnterCombatWithTarget.
-- No Assist (65536) makes Creature::CanRespondToCallForHelpAgainst() fail, so neither of them answers the
-- call for help of another creature.
--
-- Only Attack PvP Enabling (static_flags1 0x04000000) would aggro on PvP flagged players, and Ignore Combat
-- (static_flags1 0x02000000) sets REACT_PASSIVE, which would stop the two from defending themselves at all.
-- Neither of them is wanted here.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` | 2 /*No Aggro*/ | 65536 /*No Assist*/ WHERE `entry`=10182;

-- Misha has to be defensive too. The first version of this file assumed that her own AI never looks for a
-- target, but in game the beast of faction 83 does attack an Alliance player on sight, whatever `ai_name` the
-- released data gives her. Once she is in combat, the link to Rexxar reports that fight to him, so making only
-- Rexxar defensive changed nothing at all.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` | 2 /*No Aggro*/ | 65536 /*No Assist*/ WHERE `entry`=10204;

-- Link the beast to Rexxar so that the two are the only ones who join each other's fights:
-- FLAG_AGGRO_ON_AGGRO (1) brings Misha into a fight of Rexxar, FLAG_TO_AGGRO_ON_AGGRO (2) brings Rexxar into
-- a fight of Misha. With No Aggro on both, nothing of this starts on its own any more.
-- `creature_linking_template` describes such a pair by entry and map, which is what the released data does
-- for the linked NPCs of a zone; `creature_linking` would name the two spawns by their db-guid, which is a
-- detail of one database rather than of the pair. search_range (50) is the distance between the two respawn
-- positions within which the link is used; 0 would mean any distance.
DELETE FROM `creature_linking_template` WHERE `entry`=10204;
INSERT INTO `creature_linking_template` (`entry`, `map`, `master_entry`, `flag`, `search_range`) VALUES (10204, 1, 10182, 3, 50);

-- The first version of this file wrote the pair into `creature_linking` with the db-guid of Misha's spawn
-- (300850) and of Rexxar's spawn (29113). Those rows are removed wherever they were written, so that only the
-- link above is left; the first query covers the database the file was written for, the second any other one
-- where a spawn of Misha is the slave of something.
DELETE FROM `creature_linking` WHERE `guid`=300850;
DELETE FROM `creature_linking` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id`=10204);
