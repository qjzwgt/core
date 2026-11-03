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
    void WalkToCorpseReclaimPoint(Corpse* pCorpse);
    bool IsCorpseReclaimPointSafe() const;
    float GetChaseDistance() const final;
    bool CanRunAwayFrom(Unit const* pEnemy) const;
    bool RunAwayFromTarget(Unit* pEnemy);
    bool CrowdControlMarkedTargets();
    bool EnterCombatDruidForm();
    bool FollowLeaderDownTheDrop(Player* pLeader);
    // Whether the way that the follow generator has for its leader goes a long way round the edge that the
    // leader stands below, rather than down it. The bot that is walking such a way is a bot that a player would
    // send over the edge instead, and it is the case that the rule above does not reach: that one is for the bot
    // that the pathfinder stopped at an edge, and this one is for the bot that is moving.
    bool IsFollowWayRoundTheLedge(Player const* pLeader, float distToLeader) const;
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
    // What asking for the opening of a Charge answers. An opening is not one cast but the few updates around it:
    // the stance that a Charge needs is asked in the first of them, the core applies that stance when the spell
    // event of the bot runs, and the charge of the same update is refused by the global cooldown and by the spell
    // of the stance that is still in progress. Only PB_CHARGE_REFUSED gives the opening up; the other answers keep
    // it, which is what a target that stands within the charge range from the beginning used to be missing.
    enum WarriorChargeAsk
    {
        PB_CHARGE_CAST,                     // the charge went off, the opening is over
        PB_CHARGE_APPROACH,                 // the target is outside the charge range: walk to it and ask again
        PB_CHARGE_LATER,                    // this update cannot cast it: the ask stands for the next one
        PB_CHARGE_REFUSED,                  // no charge opening: the target is taken the ordinary way
    };
    bool CanOpenWarriorCombatWithCharge() const;
    WarriorChargeAsk TryChargeWarriorTarget(Unit* pTarget);
    bool CanAskWarriorChargeLater();
    void ClearWarriorChargeOpening();
    bool TryContinueWarriorCharge();
    // Whether a target is ground that Intercept is for: both sides fighting, the target out of a swing of the
    // bot's reach and inside the range that the spell covers, the spell ready, and the rage above the cost of it
    // there. One question for a shield tank and for a damage warrior, because both of them jump at a target that
    // they cannot hit; the two reach the answer in their own way. The tank asks the spell in
    // UpdateProtectionWarriorTankCombat(), and the damage warrior carries the answer into its rotation as a want
    // that names the stance the spell wants, which is what makes the change of stance paid for together with the
    // spell and undone after it.
    bool IsWarriorInterceptWanted(Unit const* pTarget) const;
    bool UpdateProtectionWarriorTankTarget();
    bool TryTauntProtectionWarriorTarget(Unit* pTarget);
    bool HasWarriorMockingBlowReady() const;
    bool IsWarriorMockingBlowWanted(Unit const* pTarget) const;
    bool IsWarriorShieldBlockWanted(Unit const* pVictim) const;
    void UpdateProtectionWarriorTankCombat();
    bool TrySwitchWarriorStance(SpellEntry const* pStance);
    void PrintWarriorStanceState(ChatHandler& handler) const;
    bool TryCastForcedWarriorSunder(Unit* pTarget);
    Unit* SelectProtectionWarriorSecondaryTarget(Unit const* pPrimaryTarget) const;
    bool HasThunderClapDebuff(Unit const* pTarget) const;
    bool IsSkullConEliteTarget(Unit const* pTarget) const;
    bool HasDemoralizingShoutDebuff(Unit const* pTarget) const;
    bool HasWakingNeutralNear(Unit const* pCenter, float radius) const;
    bool IsNeutralAtPeace(Unit const* pUnit) const;
    bool IsHeldByDamageBreakingControl(Unit const* pUnit) const;
    uint32 GetTacticalMasteryRank() const;
    // The stance that a damage warrior stands in between the spells that ask for another one. Which stance that
    // is comes from what the bot knows, because neither the asked talent nor the stance is saved with a bot.
    SpellEntry const* GetWarriorHomeStanceSpell() const;
    ShapeshiftForm GetWarriorHomeStanceForm() const;
    SpellEntry const* GetWarriorStanceSpell(ShapeshiftForm askedForm) const;
    bool IsWarriorRageShort(SpellEntry const* pSpellEntry, SpellEntry const* pExtraCost) const;
    bool TryWarriorBloodrage();
    // What one update of the rotation of a damage warrior is to do. The rules of the rotation are collected in
    // the order the maintainer ranks them, and the update answers them one at a time, because the answer to a
    // want that the stance or the rage refuses is a stance or Bloodrage rather than the next want.
    struct WarriorRotationWant
    {
        SpellEntry const* pSpell = nullptr;
        Unit* pTarget = nullptr;
        // The stance this want is to be cast in, FORM_NONE when the want does not care which stance is worn.
        // Where no stance is named, the stance that the data of the spell asks for is used, so that a spell of
        // another stance is reached by changing stance instead of by a table of which ability wants which form.
        ShapeshiftForm askedForm = FORM_NONE;
        // False when the want is only to be done in the stance the bot already wears: a change of stance costs
        // rage and a global cooldown, and some wants are not worth both.
        bool canChangeStance = true;
        // True while the rotation is to keep its rage for this want: the rage that the want costs is waited for
        // rather than spent on the wants below it, which is what a bank of rage for one spell is.
        bool keepsRage = false;
        // True for the one spell that adds a stack of its debuff by being cast again while the mob still carries
        // it: the ordinary cast gate refuses that second cast, and the want is answered by the forced cast that
        // the shield tank uses for the same reason.
        bool useForcedCast = false;
    };
    // What the area about a warrior that a spell of several mobs would step on holds: the mobs that are simply
    // hostile, a neutral at peace that the spell would wake, and a mob held by a control that damage breaks.
    struct WarriorMeleePackRead
    {
        uint8 redCount = 0;
        // Of those hostile mobs, the ones that a swing of the bot could really land on: within the reach of its
        // weapon and in the arc that it faces. A hit copied by a Sweeping Strikes lands on those and on no other,
        // so a pack that is only about the bot is not a pack that the spell was asked for.
        uint8 reachCount = 0;
        bool neutralAtPeace = false;
        bool breakableControl = false;
    };
    WarriorMeleePackRead ReadWarriorMeleePack(Unit const* pCenter) const;
    void CollectWarriorDamageWants(Unit* pVictim, std::vector<WarriorRotationWant>& wants) const;
    void UpdateWarriorDamageRotation(Unit* pVictim);
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
    // How often the opening of a charge has been refused by a stance that is not worn yet or by the global
    // cooldown of that stance, and may be asked again. The window is bounded because a tank that waits at the
    // edge of the charge range while its party fights is worse than a tank that walks in; one update of a party
    // bot is PB_UPDATE_INTERVAL, a second. The count starts with the opening and is spent only by
    // PartyBotAI::CanAskWarriorChargeLater().
    uint8 m_warriorChargeAskRetries = 0;
    ObjectGuid m_warriorPrimaryTargetGuid;
    ObjectGuid m_warriorSecondaryTargetGuid;
    // How long the tank still keeps a mob that was beating another party member. The timer runs down only in an
    // update in which a swing of the bot could really land on that mob, so a run to it, a control of the bot or a
    // walk out of reach does not spend it. It is armed when the mob is taken, at PB_WARRIOR_SECONDARY_HOLD_MS.
    ShortTimeTracker m_warriorSecondaryHoldTimer;
    // Whether the opening of a charge may spend one Thunder Clap on the pack around its target. A charge is only
    // cast in Battle Stance, so the tank that charged already stands in the stance that the clap of the pack needs,
    // and the mobs of that pack are at their closest in the seconds after the charge. The ask is one spell long: it
    // is answered when the clap is cast, and it is over when the tank stands in another stance again.
    bool m_warriorChargeOpeningThunderClap = false;

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

    // The body whose bones the spirit of this bot is walking back to, and how far that walk has got: which of the
    // sides of the bones the spirit is sent to, and how often a way has been asked for. They are for one body, and
    // they are started over when the bot dies in another one.
    ObjectGuid m_reclaimCorpseGuid;
    uint32 m_reclaimStandTry = 0;
    uint32 m_spiritWalkLegs = 0;
};

#endif
