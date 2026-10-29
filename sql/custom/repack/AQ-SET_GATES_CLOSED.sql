-- Restart the war effort so that the gates of Ahn'Qiraj are closed again.
-- `variables` holds the saved state of several systems now (elemental invasions, the
-- Stranglethorn fishing tournament, the war effort), so only the war effort indexes are
-- removed here: 30024 - 30033 are the shared contributions and 30050 - 30055 are the
-- stage and event control (see enum SavedVariableIds in src/game/ObjectMgr.h).
DELETE FROM `variables` WHERE `index` BETWEEN 30024 AND 30033;
DELETE FROM `variables` WHERE `index` BETWEEN 30050 AND 30055;
