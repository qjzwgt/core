/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MANGOS_PARTYBOTAI_H
#define MANGOS_PARTYBOTAI_H

#include "CombatBotBaseAI.h"
#include "Group.h"
#include "ObjectAccessor.h"

#include <set>

class Corpse;

class PartyBotAI : public CombatBotBaseAI
{
public:

    PartyBotAI(Player* pLeader, Player* pClone, CombatBotRoles role, uint8 race, uint8 class_, uint8 level, uint32 mapId, uint32 instanceId, float x, float y, float z, float o)
        : CombatBotBaseAI(), m_race(race), m_class(class_), m_level(level), m_mapId(mapId), m_instanceId(instanceId), m_x(x), m_y(y), m_z(z), m_o(o)
    {
        m_role = role;
        m_leaderGuid = pLeader->GetObjectGuid();
        m_cloneGuid = pClone ? pClone->GetObjectGuid() : ObjectGuid();
        m_updateTimer.Reset(2000);
    }
    PartyBotAI(Player* pLeader, uint32 mapId, uint32 instanceId, float x, float y, float z, float o)
        : CombatBotBaseAI(), m_mapId(mapId), m_instanceId(instanceId), m_x(x), m_y(y), m_z(z), m_o(o)
    {
        m_role = ROLE_INVALID;
        m_leaderGuid = pLeader->GetObjectGuid();
        m_updateTimer.Reset(2000);
    }

    bool OnSessionLoaded(PlayerBotEntry* entry, WorldSession* sess) final;
    void OnPlayerLogin() final;
    void UpdateAI(uint32 const diff) final;
    void MovementInform(uint32 movementType, uint32 data = 0) final;
    void OnPacketReceived(WorldPacket const* packet) final;

    void CloneFromPlayer(Player const* pPlayer);
    void AddToPlayerGroup();

    bool CanTryToCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry) const final;
    Player* GetPartyLeader() const;
    bool AttackStart(Unit* pVictim);
    Unit* SelectAttackTarget(Player* pLeader) const;
    Unit* SelectPartyAttackTarget() const;
    Player* SelectResurrectionTarget() const;
    Player* SelectShieldTarget() const;
    Unit* GetMarkedTarget(RaidTargetIcon mark) const;
    Unit* SelectOnlyControlledHostileTarget() const;
    bool IsValidHostileTargetForAttack(Unit const* pTarget) const;
    bool HasMeleeAttackerInRange() const;
    Unit* SelectNearestMeleeAttacker() const;
    bool IsMarkedForCrowdControl(Unit const* pTarget) const;
    uint8 CountNonRangedAttackersInRange(float range) const;
    uint8 CountUnmarkedEnemiesInRange(float range) const;
    bool CanUseCrowdControl(SpellEntry const* pSpellEntry, Unit* pTarget) const;
    bool GroupAllMounted() const;
    bool GroupHasFightingMemberMounted() const;
    bool GroupHasFightingMemberOnFoot() const;
    bool GroupInCombat() const;
    bool DrinkAndEat();
    bool ShouldAutoRevive() const;
    bool GroupHasResurrection() const;
    void UpdateSpiritResurrection();
    void WalkToCorpse(Corpse* pCorpse);
    float GetChaseDistance() const final;
    bool CanRunAwayFrom(Unit const* pEnemy) const;
    bool RunAwayFromTarget(Unit* pEnemy);
    bool CrowdControlMarkedTargets();
    bool EnterCombatDruidForm();
    bool FollowLeaderDownTheDrop(Player* pLeader);
    void CallPetBackAfterTheDrop();
    bool ShouldEnterStealth() const;
    bool EnterStealthIfNeeded(SpellEntry const* pStealthSpell);
    bool PrepareStealthOpener();
    bool IsManaUser(Player const* pPlayer) const;
    Player* SelectMissingGroupBuffTarget(SpellEntry const* pSingleSpellEntry,
        SpellEntry const* pGroupSpellEntry, bool manaUsersOnly, uint8& missingCount) const;
    bool PriestFortitudeBuffDelayPassed();
    bool PriestHasOutOfCombatBuffTarget();
    bool PriestNeedsResurrectionOrDiseaseDispel();
    bool IsTankRoleWarrior(Player* pPlayer) const;
    bool IsProtectionWarriorTank() const;
    bool IsOtherPartyMember(Unit const* pTarget) const;
    bool CanOpenWarriorCombatWithCharge() const;
    bool TryChargeWarriorTarget(Unit* pTarget);
    bool TryContinueWarriorCharge();
    bool UpdateProtectionWarriorTankTarget();
    bool TryTauntProtectionWarriorTarget(Unit* pTarget);
    void UpdateProtectionWarriorTankCombat();
    bool TrySwitchWarriorStance(SpellEntry const* pStance);
    void PrintWarriorStanceState(ChatHandler& handler) const;
    bool TryCastForcedWarriorSunder(Unit* pTarget);
    Unit* SelectProtectionWarriorSecondaryTarget(Unit const* pPrimaryTarget) const;
    bool HasThunderClapDebuff(Unit const* pTarget) const;
    uint32 GetTacticalMasteryRank() const;
    bool IsTargetedByHostile() const;
    Unit* SelectDispelTargetWithPet(SpellEntry const* pSpellEntry) const;
    SpellEntry const* SelectPrayerOfHealing() const;
    Player* SelectPrayerOfHealingTarget(uint8& memberCount) const;
    bool GroupHealthAbove(float healthPercent) const;
    Unit* SelectHostileDispelTarget(SpellEntry const* pSpellEntry) const;

    bool CheckForDispelTargets();
    void UpdateInCombatAI() final;
    void UpdateOutOfCombatAI() final;
    void UpdateInCombatAI_Paladin() final;
    void UpdateOutOfCombatAI_Paladin() final;
    void UpdateInCombatAI_Shaman() final;
    void UpdateOutOfCombatAI_Shaman() final;
    void UpdateInCombatAI_Hunter() final;
    void UpdateOutOfCombatAI_Hunter() final;
    void UpdateInCombatAI_Mage() final;
    void UpdateOutOfCombatAI_Mage() final;
    bool EnsureMageIceBarrier();
    void UpdateInCombatAI_Priest() final;
    void UpdateOutOfCombatAI_Priest() final;
    void UpdateInCombatAI_Warlock() final;
    void UpdateOutOfCombatAI_Warlock() final;
    void UpdateInCombatAI_Warrior() final;
    void UpdateOutOfCombatAI_Warrior() final;
    void UpdateInCombatAI_Rogue() final;
    void UpdateOutOfCombatAI_Rogue() final;
    void UpdateInCombatAI_Druid() final;
    void UpdateOutOfCombatAI_Druid() final;

    std::vector<RaidTargetIcon> m_marksToCC;
    std::vector<RaidTargetIcon> m_marksToFocus;
    ShortTimeTracker m_updateTimer;
    ObjectGuid m_leaderGuid;
    ObjectGuid m_cloneGuid;
    uint8 m_race = 0;
    uint8 m_class = 0;
    uint8 m_level = 0;
    uint32 m_mapId = 0;
    uint32 m_instanceId = 0;
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_o = 0.0f;
    bool m_resetSpellData = false;
    bool m_petDismissedForDrop = false;
    bool m_rogueVanishRetreat = false;
    ObjectGuid m_rogueStealthTargetGuid;
    ObjectGuid m_warriorChargeTargetGuid;
    ObjectGuid m_warriorPrimaryTargetGuid;
    ObjectGuid m_warriorSecondaryTargetGuid;
    bool m_warriorSecondarySundered = false;

    // Why the last stance that this bot asked for is not the form that it wears. The stance of a warrior is
    // decided in one place, so the last ask and the reason of it are enough to read what a bot is doing; they
    // are printed by `.partybot stance'. No cast was attempted when a result stays at -1.
    enum WarriorStanceBlock
    {
        PB_STANCE_WORN = 0,                   // the asked form is the worn one
        PB_STANCE_NO_SPELL,                   // no spell was passed: the bot does not know the stance
        PB_STANCE_NOT_READY,                  // the spell or its category is on cooldown
        PB_STANCE_GLOBAL_COOLDOWN,
        PB_STANCE_POWER,                      // not enough rage for the stance
        PB_STANCE_NO_FORM,                    // the spell has no shapeshift effect
        PB_STANCE_CAST_REFUSED,               // both casts were refused and the form did not come
    };

    uint32 m_warriorStanceAskedId = 0;
    uint32 m_warriorStanceBlock = PB_STANCE_WORN;
    uint32 m_warriorStanceNormalResult = uint32(-1);
    uint32 m_warriorStanceTriggeredResult = uint32(-1);
    float m_dropLandingZ = 0.0f;
    std::set<ObjectGuid> m_priestFortitudeMembers;
    bool m_priestFortitudeMembersInitialized = false;
    uint64 m_priestFortitudeBuffReadyAt = 0;
    uint64 m_shadowformDispelWaitUntil = 0;
    bool m_priestMindBlastOpener = false;
};

#endif
