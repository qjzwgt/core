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

#include "PartyBotAI.h"
#include "Player.h"
#include "CreatureAI.h"
#include "Creature.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "PlayerBotMgr.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "WorldPacket.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Chat.h"
#include "Utilities/Random.h"
#include "Corpse.h"
#include "PathFinder.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"

#include <algorithm>
#include <ctime>
#include <random>
#include <sstream>

enum PartyBotSpells
{
    PB_SPELL_AUTO_SHOT = 75,
    PB_SPELL_SHOOT_WAND = 5019,
    PB_SPELL_HONORLESS_TARGET = 2479,
};

#define PB_UPDATE_INTERVAL 1000
#define PB_MIN_FOLLOW_DIST 3.0f
#define PB_MAX_FOLLOW_DIST 6.0f
#define PB_MIN_FOLLOW_ANGLE 0.0f
#define PB_MAX_FOLLOW_ANGLE 6.0f
#define PB_RAGE_PER_POINT 10

// How long a shield tank keeps a mob that was beating another party member, counted in the seconds in which a
// swing of the tank could really land on that mob. The visit is not asked to end with a spell that lands, because
// the threat that a cast makes on one mob is not known here; it is asked to be long enough to be an answer, and
// the maintainer extends this when the mobs are seen to go back to the party member too soon.
#define PB_WARRIOR_SECONDARY_HOLD_MS 6000

// How far a shield tank looks, before it lets off a shout, for a neutral that the shout would bring into the
// fight. The shout of the warrior is centred on the tank, and this look is kept wider than that area on purpose,
// as the maintainer asks: a neutral that stands at peace one step outside the shout is a neutral that will be
// inside it a moment later, and the shout is not worth the extra fight that it brings.
#define PB_WARRIOR_SHOUT_NEUTRAL_WATCH_DIST 10.0f

// How many updates of the bot the opening of a charge may wait for the stance that it asked and for the global
// cooldown of that stance. The core applies a stance when the spell event of the caster runs and puts a global
// cooldown back for it, so the charge of the update that asks the stance cannot go off; the next updates can. The
// window is short on purpose, because the opening is worth nothing when the party has to wait for its tank: one
// update of a party bot is PB_UPDATE_INTERVAL, a second. The maintainer lengthens this when the stance of a bot
// comes later than that.
#define PB_WARRIOR_CHARGE_ASK_UPDATES 3

// The Thunder Clap of a shield tank is asked for in two cases. The opening of a charge may spend one clap on the
// pack of red-name mobs about its target, which is the number below, because that clap puts the first threat on
// every mob that is coming. Before that clap the tank looks for a neutral at peace within the area that the clap
// covers, the yards below, because the debuff of the clap would wake such a neutral into a second fight that the
// tank did not ask for. An elite keeps the debuff of the clap always, because the debuff takes attack power off
// the mob that hits the tank; the number of levels below is what the client shows as the skull of a boss. All of
// these are values for the maintainer: the core takes the area that the spell of the clap really covers from its
// own spell data, which is not read here.
#define PB_WARRIOR_THUNDER_CLAP_PACK_COUNT 3
#define PB_WARRIOR_THUNDER_CLAP_LEVEL_GAP 3
#define PB_WARRIOR_THUNDER_CLAP_NEUTRAL_WATCH_DIST 8.0f

// How far about a damage warrior the area of a spell of several mobs reaches: Whirlwind and the bank of a
// Sweeping Strikes are asked for at the pack that the warrior can reach, and not at the pack that its target
// could reach from where that mob stands. It is a value for the maintainer; the core takes the area that the
// spell of the running patch really covers from its own spell data, which is not read here.
#define PB_WARRIOR_MULTI_MOB_DIST 8.0f

// The rage that the rotation of a damage warrior keeps before it whirlwinds, in points of rage. Whirlwind is the
// most damage that the rage of a melee warrior buys at once, and rage below the point where it cannot be paid
// for is better spent on the abilities of the stance that the bot is standing in.
#define PB_WARRIOR_WHIRLWIND_RAGE 50

// The window of rage, in points of rage, in which a damage warrior that stands in the Battle Stance will change
// to the Berserker Stance to interrupt with Pummel: above the cost of the change of stance, and below the point
// where the rage is worth keeping for the abilities of the Battle Stance. An interrupt is never worth a change of
// stance outside that window, which is the same reason why the interrupt of a damage warrior is ranked below its
// abilities rather than above them.
#define PB_WARRIOR_PUMMEL_STANCE_RAGE_MIN 10
#define PB_WARRIOR_PUMMEL_STANCE_RAGE_MAX 25

// The rage, in points of rage, that a warrior asks to have before it jumps to a target with Intercept. It is
// above the cost of the spell on purpose: the jump ends with the bot beside the mob it stunned, and a warrior that
// arrives with nothing left cannot spend the ability that the stun opened. The rage of the change of stance that
// the spell wants is paid for together with the spell, as everywhere else in these rules, and the floor is the
// same for a shield tank and for a damage warrior.
#define PB_WARRIOR_INTERCEPT_RAGE_MIN 10

// A bank of rage for a Sweeping Strikes is kept for the mobs that the copied hit would really land on, and not
// for the mobs that merely stand about the warrior: the spell copies a swing onto another target, so one target
// in front of the bot is a swing copied onto nothing. How far a swing of this bot reaches is the core's own
// answer, the one that its auto attack is gated by; the arc below, in radians from the direction that the bot
// faces, is what this file calls the front of a character, and a mob at the back of the warrior is not a second
// target of the copy. Both numbers are for the maintainer to tune.
#define PB_WARRIOR_SWEEPING_STRIKE_TARGETS 2
#define PB_WARRIOR_FRONT_HALF_ANGLE (M_PI_F / 3.0f)

// What is taken for a ledge that a player jumps down instead of walking around: the leader stands that much lower
// than the bot and that close by, and the pathfinder of the follow generator has no way down to offer. The ground
// to land on is looked for along the line from the bot to its leader, in steps of PB_DROP_STEP_LAND_DIST, and it
// may be at most PB_DROP_LAND_BELOW_LEADER under the feet of the leader.
#define PB_DROP_MIN_Z_DIFF 3.0f
#define PB_DROP_MAX_Z_DIFF 25.0f
#define PB_DROP_MAX_DIST 20.0f
#define PB_DROP_STEP_LAND_DIST 2.0f
#define PB_DROP_LAND_BELOW_LEADER 6.0f
// A bot that the pathfinder has stopped at an edge is the case above. A bot that is walking is not stopped, and
// the way that its generator has for a leader below an edge may go a long way around that edge instead of down
// it: the detour is a way longer than the line to the leader by the factor and the extra distance below, and the
// ledge that the bot stands on is lower than PB_DROP_MIN_Z_DIFF. That is ground that a player jumps down and that
// a party should not wait for, so the bot goes over the edge with the smaller floor of the two. A slope that the
// generator can walk down answers with a way that is the line itself, and is walked as it always was.
#define PB_DROP_DETOUR_MIN_Z_DIFF 1.5f
#define PB_DROP_DETOUR_FACTOR 1.5f
#define PB_DROP_DETOUR_EXTRA_DIST 8.0f
// How far above the ground it aimed for a bot may stand before it calls its pet back to it.
#define PB_DROP_LAND_TOLERANCE 2.0f

// The distance that a ranged party bot keeps while it chases and casts. The room that a party has for that depends
// on the ground that it fights on: a dungeon is small, the open world gives more room, and a battleground is the
// widest of the three. Running away from a target uses PB_RUN_AWAY_DISTANCE instead.
#define PB_CHASE_DISTANCE_INSTANCE 10.0f
#define PB_CHASE_DISTANCE_OUTDOORS 20.0f
#define PB_CHASE_DISTANCE_BATTLEGROUND 30.0f

// A ranged damage bot keeps the range of its longest common attack spell. It approaches only while the target is
// beyond that range; it does not walk closer merely to make a short-range spell such as Fire Blast usable.
#define PB_RANGED_DPS_DISTANCE 30.0f

// The distance that a party bot steps back when it runs away, and the distance that it then keeps with the chase
// generator, is the same on every kind of ground.
#define PB_RUN_AWAY_DISTANCE 8.0f

// Where the spirit of a bot takes its body back. Standing on the bones themselves stands the bot up in the middle
// of whatever killed it, and a player that runs back for its body takes it from where it stopped, which the core
// allows from anywhere inside CORPSE_RECLAIM_RADIUS of it. So the spirit is sent to PB_RECLAIM_STAND_DISTANCE from
// the bones, at the side it came to first - that is the way out of the fight that killed the bot - and at the
// sides around the bones after that: PB_RECLAIM_STAND_SIDES of them are asked, and past them the body is taken
// where the bot stands. A place is given up when a creature that would reach the bot there stands on it, and
// PB_RECLAIM_AGGRO_SEARCH is how far round the place such a creature is looked for - the detection range that
// `Creature::GetDetectionRange` starts a creature at, with the MAX_LEVEL_DIFF_FOR_AGGRO_RANGE that a creature much
// above the bot adds to it, which is the bound `Creature::SelectNearestHostileUnitInAggroRange` gives its own
// search. A spirit that stands at the place it was sent to, and can take its body from there, waits there as a
// player in front of its own bones does, and that waiting is not counted as walking. The walking is asked for
// PB_SPIRIT_WALK_LEGS times in all, which is the guard for ground that the spirit can not walk over at all: the
// spirit healer of the graveyard raises the bot then.
#define PB_RECLAIM_STAND_DISTANCE 25.0f
#define PB_RECLAIM_STAND_SIDES 8
#define PB_RECLAIM_STAND_ARRIVED 3.0f
#define PB_RECLAIM_AGGRO_SEARCH 45.0f
#define PB_SPIRIT_WALK_LEGS 40

// The form that a stance spell puts on its caster: the shapeshift effect of the spell, so that the rules of the
// bots follow the spell of the running patch rather than a list of the three stances of the class. FORM_NONE for
// a spell that asks for no form, which is not a stance spell at all.
static ShapeshiftForm GetStanceSpellForm(SpellEntry const* pStanceSpell)
{
    if (!pStanceSpell)
        return FORM_NONE;

    for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (pStanceSpell->EffectApplyAuraName[i] == SPELL_AURA_MOD_SHAPESHIFT)
            return ShapeshiftForm(pStanceSpell->EffectMiscValue[i]);
    }

    return FORM_NONE;
}

// The stance that the data of a warrior spell asks for. The three stances are bits of the field that this fork
// keeps in the database next to the rest of the spell, so the rotation of a damage warrior can reach a spell of
// another stance by changing stance and take the stance of its talent back after it, without naming here which
// ability wants which form. FORM_NONE when the data of the spell names no stance: such a spell goes off in the
// stance that the bot is standing in, which is the stance of its talent.
static ShapeshiftForm GetSpellStanceRequirement(SpellEntry const* pSpellEntry)
{
    if (!pSpellEntry)
        return FORM_NONE;

    for (uint32 bit = 0; bit < 32; ++bit)
    {
        if (!(pSpellEntry->Stances & (1u << bit)))
            continue;

        // The bit convention of GetErrorAtShapeshiftedCast: a form is the bit below itself. Only the stances of
        // the class are of interest here, and a spell that names another shapeshift is not a warrior ability.
        ShapeshiftForm const form = ShapeshiftForm(bit + 1);
        if (form == FORM_BATTLESTANCE || form == FORM_DEFENSIVESTANCE || form == FORM_BERSERKERSTANCE)
            return form;
    }

    return FORM_NONE;
}

bool PartyBotAI::OnSessionLoaded(PlayerBotEntry* entry, WorldSession* sess)
{
    if (!m_race && !m_class)
    {
        sess->LoginPlayer(entry->playerGUID);
        return true;
    }

    return SpawnNewPlayer(sess, m_class, m_race, m_mapId, m_instanceId, m_x, m_y, m_z, m_o, sObjectAccessor.FindPlayer(m_cloneGuid));
}

void PartyBotAI::CloneFromPlayer(Player const* pPlayer)
{
    if (!pPlayer)
        return;

    if (pPlayer->GetLevel() != me->GetLevel())
    {
        me->GiveLevel(pPlayer->GetLevel());
        me->InitTalentForLevel();
        me->SetUInt32Value(PLAYER_XP, 0);
    }

    // Learn all of the target's spells.
    for (const auto& spell : pPlayer->GetSpellMap())
    {
        if (spell.second.disabled)
            continue;

        if (spell.second.state == PLAYERSPELL_REMOVED)
            continue;

        SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(spell.first);
        if (!pSpellEntry)
            continue;

        uint32 const firstRankId = sSpellMgr.GetFirstSpellInChain(spell.first);
        if (!me->HasSpell(spell.first))
            me->LearnSpell(spell.first, false, (firstRankId == spell.first && GetTalentSpellPos(firstRankId)));
    }

    me->GetHonorMgr().SetHighestRank(pPlayer->GetHonorMgr().GetHighestRank());
    me->GetHonorMgr().SetRank(pPlayer->GetHonorMgr().GetRank());

    // Unequip current gear
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        me->AutoUnequipItemFromSlot(i);

    // Copy gear from target.
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = pPlayer->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            me->SatisfyItemRequirements(pItem->GetProto());
            me->StoreNewItemInBestSlots(pItem->GetEntry(), 1, pItem->GetEnchantmentId(EnchantmentSlot(0)));
        }
    }
}

Player* PartyBotAI::GetPartyLeader() const
{
    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return nullptr;

    if (Player* originalLeader = ObjectAccessor::FindPlayerNotInWorld(m_leaderGuid))
    {
        if (me->InBattleGround() == originalLeader->InBattleGround())
        {
            // In case the original spawner is not in the same group as the bots anymore.
            if (pGroup != originalLeader->GetGroup())
                return nullptr;

            // In case the current leader is the bot itself and it's not inside a Battleground.
            ObjectGuid currentLeaderGuid = pGroup->GetLeaderGuid();
            if (currentLeaderGuid == me->GetObjectGuid() && !me->InBattleGround())
                return nullptr;
        }

        return originalLeader;
    }
    return nullptr;
}

// A bot of a ranged talent steps back from an enemy that came close only when the ground it wins is ground that it
// keeps: the enemy must be held from walking after the bot, and the bot must be free to walk. Otherwise the bot
// stands its ground and takes the attacks, going on with what its talent does - a healer goes on healing, a caster
// goes on casting, a hunter swings the melee weapon that it wears - because running from an enemy that follows at
// full speed only moves the fight somewhere else. What the enemy is itself busy with is not asked for: an enemy
// that is held and cannot reach the bot turns to the one that it can reach, and the bot is to walk it around all
// the same.
bool PartyBotAI::CanRunAwayFrom(Unit const* pEnemy) const
{
    if (!pEnemy || !pEnemy->IsAlive())
        return false;

    // A root, a stun or a confusion keeps the enemy where it stands. A snare alone is not enough: the enemy can
    // still close the distance, so the bot stays and fights. A fear is not counted here either, because a feared
    // enemy is already running away from the bot.
    bool const enemyHeld =
        pEnemy->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED) ||
        pEnemy->HasAuraType(SPELL_AURA_MOD_ROOT) ||
        pEnemy->HasAuraType(SPELL_AURA_MOD_STUN) ||
        pEnemy->HasAuraType(SPELL_AURA_MOD_CONFUSE);

    if (!enemyHeld)
        return false;

    // Once the bot has enough room, there is nothing left to win by running again.
    if (me->GetCombatDistance(pEnemy) >= PB_RUN_AWAY_DISTANCE)
        return false;

    // And the bot must have its own movement: rooted or stunned it has nothing to gain, and confused or feared it
    // goes where the effect takes it and not where it would run to.
    if (me->HasUnitState(UNIT_STATE_NO_FREE_MOVE) ||
        me->HasAuraType(SPELL_AURA_MOD_ROOT) ||
        me->HasAuraType(SPELL_AURA_MOD_STUN) ||
        me->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
        me->HasAuraType(SPELL_AURA_MOD_FEAR))
        return false;

    return true;
}

// A ranged damage bot keeps spell range everywhere. The other roles use the distance of the ground that the party
// is fighting on. A battleground is asked for first, because a battleground is an instance too and the party that
// stands in its yard is not in a dungeon; the party that has a battleground queued but has not landed keeps the
// distance of the ground that it is standing on.
float PartyBotAI::GetChaseDistance() const
{
    if (m_role == ROLE_RANGE_DPS)
        return PB_RANGED_DPS_DISTANCE;

    if (me->InBattleGround())
        return PB_CHASE_DISTANCE_BATTLEGROUND;

    Map* pMap = me->GetMap();
    if (pMap && pMap->IsDungeon())
        return PB_CHASE_DISTANCE_INSTANCE;

    return PB_CHASE_DISTANCE_OUTDOORS;
}

bool PartyBotAI::RunAwayFromTarget(Unit* pEnemy)
{
    return me->GetMotionMaster()->MoveDistance(pEnemy, PB_RUN_AWAY_DISTANCE);
}

bool PartyBotAI::GroupAllMounted() const
{
    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return me->IsMounted();

    if (Player* pLeader = GetPartyLeader())
    {
        if (!pLeader->IsMounted())
            return false;
    }

    bool foundMember = false;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (!pMember)
            continue;

        foundMember = true;
        if (!pMember->IsMounted())
            return false;
    }

    return foundMember;
}

bool PartyBotAI::GroupHasFightingMemberMounted() const
{
    if (me->IsMounted() &&
        (me->IsInCombat() || me->GetVictim() || me->GetAttackerForHelper()))
        return true;

    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return false;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (pMember && pMember != me && pMember->IsMounted() &&
            (pMember->IsInCombat() || pMember->GetVictim() || pMember->GetAttackerForHelper()))
            return true;
    }

    return false;
}

bool PartyBotAI::GroupHasFightingMemberOnFoot() const
{
    if (!me->IsMounted() &&
        (me->IsInCombat() || me->GetVictim() || me->GetAttackerForHelper()))
        return true;

    if (Player* pLeader = GetPartyLeader())
    {
        // A player that is only being attacked has no victim of their own, and the combat flag can arrive after the
        // attack relationship exists. Either relation means that the party is already fighting.
        if (!pLeader->IsMounted() &&
            (pLeader->IsInCombat() || pLeader->GetVictim() || pLeader->GetAttackerForHelper()))
            return true;
    }

    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return false;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (pMember && pMember != me && !pMember->IsMounted() &&
            (pMember->IsInCombat() || pMember->GetVictim() || pMember->GetAttackerForHelper()))
            return true;
    }

    return false;
}

bool PartyBotAI::GroupInCombat() const
{
    // One mounted member can be attacked without pulling the others off their mounts; somebody has to put both feet
    // on the ground and fight.
    return GroupHasFightingMemberOnFoot();
}

bool PartyBotAI::DrinkAndEat()
{
    if (m_isBuffing)
        return false;

    if (me->IsMounted())
        return false;

    if (me->GetVictim())
        return false;

    // A group that has entered combat calls only a bot that is still critically low back to eating or drinking. Once
    // both of its pools are above thirty per cent it stands up and joins the fight.
    bool const criticalRecovery =
        me->GetHealthPercent() <= 30.0f ||
        (me->GetPowerType() == POWER_MANA && me->GetPowerPercent(POWER_MANA) <= 30.0f);
    if (!criticalRecovery && GroupInCombat())
        return false;

    // A player that is nearly whole does not sit down: the food and the water are for the health and the mana that
    // have gone far enough down to be worth the time that eating takes.
    bool const enoughHealth = me->GetHealthPercent() >= 80.0f;
    bool const enoughMana = me->GetPowerType() != POWER_MANA || me->GetPowerPercent(POWER_MANA) >= 70.0f;
    if (enoughHealth && enoughMana)
        return false;

    bool const needToEat = me->GetHealthPercent() < 100.0f;
    bool const needToDrink = (me->GetPowerType() == POWER_MANA) && (me->GetPowerPercent(POWER_MANA) < 100.0f);

    if (!needToEat && !needToDrink)
        return false;

    // A bot that has lost a lot of its health puts a bandage on first, which is what a player does, and it takes the
    // food and the water only when its first aid has no bandage ready.
    if (me->GetHealthPercent() < 50.0f)
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
        {
            me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }

        if (BandageSelf())
            return true;
    }

    // The rank of the food and of the water is that of the level of the bot, as it is for a player, whose vendors
    // sell the victuals of the region it is adventuring in.
    uint32 const foodSpell = GetFoodSpellId();
    uint32 const waterSpell = GetDrinkSpellId();

    bool const isEating = me->HasAura(foodSpell);
    bool const isDrinking = me->HasAura(waterSpell);

    if (!isEating && needToEat)
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
        {
            me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }
        if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(foodSpell))
        {
            me->CastSpell(me, pSpellEntry, true);
            me->RemoveSpellCooldown(pSpellEntry);
        }
        return true;
    }

    if (!isDrinking && needToDrink)
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
        {
            me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }
        if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(waterSpell))
        {
            me->CastSpell(me, pSpellEntry, true);
            me->RemoveSpellCooldown(pSpellEntry);
        }
        return true;
    }

    return needToEat || needToDrink;
}

bool PartyBotAI::ShouldAutoRevive() const
{
    if (me->GetDeathState() == DEAD)
        return true;

    bool alivePlayerNearby = false;
    Group* pGroup = me->GetGroup();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            if (pMember == me)
                continue;

            if (pMember->IsInCombat())
                return false;

            if (pMember->IsAlive())
            {
                if (IsHealerClass(pMember->GetClass()))
                    return false;

                if (me->IsWithinDistInMap(pMember, 15.0f))
                    alivePlayerNearby = true;
            }
        }
    }

    return alivePlayerNearby;
}

// True when a member of the group is to be counted on to raise the bot. The class of the member is not the
// question: a priest has Resurrection and a shaman Ancestral Spirit, a paladin has Redemption only where its
// talents give the talent, and a warrior has nothing at all. The resurrection spells are the ones with the
// resurrect effect, which is also how PopulateSpellData() finds the spell with which the bot itself would raise
// someone. Rebirth is not one of them for this: a druid keeps it for an emergency behind a long cooldown and a
// reagent, so a group whose only resurrection is that one must not keep the bot standing by its body.
bool PartyBotAI::GroupHasResurrection() const
{
    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return false;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        // A member that is dead can not cast, and the bot can not raise itself.
        if (!pMember || pMember == me || !pMember->IsAlive())
            continue;

        for (auto const& spell : pMember->GetSpellMap())
        {
            if (!spell.second.active || spell.second.disabled)
                continue;

            SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(spell.first);
            if (!pSpellEntry)
                continue;

            for (uint32 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                if (pSpellEntry->Effect[i] != SPELL_EFFECT_RESURRECT &&
                    pSpellEntry->Effect[i] != SPELL_EFFECT_RESURRECT_NEW)
                    continue;

                // Rebirth, of which the trainer lists five ranks between 20485 and 20750, is the one resurrection
                // that the core marks with SPELL_ATTR_EX3_NO_RES_TIMER (see the note in SpellDefines.h), so the
                // attribute finds every rank of every patch where a spell id would find one.
                if (pSpellEntry->HasAttribute(SPELL_ATTR_EX3_NO_RES_TIMER))
                    continue;

                return true;
            }
        }
    }

    return false;
}

// A creature that would take the bot for an enemy if the bot stood up where its spirit has come to a stop. The
// question is the one that `BasicAI::MoveInLineOfSight` answers when a creature of the open world sees a player
// walk into its range, with the aggro radius of `Creature::GetAttackDistance`, the same hostility, the same line
// of sight and the same ground the creature can walk on. The bot is asked as the living character that it becomes
// when it takes its body back, because at the moment of the question it is a spirit, and a spirit is nothing that
// a creature attacks.
class ReclaimPointAggroCheck
{
public:
    explicit ReclaimPointAggroCheck(Player* pBot) : m_pBot(pBot) {}

    bool operator()(Unit* pUnit)
    {
        Creature const* pCreature = pUnit->ToCreature();
        if (!pCreature)
            return false;

        // A creature that can not start a fight of its own pulls nothing when the bot stands up: a passive or a
        // tame one, a stunned one, one that the data does not let attack. A creature that is fighting is not pulled
        // either, but the bot would stand in that fight all the same, so it answers for a place that is not safe.
        if (!pCreature->CanInitiateAttack() || pCreature->IsInCombat())
            return false;

        // `checkAlive` is off because the bot is a spirit here and the answer is wanted for the body that it is
        // about to take.
        if (!m_pBot->IsTargetableBy(pCreature, false, false))
            return false;

        if (!pCreature->IsHostileTo(m_pBot))
            return false;

        // The creature on the ledge above does not reach the bot that stands below it, which is how the core reads
        // the same question.
        if (pCreature->GetDistanceZ(m_pBot) > CREATURE_Z_ATTACK_RANGE)
            return false;

        float const aggroRange = pCreature->GetAttackDistance(m_pBot);
        if (aggroRange <= 0.0f)
            return false;

        return pCreature->IsWithinDistInMap(m_pBot, aggroRange, true, SizeFactor::None) &&
            pCreature->IsWithinLOSInMap(m_pBot) &&
            m_pBot->IsInAccessablePlaceFor(pCreature);
    }

private:
    Player* m_pBot;
};

// True when nothing that would take the bot for an enemy reaches the place where its spirit stands, which is a
// place it can take its body back from without the pack that killed it coming over again.
bool PartyBotAI::IsCorpseReclaimPointSafe() const
{
    ReclaimPointAggroCheck check(me);

    Unit* pReaching = nullptr;
    MaNGOS::UnitSearcher<ReclaimPointAggroCheck> searcher(pReaching, check);
    Cell::VisitAllObjects(me, searcher, PB_RECLAIM_AGGRO_SEARCH);

    return !pReaching;
}

// Walk the spirit of the bot towards the bones it left behind, to a place beside them rather than onto them, and
// let the spirit healer of its graveyard raise the bot when no ground to the bones can be walked at all.
void PartyBotAI::WalkToCorpseReclaimPoint(Corpse* pCorpse)
{
    // A point movement that has been launched walks on its own, so it is launched again only once it is over.
    if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE)
        return;

    // The bones in another copy of the map are out of reach for the spirit, and the spirit healer raises the bot
    // there, as it does for a player whose body lies in an instance that the spirit was released in.
    if (pCorpse->GetMapId() != me->GetMapId() || pCorpse->GetInstanceId() != me->GetInstanceId())
    {
        if (WorldSession* pSession = me->GetSession())
            pSession->SendSpiritResurrect();
        return;
    }

    // The walking of a spirit is not endless either: a bot whose bones are behind ground it can not walk over is
    // raised by the spirit healer rather than left standing at the graveyard for the rest of the session.
    if (m_spiritWalkLegs >= PB_SPIRIT_WALK_LEGS)
    {
        if (WorldSession* pSession = me->GetSession())
            pSession->SendSpiritResurrect();
        return;
    }

    // The side of the bones that the spirit came to is the first one asked for: it is the way out of the fight
    // that killed the bot, and it is the ground that the bot has seen walked on its way to the graveyard. The
    // sides after that turn around the bones, so a bot that is asked for a place again has the whole of them.
    float const angle = pCorpse->GetAngle(me) +
        (float(m_reclaimStandTry % PB_RECLAIM_STAND_SIDES) * 2.0f * M_PI_F) / float(PB_RECLAIM_STAND_SIDES);

    float standX = 0.0f;
    float standY = 0.0f;
    pCorpse->GetNearPoint2D(standX, standY, PB_RECLAIM_STAND_DISTANCE, angle);

    // The ground of that place, so that the way is asked for on the ground and not through a wall or down a cliff.
    // Where the map answers with no ground at all the height of the bones is kept, and the pathfinder gives the
    // answer for that place.
    float standZ = pCorpse->GetPositionZ();
    me->UpdateGroundPositionZ(standX, standY, standZ);

    // A spirit that stands at the place it was sent to, and can take its body from there, waits there rather than
    // walking it again: a body is not worth taking the moment the spirit stands by it, and the reclaim delay of a
    // player runs in the meantime, which is what a player in front of its own bones does. The reclaim radius of the
    // core is a three-dimensional one, so the bot that stands too high above or below its bones to reach them from
    // there is not waiting: it is walking, and it is counted as such.
    if (pCorpse->IsWithinDistInMap(me, CORPSE_RECLAIM_RADIUS, true) &&
        me->GetDistance(standX, standY, standZ, SizeFactor::None) < PB_RECLAIM_STAND_ARRIVED)
        return;

    // The path is asked for before the walk because a spline without one would walk through the geometry. A place
    // that answers with no way - behind a wall, over a cliff, across water the way round does not reach - is
    // turned over like an unsafe one, so that the sides of the bones the bot can walk to are the sides it uses.
    // The spirit healer is asked only when the walking has been asked for often enough to say that the ground
    // between the graveyard and the bones can not be walked over at all.
    PathFinder path(me);
    path.calculate(standX, standY, standZ);

    ++m_spiritWalkLegs;

    if (path.getPathType() & PATHFIND_NOPATH)
    {
        ++m_reclaimStandTry;
        return;
    }

    me->GetMotionMaster()->MovePoint(0, standX, standY, standZ, MOVE_PATHFINDING);
}

// Whether the way that the follow generator of this bot has for its leader goes a long way round the edge that the
// leader stands below, rather than down it. The pathfinder is asked for the same way that the generator is asking
// for, and the length of that way is read against the line from the bot to its leader. A slope that the navigation
// of this place can be walked down answers with a way that is that line, and the bot walks it as it always did;
// only a way that is both a multiple of the line and longer than it by a length of ground of its own is a way that
// goes round something. Where this place has no navigation to ask, and where no way reaches the leader at all, the
// bot is not walking round an edge: the first is a world that has no way round anything, and the second is the bot
// that stands stopped at the edge, which the rule below answers without asking the pathfinder again.
bool PartyBotAI::IsFollowWayRoundTheLedge(Player const* pLeader, float distToLeader) const
{
    PathFinder path(me);
    path.calculate(pLeader->GetPositionX(), pLeader->GetPositionY(), pLeader->GetPositionZ());

    if (path.getPathType() & (PATHFIND_NOPATH | PATHFIND_NOT_USING_PATH))
        return false;

    if (path.getPath().size() < 2)
        return false;

    float const wayLength = path.Length();
    return wayLength > distToLeader * PB_DROP_DETOUR_FACTOR &&
        wayLength - distToLeader > PB_DROP_DETOUR_EXTRA_DIST;
}

// A party that goes down over a ledge goes down together. The leader of a party is a player that jumps, so the bot
// that follows it goes over the edge as well instead of waiting for a way down that the pathfinder of the follow
// generator does not find over an edge. Two shapes of that are asked here: the bot that the pathfinder stopped at
// an edge, which is the case this rule was first written for, and the bot that is walking a way which its generator
// found the long way round the edge. The pet of the bot is sent away first, because a pet that is called at the top
// of a ledge stands there until its master walks back for it, and it is called again once the bot stands on the
// ground below.
bool PartyBotAI::FollowLeaderDownTheDrop(Player* pLeader)
{
    if (!pLeader || !pLeader->IsAlive() || me->IsTaxiFlying() || pLeader->IsTaxiFlying())
        return false;

    // A leader that rides goes down a slope rather than over an edge, and a bot that rides stays on its mount; that
    // mount is taken off in the same update, once the leader has gotten off its own.
    if (me->IsMounted() || pLeader->IsMounted())
        return false;

    // A boat that leaves a dock is not a ledge, so the two must stand on the same one, or on the ground.
    if (me->GetTransport() != pLeader->GetTransport())
        return false;

    if (me->HasUnitState(UNIT_STATE_NO_FREE_MOVE))
        return false;

    float const zDrop = me->GetPositionZ() - pLeader->GetPositionZ();
    // A step or a slope of less than the ledge floor is walked down by the follow generator as it always is; a
    // player jumps down what is more than that, and past PB_DROP_MAX_Z_DIFF the bot had better look for a way
    // around too. A bot that is walking is not a bot that the pathfinder stopped, and the edge that it walks round
    // is one it would jump down itself: for that bot the ledge starts lower, and the way round is only given up
    // when it really is a way round, which is asked of the pathfinder below rather than here.
    bool const walking = me->IsMoving();
    float const minLedgeDrop = walking ? PB_DROP_DETOUR_MIN_Z_DIFF : PB_DROP_MIN_Z_DIFF;
    if (zDrop < minLedgeDrop || zDrop > PB_DROP_MAX_Z_DIFF)
        return false;

    // Only the way to the leader is a way that the party walks together. A bot whose movement generator is a chase
    // or a place of its own routine is not following its leader over an edge.
    if (walking && me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        return false;

    // Only a leader that waits close by has just jumped down; the ground below an edge that is farther out than
    // this is not a place the party is standing on.
    if (!me->IsWithinDist2d(pLeader->GetPositionX(), pLeader->GetPositionY(), PB_DROP_MAX_DIST))
        return false;

    // The ground at the foot of the ledge is looked for along the line from the bot to its leader and the place
    // nearest to the leader is taken: a player jumps down beside the party that waits there, and the foot of an
    // overhang lies farther out than the edge does. A place on the ledge of the bot itself answers as no landing
    // place, because no ground lower than the one the bot stands on is found there.
    float const angleToLeader = me->GetAngle(pLeader);
    float const distToLeader = me->GetDistance2d(pLeader->GetPositionX(), pLeader->GetPositionY());
    float landX = 0.0f;
    float landY = 0.0f;
    float landZ = 0.0f;
    bool landingFound = false;
    // A leader that has come down right at the foot of the edge is looked for beside it all the same, so the
    // search starts close enough to the bot to answer for the place the bot must step over to fall.
    float firstStep = PB_DROP_STEP_LAND_DIST;
    if (firstStep > distToLeader * 0.5f)
        firstStep = distToLeader * 0.5f;

    for (float dist = firstStep; dist < distToLeader; dist += PB_DROP_STEP_LAND_DIST)
    {
        float x;
        float y;
        me->GetNearPoint2D(x, y, dist, angleToLeader);

        // Where no ground is found at all the height stays as it was asked for, which is the ground the bot stands
        // on, and that answers as no landing place.
        float z = me->GetPositionZ();
        me->UpdateGroundPositionZ(x, y, z);
        if (me->GetPositionZ() - z < minLedgeDrop)
            continue;

        // The bot lands beside its leader rather than in a pit below the place where the leader stands.
        if (z < pLeader->GetPositionZ() - PB_DROP_LAND_BELOW_LEADER)
            continue;

        // The way over the edge must be free: over an overhang or a wall the bot has to find its way around, and
        // the follow generator looks for that way on its own.
        if (!me->IsWithinLOS(x, y, z, true, me->GetCollisionHeight()))
            continue;

        landX = x;
        landY = y;
        landZ = z;
        landingFound = true;
    }

    if (!landingFound)
        return false;

    // The bot that stands at an edge was stopped there and has no way down that it is waiting for. A bot that is
    // walking has a way, and the edge is jumped rather than walked round only when that way goes round it: the
    // ground to land on is looked for first because it is the question that says whether there is an edge here at
    // all, and the pathfinder is not asked for a place where nothing is to be jumped.
    if (walking && !IsFollowWayRoundTheLedge(pLeader, distToLeader))
        return false;

    // A hunter that jumps down and leaves its pet above has to walk back for it, so the bot sends the pet away for
    // the drop. A pet that fights is not sent away, and a bot that stands at an edge out of combat has none that
    // fights.
    if (Pet* pPet = me->GetPet())
    {
        if (!pPet->IsInCombat())
        {
            me->UnsummonPetTemporaryIfAny();
            m_petDismissedForDrop = true;
        }
    }

    m_dropLandingZ = landZ;

    // The fall is a movement that the gravity of the client moves: the time it takes and the way down are its own,
    // which is how a character that steps over an edge is seen. MOVE_PATHFINDING is not asked for on purpose, as
    // there is no path over an edge to be found and a path would lead the bot back up to the edge.
    me->GetMotionMaster()->MovePoint(0, landX, landY, landZ, MOVE_FALLING);
    return true;
}

void PartyBotAI::CallPetBackAfterTheDrop()
{
    if (!m_petDismissedForDrop)
        return;

    // A pet that is back - the summon of the class update, or the call of a player that controls the bot again -
    // makes the dismissal over.
    if (me->GetPet())
    {
        m_petDismissedForDrop = false;
        return;
    }

    if (me->IsMounted() || me->IsTaxiFlying())
        return;

    // A pet is called to where its master stands, so the bot that has not come down keeps it dismissed: called at
    // the top of the ledge it would stand there again. A bot that must fight calls its pet to where it is.
    if (!me->IsInCombat() && me->GetPositionZ() > m_dropLandingZ + PB_DROP_LAND_TOLERANCE)
        return;

    me->ResummonPetTemporaryUnSummonedIfAny();
    m_petDismissedForDrop = false;
}

// How a party bot that died outside a battleground comes back: the group must raise it with a resurrection
// spell, or the bot must do what a player does - release the body, go to the graveyard as a spirit, and walk back
// to the bones to take the body back.
void PartyBotAI::UpdateSpiritResurrection()
{
    // The body is not released yet: the spirit still stands beside it. As long as a member of the group that is
    // alive is to be counted on to raise the bot, the bot waits for that spell, the way a player that died with a
    // priest in its group does. Where nobody is to be counted on the body is released at once; outside an instance
    // the release timer of a player, which the core runs for the bot too, releases it when it runs out instead.
    // BuildPlayerRepop() is what makes the bot a ghost, so it is asked only once.
    if (me->GetDeathState() != DEAD)
    {
        if (GroupHasResurrection())
            return;

        me->BuildPlayerRepop();
        me->RepopAtGraveyard();
        return;
    }

    Corpse* pCorpse = me->GetCorpse();
    if (!pCorpse)
    {
        // The bones are gone, so there is nothing to walk back to and the spirit healer raises the bot.
        if (WorldSession* pSession = me->GetSession())
            pSession->SendSpiritResurrect();
        return;
    }

    // The sides of the bones and the walking to them belong to one body, so another death - or a body that the
    // core has moved to another copy of the map - is started over.
    if (m_reclaimCorpseGuid != pCorpse->GetObjectGuid())
    {
        m_reclaimCorpseGuid = pCorpse->GetObjectGuid();
        m_reclaimStandTry = 0;
        m_spiritWalkLegs = 0;
    }

    // Taking ones own body back follows the same rules as for a player, see WorldSession::HandleReclaimCorpseOpcode:
    // the spirit must stand by the bones, and the body must have lain there long enough to be worth taking.
    if (pCorpse->IsWithinDistInMap(me, CORPSE_RECLAIM_RADIUS, true) &&
        pCorpse->GetGhostTime() + me->GetCorpseReclaimDelay(pCorpse->GetType() == CORPSE_RESURRECTABLE_PVP) <= time(nullptr))
    {
        // The pack that killed the bot often stands on its bones, and a bot that is raised in the middle of that
        // pack is back in the fight before its resurrection sickness has done anything, which is just what the
        // party hoped to avoid by letting the body go. So the body is taken only where nothing reaches the bot:
        // where it does, the spirit steps to the next side of the bones and asks again there. Once all the sides
        // have been asked, the body is taken where the bot stands, as a player that wants its body back does.
        if (IsCorpseReclaimPointSafe() || m_reclaimStandTry >= PB_RECLAIM_STAND_SIDES)
        {
            me->ResurrectPlayer(0.5f);
            me->SpawnCorpseBones();
            m_reclaimCorpseGuid.Clear();
            return;
        }

        ++m_reclaimStandTry;
    }

    WalkToCorpseReclaimPoint(pCorpse);
}

bool PartyBotAI::CanTryToCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry) const
{
    if (!CombatBotBaseAI::CanTryToCastSpell(pTarget, pSpellEntry))
        return false;

    if (pSpellEntry->IsAreaOfEffectSpell() && !pSpellEntry->IsPositiveSpell() && !IsInDuel())
    {
        if (!m_marksToCC.empty())
            return false;

        // do not cast aoe if it will pull aggro
        if (m_role != ROLE_TANK)
        {
            float radius;
            if (pSpellEntry->EffectRadiusIndex[0])
                radius = Spells::GetSpellRadius(sSpellRadiusStore.LookupEntry(pSpellEntry->EffectRadiusIndex[0]));
            else if (pSpellEntry->EffectRadiusIndex[1])
                radius = Spells::GetSpellRadius(sSpellRadiusStore.LookupEntry(pSpellEntry->EffectRadiusIndex[1]));
            else if (pSpellEntry->EffectRadiusIndex[2])
                radius = Spells::GetSpellRadius(sSpellRadiusStore.LookupEntry(pSpellEntry->EffectRadiusIndex[2]));
            else
                radius = 10.0f;

            std::list<Unit*> targets;
            me->GetEnemyListInRadiusAround(pTarget, radius, targets);

            for (auto const& pEnemy : targets)
            {
                if (((pEnemy->GetLevel() + 5) > me->GetLevel()) &&
                    ((pEnemy->GetHealth() * 4) > me->GetHealth()) &&
                    pEnemy->GetVictim() && pEnemy->GetVictim() != me &&
                    pEnemy->IsValidAttackTarget(me) &&
                    pEnemy->CanHaveThreatList())
                {
                    float const myThreat = pEnemy->GetThreatManager().getThreat(me);
                    float const victimThreat = pEnemy->GetThreatManager().getThreat(pEnemy->GetVictim());

                    if (victimThreat < (myThreat + me->GetMaxHealth()))
                        return false;
                }
            }
        }
    }

    return true;
}

bool PartyBotAI::CanUseCrowdControl(SpellEntry const* pSpellEntry, Unit* pTarget) const
{
    if (IsInDuel())
        return true;

    if (pSpellEntry->HasAuraInterruptFlag(AURA_INTERRUPT_DAMAGE_CANCELS) &&
        AreOthersOnSameTarget(pTarget->GetObjectGuid()))
        return false;

    if (pSpellEntry->HasSingleTargetAura())
    {
        auto const& singleAuras = me->GetSingleCastSpellTargets();
        if (singleAuras.find(pSpellEntry) != singleAuras.end())
            return false;
    }

    return true;
}

bool PartyBotAI::AttackStart(Unit* pVictim)
{
    m_isBuffing = false;

    if (me->IsMounted())
        me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    if (me->GetClass() == CLASS_WARRIOR && !me->IsInCombat())
    {
        // Keep the target even while the stance change is still being applied. In particular, an explicit
        // AttackStart() must not leave a shield tank without a victim merely because the next cast is a stance.
        if (!me->Attack(pVictim, true) && me->GetVictim() != pVictim)
            return false;

        // Every warrior opens the same way. The stance of an opening that goes to the charge is the stance that
        // the charge wants, and where no charge is to be made - in an instance, or without the spell, the range or
        // the rage for it - the bot keeps the stance that its own rules ask for, which is the Defensive Stance of
        // a shield tank and the stance of the talents of a damage warrior.
        bool const openWithCharge = CanOpenWarriorCombatWithCharge();
        SpellEntry const* pStance = openWithCharge ?
            m_spells.warrior.pBattleStance :
            (IsProtectionWarriorTank() ? m_spells.warrior.pDefensiveStance : GetWarriorHomeStanceSpell());
        ShapeshiftForm const desiredForm = GetStanceSpellForm(pStance);

        if (pStance && me->GetShapeshiftForm() != desiredForm)
            TrySwitchWarriorStance(pStance);

        if (openWithCharge)
        {
            // The opening begins here and may not end here. The stance that a Charge needs is the one asked above,
            // the core applies a stance when the spell event of the caster runs, which can be after this update of
            // the bot, and that stance puts a global cooldown back and leaves its own spell in progress, either of
            // which refuses the charge of this update. So the ask stands for the target, and the following updates
            // answer it at PartyBotAI::TryContinueWarriorCharge, which is also what walks a bot that stands too
            // far from its target into the charge range. A target that is inside that range from the beginning was
            // the case that this lost: its ask was refused once, the bot walked to the mob in its own Defensive
            // Stance, and the charge never came.
            m_warriorChargeTargetGuid = pVictim->GetObjectGuid();
            m_warriorChargeAskRetries = 0;

            if (TryChargeWarriorTarget(pVictim) != PB_CHARGE_REFUSED)
                return true;

            ClearWarriorChargeOpening();
        }
    }

    if (PrepareStealthOpener())
        return true;

    bool const waitForStealthOpener =
        me->GetClass() == CLASS_ROGUE &&
        me->HasAuraType(SPELL_AURA_MOD_STEALTH) &&
        (m_spells.rogue.pCheapShot || m_spells.rogue.pGarrote);

    if (waitForStealthOpener)
    {
        // Both Attack() and ChaseMovementGenerator() can start melee attacks when the target is reached. Follow
        // movement keeps the same behind-target offset without starting an attack, so stealth survives the approach.
        m_rogueStealthTargetGuid = pVictim->GetObjectGuid();
        me->SetTargetGuid(m_rogueStealthTargetGuid);
        me->GetMotionMaster()->MoveFollow(pVictim, 1.0f, M_PI_F);
        return true;
    }

    m_rogueStealthTargetGuid.Clear();
    // A mage fights by casting. Unit::Attack(..., true) puts it in the melee attacking state, and the core then swings
    // at anything that closes while no spell is being prepared. Keep the mage's victim without enabling melee attacks,
    // but put it in combat explicitly: nothing else in a spell-only attack enters combat for it.
    bool const wasInCombat = me->IsInCombat();
    bool const useMeleeAttack = me->GetClass() != CLASS_MAGE;
    bool const attackStarted = me->Attack(pVictim, useMeleeAttack);
    if (attackStarted || me->GetVictim() == pVictim)
    {
        if (me->GetClass() == CLASS_PRIEST &&
            GetRole() == ROLE_RANGE_DPS &&
            !wasInCombat)
            m_priestMindBlastOpener = true;

        if (!useMeleeAttack)
            me->SetInCombatWithVictim(pVictim);

        BeginChasing(pVictim);
        return true;
    }

    return false;
}

Unit* PartyBotAI::GetMarkedTarget(RaidTargetIcon mark) const
{
    ObjectGuid targetGuid = me->GetGroup()->GetTargetWithIcon(mark);
    if (targetGuid.IsUnit())
        return me->GetMap()->GetUnit(targetGuid);

    return nullptr;
}

Unit* PartyBotAI::SelectOnlyControlledHostileTarget() const
{
    std::list<Unit*> targets;
    me->GetEnemyListInRadiusAround(me, 50.0f, targets);

    Unit* pOnlyTarget = nullptr;
    for (Unit* pTarget : targets)
    {
        if (!pTarget->IsInCombat() || !me->IsHostileTo(pTarget) ||
            !pTarget->IsValidAttackTarget(me) ||
            !pTarget->IsVisibleForOrDetect(me, me, false) ||
            pTarget->IsTotalImmune() ||
            pTarget->GetTransport() != me->GetTransport())
            continue;

        if (pOnlyTarget)
            return nullptr;

        pOnlyTarget = pTarget;
    }

    return pOnlyTarget && pOnlyTarget->HasBreakableByDamageCrowdControlAura() ? pOnlyTarget : nullptr;
}

bool PartyBotAI::IsValidHostileTargetForAttack(Unit const* pTarget) const
{
    if (!pTarget)
        return false;

    return IsValidHostileTarget(pTarget) || SelectOnlyControlledHostileTarget() == pTarget;
}

bool PartyBotAI::HasMeleeAttackerInRange() const
{
    for (Unit* pAttacker : me->GetAttackers())
    {
        if (pAttacker->GetVictim() == me && pAttacker->CanReachWithMeleeAutoAttack(me))
            return true;
    }

    return false;
}

Unit* PartyBotAI::SelectNearestMeleeAttacker() const
{
    Unit* pNearest = nullptr;
    float nearestDistance = 0.0f;

    for (Unit* pAttacker : me->GetAttackers())
    {
        if (pAttacker->GetVictim() != me ||
            !pAttacker->IsAlive() ||
            !pAttacker->CanReachWithMeleeAutoAttack(me))
            continue;

        float const distance = me->GetCombatDistance(pAttacker);
        if (!pNearest || distance < nearestDistance)
        {
            pNearest = pAttacker;
            nearestDistance = distance;
        }
    }

    return pNearest;
}

bool PartyBotAI::IsMarkedForCrowdControl(Unit const* pTarget) const
{
    for (RaidTargetIcon mark : m_marksToCC)
        if (GetMarkedTarget(mark) == pTarget)
            return true;

    return false;
}

uint8 PartyBotAI::CountNonRangedAttackersInRange(float range) const
{
    uint8 count = 0;
    for (Unit* pAttacker : me->GetAttackers())
        if (pAttacker->GetVictim() == me &&
            me->GetCombatDistance(pAttacker) <= range &&
            !IsRangedDamageClass(pAttacker->GetClass()))
            ++count;

    return count;
}

uint8 PartyBotAI::CountUnmarkedEnemiesInRange(float range) const
{
    uint8 count = 0;
    std::list<Unit*> targets;
    me->GetEnemyListInRadiusAround(me, range, targets);
    for (Unit* pTarget : targets)
        if (IsValidHostileTarget(pTarget) && !IsMarkedForCrowdControl(pTarget))
            ++count;

    return count;
}

bool PartyBotAI::IsManaUser(Player const* pPlayer) const
{
    return pPlayer && pPlayer->GetCreateMana() > 0;
}

Player* PartyBotAI::SelectMissingGroupBuffTarget(SpellEntry const* pSingleSpellEntry,
    SpellEntry const* pGroupSpellEntry, bool manaUsersOnly, uint8& missingCount) const
{
    missingCount = 0;
    Player* pFirstMissing = nullptr;

    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return nullptr;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (!pMember ||
            pMember->IsGameMaster() ||
            !pMember->IsAlive() ||
            (manaUsersOnly && !IsManaUser(pMember)))
            continue;

        // A member has the buff when any rank of either the single target or the group spell is present.
        if (pSingleSpellEntry && !IsValidBuffTarget(pMember, pSingleSpellEntry))
            continue;
        if (pGroupSpellEntry && !IsValidBuffTarget(pMember, pGroupSpellEntry))
            continue;

        if (!pFirstMissing &&
            me->IsValidHelpfulTarget(pMember) &&
            me->IsWithinLOSInMap(pMember) &&
            me->IsWithinDist(pMember, 30.0f))
            pFirstMissing = pMember;
        ++missingCount;
    }

    return pFirstMissing;
}

bool PartyBotAI::PriestFortitudeBuffDelayPassed()
{
    std::set<ObjectGuid> currentMembers;
    Group* pGroup = me->GetGroup();
    if (pGroup)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
                currentMembers.insert(pMember->GetObjectGuid());
        }
    }

    if (!m_priestFortitudeMembersInitialized)
    {
        m_priestFortitudeMembers = currentMembers;
        m_priestFortitudeMembersInitialized = true;
        return true;
    }

    bool newMemberJoined = false;
    for (ObjectGuid const& guid : currentMembers)
    {
        if (m_priestFortitudeMembers.find(guid) == m_priestFortitudeMembers.end())
        {
            newMemberJoined = true;
            break;
        }
    }

    m_priestFortitudeMembers = currentMembers;
    if (newMemberJoined)
        m_priestFortitudeBuffReadyAt = uint64(time(nullptr)) + 30;

    return uint64(time(nullptr)) >= m_priestFortitudeBuffReadyAt;
}

bool PartyBotAI::PriestHasOutOfCombatBuffTarget()
{
    uint8 missing = 0;
    if (m_spells.priest.pPrayerofFortitude)
    {
        if (!PriestFortitudeBuffDelayPassed())
            return true;
        if (SelectMissingGroupBuffTarget(m_spells.priest.pPowerWordFortitude,
                m_spells.priest.pPrayerofFortitude, false, missing))
            return true;
    }
    else if (m_spells.priest.pPowerWordFortitude &&
             SelectMissingGroupBuffTarget(m_spells.priest.pPowerWordFortitude,
                 nullptr, false, missing))
        return true;

    Map* pMap = me->GetMap();
    bool const inDungeon = pMap && pMap->IsDungeon();
    if (!inDungeon)
    {
        if (m_spells.priest.pDivineSpirit &&
            SelectMissingGroupBuffTarget(m_spells.priest.pDivineSpirit,
                nullptr, true, missing))
            return true;
    }
    else if (m_spells.priest.pPrayerofSpirit)
    {
        Group* pGroup = me->GetGroup();
        if (pGroup && pGroup->GetMembersCount() >= 5 &&
            SelectMissingGroupBuffTarget(m_spells.priest.pDivineSpirit,
                m_spells.priest.pPrayerofSpirit, true, missing) &&
            missing >= 3)
            return true;
    }

    return false;
}

bool PartyBotAI::PriestNeedsResurrectionOrDiseaseDispel()
{
    if (m_resurrectionSpell && SelectResurrectionTarget())
        return true;

    return m_spells.priest.pAbolishDisease &&
        SelectDispelTarget(m_spells.priest.pAbolishDisease);
}

bool PartyBotAI::IsTankRoleWarrior(Player* pPlayer) const
{
    if (!pPlayer || pPlayer->GetClass() != CLASS_WARRIOR || !pPlayer->AI())
        return false;

    CombatBotBaseAI* pAI = dynamic_cast<CombatBotBaseAI*>(pPlayer->AI());
    return pAI && pAI->GetRole() == ROLE_TANK;
}

bool PartyBotAI::IsProtectionWarriorTank() const
{
    if (me->GetClass() != CLASS_WARRIOR)
        return false;

    // A warrior of the party that wears a shield is the shield tank of that party whatever its role says. The
    // role of a bot that comes back from the database is derived again (see CombatBotBaseAI::AutoAssignRole),
    // and the taunts and the threat of a shield tank must not disappear because that derivation guessed a
    // damage talent: no other warrior talent is dressed with a shield (see GetBotWeaponRules), so the shield is
    // the rule that cannot be guessed wrong.
    if (IsWearingShield(me))
        return true;

    // The role that a warrior carries was itself given by the shield, by the talent build that its learned
    // talents complete, or by `.partybot add <talent>', so a protection warrior whose gear put no shield in its
    // off hand still keeps the Defensive Stance, the taunts and the threat rules of its talent instead of
    // fighting in the stance of a damage dealer.
    return m_role == ROLE_TANK;
}

bool PartyBotAI::IsOtherPartyMember(Unit const* pTarget) const
{
    if (!pTarget || pTarget == me || !pTarget->IsPlayer())
        return false;

    Group* pGroup = me->GetGroup();
    return pGroup && pGroup->IsMember(pTarget->GetObjectGuid());
}

// A warrior opens a fight with Charge, and the opening is the same for every warrior of the party: a damage
// warrior and a shield tank are asked in the same place and keep the target through the change of stance that the
// charge wants. A bot has no keyboard to keep the target at the outside edge of the charge range, so the chase
// generator stops at the maximum range and the spell is cast as soon as the target enters it.
//
// The opening is asked for outside an instance only. The ground of a dungeon or a raid is ground that the party
// has already walked into: the mobs are pulled by whoever stands in front, the warrior is standing in the middle
// of them when its bot joins, and the rage and the global cooldown of a charge that hits nothing are lost from
// the rotation of the fight. A battleground is not an instance for the map of the core, and the yard of a
// battleground is the open ground that the rule is about.
bool PartyBotAI::CanOpenWarriorCombatWithCharge() const
{
    Map* pMap = me->GetMap();
    return me->GetClass() == CLASS_WARRIOR &&
        m_spells.warrior.pCharge &&
        !me->IsInCombat() &&
        pMap && !pMap->IsDungeon() &&
        me->IsSpellReady(m_spells.warrior.pCharge) &&
        me->GetPower(POWER_RAGE) >= Spell::CalculatePowerCost(m_spells.warrior.pCharge, me);
}

// Whether the opening of a charge may be asked again on the next update of the bot. A stance that was asked in
// this update, the global cooldown of it and the spell of it that is still in progress all answer themselves, so
// they are not reasons to give the opening up; PB_WARRIOR_CHARGE_ASK_UPDATES of them are spent, though, because a
// tank that waits at the edge of the charge range while its party fights is worse than a tank that walks in.
bool PartyBotAI::CanAskWarriorChargeLater()
{
    if (m_warriorChargeAskRetries >= PB_WARRIOR_CHARGE_ASK_UPDATES)
        return false;

    ++m_warriorChargeAskRetries;
    return true;
}

// The opening of a charge is over, whatever the reason: nothing is remembered for it, and the chase of the bot is
// the ordinary one again.
void PartyBotAI::ClearWarriorChargeOpening()
{
    m_warriorChargeTargetGuid.Clear();
    m_warriorChargeAskRetries = 0;
    me->SetCasterChaseDistance(0.0f);
}

PartyBotAI::WarriorChargeAsk PartyBotAI::TryChargeWarriorTarget(Unit* pTarget)
{
    if (!pTarget || !m_spells.warrior.pCharge || me->IsInCombat())
        return PB_CHARGE_REFUSED;

    Map* pMap = me->GetMap();
    if (!pMap || pMap->IsDungeon())
        return PB_CHARGE_REFUSED;

    SpellEntry const* pCharge = m_spells.warrior.pCharge;
    uint32 const rageCost = Spell::CalculatePowerCost(pCharge, me);
    if (!me->IsSpellReady(pCharge) ||
        me->GetPower(POWER_RAGE) < rageCost ||
        pTarget->IsImmuneToSpell(pCharge, false) ||
        !me->IsWithinLOSInMap(pTarget))
        return PB_CHARGE_REFUSED;

    // A Charge belongs to the Battle Stance, and the stance that the opening asked is applied when the spell event
    // of the caster runs, which can be after the update that asked it: the ask of such an update stands, bounded by
    // CanAskWarriorChargeLater(). A bot that has not learned the stance is refused at once, so that an ask cannot
    // stand for a form that would never come.
    if (pCharge->GetErrorAtShapeshiftedCast(me->GetShapeshiftForm()) != SPELL_CAST_OK)
        return m_spells.warrior.pBattleStance && CanAskWarriorChargeLater() ?
            PB_CHARGE_LATER : PB_CHARGE_REFUSED;

    float const chargeRange = GetSpellMaxRangeForBot(pCharge);
    if (me->GetCombatDistance(pTarget) > chargeRange)
    {
        m_warriorChargeTargetGuid = pTarget->GetObjectGuid();
        me->SetTargetGuid(m_warriorChargeTargetGuid);
        me->SetCasterChaseDistance(std::max(chargeRange - 1.0f, 1.0f));
        me->GetMotionMaster()->MoveChase(pTarget, 1.0f, 0.0f);
        return PB_CHARGE_APPROACH;
    }

    if (!IsTargetInSpellRange(pTarget, pCharge))
        return PB_CHARGE_REFUSED;

    // The two gates that the stance of the opening leaves, and that clear by themselves: the global cooldown that
    // the core puts back for a stance, and the spell of that stance which has not finished. The core refuses the
    // charge of this update with either of them, and the next update of the bot is free of both.
    if (me->HasGCD(pCharge) || me->IsNonMeleeSpellCasted())
        return CanAskWarriorChargeLater() ? PB_CHARGE_LATER : PB_CHARGE_REFUSED;

    if (!CanTryToCastSpell(pTarget, pCharge))
        return PB_CHARGE_REFUSED;

    if (DoCastSpell(pTarget, pCharge) != SPELL_CAST_OK)
        return CanAskWarriorChargeLater() ? PB_CHARGE_LATER : PB_CHARGE_REFUSED;

    ClearWarriorChargeOpening();

    // A charge is only cast in Battle Stance, so the tank that charged stands in the stance that a Thunder Clap
    // would otherwise have to change to. The pack around the target is at its closest in the seconds after the
    // charge, and the area of the clap puts the first threat on every mob that is coming; the rotation of the tank
    // is told that this opening may spend one clap, and the ask is over with the Battle Stance of the charge.
    if (IsProtectionWarriorTank())
        m_warriorChargeOpeningThunderClap = true;

    return PB_CHARGE_CAST;
}

bool PartyBotAI::TryContinueWarriorCharge()
{
    if (m_warriorChargeTargetGuid.IsEmpty())
        return false;

    Unit* pTarget = me->GetMap()->GetUnit(m_warriorChargeTargetGuid);
    if (!pTarget || !IsValidHostileTarget(pTarget) || me->IsInCombat())
    {
        ClearWarriorChargeOpening();
        return false;
    }

    // The opening owns this update while it stands: the walk to the charge range, the wait for the stance that was
    // asked, and the charge itself. Nothing else in the update moves the bot or changes its stance, which is what
    // keeps the Defensive Stance from being asked back between the ask of the opening and its charge.
    if (TryChargeWarriorTarget(pTarget) == PB_CHARGE_REFUSED)
    {
        ClearWarriorChargeOpening();
        return false;
    }

    return true;
}

// Intercept is the reach that a warrior has without a mount, and both of the warriors of the party are asked the
// same question before they use it. What the spell itself gates is asked of the spell: the rank that the bot
// knows, its cooldown, the global cooldown, the immunity of the target and the range that the data of the spell
// covers, its minimum range with it. Two of the gates are rules rather than gates of the spell. The bot and its
// target must both be fighting: out of a fight a warrior crosses that ground on its legs or with the opening that
// Charge is, and the rage of a jump that nothing follows is the rage of a fight that the bot did not join. And
// the target must be beyond a swing of the bot, which is the same ground as the minimum range of the spell read a
// line above: a mob that the bot can already hit is not a mob to jump at.
//
// A target outside the far end of that range is not a target that the bot waits for. The chase that the rotation
// of a warrior asks anyway walks it through the range of the spell, and the jump is asked on the update in which
// the bot stands inside it; a bot that stopped at the edge of the range to cast would stand twenty yards from the
// mob it is fighting and hit nothing, which is worse than the few steps of the walk.
bool PartyBotAI::IsWarriorInterceptWanted(Unit const* pTarget) const
{
    SpellEntry const* pIntercept = m_spells.warrior.pIntercept;
    if (!pTarget || !pIntercept || !me->IsInCombat() || !pTarget->IsInCombat())
        return false;

    if (!IsValidHostileTarget(pTarget) ||
        !me->IsSpellReady(pIntercept) ||
        me->HasGCD(pIntercept) ||
        me->IsNonMeleeSpellCasted())
        return false;

    if (me->GetPower(POWER_RAGE) <= uint32(PB_WARRIOR_INTERCEPT_RAGE_MIN * PB_RAGE_PER_POINT) ||
        IsWarriorRageShort(pIntercept, nullptr))
        return false;

    if (pTarget->IsImmuneToSpell(pIntercept, false) || !me->IsWithinLOSInMap(pTarget))
        return false;

    return !me->CanReachWithMeleeAutoAttack(pTarget) && IsTargetInSpellRange(pTarget, pIntercept);
}

uint32 PartyBotAI::GetTacticalMasteryRank() const
{
    if (!m_spells.warrior.pTacticalMastery)
        return 0;

    TalentSpellPos const* pTalentPos = GetTalentSpellPos(m_spells.warrior.pTacticalMastery->Id);
    return pTalentPos ? uint32(pTalentPos->rank) + 1 : 0;
}

bool PartyBotAI::HasThunderClapDebuff(Unit const* pTarget) const
{
    if (!pTarget)
        return false;

    if (m_spells.warrior.pThunderClap && pTarget->HasAura(m_spells.warrior.pThunderClap->Id))
        return true;

    for (auto const& holder : pTarget->GetSpellAuraHolderMap())
    {
        if (holder.second &&
            holder.second->GetSpellProto()->IsFitToFamily<SPELLFAMILY_WARRIOR, CF_WARRIOR_THUNDER_CLAP>())
            return true;
    }

    return false;
}

// Whether the mob already carries the attack-power debuff of Demoralizing Shout. The family of the spell is asked
// for besides the rank that the bot knows, because the debuff of another rank of that spell is the same debuff.
bool PartyBotAI::HasDemoralizingShoutDebuff(Unit const* pTarget) const
{
    if (!pTarget)
        return false;

    if (m_spells.warrior.pDemoralizingShout && pTarget->HasAura(m_spells.warrior.pDemoralizingShout->Id))
        return true;

    for (auto const& holder : pTarget->GetSpellAuraHolderMap())
    {
        if (holder.second &&
            holder.second->GetSpellProto()->IsFitToFamily<SPELLFAMILY_WARRIOR, CF_WARRIOR_DEMORALIZING_SHOUT>())
            return true;
    }

    return false;
}

// Whether the mob that the tank holds is an elite far enough above the bot to be the skull that the client shows
// for a boss. The debuff of a Thunder Clap takes attack power off such a mob, which is the cheaper defence for a
// tank than the armor that the same rage would buy.
bool PartyBotAI::IsSkullConEliteTarget(Unit const* pTarget) const
{
    if (!pTarget)
        return false;

    Creature const* pCreature = pTarget->ToCreature();
    return pCreature && pCreature->IsElite() &&
        pTarget->GetLevel() >= me->GetLevel() + PB_WARRIOR_THUNDER_CLAP_LEVEL_GAP;
}

// Whether a neutral stands at peace within the area of one of the spells of the tank, looked for about the point
// pCenter. A neutral that is not in the fight yet is woken by the debuff of such a spell and joins it, which is a
// fight that the tank did not ask for; a neutral that is fighting already, or whose faction data makes it hostile
// to the bot, is in that fight whatever the spell does to it, so it is no reason to hold the spell back. The
// reaction of a mob is what its faction and the reputation of the bot say, and a fight does not change it, so the
// fight itself is what tells a neutral that is already engaged from one that a spell would engage. Only the units
// that the spell could really reach are looked at: the gate of the bot for a target is the gate that the area of
// effect of the core puts in front of its own targets, and the reaction of the bot is the one that the client
// colours yellow.
// Whether a neutral stands at peace about the bot: the area of a spell of the warrior would bring such a mob into
// the fight, which is a fight that the party did not ask for. The yellow name is the reaction of the bot, which is
// what the client colours, and a player that wears one in a neutral town is not a mob that a spell would wake. A
// creature that is in a fight already is in it whatever the spell does to it, and a creature that the faction data
// makes hostile wears a red name; the reaction of a mob is what its faction and the reputation of the bot say, and
// a fight does not change it, so the fight itself is what tells a neutral that is engaged from one at peace.
bool PartyBotAI::IsNeutralAtPeace(Unit const* pUnit) const
{
    if (!pUnit || !pUnit->IsCreature() || !IsValidHostileTarget(pUnit) || pUnit->IsInCombat())
        return false;

    return me->GetReactionTo(pUnit) == REP_NEUTRAL;
}

bool PartyBotAI::HasWakingNeutralNear(Unit const* pCenter, float radius) const
{
    std::list<Unit*> nearbyUnits;
    me->GetEnemyListInRadiusAround(pCenter, radius, nearbyUnits);

    for (Unit* pUnit : nearbyUnits)
    {
        if (IsNeutralAtPeace(pUnit))
            return true;
    }

    return false;
}

// Whether a mob is held by a control that damage breaks. The flag that says so is the one that
// CanUseCrowdControl() reads before the bot controls a mob itself, because what breaks such a control is damage
// on the held mob: the control that another party member is keeping is not to be spoiled by the splash of a
// Whirlwind or of a Sweeping Strikes, which is the damage that those two put on the pack about the bot.
bool PartyBotAI::IsHeldByDamageBreakingControl(Unit const* pUnit) const
{
    if (!pUnit)
        return false;

    for (auto const& holder : pUnit->GetSpellAuraHolderMap())
    {
        if (!holder.second)
            continue;

        SpellEntry const* pControl = holder.second->GetSpellProto();
        if (pControl && pControl->HasAuraInterruptFlag(AURA_INTERRUPT_DAMAGE_CANCELS))
            return true;
    }

    return false;
}

bool PartyBotAI::TrySwitchWarriorStance(SpellEntry const* pStance)
{
    // Which gate refused the stance is remembered for `.partybot stance': the spells, the shield and the talent
    // that decide the stance of a bot are not visible from the chat window otherwise, and a log line of DEBUG
    // level is not open in a normal session.
    m_warriorStanceAskedId = pStance ? pStance->Id : 0;
    m_warriorStanceNormalResult = uint32(-1);
    m_warriorStanceTriggeredResult = uint32(-1);

    if (!pStance)
    {
        m_warriorStanceBlock = PB_STANCE_NO_SPELL;
        return false;
    }

    if (!me->IsSpellReady(pStance))
    {
        m_warriorStanceBlock = PB_STANCE_NOT_READY;
        return false;
    }

    if (me->HasGCD(pStance))
    {
        m_warriorStanceBlock = PB_STANCE_GLOBAL_COOLDOWN;
        return false;
    }

    if (me->GetPower(POWER_RAGE) < Spell::CalculatePowerCost(pStance, me))
    {
        m_warriorStanceBlock = PB_STANCE_POWER;
        return false;
    }

    ShapeshiftForm const stanceForm = GetStanceSpellForm(pStance);

    if (stanceForm == FORM_NONE || stanceForm == me->GetShapeshiftForm())
    {
        // A spell that asks for no form is not a stance spell, and the answer is false for it: a caller that is
        // told that its stance is worn stops there, and a shield tank that believes it stands in Defensive Stance
        // while it does not spends every update of its bot on a stance it never takes instead of on Revenge,
        // Shield Block, Shield Slam or Sunder Armor.
        if (stanceForm == FORM_NONE)
        {
            m_warriorStanceBlock = PB_STANCE_NO_FORM;
            sLog.Out(LOG_BASIC, LOG_LVL_DEBUG, "Party bot %s: stance spell %u has no shapeshift effect",
                me->GetName(), pStance->Id);
            return false;
        }

        m_warriorStanceBlock = PB_STANCE_WORN;
        return true;
    }

    SpellCastResult const result = DoCastSpell(me, pStance);
    m_warriorStanceNormalResult = uint32(result);
    if (me->GetShapeshiftForm() == stanceForm)
    {
        m_warriorStanceBlock = PB_STANCE_WORN;
        return true;
    }

    // A bot can issue a stance change between its previous action and the next melee swing, where the ordinary cast
    // checks may still reject it as a spell in progress or a global cooldown. The triggered path applies the same
    // shapeshift aura that a player cast would apply; the GCD is still put back so the stance is not free.
    SpellCastResult const triggeredResult = me->CastSpell(me, pStance, true);
    m_warriorStanceTriggeredResult = uint32(triggeredResult);
    me->AddGCD(pStance);
    if (me->GetShapeshiftForm() == stanceForm)
    {
        m_warriorStanceBlock = PB_STANCE_WORN;
        return true;
    }

    // Only the worn form says that a stance is taken. The core accepts the cast of a stance and applies it when
    // the spell event of the caster runs, which can be after this update of the bot, and the caller must then go
    // on with its rotation rather than wait for it: an asked stance comes, a rotation that waits for it does not.
    m_warriorStanceBlock = PB_STANCE_CAST_REFUSED;
    sLog.Out(LOG_BASIC, LOG_LVL_DEBUG, "Party bot %s failed to switch to stance %u: normal %u, triggered %u",
        me->GetName(), pStance->Id, uint32(result), uint32(triggeredResult));
    return false;
}

// The stance that a damage warrior stands in between the spells that ask for another one: the Berserker Stance
// for a bot that learned Bloodthirst and did not take Improved Overpower, the Battle Stance for every other
// damage warrior. Both are read from what the bot knows, because neither the asked talent nor the worn stance is
// saved with a bot: the one is the spell that the Fury tree gives at its fortieth point, the other the talent of
// the Arms tree that a build reaches later, and a bot of the levels between them has one without the other.
SpellEntry const* PartyBotAI::GetWarriorHomeStanceSpell() const
{
    bool const wantsBerserkerStance =
        m_spells.warrior.pBloodthirst && !m_spells.warrior.pImprovedOverpower;

    if (wantsBerserkerStance && m_spells.warrior.pBerserkerStance)
        return m_spells.warrior.pBerserkerStance;

    // A stance that was never taught is not a stance to keep: the quests of this world give the Berserker Stance
    // at thirty levels and the defensive and battle stances at ten, and a bot that has one of the two would else
    // spend every update of its bot asking for a form that it cannot wear.
    if (m_spells.warrior.pBattleStance)
        return m_spells.warrior.pBattleStance;

    return m_spells.warrior.pBerserkerStance;
}

ShapeshiftForm PartyBotAI::GetWarriorHomeStanceForm() const
{
    return GetStanceSpellForm(GetWarriorHomeStanceSpell());
}

// The spell that puts the asked form on the bot, looked up by the form of the spell rather than by its name: the
// rotation is told which form a spell wants by the data of that spell, and has to find the stance spell that
// answers it.
SpellEntry const* PartyBotAI::GetWarriorStanceSpell(ShapeshiftForm askedForm) const
{
    if (askedForm == FORM_NONE)
        return nullptr;

    SpellEntry const* const stances[] = {
        m_spells.warrior.pBattleStance,
        m_spells.warrior.pDefensiveStance,
        m_spells.warrior.pBerserkerStance,
    };

    for (SpellEntry const* pStance : stances)
    {
        if (GetStanceSpellForm(pStance) == askedForm)
            return pStance;
    }

    return nullptr;
}

// Whether the rage that a spell costs is not there. The extra cost is the stance that the spell is to be cast in,
// which the bot has to pay for as well: the two are asked for together because a change of stance that leaves the
// bot with nothing to cast is rage spent on nothing. Health is not asked about here: Bloodrage is a spell that
// costs health rather than rage, and it is the answer that this question is asked for.
bool PartyBotAI::IsWarriorRageShort(SpellEntry const* pSpellEntry, SpellEntry const* pExtraCost) const
{
    if (!pSpellEntry || Powers(pSpellEntry->powerType) != POWER_RAGE)
        return false;

    uint32 neededCost = Spell::CalculatePowerCost(pSpellEntry, me);
    if (pExtraCost && Powers(pExtraCost->powerType) == POWER_RAGE)
        neededCost += Spell::CalculatePowerCost(pExtraCost, me);

    return me->GetPower(POWER_RAGE) < neededCost;
}

// Bloodrage is the rage that a damage warrior makes for itself, and it is asked for when a want of the rotation
// is refused by nothing but the rage that it costs. The cast gate refuses the spell of a bot whose health is too
// low to pay for it, which is where the rotation goes back to its swings instead of buying rage with the little
// health that is left.
bool PartyBotAI::TryWarriorBloodrage()
{
    if (!m_spells.warrior.pBloodrage)
        return false;

    return CanTryToCastSpell(me, m_spells.warrior.pBloodrage) &&
        DoCastSpell(me, m_spells.warrior.pBloodrage) == SPELL_CAST_OK;
}

static char const* GetWarriorStanceBlockName(uint32 block)
{
    switch (block)
    {
        case PartyBotAI::PB_STANCE_WORN:            return "worn";
        case PartyBotAI::PB_STANCE_NO_SPELL:        return "no spell was given";
        case PartyBotAI::PB_STANCE_NOT_READY:       return "the spell or its category is on cooldown";
        case PartyBotAI::PB_STANCE_GLOBAL_COOLDOWN: return "the global cooldown of another spell";
        case PartyBotAI::PB_STANCE_POWER:           return "not enough rage";
        case PartyBotAI::PB_STANCE_NO_FORM:         return "the spell has no shapeshift effect";
        case PartyBotAI::PB_STANCE_CAST_REFUSED:    return "both casts were refused";
    }

    return "nothing was asked";
}

static char const* GetWarriorStanceFormName(uint32 form)
{
    switch (form)
    {
        case FORM_BATTLESTANCE:      return "Battle Stance";
        case FORM_DEFENSIVESTANCE:   return "Defensive Stance";
        case FORM_BERSERKERSTANCE:   return "Berserker Stance";
    }

    return "no form";
}

// A stance that a warrior party bot has not been taught is a question for the data of the world and not for the
// stance rules: a class ability comes either from a trainer of the class or from the reward of a quest. For the
// For the class abilities that the bot is missing, the quests that would give one to a character of this class and
// race are printed with what their row of `quest_template` asks for, which is what tells a world without such a row
// from a row that asks for a level the bot has not reached.
static void AppendClassQuestRewardQuests(std::ostringstream& out, Player const* me,
    std::vector<char const*> const& missingSpells)
{
    if (missingSpells.empty())
        return;

    uint32 const classMask = me->GetClassMask();
    uint32 const raceMask = me->GetRaceMask();
    std::vector<uint32> rewardSpells;

    for (char const* spellName : missingSpells)
    {
        bool found = false;

        for (auto const& questItr : sObjectMgr.GetQuestTemplates())
        {
            Quest const* quest = questItr.second.get();
            if (!quest)
                continue;

            if ((quest->GetRequiredClasses() && (quest->GetRequiredClasses() & classMask) == 0) ||
                (quest->GetRequiredRaces() && (quest->GetRequiredRaces() & raceMask) == 0))
                continue;

            rewardSpells.clear();
            CombatBotBaseAI::CollectQuestRewardSpells(
                quest->GetRewSpell(), quest->GetRewSpellCast(), rewardSpells);

            for (uint32 spellId : rewardSpells)
            {
                SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(spellId);
                if (!pSpellEntry || pSpellEntry->SpellName[0].find(spellName) != std::string::npos)
                    continue;

                out << "\n        " << spellName << " is the reward of quest " << quest->GetQuestId()
                    << " \"" << quest->GetTitle() << "\", which asks for minimum level " << quest->GetMinLevel()
                    << ", class mask 0x" << std::hex << quest->GetRequiredClasses() << std::dec
                    << ", race mask 0x" << std::hex << quest->GetRequiredRaces() << std::dec;
                found = true;
            }
        }

        if (!found)
            out << "\n        no quest of this world gives " << spellName;
    }
}

// `.partybot stance'. Whether a warrior party bot wears a stance follows from what it knows, what it wears and the
// talent that its gear is rated for, and none of that can be read from the chat window: a bot that stands in the
// stance of its charge is asked to change, but which gate refuses the change, and whether the bot knows the stance
// at all, is not written anywhere that a normal session shows.
void PartyBotAI::PrintWarriorStanceState(ChatHandler& handler) const
{
    if (me->GetClass() != CLASS_WARRIOR)
    {
        handler.SendSysMessage("The selected party bot is not a warrior.");
        return;
    }

    bool const protectionTank = IsProtectionWarriorTank();
    SpellEntry const* pDesired = protectionTank ?
        m_spells.warrior.pDefensiveStance : GetWarriorHomeStanceSpell();
    ShapeshiftForm const desiredForm = GetStanceSpellForm(pDesired);
    Item const* pOffhand = me->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

    std::ostringstream out;
    out << "Stance state of " << me->GetName()
        << "\n    rules: " << (protectionTank ? "shield tank" : "warrior damage")
        << ", role of the bot: " << (m_role == ROLE_TANK ? "tank" : "not tank")
        << ", off hand wears: " << (pOffhand ? pOffhand->GetProto()->Name1 : "nothing")
        << ", gear is rated for: " << GetBotGearSpecName(GetBotGearSpec(me, m_role, m_requestedTalentPage));

    out << "\n    asked talent page: ";
    if (m_requestedTalentPage < MAX_GEAR_SPEC_TABS)
        out << int(m_requestedTalentPage);
    else
        out << "none";

    out << "\n    worn form: " << GetWarriorStanceFormName(me->GetShapeshiftForm())
        << " (" << uint32(me->GetShapeshiftForm()) << ")"
        << ", Tactical Mastery rank " << GetTacticalMasteryRank()
        << ", rage " << me->GetPower(POWER_RAGE) / PB_RAGE_PER_POINT;

    struct
    {
        char const* name;
        SpellEntry const* pSpell;
    } const stances[] = {
        { "Battle Stance", m_spells.warrior.pBattleStance },
        { "Defensive Stance", m_spells.warrior.pDefensiveStance },
        { "Berserker Stance", m_spells.warrior.pBerserkerStance },
    };

    // The abilities that the bot does not know are looked for in the quests of this world below, which is where an
    // ability that no trainer sells comes from.
    std::vector<char const*> missingClassQuestSpells;

    for (auto const& stance : stances)
    {
        out << "\n    " << stance.name << ": ";
        if (stance.pSpell)
            out << "known (" << stance.pSpell->Id << ")";
        else
        {
            out << "not known by this bot";
            missingClassQuestSpells.push_back(stance.name);
        }
    }

    // Intercept is asked by the rules of a shield tank and by the rotation of a damage warrior, and it reaches a
    // warrior of this world from the same source as the stances: the reward of a class quest rather than the list
    // of a trainer. A warrior that has reached the level of that quest and does not know the spell is therefore
    // asked here the same question as a warrior without a stance, because the answer is the same row of data.
    out << "\n    Intercept: ";
    if (m_spells.warrior.pIntercept)
        out << "known (" << m_spells.warrior.pIntercept->Id << ")";
    else
    {
        out << "not known by this bot";
        missingClassQuestSpells.push_back("Intercept");
    }

    AppendClassQuestRewardQuests(out, me, missingClassQuestSpells);

    // The two facts that the stance of a damage warrior is made of: the rotation keeps the Berserker Stance while
    // the bot has one and not the other, and a bot that changed the stance it keeps in the middle of its levels
    // has taken or not taken one talent. Nothing else about the bot says so, and neither is saved with it.
    if (!protectionTank)
    {
        out << "\n    Bloodthirst: " << (m_spells.warrior.pBloodthirst ? "learned" : "not learned")
            << ", Improved Overpower: " << (m_spells.warrior.pImprovedOverpower ? "taken" : "not taken");
    }

    out << "\n    the rules ask for: " << GetWarriorStanceFormName(desiredForm);
    if (!pDesired)
    {
        out << ", which the bot has not learned";
    }
    else
    {
        out << " (" << pDesired->Id << "):"
            << " cooldown " << (me->IsSpellReady(pDesired) ? "clear" : "running")
            << ", global cooldown " << (me->HasGCD(pDesired) ? "running" : "clear")
            << ", cost " << Spell::CalculatePowerCost(pDesired, me) / PB_RAGE_PER_POINT
            << " rage";
    }

    out << "\n    last stance asked: ";
    if (!m_warriorStanceAskedId)
    {
        out << "none since this bot entered the world";
    }
    else
    {
        SpellEntry const* pAsked = sSpellMgr.GetSpellEntry(m_warriorStanceAskedId);
        out << (pAsked ? pAsked->SpellName[0].c_str() : "unknown spell") << " (" << m_warriorStanceAskedId
            << ") blocked by " << GetWarriorStanceBlockName(m_warriorStanceBlock);

        if (m_warriorStanceNormalResult != uint32(-1))
            out << ", normal cast result " << m_warriorStanceNormalResult;
        if (m_warriorStanceTriggeredResult != uint32(-1))
            out << ", triggered cast result " << m_warriorStanceTriggeredResult;
    }

    // An opening of a charge owns the stance of the bot while it stands, and a shield tank that stands in the
    // Battle Stance out of combat is in it because of that ask and not because of its rules. Which stance was last
    // asked and why it did not come is read above; whether an opening still stands is what this line answers.
    out << "\n    charge opening: ";
    if (m_warriorChargeTargetGuid.IsEmpty())
        out << "none is standing";
    else
    {
        Unit const* pChargeTarget = me->GetMap() ? me->GetMap()->GetUnit(m_warriorChargeTargetGuid) : nullptr;
        if (pChargeTarget)
            out << "standing for " << pChargeTarget->GetName();
        else
            out << "standing for a target that is gone";

        out << ", " << int(m_warriorChargeAskRetries) << " of " << PB_WARRIOR_CHARGE_ASK_UPDATES
            << " updates spent on the stance and its global cooldown";
    }

    handler.SendSysMessage(out.str().c_str());
}

// The ordinary spell gate refuses a second aura-only cast while the target still
// carries it. A forced Sunder is an instruction to spend one cast on this target,
// which is also how the next stack is added, so that gate is skipped for this one.
bool PartyBotAI::TryCastForcedWarriorSunder(Unit* pTarget)
{
    SpellEntry const* pSunder = m_spells.warrior.pSunderArmor;
    if (!pTarget ||
        !pSunder ||
        !me->IsSpellReady(pSunder) ||
        me->HasGCD(pSunder) ||
        me->GetPower(POWER_RAGE) < Spell::CalculatePowerCost(pSunder, me) ||
        pSunder->GetErrorAtShapeshiftedCast(me->GetShapeshiftForm()) != SPELL_CAST_OK ||
        pTarget->IsImmuneToSpell(pSunder, false) ||
        !IsTargetInSpellRange(pTarget, pSunder) ||
        !me->IsWithinLOSInMap(pTarget))
        return false;

    if (DoCastSpell(pTarget, pSunder) != SPELL_CAST_OK)
        return false;

    return true;
}

Unit* PartyBotAI::SelectProtectionWarriorSecondaryTarget(Unit const* pPrimaryTarget) const
{
    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return nullptr;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (!pMember || pMember == me)
            continue;

        for (Unit* pAttacker : pMember->GetAttackers())
        {
            if (pAttacker &&
                pAttacker != pPrimaryTarget &&
                pAttacker->GetVictim() == pMember &&
                IsValidHostileTarget(pAttacker) &&
                me->IsWithinDist(pAttacker, 50.0f))
                return pAttacker;
        }
    }

    return nullptr;
}

// Whether the emergency grab of the shield tank is in hand: learned, off its cooldown, and paid for. The tank is
// asked to hold a little rage above its cost, because the grab is worth a stance change, and a cast that the tank
// cannot pay for is no help to the party member that waits for it.
bool PartyBotAI::HasWarriorMockingBlowReady() const
{
    return m_spells.warrior.pMockingBlow &&
        me->IsSpellReady(m_spells.warrior.pMockingBlow) &&
        me->GetPower(POWER_RAGE) > 10 * PB_RAGE_PER_POINT;
}

// Whether the mob the tank is attacking is asking for that grab. Taunt is the tool of an ordinary grab, so the
// emergency is only open while Taunt is on its cooldown, and the grab is only spent once the mob is really at the
// party member: `Unit::CanReachWithMeleeAutoAttack` is the reach test that the core puts in front of its own auto
// attack, so a mob that is still running across the room is not reached, and the tank walks to that mob and takes
// it with its rotation instead of with the one spell that a long cooldown allows. Either side of that reach is
// asked for, because a mob that beats a party member from the far side of a ledge is reached by the member who
// swings at it.
bool PartyBotAI::IsWarriorMockingBlowWanted(Unit const* pTarget) const
{
    if (!HasWarriorMockingBlowReady())
        return false;

    Unit const* pMember = pTarget ? pTarget->GetVictim() : nullptr;
    if (!IsOtherPartyMember(pMember))
        return false;

    if (m_spells.warrior.pTaunt && me->IsSpellReady(m_spells.warrior.pTaunt))
        return false;

    return pTarget->CanReachWithMeleeAutoAttack(pMember) || pMember->CanReachWithMeleeAutoAttack(pTarget);
}

// Whether the shield block is asked for the mob that the tank holds. The block blunts the next physical blows
// landed on the tank, so it is asked for a mob that hurts the party with its body and not with a bar: what a mob
// mainly does is asked of its own class, the same reading that the battle bot warrior makes before its Shield
// Block, and a caster mob that walks in and swings once in a while is not what a minute of a cooldown and ten of
// the rage are spent on. The rage of the block is the rage that Revenge, Shield Slam and Sunder Armor wait for,
// and against a caster those three do more for the hold than a block that nothing lands on.
bool PartyBotAI::IsWarriorShieldBlockWanted(Unit const* pVictim) const
{
    // The block is an answer to being hit: a tank that nothing is swinging at keeps its rage and its cooldown.
    if (!pVictim || me->GetAttackers().empty())
        return false;

    return IsPhysicalDamageClass(pVictim->GetClass());
}

bool PartyBotAI::TryTauntProtectionWarriorTarget(Unit* pTarget)
{
    if (!pTarget || !IsOtherPartyMember(pTarget->GetVictim()))
        return false;

    if (m_spells.warrior.pTaunt && CanTryToCastSpell(pTarget, m_spells.warrior.pTaunt))
    {
        if (DoCastSpell(pTarget, m_spells.warrior.pTaunt) == SPELL_CAST_OK)
            return true;
    }

    // Taunt being ready but out of range is not the cooldown case: the ordinary
    // chase takes the bot into range, where the next update casts Taunt.
    if (m_spells.warrior.pTaunt && me->IsSpellReady(m_spells.warrior.pTaunt))
        return false;

    // Mocking Blow is not cast here. It belongs to Battle Stance, and the stance of a shield tank is decided by
    // PartyBotAI::UpdateProtectionWarriorTankCombat() in combat, the warrior routine out of it. An ask from this
    // fallback took the Defensive Stance away on one update and asked it back on the next, both updates stopped
    // at the stance, and the rotation of the tank - Revenge, Shield Block, Shield Slam, Sunder Armor - was never
    // reached while the fallback answered for a mob that beats another party member. The grab is asked for in that
    // rotation instead, at PartyBotAI::IsWarriorMockingBlowWanted, where the switch that the spell needs is the
    // switch that the rotation already knows how to make and to take back.
    bool const canMock = HasWarriorMockingBlowReady();

    // A target out of melee reach is chased, but the answer stays false: the caller then runs the rotation of the
    // tank, which follows the same target at its end and is the place that decides the stance of the bot. Stopping
    // the update here was what kept a shield tank in the stance of its charge.
    if (canMock &&
        !me->CanReachWithMeleeAutoAttack(pTarget) &&
        me->GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
        BeginChasing(pTarget);

    return false;
}

bool PartyBotAI::UpdateProtectionWarriorTankTarget()
{
    Unit* pCurrentTarget = me->GetVictim();

    Unit* pSecondaryTarget = m_warriorSecondaryTargetGuid.IsEmpty() ? nullptr :
        me->GetMap()->GetUnit(m_warriorSecondaryTargetGuid);
    if (pSecondaryTarget && !IsValidHostileTarget(pSecondaryTarget))
        pSecondaryTarget = nullptr;

    if (!pSecondaryTarget && !m_warriorSecondaryTargetGuid.IsEmpty())
    {
        m_warriorSecondaryTargetGuid.Clear();
        m_warriorSecondaryHoldTimer.Reset(0);
    }

    if (pSecondaryTarget)
    {
        // The hold of the tank on that mob is a length of time and not the sight of a spell: whether one mob keeps
        // beating the party member after a cast is not known here, and the tank is asked to be on the mob long
        // enough for its rotation and its taunts to answer. The time goes by only in an update in which a swing of
        // the bot could really land on the mob - `Unit::CanAutoAttackTarget` is the gate that the core puts in
        // front of its own auto attack, and it says that no control holds the bot, that both are alive, that the
        // mob is within melee reach and that the bot stands facing it - so a run to a mob that stands away, a stun
        // or a fear of the bot, and a mob that walks out of reach spend nothing of the hold.
        if (m_warriorSecondaryHoldTimer.Passed())
        {
            m_warriorSecondaryTargetGuid.Clear();
            m_warriorSecondaryHoldTimer.Reset(0);

            Unit* pPrimaryTarget = m_warriorPrimaryTargetGuid.IsEmpty() ? nullptr :
                me->GetMap()->GetUnit(m_warriorPrimaryTargetGuid);
            if (pPrimaryTarget && IsValidHostileTarget(pPrimaryTarget))
            {
                if (pCurrentTarget && pCurrentTarget != pPrimaryTarget)
                    me->AttackStop(true);
                AttackStart(pPrimaryTarget);
                return false;
            }

            m_warriorPrimaryTargetGuid.Clear();
        }
        else
        {
            if (pCurrentTarget != pSecondaryTarget)
            {
                if (pCurrentTarget)
                    me->AttackStop(true);
                AttackStart(pSecondaryTarget);
            }

            // The state machine runs once per PB_UPDATE_INTERVAL, so an update that the gate of the core lets
            // through is a full step of the hold. The rotation of the tank runs on this mob as on any other
            // target, and that rotation is what the hold is spent on.
            if (me->CanAutoAttackTarget(pSecondaryTarget) == ATTACK_RESULT_OK)
                m_warriorSecondaryHoldTimer.Update(PB_UPDATE_INTERVAL);

            return TryTauntProtectionWarriorTarget(pSecondaryTarget);
        }
    }

    pCurrentTarget = me->GetVictim();
    Unit* pPrimaryTarget = m_warriorPrimaryTargetGuid.IsEmpty() ? nullptr :
        me->GetMap()->GetUnit(m_warriorPrimaryTargetGuid);
    if (pPrimaryTarget && !IsValidHostileTarget(pPrimaryTarget))
        pPrimaryTarget = nullptr;

    if (!pPrimaryTarget)
    {
        m_warriorPrimaryTargetGuid.Clear();

        if (!pCurrentTarget || !IsValidHostileTarget(pCurrentTarget))
            return false;

        pPrimaryTarget = pCurrentTarget;
        m_warriorPrimaryTargetGuid = pPrimaryTarget->GetObjectGuid();
    }
    else if (pCurrentTarget != pPrimaryTarget)
    {
        if (pCurrentTarget)
            me->AttackStop(true);
        if (!AttackStart(pPrimaryTarget))
            return false;
        pCurrentTarget = pPrimaryTarget;
    }

    // The current primary target is still the tank's responsibility. Handle its
    // threat before looking at another mob that has pulled one of the party.
    if (IsOtherPartyMember(pPrimaryTarget->GetVictim()))
        return TryTauntProtectionWarriorTarget(pPrimaryTarget);

    Unit* pNextSecondaryTarget = SelectProtectionWarriorSecondaryTarget(pPrimaryTarget);
    if (!pNextSecondaryTarget)
        return false;

    m_warriorSecondaryTargetGuid = pNextSecondaryTarget->GetObjectGuid();
    m_warriorSecondaryHoldTimer.Reset(PB_WARRIOR_SECONDARY_HOLD_MS);
    if (pCurrentTarget != pNextSecondaryTarget)
    {
        if (pCurrentTarget)
            me->AttackStop(true);
        AttackStart(pNextSecondaryTarget);
    }

    return TryTauntProtectionWarriorTarget(pNextSecondaryTarget);
}

bool PartyBotAI::IsTargetedByHostile() const
{
    for (Unit* pAttacker : me->GetAttackers())
        if (pAttacker && pAttacker->IsAlive() && pAttacker->GetVictim() == me && pAttacker->IsHostileTo(me))
            return true;

    return false;
}

Unit* PartyBotAI::SelectDispelTargetWithPet(SpellEntry const* pSpellEntry) const
{
    if (Unit* pTarget = SelectDispelTarget(pSpellEntry))
        return pTarget;

    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return nullptr;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (!pMember)
            continue;

        Pet* pPet = pMember->GetPet();
        if (pPet &&
            me->IsValidHelpfulTarget(pPet) &&
            IsValidDispelTarget(pPet, pSpellEntry) &&
            me->IsWithinLOSInMap(pPet) &&
            me->IsWithinDist(pPet, 30.0f))
            return pPet;
    }

    return nullptr;
}

SpellEntry const* PartyBotAI::SelectPrayerOfHealing() const
{
    for (SpellEntry const* pSpellEntry : m_spellListDirectHeal)
        if (pSpellEntry->SpellName[0].find("Prayer of Healing") != std::string::npos)
            return pSpellEntry;

    return nullptr;
}

Player* PartyBotAI::SelectPrayerOfHealingTarget(uint8& memberCount) const
{
    memberCount = 0;
    Player* pFirstTarget = nullptr;

    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return nullptr;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (!pMember ||
            pMember == me ||
            pMember->GetHealthPercent() >= 70.0f ||
            !me->IsValidHelpfulTarget(pMember) ||
            !me->IsWithinLOSInMap(pMember) ||
            !me->IsWithinDist(pMember, 30.0f))
            continue;

        if (!pFirstTarget)
            pFirstTarget = pMember;
        ++memberCount;
    }

    return pFirstTarget;
}

bool PartyBotAI::GroupHealthAbove(float healthPercent) const
{
    if (me->GetHealthPercent() < healthPercent)
        return false;

    Group* pGroup = me->GetGroup();
    if (!pGroup)
        return true;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pMember = itr->getSource();
        if (pMember && pMember != me)
        {
            if (!pMember->IsAlive() || pMember->GetHealthPercent() < healthPercent)
                return false;
        }
    }

    return true;
}

Unit* PartyBotAI::SelectAttackTarget(Player* pLeader) const
{
    if (IsInDuel())
    {
        if (me->m_duel->opponent && IsValidHostileTarget(me->m_duel->opponent))
            return me->m_duel->opponent;
    }
    else
    {
        bool const canOpenWithCharge = CanOpenWarriorCombatWithCharge();
        bool const canTakeFocusedTarget = canOpenWithCharge || IsProtectionWarriorTank();

        // Stick to marked target in combat. A shield tank also answers a focused target out of combat without
        // waiting for Charge, since that is the target that its party has already chosen to fight.
        if (me->IsInCombat() || pLeader->GetVictim() || canTakeFocusedTarget)
        {
            for (auto markId : m_marksToFocus)
            {
                ObjectGuid targetGuid = me->GetGroup()->GetTargetWithIcon(markId);
                if (targetGuid.IsUnit())
                    if (Unit* pVictim = me->GetMap()->GetUnit(targetGuid))
                        if (IsValidHostileTarget(pVictim))
                            return pVictim;
            }
        }

        // A leader that only selects a target has not entered combat yet. A warrior with Charge, and any shield
        // tank, may still open on that selection.
        if (canTakeFocusedTarget && pLeader->GetTargetGuid().IsUnit())
        {
            if (Unit* pVictim = me->GetMap()->GetUnit(pLeader->GetTargetGuid()))
                if (IsValidHostileTarget(pVictim))
                    return pVictim;
        }

        // Who is the leader attacking.
        if (Unit* pVictim = pLeader->GetVictim())
        {
            if (IsValidHostileTarget(pVictim))
                return pVictim;
        }
    }

    // Who is attacking me.
    for (const auto pAttacker : me->GetAttackers())
    {
        if (IsValidHostileTarget(pAttacker))
            return pAttacker;
    }

    if (!IsInDuel())
    {
        // Assist the target of another fighting group member.
        if (Unit* pPartyAttacker = SelectPartyAttackTarget())
            return pPartyAttacker;
    }

    // Assist pet if its in combat.
    if (Pet* pPet = me->GetPet())
    {
        if (Unit* pPetAttacker = pPet->GetAttackerForHelper())
            if (IsValidHostileTarget(pPetAttacker))
                return pPetAttacker;
    }

    if (Unit* pControlledTarget = SelectOnlyControlledHostileTarget())
        return pControlledTarget;

    return nullptr;
}

Unit* PartyBotAI::SelectPartyAttackTarget() const
{
    Group* pGroup = me->GetGroup();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            // We already checked self.
            if (pMember == me)
                continue;

            // A member can have a target before its first swing has set the in-combat flag.
            if (Unit* pVictim = pMember->GetVictim())
            {
                if (IsValidHostileTarget(pVictim) &&
                    me->IsWithinDist(pVictim, 50.0f))
                    return pVictim;
            }

            for (const auto pAttacker : pMember->GetAttackers())
            {
                if (IsValidHostileTarget(pAttacker) &&
                    me->IsWithinDist(pAttacker, 50.0f))
                    return pAttacker;
            }
        }
    }

    return nullptr;
}

Player* PartyBotAI::SelectResurrectionTarget() const
{
    if (IsInDuel())
        return nullptr;

    Group* pGroup = me->GetGroup();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            // Can't resurrect self.
            if (pMember == me)
                continue;

            if (pMember->GetDeathState() != CORPSE)
                continue;

            if (!me->IsWithinLOSInMap(pMember))
                continue;

            if (m_resurrectionSpell->IsTargetInRange(me, pMember))
                return pMember;
        }
    }

    return nullptr;
}

Player* PartyBotAI::SelectShieldTarget() const
{
    if (IsInDuel())
        return nullptr;

    Group* pGroup = me->GetGroup();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            // We already checked self.
            if (pMember == me)
                continue;

            if ((pMember->GetHealthPercent() < 90.0f) &&
                !pMember->GetAttackers().empty() &&
                !pMember->IsImmuneToMechanic(MECHANIC_SHIELD) &&
                !(m_role == ROLE_HEALER &&
                  IsTankRoleWarrior(pMember) &&
                  pMember->GetHealthPercent() > 60.0f))
                return pMember;
        }
    }

    return nullptr;
}

bool PartyBotAI::CrowdControlMarkedTargets()
{
    SpellEntry const* pSpellEntry = GetCrowdControlSpell();
    if (!pSpellEntry)
        return false;

    for (auto mark : m_marksToCC)
    {
        if (Unit* pTarget = GetMarkedTarget(mark))
        {
            if (!pTarget->HasUnitState(UNIT_STATE_CAN_NOT_REACT_OR_LOST_CONTROL) &&
                IsValidHostileTarget(pTarget) && !AreOthersOnSameTarget(pTarget->GetObjectGuid()))
            {
                if (CanTryToCastSpell(pTarget, pSpellEntry))
                {
                    if (DoCastSpell(pTarget, pSpellEntry) == SPELL_CAST_OK)
                    {
                        me->ClearUnitState(UNIT_STATE_MELEE_ATTACKING);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void PartyBotAI::AddToPlayerGroup()
{
    Player* pPlayer = ObjectAccessor::FindPlayer(m_leaderGuid);
    if (!pPlayer)
        return;

    Group* group = pPlayer->GetGroup();
    if (!group)
    {
        group = new Group;
        // new group: if can't add then delete
        if (!group->Create(pPlayer->GetObjectGuid(), pPlayer->GetName()))
        {
            delete group;
            return;
        }
        sObjectMgr.AddGroup(group);
    }

    if (me->GetGroup() != group)
    {
        if (me->GetGroup())
            me->RemoveFromGroup();

        group->AddMember(me->GetObjectGuid(), me->GetName());
    }
}

void PartyBotAI::OnPacketReceived(WorldPacket const* packet)
{
    //printf("Bot received %s\n", LookupOpcodeName(packet->GetOpcode()));
    switch (packet->GetOpcode())
    {
        case SMSG_LEARNED_SPELL:
        case SMSG_SUPERCEDED_SPELL:
        case SMSG_REMOVED_SPELL:
        {
            if (m_initialized)
                m_resetSpellData = true;
            return;
        }
        case SMSG_DUEL_REQUESTED:
        {
            auto data = std::make_unique<WorldPackets::Duel::DuelAccepted>();
            data->playerGuid = me->GetObjectGuid();
            me->GetSession()->QueuePacket(std::move(data));
            return;
        }
        case SMSG_PARTYKILLLOG:
        {
            if (!me)
                return;

            if (Group const* pGroup = me->GetGroup())
            {
                if (pGroup->GetLootMethod() == ROUND_ROBIN ||
                    pGroup->GetLootMethod() == GROUP_LOOT ||
                    pGroup->GetLootMethod() == NEED_BEFORE_GREED)
                {
                    ObjectGuid victimGuid = *(((uint64*)(*packet).contents()) + 1);
                    if (Creature* pCreature = me->GetMap()->GetCreature(victimGuid))
                    {
                        pCreature->m_Events.AddLambdaEventAtOffset([pCreature, guid = me->GetGUID()]()
                        {
                            if (pCreature->loot.roundRobinPlayer == guid)
                            {
                                // unassign loot from bot so real players can loot
                                pCreature->loot.roundRobinPlayer = 0;
                                pCreature->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
                            }
                        }, 1);
                    }
                }
            }
            
            return;
        }
    }

    CombatBotBaseAI::OnPacketReceived(packet);
}

void PartyBotAI::OnPlayerLogin()
{
    if (!m_initialized)
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SPAWNING);
}

void PartyBotAI::MovementInform(uint32 movementType, uint32 /*data*/)
{
    if (movementType != DISTANCING_MOTION_TYPE)
        return;

    bool const casterFinishedKiting =
        me->GetClass() == CLASS_MAGE ||
        (me->GetClass() == CLASS_PRIEST && GetRole() == ROLE_RANGE_DPS);
    if (casterFinishedKiting && me->IsInCombat() && me->GetVictim())
        m_updateTimer.Reset(0);
}

void PartyBotAI::UpdateAI(uint32 const diff)
{
    m_updateTimer.Update(diff);
    if (m_updateTimer.Passed())
        m_updateTimer.Reset(PB_UPDATE_INTERVAL);
    else
        return;

    if (!me->IsInWorld() || me->IsBeingTeleported())
        return;

    if (!m_initialized)
    {
        AddToPlayerGroup();

        if (m_race && m_class) // temporary character
        {
            if (m_level && m_level != me->GetLevel())
            {
                me->GiveLevel(m_level);
                me->InitTalentForLevel();
                me->SetUInt32Value(PLAYER_XP, 0);
            }

            if (!m_cloneGuid.IsEmpty())
            {
                CloneFromPlayer(sObjectAccessor.FindPlayer(m_cloneGuid));
                AutoAssignRole();
            }
            else
            {
                LearnPremadeSpecForClass();

                if (m_role == ROLE_INVALID)
                    AutoAssignRole();

                AutoEquipGear(sWorld.getConfig(CONFIG_UINT32_PARTY_BOT_AUTO_EQUIP));

                // fix client bug causing some item slots to not be visible
                if (Player* pLeader = GetPartyLeader())
                {
                    me->SetVisibility(VISIBILITY_OFF);
                    pLeader->UpdateVisibilityOf(pLeader, me);
                    me->SetVisibility(VISIBILITY_ON);
                }
            }
            me->UpdateSkillsToMaxSkillsForLevel();
        }
        else // loaded from db
        {
            if (m_role == ROLE_INVALID)
                AutoAssignRole();

            // The right to wear a weapon in the other hand is not saved with the character, so the bot that was
            // dressed with two weapons must take it back from the spell that it learned before it fights again.
            PrepareDualWield();

            // The gear that a bot wears comes back from the database, and a bot that was dressed before the
            // property of an item was put on it before the item was equipped wears items with no property at all.
            // Such an item shows no suffix and no attributes to the one who looks at the bot, and it gives the bot
            // none of what the rating of it counted, so the gear is taken off and the slots are dressed again.
            RepairRandomGear(sWorld.getConfig(CONFIG_UINT32_PARTY_BOT_AUTO_EQUIP));

            if (me->IsGameMaster())
                me->SetGameMaster(false);

            me->TeleportTo(m_mapId, m_x, m_y, m_z, m_o);
        }

        // A new bot and a bot loaded from the database take every class ability of their level from the same four
        // sources: the trainer, quest rewards, the build of their talent and the usable skill books.
        LearnSpellsForLevel();

        // The riding skill and the mount of the bot's own race or class, for a bot created at the level that asks
        // for them and for a bot that was saved as a rider already: what the bot knows is left as it is.
        LearnRidingAndMounts();

        ResetSpellData();
        PopulateSpellData();
        AddAllSpellReagents();
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SPAWNING);
        SummonPetIfNeeded();
        me->SetHealthPercent(100.0f);

        // A player enters the world without rage. The other power types start full for a summoned bot.
        if (me->GetClass() == CLASS_WARRIOR)
            me->SetPower(POWER_RAGE, 0);
        else
            me->SetPowerPercent(me->GetPowerType(), 100.0f);

        uint32 newzone, newarea;
        me->GetZoneAndAreaId(newzone, newarea);
        me->UpdateZone(newzone, newarea);

        m_initialized = true;
        return;
    }

    if (m_resetSpellData)
    {
        ResetSpellData();
        PopulateSpellData();
        m_resetSpellData = false;
    }

    Player* pLeader = GetPartyLeader();
    if (!pLeader)
    {
        botEntry->requestRemoval = true;
        return;
    }

    if (!pLeader->IsInWorld())
        return;

    if (me->GetClass() == CLASS_PRIEST && m_spells.priest.pPrayerofFortitude)
        PriestFortitudeBuffDelayPassed();

    if (pLeader->InBattleGround() &&
        !me->InBattleGround())
    {
        if (m_receivedBgInvite)
        {
            SendBattlefieldPortPacket();
            m_receivedBgInvite = false;
            return;
        }

        // Remain idle until we can join battleground.
        return;
    }

    if (pLeader->IsTaxiFlying())
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
        {
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }
        return;
    }

    if (me->HasUnitState(UNIT_STATE_FEIGN_DEATH) && me->HasAuraType(SPELL_AURA_FEIGN_DEATH) &&
       !me->IsInCombat() && (!me->GetPet() || !me->GetPet()->IsInCombat()) &&
       !me->SelectRandomUnfriendlyTarget(nullptr, 20.0f, false, true))
        me->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

    if (me->HasUnitState(UNIT_STATE_CAN_NOT_REACT_OR_LOST_CONTROL))
    {
        // A party mage waits the effect out. Ice Block is kept for the low-health branch of its combat routine, and
        // neither it nor Blink is used to leave a control effect.
        if (me->GetClass() != CLASS_MAGE)
            BreakCrowdControlEffects();
        return;
    }

    if (me->IsDead())
    {
        if (me->InBattleGround())
        {
            if (me->GetDeathState() == CORPSE)
            {
                me->BuildPlayerRepop();
                me->RepopAtGraveyard();
            }
        }
        // A bot that is not in a battleground comes back on its own as soon as a player stands by its corpse,
        // unless PartyBot.SpiritResurrection asks for the way a player comes back instead.
        else if (sWorld.getConfig(CONFIG_BOOL_PARTY_BOT_SPIRIT_RESURRECTION))
            UpdateSpiritResurrection();
        else
        {
            if (ShouldAutoRevive())
            {
                me->ResurrectPlayer(0.5f);
                me->SpawnCorpseBones();
                me->CastSpell(me, PB_SPELL_HONORLESS_TARGET, true);
            }
        }

        return;
    }

    if (me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
    {
        // Stop auto shot if no target.
        if (!me->GetVictim())
        {
            me->InterruptSpell(CURRENT_AUTOREPEAT_SPELL, true);
            return;
        }
        else if (me->GetClass() == CLASS_HUNTER)
        {
            if (me->GetCombatDistance(me->GetVictim()) < 8.0f)
                me->InterruptSpell(CURRENT_AUTOREPEAT_SPELL, true);
            else
                UpdateInCombatAI_Hunter();
            return;
        }
        else if (me->GetClass() == CLASS_PRIEST &&
                 GetRole() == ROLE_RANGE_DPS &&
                 m_spells.priest.pMindFlay &&
                 IsTargetInSpellRange(me->GetVictim(), m_spells.priest.pMindFlay) &&
                 CanTryToCastSpell(me->GetVictim(), m_spells.priest.pMindFlay))
        {
            // A wand is only a filler for a shadow priest. Let the class routine take over as soon as Mind Flay can.
            me->InterruptSpell(CURRENT_AUTOREPEAT_SPELL, true);
        }
        else
            return;
    }

    if (Spell* pCurrentSpell = me->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        // Interrupt pre casted heals if target is not injured.
        if (pCurrentSpell->getState() == SPELL_STATE_PREPARING &&
            pCurrentSpell->m_spellInfo->IsHealSpell())
        {
            if (Unit* pTarget = pCurrentSpell->m_targets.getUnitTarget())
            {
                if (pTarget->GetHealth() == pTarget->GetMaxHealth())
                {
                    me->InterruptSpell(CURRENT_GENERIC_SPELL, true);
                }
            }
        }
    }

    if (me->IsNonMeleeSpellCasted(false, false, true))
        return;

    if (me->GetClass() == CLASS_WARRIOR && TryContinueWarriorCharge())
        return;

    if (EnsureMageIceBarrier())
        return;

    if (me->GetClass() == CLASS_PRIEST &&
        GetRole() == ROLE_HEALER &&
        me->GetShapeshiftForm() == FORM_SHADOW)
        me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

    if (me->GetTargetGuid() == me->GetObjectGuid())
        me->ClearTarget();

    if (!me->IsInCombat())
    {
        if (DrinkAndEat())
        {
            if (!me->IsWithinDistInMap(pLeader, 100.0f))
            {
                me->SetHealth(me->GetMaxHealth());
                if (me->GetPowerType() == POWER_MANA)
                    me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA));
            }
            else if (me->IsMounted())
                me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
            return;
        }

        // Teleport to leader if too far away.
        if (!me->IsWithinDistInMap(pLeader, 100.0f) && !IsInDuel())
        {
            if (!me->IsStopped())
                me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
            char name[128] = {};
            snprintf(name, sizeof(name), "%s", pLeader->GetName());
            ChatHandler(me).HandleGonameCommand(name);
            return;
        }
    }

    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);

    if (me->GetSheath() == SHEATH_STATE_UNARMED && !me->IsMounted())
        me->SetSheath(SHEATH_STATE_MELEE);

    if (!me->IsInCombat() && !me->IsMounted() && !GroupInCombat())
    {
        UpdateOutOfCombatAI();

        if (m_isBuffing)
            return;

        if (me->IsNonMeleeSpellCasted())
            return;
    }

    if (m_rogueVanishRetreat)
    {
        if (me->GetHealthPercent() >= 40.0f)
            m_rogueVanishRetreat = false;
        else
        {
            if (me->GetVictim())
                me->AttackStop();
            return;
        }
    }

    if (me->GetClass() == CLASS_ROGUE && !m_rogueStealthTargetGuid.IsEmpty())
    {
        // A party member may put the rogue into combat before the stealth approach reaches its target. The rogue
        // routine must still run, or the pending opener can wait forever outside the generic target handling.
        UpdateInCombatAI_Rogue();
        return;
    }

    Unit* pVictim = me->GetVictim();
    bool const groupAllMounted = GroupAllMounted();
    bool const groupFightingOnFoot = GroupHasFightingMemberOnFoot();
    bool const blockMountedGroupCombat =
        groupAllMounted ||
        (GroupHasFightingMemberMounted() && !groupFightingOnFoot);

    if (blockMountedGroupCombat && pVictim)
    {
        me->AttackStop();
        pVictim = nullptr;
        if (me->GetClass() == CLASS_PRIEST)
            m_priestMindBlastOpener = false;
    }

    if (!blockMountedGroupCombat && GetRole() != ROLE_HEALER)
    {
        if (!pVictim || !IsValidHostileTargetForAttack(pVictim))
        {
            if (pVictim)
            {
                me->AttackStop();
                if (me->GetClass() == CLASS_PRIEST)
                    m_priestMindBlastOpener = false;
            }

            if (Unit* pVictim = SelectAttackTarget(pLeader))
            {
                AttackStart(pVictim);
                return;
            }
        }
    }

    if (!me->IsInCombat())
    {
        // The party and its bots grow together while it plays. A new level can bring trainer ranks, a quest reward,
        // a skill book and a talent point at once, so every source is synchronised here, out of the way of a fight
        // and on the walk. Nothing is asked of a source whose level has not moved, and a bot that is called into the
        // fight before it takes this step gets the abilities on the next step that it takes on foot.
        bool newSpells = LearnSpellsForLevel();

        if (newSpells)
        {
            ResetSpellData();
            PopulateSpellData();
        }

        LearnRidingAndMounts();

        // A party that rides is a party that is travelling, so the bot gets on a mount when the leader does and gets
        // off when the leader gets off. It rides the mount of its own race or class and calls it the way a player
        // does, with the three seconds that a summon of these mounts takes; a bot that has no mount of its own to
        // ride - because it has not reached the level of one, or because it has been robbed of the spell - stays on
        // foot rather than wearing the mount of somebody else.
        if (pLeader->IsMounted() && !me->GetVictim() && !GroupInCombat())
        {
            if (!me->IsMounted() && !me->IsNonMeleeSpellCasted())
            {
                // Leave shapeshift before mounting.
                if (me->IsInDisallowedMountForm() &&
                    me->GetDisplayId() != me->GetNativeDisplayId() &&
                    me->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
                    me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

                SummonMount();
            }
        }
        else if (me->IsMounted())
            me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
    }

    // The leader that has jumped down over an edge is followed over it, and the pet that was sent away for the
    // drop is called back once the bot stands on the ground below again.
    if (!me->IsInCombat() && !pVictim)
        FollowLeaderDownTheDrop(pLeader);

    CallPetBackAfterTheDrop();

    if (!me->IsMoving())
    {
        if (!pVictim)
        {
            if (!me->IsInCombat() &&
                me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
                me->GetMotionMaster()->MoveFollow(pLeader, urand(PB_MIN_FOLLOW_DIST, PB_MAX_FOLLOW_DIST), frand(PB_MIN_FOLLOW_ANGLE, PB_MAX_FOLLOW_ANGLE));
        }
        else
        {
            if (!me->HasUnitState(UNIT_STATE_MELEE_ATTACKING) &&
               (GetRole() == ROLE_MELEE_DPS || m_role == ROLE_TANK) &&
                IsValidHostileTarget(pVictim) &&
                AttackStart(pVictim))
                return;

            switch (me->GetMotionMaster()->GetCurrentMovementGeneratorType())
            {
                case IDLE_MOTION_TYPE:
                case FOLLOW_MOTION_TYPE:
                    BeginChasing(pVictim);
                    break;
            }
        }
    }

    if (!blockMountedGroupCombat &&
        (me->IsInCombat() ||
         me->GetVictim() ||
         (GetRole() == ROLE_HEALER && groupFightingOnFoot)))
        UpdateInCombatAI();
}


void PartyBotAI::UpdateOutOfCombatAI()
{
    if (me->IsMounted())
        return;

    bool const waitInShadowform =
        me->GetClass() == CLASS_PRIEST &&
        GetRole() != ROLE_HEALER &&
        me->GetShapeshiftForm() == FORM_SHADOW &&
        PriestNeedsResurrectionOrDiseaseDispel();

    if (!IsInDuel())
    {
        if (!waitInShadowform && m_resurrectionSpell)
            if (Player* pTarget = SelectResurrectionTarget())
                if (CanTryToCastSpell(pTarget, m_resurrectionSpell))
                    if (DoCastSpell(pTarget, m_resurrectionSpell) == SPELL_CAST_OK)
                        return;

        if (m_role != ROLE_TANK && me->GetVictim() && CrowdControlMarkedTargets())
            return;
    }

    if (CheckForDispelTargets())
        return;

    switch (me->GetClass())
    {
        case CLASS_PALADIN:
            UpdateOutOfCombatAI_Paladin();
            break;
        case CLASS_SHAMAN:
            UpdateOutOfCombatAI_Shaman();
            break;
        case CLASS_HUNTER:
            UpdateOutOfCombatAI_Hunter();
            break;
        case CLASS_MAGE:
            UpdateOutOfCombatAI_Mage();
            break;
        case CLASS_PRIEST:
            UpdateOutOfCombatAI_Priest();
            break;
        case CLASS_WARLOCK:
            UpdateOutOfCombatAI_Warlock();
            break;
        case CLASS_WARRIOR:
            UpdateOutOfCombatAI_Warrior();
            break;
        case CLASS_ROGUE:
            UpdateOutOfCombatAI_Rogue();
            break;
        case CLASS_DRUID:
            UpdateOutOfCombatAI_Druid();
            break;
    }
}

void PartyBotAI::UpdateInCombatAI()
{
    if (me->GetClass() == CLASS_PRIEST &&
        IsTargetedByHostile() &&
        m_spells.priest.pFade &&
        CanTryToCastSpell(me, m_spells.priest.pFade))
    {
        if (DoCastSpell(me, m_spells.priest.pFade) == SPELL_CAST_OK)
            return;
    }

    if (!IsInDuel())
    {
        if (IsProtectionWarriorTank())
        {
            if (UpdateProtectionWarriorTankTarget())
                return;
        }
        else if (m_role == ROLE_TANK)
        {
            Unit* pVictim = me->GetVictim();

            // Defend party members.
            if (!pVictim || pVictim->GetVictim() == me)
            {
                if (pVictim = SelectPartyAttackTarget())
                {
                    me->AttackStop(true);
                    AttackStart(pVictim);
                }
            }

            // Taunt target if its attacking someone else.
            if (pVictim && pVictim->GetVictim() != me)
            {
                for (const auto& pSpellEntry : m_spellListTaunt)
                {
                    if (CanTryToCastSpell(pVictim, pSpellEntry))
                    {
                        if (DoCastSpell(pVictim, pSpellEntry) == SPELL_CAST_OK)
                            return;
                    }
                }
            }
        }
        else if (CrowdControlMarkedTargets())
            return;
    }

    if (CheckForDispelTargets())
        return;

    // Every healing talent uses the shadow priest's retreat: step to eight yards from a melee attacker that is held,
    // then stop there and return to the class healing routine.
    if (m_role == ROLE_HEALER)
    {
        Unit* pMeleeAttacker = SelectNearestMeleeAttacker();
        MovementGeneratorType const movementType =
            me->GetMotionMaster()->GetCurrentMovementGeneratorType();

        if (pMeleeAttacker &&
            movementType != DISTANCING_MOTION_TYPE &&
            !me->HasUnitState(UNIT_STATE_CAN_NOT_MOVE) &&
            CanRunAwayFrom(pMeleeAttacker) &&
            RunAwayFromTarget(pMeleeAttacker))
        {
            me->SetCasterChaseDistance(GetChaseDistance());
            return;
        }

        if (!pMeleeAttacker && movementType == DISTANCING_MOTION_TYPE)
        {
            if (!me->IsStopped())
                me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == DISTANCING_MOTION_TYPE)
            return;
    }

    // The chase generator keeps a distance from the target all by itself, and it is meant for the same thing as
    // the running of the bot: a distance is worth keeping only while the target cannot close it again. So when the
    // target stands next to the bot and nothing holds it, the distance is given back and the bot stands and
    // fights; the class handler below goes on healing, casting, or swinging the melee weapon.
    if (Unit* pVictim = me->GetVictim())
    {
        if (me->HasDistanceCasterMovement() &&
            pVictim->CanReachWithMeleeAutoAttack(me) &&
            !CanRunAwayFrom(pVictim))
            me->SetCasterChaseDistance(0.0f);
    }

    switch (me->GetClass())
    {
        case CLASS_PALADIN:
            UpdateInCombatAI_Paladin();
            break;
        case CLASS_SHAMAN:
            UpdateInCombatAI_Shaman();
            break;
        case CLASS_HUNTER:
            UpdateInCombatAI_Hunter();
            break;
        case CLASS_MAGE:
            UpdateInCombatAI_Mage();
            break;
        case CLASS_PRIEST:
            UpdateInCombatAI_Priest();
            break;
        case CLASS_WARLOCK:
            UpdateInCombatAI_Warlock();
            break;
        case CLASS_WARRIOR:
            UpdateInCombatAI_Warrior();
            break;
        case CLASS_ROGUE:
            UpdateInCombatAI_Rogue();
            break;
        case CLASS_DRUID:
            UpdateInCombatAI_Druid();
            break;
    }

    if (me->GetVictim())
        UseTrinketEffects();
}

Unit* PartyBotAI::SelectHostileDispelTarget(SpellEntry const* pSpellEntry) const
{
    if (!pSpellEntry || GroupAllMounted() || (!me->IsInCombat() && !GroupInCombat()))
        return nullptr;

    if (Unit* pVictim = me->GetVictim())
    {
        if (IsValidDispelTarget(pVictim, pSpellEntry) &&
            CanTryToCastSpell(pVictim, pSpellEntry))
            return pVictim;
    }

    std::list<Unit*> targets;
    me->GetEnemyListInRadiusAround(me, GetSpellMaxRangeForBot(pSpellEntry), targets);
    for (Unit* pTarget : targets)
    {
        if (IsValidHostileTarget(pTarget) &&
            me->IsWithinLOSInMap(pTarget) &&
            IsValidDispelTarget(pTarget, pSpellEntry) &&
            CanTryToCastSpell(pTarget, pSpellEntry))
            return pTarget;
    }

    return nullptr;
}

bool PartyBotAI::CheckForDispelTargets()
{
    bool const priestInShadowform =
        me->GetClass() == CLASS_PRIEST &&
        me->GetShapeshiftForm() == FORM_SHADOW;
    if (me->GetShapeshiftForm() != FORM_NONE && !priestInShadowform)
        return false;

    switch (me->GetClass())
    {
        case CLASS_PALADIN:
        {
            if (m_spells.paladin.pCleanse)
            {
                if (Unit* pFriend = SelectDispelTarget(m_spells.paladin.pCleanse))
                {
                    if (CanTryToCastSpell(pFriend, m_spells.paladin.pCleanse))
                    {
                        if (DoCastSpell(pFriend, m_spells.paladin.pCleanse) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }
            break;
        }
        case CLASS_SHAMAN:
        {
            if (m_spells.shaman.pCureDisease)
            {
                if (Unit* pFriend = SelectDispelTarget(m_spells.shaman.pCureDisease))
                {
                    if (CanTryToCastSpell(pFriend, m_spells.shaman.pCureDisease))
                    {
                        if (DoCastSpell(pFriend, m_spells.shaman.pCureDisease) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }

            if (m_spells.shaman.pCurePoison)
            {
                if (Unit* pFriend = SelectDispelTarget(m_spells.shaman.pCurePoison))
                {
                    if (CanTryToCastSpell(pFriend, m_spells.shaman.pCurePoison))
                    {
                        if (DoCastSpell(pFriend, m_spells.shaman.pCurePoison) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }
            break;
        }
        case CLASS_MAGE:
        {
            if (m_spells.mage.pRemoveLesserCurse)
            {
                if (Unit* pFriend = SelectDispelTarget(m_spells.mage.pRemoveLesserCurse))
                {
                    if (CanTryToCastSpell(pFriend, m_spells.mage.pRemoveLesserCurse))
                    {
                        if (DoCastSpell(pFriend, m_spells.mage.pRemoveLesserCurse) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }
            break;
        }
        case CLASS_PRIEST:
        {
            if (m_spells.priest.pDispelMagic)
            {
                if (Unit* pVictim = SelectHostileDispelTarget(m_spells.priest.pDispelMagic))
                {
                    if (DoCastSpell(pVictim, m_spells.priest.pDispelMagic) == SPELL_CAST_OK)
                        return true;
                }

                Unit* pFriend = m_role == ROLE_HEALER ?
                    SelectDispelTargetWithPet(m_spells.priest.pDispelMagic) :
                    SelectDispelTarget(m_spells.priest.pDispelMagic);
                if (pFriend)
                {
                    if (CanTryToCastSpell(pFriend, m_spells.priest.pDispelMagic))
                    {
                        if (DoCastSpell(pFriend, m_spells.priest.pDispelMagic) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }

            bool const shadowPriestNeedsOnlyMagicDispel =
                me->GetShapeshiftForm() == FORM_SHADOW;
            if (!shadowPriestNeedsOnlyMagicDispel && m_spells.priest.pAbolishDisease)
            {
                Unit* pFriend = m_role == ROLE_HEALER ?
                    SelectDispelTargetWithPet(m_spells.priest.pAbolishDisease) :
                    SelectDispelTarget(m_spells.priest.pAbolishDisease);
                if (pFriend)
                {
                    if (CanTryToCastSpell(pFriend, m_spells.priest.pAbolishDisease))
                    {
                        if (DoCastSpell(pFriend, m_spells.priest.pAbolishDisease) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }
            break;
        }
        case CLASS_DRUID:
        {
            SpellEntry const* pDispelSpell = m_spells.druid.pAbolishPoison ?
                m_spells.druid.pAbolishPoison :
                m_spells.druid.pCurePoison;

            if (pDispelSpell)
            {
                if (Unit* pFriend = SelectDispelTarget(pDispelSpell))
                {
                    if (CanTryToCastSpell(pFriend, pDispelSpell))
                    {
                        if (DoCastSpell(pFriend, pDispelSpell) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }

            if (m_spells.druid.pRemoveCurse)
            {
                if (Unit* pFriend = SelectDispelTarget(m_spells.druid.pRemoveCurse))
                {
                    if (CanTryToCastSpell(pFriend, m_spells.druid.pRemoveCurse))
                    {
                        if (DoCastSpell(pFriend, m_spells.druid.pRemoveCurse) == SPELL_CAST_OK)
                            return true;
                    }
                }
            }
            break;
        }
    }

    return false;
}

void PartyBotAI::UpdateOutOfCombatAI_Paladin()
{
    if (m_spells.paladin.pAura &&
        CanTryToCastSpell(me, m_spells.paladin.pAura))
    {
        if (DoCastSpell(me, m_spells.paladin.pAura) == SPELL_CAST_OK)
            return;
    }

    if (m_role == ROLE_TANK &&
        m_spells.paladin.pRighteousFury &&
        CanTryToCastSpell(me, m_spells.paladin.pRighteousFury))
    {
        if (DoCastSpell(me, m_spells.paladin.pRighteousFury) == SPELL_CAST_OK)
            return;
    }

    // The blessings go one member of the group at a time, which is the reason the bot puts its own attack target
    // down while it stands about the group to put them up.
    if (UpdatePaladinGroupBlessings())
    {
        me->ClearTarget();
        return;
    }

    // The blessings are all that a paladin buffs the group with out of combat, so the global cooldown of the last
    // of them is what says that the bot is still at it.
    if (m_isBuffing &&
        !me->HasGCD(nullptr))
    {
        m_isBuffing = false;
    }

    if (m_role == ROLE_HEALER &&
        FindAndHealInjuredAlly())
        return;
}

void PartyBotAI::UpdateInCombatAI_Paladin()
{
    if (m_spells.paladin.pDivineShield &&
       (me->GetHealthPercent() < 20.0f) &&
       (m_role != ROLE_TANK) &&
        CanTryToCastSpell(me, m_spells.paladin.pDivineShield))
    {
        if (DoCastSpell(me, m_spells.paladin.pDivineShield) == SPELL_CAST_OK)
            return;
    }

    // The blessings of an emergency: the Protection is for the caster of the group that is closest to death and
    // can not stand a physical hit with armour or a pet of its own, and the Lay on Hands is kept for the tank of
    // the group, which is the member that the party leans on. A Sacrifice is not used: the life that it moves
    // onto a member is the life that the paladin then lacks for the healing of the party.
    if (m_spells.paladin.pBlessingOfProtection)
    {
        Player* pCaster = SelectPaladinProtectionTarget();
        if (pCaster && CanTryToCastSpell(pCaster, m_spells.paladin.pBlessingOfProtection))
        {
            if (DoCastSpell(pCaster, m_spells.paladin.pBlessingOfProtection) == SPELL_CAST_OK)
                return;
        }
    }

    if (m_spells.paladin.pLayOnHands)
    {
        Player* pTank = SelectPaladinLayOnHandsTarget();
        if (pTank && CanTryToCastSpell(pTank, m_spells.paladin.pLayOnHands))
        {
            if (DoCastSpell(pTank, m_spells.paladin.pLayOnHands) == SPELL_CAST_OK)
                return;
        }
    }

    if (!me->GetAttackers().empty())
    {
        if (m_spells.paladin.pHolyShield &&
            CanTryToCastSpell(me, m_spells.paladin.pHolyShield))
        {
            if (DoCastSpell(me, m_spells.paladin.pHolyShield) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.paladin.pTurnEvil &&
            m_role != ROLE_TANK)
        {
            Unit* pAttacker = SelectAttackerDifferentFrom(me->GetVictim());
            if (pAttacker && pAttacker->GetCreatureType() == CREATURE_TYPE_UNDEAD &&
                CanTryToCastSpell(pAttacker, m_spells.paladin.pTurnEvil))
            {
                if (DoCastSpell(pAttacker, m_spells.paladin.pTurnEvil) == SPELL_CAST_OK)
                    return;
            }
        }
    }

    if (GetRole() == ROLE_HEALER)
    {
        if (m_spells.paladin.pHolyShock &&
            me->GetHealthPercent() < 50.0f &&
            CanTryToCastSpell(me, m_spells.paladin.pHolyShock))
        {
            if (m_spells.paladin.pDivineFavor &&
                CanTryToCastSpell(me, m_spells.paladin.pDivineFavor))
            {
                DoCastSpell(me, m_spells.paladin.pDivineFavor);
            }

            if (DoCastSpell(me, m_spells.paladin.pHolyShock) == SPELL_CAST_OK)
                return;
        }

        if (FindAndHealInjuredAlly(80.0f, 90.0f))
            return;

        if (FindAndPreHealTarget())
            return;
    }
    else
    {
        bool const hasSeal = m_spells.paladin.pSeal && me->HasAura(m_spells.paladin.pSeal->Id);

        if (!hasSeal &&
            m_spells.paladin.pSeal &&
            CanTryToCastSpell(me, m_spells.paladin.pSeal))
        {
            me->CastSpell(me, m_spells.paladin.pSeal, false);
        }

        if (Unit* pVictim = me->GetVictim())
        {
            if (hasSeal && m_spells.paladin.pJudgement &&
               (me->GetPowerPercent(POWER_MANA) > 30.0f) &&
                CanTryToCastSpell(pVictim, m_spells.paladin.pJudgement))
            {
                if (DoCastSpell(pVictim, m_spells.paladin.pJudgement) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.paladin.pHammerOfJustice &&
               (pVictim->IsNonMeleeSpellCasted() ||
               (me->GetHealthPercent() < 20.0f && !me->GetAttackers().empty())) &&
                CanTryToCastSpell(pVictim, m_spells.paladin.pHammerOfJustice))
            {
                if (DoCastSpell(pVictim, m_spells.paladin.pHammerOfJustice) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.paladin.pHammerOfWrath &&
                pVictim->GetHealthPercent() < 20.0f &&
                CanTryToCastSpell(pVictim, m_spells.paladin.pHammerOfWrath))
            {
                if (DoCastSpell(pVictim, m_spells.paladin.pHammerOfWrath) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.paladin.pConsecration &&
               (GetAttackersInRangeCount(10.0f) > 2) &&
                CanTryToCastSpell(me, m_spells.paladin.pConsecration))
            {
                if (DoCastSpell(me, m_spells.paladin.pConsecration) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.paladin.pHolyShock &&
                CanTryToCastSpell(pVictim, m_spells.paladin.pHolyShock))
            {
                if (m_spells.paladin.pDivineFavor &&
                    CanTryToCastSpell(me, m_spells.paladin.pDivineFavor))
                {
                    DoCastSpell(me, m_spells.paladin.pDivineFavor);
                }

                if (DoCastSpell(pVictim, m_spells.paladin.pHolyShock) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.paladin.pExorcism &&
                pVictim->IsCreature() &&
                (pVictim->GetCreatureType() == CREATURE_TYPE_UNDEAD) &&
                CanTryToCastSpell(pVictim, m_spells.paladin.pExorcism))
            {
                if (DoCastSpell(pVictim, m_spells.paladin.pExorcism) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.paladin.pHolyWrath &&
                pVictim->IsCreature() &&
               (pVictim->GetCreatureType() == CREATURE_TYPE_UNDEAD ||
                pVictim->GetCreatureType() == CREATURE_TYPE_DEMON) &&
               (me->GetAttackers().size() < 3) && // too much pushback
                CanTryToCastSpell(pVictim, m_spells.paladin.pHolyWrath))
            {
                if (DoCastSpell(pVictim, m_spells.paladin.pHolyWrath) == SPELL_CAST_OK)
                    return;
            }
            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE
                && !me->CanReachWithMeleeAutoAttack(pVictim))
            {
                BeginChasing(pVictim);
            }
        }
    }

    // The freedom of the member that stands held in place, which a paladin asks for the tank of the group while
    // the group has one and for itself when the group has none. The Cleanse of the bot stands above this: a
    // party bot reaches the routine of its class only after CheckForDispelTargets(), and the three seconds of
    // the wait are what that Cleanse and the mobs of the party have to answer the hold themselves.
    if (m_spells.paladin.pBlessingOfFreedom)
    {
        Player* pMember = SelectPaladinFreedomTarget();
        if (pMember && CanTryToCastSpell(pMember, m_spells.paladin.pBlessingOfFreedom))
        {
            if (DoCastSpell(pMember, m_spells.paladin.pBlessingOfFreedom) == SPELL_CAST_OK)
                return;
        }
    }

    if (GetRole() != ROLE_HEALER &&
        me->GetHealthPercent() < 30.0f)
        HealInjuredTarget(me);
}

void PartyBotAI::UpdateOutOfCombatAI_Shaman()
{
    if (m_spells.shaman.pWeaponBuff &&
        CanTryToCastSpell(me, m_spells.shaman.pWeaponBuff))
    {
        if (CastWeaponBuff(m_spells.shaman.pWeaponBuff, EQUIPMENT_SLOT_MAINHAND) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.shaman.pLightningShield &&
        CanTryToCastSpell(me, m_spells.shaman.pLightningShield))
    {
        if (DoCastSpell(me, m_spells.shaman.pLightningShield) == SPELL_CAST_OK)
            return;
    }

    if (m_role == ROLE_HEALER &&
        FindAndHealInjuredAlly())
        return;

    if (me->GetVictim())
    {
        if (SummonShamanTotems())
            return;

        UpdateInCombatAI_Shaman();
    }
}

void PartyBotAI::UpdateInCombatAI_Shaman()
{
    if (m_spells.shaman.pManaTideTotem &&
       (me->GetPowerPercent(POWER_MANA) < 50.0f) &&
        CanTryToCastSpell(me, m_spells.shaman.pManaTideTotem))
    {
        if (DoCastSpell(me, m_spells.shaman.pManaTideTotem) == SPELL_CAST_OK)
            return;
    }

    if (GetRole() != ROLE_HEALER)
    {
        if (Unit* pVictim = me->GetVictim())
        {
            if (m_spells.shaman.pElementalMastery &&
                me->GetAttackers().empty() &&
                CanTryToCastSpell(me, m_spells.shaman.pElementalMastery))
            {
                if (DoCastSpell(me, m_spells.shaman.pElementalMastery) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pEarthShock &&
                pVictim->IsNonMeleeSpellCasted(false, false, true) &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pEarthShock))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pEarthShock) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pFrostShock &&
                pVictim->IsMoving() &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pFrostShock))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pFrostShock) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pStormstrike &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pStormstrike))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pStormstrike) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pChainLightning &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pChainLightning))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pChainLightning) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pPurge &&
                IsValidDispelTarget(pVictim, m_spells.shaman.pPurge) &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pPurge))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pPurge) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pFlameShock &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pFlameShock))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pFlameShock) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.shaman.pLightningBolt &&
               (GetRole() == ROLE_RANGE_DPS || !me->CanReachWithMeleeAutoAttack(pVictim)) &&
                CanTryToCastSpell(pVictim, m_spells.shaman.pLightningBolt))
            {
                if (DoCastSpell(pVictim, m_spells.shaman.pLightningBolt) == SPELL_CAST_OK)
                    return;
            }
        }
    }

    if (SummonShamanTotems())
        return;

    if (GetRole() == ROLE_HEALER)
    {
        if (FindAndHealInjuredAlly(50.0f, 90.0f))
            return;

        if (FindAndPreHealTarget())
            return;
    }
    else if (me->GetHealthPercent() < 20.0f)
        HealInjuredTarget(me);
}

void PartyBotAI::UpdateOutOfCombatAI_Hunter()
{
    if (m_spells.hunter.pAspectOfTheHawk &&
        CanTryToCastSpell(me, m_spells.hunter.pAspectOfTheHawk))
    {
        if (DoCastSpell(me, m_spells.hunter.pAspectOfTheHawk) == SPELL_CAST_OK)
            return;
    }

    if (Unit* pVictim = me->GetVictim())
    {
        if (m_spells.hunter.pHuntersMark &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pHuntersMark))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pHuntersMark) == SPELL_CAST_OK)
                return;
        }

        if (Pet* pPet = me->GetPet())
        {
            if (!pPet->GetVictim())
            {
                pPet->GetCharmInfo()->SetIsCommandAttack(true);
                pPet->AI()->AttackStart(pVictim);
            }
        }

        UpdateInCombatAI_Hunter();
    }
    // A pet is called to where its master stands, so a bot that has dismissed it for a drop and has not come down
    // yet keeps it dismissed; it is called back below the ledge, see CallPetBackAfterTheDrop().
    else if (!m_petDismissedForDrop)
        SummonPetIfNeeded();
}

void PartyBotAI::UpdateInCombatAI_Hunter()
{
    if (Unit* pVictim = me->GetVictim())
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE
            && me->GetDistance(pVictim) > 30.0f)
        {
            BeginChasing(pVictim);
        }

        if (m_spells.hunter.pVolley &&
           (me->GetEnemyCountInRadiusAround(pVictim, 10.0f) > 2) &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pVolley))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pVolley) == SPELL_CAST_OK)
                return;
        }

        if (me->HasSpell(PB_SPELL_AUTO_SHOT) &&
            !me->IsMoving() &&
            (me->GetCombatDistance(pVictim) > 8.0f) &&
            !me->IsNonMeleeSpellCasted())
        {
            switch (me->CastSpell(pVictim, PB_SPELL_AUTO_SHOT, false))
            {
                case SPELL_FAILED_NEED_AMMO:
                case SPELL_FAILED_NO_AMMO:
                {
                    AddHunterAmmo();
                    break;
                }
            }
        }

        if (m_spells.hunter.pConcussiveShot &&
            pVictim->IsMoving() && (pVictim->GetVictim() == me) &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pConcussiveShot))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pConcussiveShot) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.hunter.pAimedShot &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pAimedShot))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pAimedShot) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.hunter.pArcaneShot &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pArcaneShot))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pArcaneShot) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.hunter.pSerpentSting &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pSerpentSting))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pSerpentSting) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.hunter.pMultiShot &&
            CanTryToCastSpell(pVictim, m_spells.hunter.pMultiShot))
        {
            if (DoCastSpell(pVictim, m_spells.hunter.pMultiShot) == SPELL_CAST_OK)
                return;
        }

        if (GetAttackersInRangeCount(8.0f))
        {
            Unit* pAttacker = *me->GetAttackers().begin();

            if (m_spells.hunter.pScareBeast &&
                CanTryToCastSpell(pAttacker, m_spells.hunter.pScareBeast))
            {
                if (DoCastSpell(pAttacker, m_spells.hunter.pScareBeast) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.hunter.pDisengage &&
                CanTryToCastSpell(pAttacker, m_spells.hunter.pDisengage))
            {
                if (DoCastSpell(pAttacker, m_spells.hunter.pDisengage) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.hunter.pAspectOfTheMonkey &&
                CanTryToCastSpell(me, m_spells.hunter.pAspectOfTheMonkey))
            {
                if (DoCastSpell(me, m_spells.hunter.pAspectOfTheMonkey) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.hunter.pFeignDeath &&
               (me->GetHealthPercent() < 20.0f) &&
                CanTryToCastSpell(me, m_spells.hunter.pFeignDeath))
            {
                if (DoCastSpell(me, m_spells.hunter.pFeignDeath) == SPELL_CAST_OK)
                    return;
            }
        }

        if (pVictim->CanReachWithMeleeAutoAttack(me))
        {
            if (m_spells.hunter.pWingClip &&
                CanTryToCastSpell(pVictim, m_spells.hunter.pWingClip))
            {
                DoCastSpell(pVictim, m_spells.hunter.pWingClip);
            }

            if (m_spells.hunter.pMongooseBite &&
                CanTryToCastSpell(pVictim, m_spells.hunter.pMongooseBite))
            {
                DoCastSpell(pVictim, m_spells.hunter.pMongooseBite);
            }

            if (m_spells.hunter.pRaptorStrike &&
                CanTryToCastSpell(pVictim, m_spells.hunter.pRaptorStrike))
            {
                DoCastSpell(pVictim, m_spells.hunter.pRaptorStrike);
            }
        }
        else
        {
            if (m_spells.hunter.pAspectOfTheHawk &&
                CanTryToCastSpell(me, m_spells.hunter.pAspectOfTheHawk))
            {
                if (DoCastSpell(me, m_spells.hunter.pAspectOfTheHawk) == SPELL_CAST_OK)
                    return;
            }
        }

        if (CanRunAwayFrom(pVictim) &&
            (me->GetCombatDistance(pVictim) < 8.0f) &&
            (GetRole() != ROLE_MELEE_DPS) &&
             me->GetMotionMaster()->GetCurrentMovementGeneratorType() != DISTANCING_MOTION_TYPE)
        {
            if (!me->IsStopped())
                me->StopMoving();
            me->GetMotionMaster()->Clear();
            if (RunAwayFromTarget(pVictim))
                return;
        }
    }
}

void PartyBotAI::UpdateOutOfCombatAI_Mage()
{
    SpellEntry const* pBuffSpell = nullptr;
    Player* pTarget = nullptr;
    uint8 missingIntellect = 0;
    Map* pMap = me->GetMap();
    bool const inDungeon = pMap && pMap->IsDungeon();

    if (!inDungeon)
    {
        pTarget = SelectMissingGroupBuffTarget(m_spells.mage.pArcaneIntellect, nullptr, true, missingIntellect);
        pBuffSpell = m_spells.mage.pArcaneIntellect;
    }
    else if (m_spells.mage.pArcaneBrilliance)
    {
        Group* pGroup = me->GetGroup();
        if (pGroup && pGroup->GetMembersCount() >= 5)
        {
            pTarget = SelectMissingGroupBuffTarget(m_spells.mage.pArcaneIntellect,
                m_spells.mage.pArcaneBrilliance, true, missingIntellect);
            pBuffSpell = missingIntellect >= 2 ?
                m_spells.mage.pArcaneBrilliance : m_spells.mage.pArcaneIntellect;
        }
    }
    else
    {
        pTarget = SelectMissingGroupBuffTarget(m_spells.mage.pArcaneIntellect, nullptr, true, missingIntellect);
        pBuffSpell = m_spells.mage.pArcaneIntellect;
    }

    if (pTarget && pBuffSpell)
    {
        if (CanTryToCastSpell(pTarget, pBuffSpell))
        {
            if (DoCastSpell(pTarget, pBuffSpell) == SPELL_CAST_OK)
            {
                m_isBuffing = true;
                me->ClearTarget();
                return;
            }
        }
    }

    if (m_spells.mage.pIceArmor &&
        CanTryToCastSpell(me, m_spells.mage.pIceArmor))
    {
        if (DoCastSpell(me, m_spells.mage.pIceArmor) == SPELL_CAST_OK)
        {
            m_isBuffing = true;
            me->ClearTarget();
            return;
        }
    }

    if (m_isBuffing &&
       (!m_spells.mage.pArcaneIntellect ||
        !me->HasGCD(m_spells.mage.pArcaneIntellect)))
    {
        m_isBuffing = false;
    }

    if (me->GetVictim())
        UpdateInCombatAI_Mage();
}

bool PartyBotAI::EnsureMageIceBarrier()
{
    if (me->GetClass() != CLASS_MAGE || me->IsMounted() || !m_spells.mage.pIceBarrier)
        return false;

    if (me->HasAura(m_spells.mage.pIceBarrier->Id) ||
        !CanTryToCastSpell(me, m_spells.mage.pIceBarrier))
        return false;

    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);

    return DoCastSpell(me, m_spells.mage.pIceBarrier) == SPELL_CAST_OK;
}

void PartyBotAI::UpdateInCombatAI_Mage()
{
    if (Unit* pVictim = me->GetVictim())
    {
        if (m_spells.mage.pCounterspell &&
            pVictim->IsNonMeleeSpellCasted(false, false, true) &&
            CanTryToCastSpell(pVictim, m_spells.mage.pCounterspell))
        {
            if (DoCastSpell(pVictim, m_spells.mage.pCounterspell) == SPELL_CAST_OK)
                return;
        }

        // Fire Blast is used only inside combat and never against a frozen target, whose root is meant to hold.
        bool canUseFireBlast =
            me->IsInCombat() &&
            !pVictim->IsFrozen() &&
            m_spells.mage.pFireBlast &&
            CanTryToCastSpell(pVictim, m_spells.mage.pFireBlast);
        if (canUseFireBlast)
        {
            if (DoCastSpell(pVictim, m_spells.mage.pFireBlast) == SPELL_CAST_OK)
                return;

            canUseFireBlast = false;
        }

        // Finish a trivial target with the wand when Fire Blast cannot do it.
        if (!canUseFireBlast &&
           (pVictim->GetHealthPercent() < 2.0f) &&
           (!pVictim->IsCreature() || !static_cast<Creature*>(pVictim)->IsElite()) &&
            me->HasSpell(PB_SPELL_SHOOT_WAND) &&
            !me->IsMoving() &&
            !me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
        {
            if (me->CastSpell(pVictim, PB_SPELL_SHOOT_WAND, false) == SPELL_CAST_OK)
                return;
        }

        // Cone of Cold shares the multi-target condition of Arcane Explosion. Frostbolt is the single-target filler,
        // with Fireball as its fallback.
        if (m_spells.mage.pConeofCold &&
            (me->GetEnemyCountInRadiusAround(me, 10.0f) > 1) &&
            m_spells.mage.pConeofCold->IsTargetInRange(me, pVictim) &&
            CanTryToCastSpell(pVictim, m_spells.mage.pConeofCold))
        {
            if (DoCastSpell(pVictim, m_spells.mage.pConeofCold) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.mage.pEvocation &&
           (me->GetPowerPercent(POWER_MANA) < 20.0f) &&
           !HasMeleeAttackerInRange() &&
            CanTryToCastSpell(me, m_spells.mage.pEvocation))
        {
            if (DoCastSpell(me, m_spells.mage.pEvocation) == SPELL_CAST_OK)
                return;
        }

        SpellEntry const* pRangeSpell = m_spells.mage.pFrostbolt ?
            m_spells.mage.pFrostbolt : m_spells.mage.pFireball;
        if (GetAttackersInRangeCount(10.0f))
        {
            if (m_spells.mage.pManaShield &&
               (me->GetHealthPercent() < 30.0f) &&
               (me->GetPowerPercent(POWER_MANA) > 20.0f) &&
                CanTryToCastSpell(me, m_spells.mage.pManaShield))
            {
                if (DoCastSpell(me, m_spells.mage.pManaShield) == SPELL_CAST_OK)
                    return;
            }

            if ((GetRole() != ROLE_MELEE_DPS) &&
                (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != DISTANCING_MOTION_TYPE) &&
                !me->HasUnitState(UNIT_STATE_CAN_NOT_MOVE))
            {
                if (m_spells.mage.pFrostNova &&
                   (me->GetCombatDistance(pVictim) < PB_RUN_AWAY_DISTANCE) &&
                   !pVictim->HasUnitState(UNIT_STATE_ROOT) &&
                   !pVictim->HasUnitState(UNIT_STATE_CAN_NOT_REACT_OR_LOST_CONTROL) &&
                    CanTryToCastSpell(me, m_spells.mage.pFrostNova))
                {
                    DoCastSpell(me, m_spells.mage.pFrostNova);
                }

                if (CanRunAwayFrom(pVictim) && RunAwayFromTarget(pVictim))
                {
                    me->SetCasterChaseDistance(pRangeSpell ?
                        GetSpellMaxRangeForBot(pRangeSpell) : GetChaseDistance());
                    return;
                }
            }
        }

        if (m_spells.mage.pFrostbolt &&
            CanTryToCastSpell(pVictim, m_spells.mage.pFrostbolt))
        {
            if (DoCastSpell(pVictim, m_spells.mage.pFrostbolt) == SPELL_CAST_OK)
                return;
        }

        if (ShouldUseFireballInsteadOfFrostbolt(pVictim) &&
            m_spells.mage.pFireball &&
            CanTryToCastSpell(pVictim, m_spells.mage.pFireball))
        {
            if (DoCastSpell(pVictim, m_spells.mage.pFireball) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.mage.pCombustion &&
            CanTryToCastSpell(me, m_spells.mage.pCombustion))
        {
            if (DoCastSpell(me, m_spells.mage.pCombustion) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.mage.pPyroblast &&
           ((m_spells.mage.pPresenceOfMind && me->HasAura(m_spells.mage.pPresenceOfMind->Id)) ||
            (!pVictim->IsInCombat() && (pVictim->GetMaxHealth() > me->GetMaxHealth()) && (me->GetDistance(pVictim) > 30.0f))) &&
            CanTryToCastSpell(pVictim, m_spells.mage.pPyroblast))
        {
            if (DoCastSpell(pVictim, m_spells.mage.pPyroblast) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.mage.pIceBlock &&
           (me->GetHealthPercent() < 10.0f) &&
            CanTryToCastSpell(me, m_spells.mage.pIceBlock))
        {
            if (DoCastSpell(me, m_spells.mage.pIceBlock) == SPELL_CAST_OK)
                return;
        }

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE &&
            pRangeSpell &&
            !IsTargetInSpellRange(pVictim, pRangeSpell))
        {
            BeginChasing(pVictim);
        }

        if (me->GetEnemyCountInRadiusAround(me, 10.0f) > 1)
        {
            if (m_spells.mage.pBlastWave &&
                CanTryToCastSpell(me, m_spells.mage.pBlastWave))
            {
                if (DoCastSpell(me, m_spells.mage.pBlastWave) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.mage.pArcaneExplosion &&
                CanTryToCastSpell(me, m_spells.mage.pArcaneExplosion))
            {
                if (DoCastSpell(me, m_spells.mage.pArcaneExplosion) == SPELL_CAST_OK)
                    return;
            }
        }

        bool const enemyHeld =
            pVictim->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED) ||
            pVictim->HasAuraType(SPELL_AURA_MOD_ROOT) ||
            pVictim->HasAuraType(SPELL_AURA_MOD_STUN) ||
            pVictim->HasAuraType(SPELL_AURA_MOD_CONFUSE);
        bool const hasEnoughDistance = me->GetCombatDistance(pVictim) >= PB_RUN_AWAY_DISTANCE;
        if (enemyHeld && hasEnoughDistance &&
            (me->IsMoving() ||
             me->GetMotionMaster()->GetCurrentMovementGeneratorType() == DISTANCING_MOTION_TYPE))
        {
            // The distance move has done its job. Remove its movement before the spell so a stale chase or distance
            // generator cannot leave the mage facing the target without casting.
            if (!me->IsStopped())
                me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == DISTANCING_MOTION_TYPE)
            return;

        if (m_spells.mage.pBlizzard &&
           (me->GetEnemyCountInRadiusAround(pVictim, 10.0f) > 2) &&
            CanTryToCastSpell(pVictim, m_spells.mage.pBlizzard))
        {
            if (DoCastSpell(pVictim, m_spells.mage.pBlizzard) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.mage.pPolymorph)
        {
            if (Unit* pTarget = SelectAttackerDifferentFrom(pVictim))
            {
                if (pTarget->GetHealthPercent() > 20.0f &&
                    CanTryToCastSpell(pTarget, m_spells.mage.pPolymorph) &&
                    CanUseCrowdControl(m_spells.mage.pPolymorph, pTarget))
                {
                    if (DoCastSpell(pTarget, m_spells.mage.pPolymorph) == SPELL_CAST_OK)
                        return;
                }
            }
        }

        if (m_spells.mage.pArcanePower &&
            (me->GetPowerPercent(POWER_MANA) > 50.0f) &&
            CanTryToCastSpell(me, m_spells.mage.pArcanePower))
        {
            if (DoCastSpell(me, m_spells.mage.pArcanePower) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.mage.pPresenceOfMind &&
           (me->GetPowerPercent(POWER_MANA) > 50.0f) &&
            CanTryToCastSpell(me, m_spells.mage.pPresenceOfMind))
        {
            if (DoCastSpell(me, m_spells.mage.pPresenceOfMind) == SPELL_CAST_OK)
                return;
        }

        if (me->HasSpell(PB_SPELL_SHOOT_WAND) &&
           !me->IsMoving() &&
           (me->GetPowerPercent(POWER_MANA) < 5.0f) &&
           !me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
            me->CastSpell(pVictim, PB_SPELL_SHOOT_WAND, false);
    }
}

void PartyBotAI::UpdateOutOfCombatAI_Priest()
{
    if (me->IsMounted())
        return;

    bool const needsResurrectionOrDisease = PriestNeedsResurrectionOrDiseaseDispel();
    if (needsResurrectionOrDisease &&
        GetRole() != ROLE_HEALER &&
        me->GetShapeshiftForm() == FORM_SHADOW)
    {
        uint64 const now = uint64(time(nullptr));
        if (!m_shadowformDispelWaitUntil)
        {
            m_shadowformDispelWaitUntil = now + 20;
            return;
        }

        if (now < m_shadowformDispelWaitUntil)
            return;

        me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
        m_shadowformDispelWaitUntil = 0;

        if (m_resurrectionSpell)
        {
            if (Player* pTarget = SelectResurrectionTarget())
                if (CanTryToCastSpell(pTarget, m_resurrectionSpell))
                    if (DoCastSpell(pTarget, m_resurrectionSpell) == SPELL_CAST_OK)
                        return;
        }

        if (m_spells.priest.pAbolishDisease)
        {
            if (Player* pTarget = SelectDispelTarget(m_spells.priest.pAbolishDisease))
                if (CanTryToCastSpell(pTarget, m_spells.priest.pAbolishDisease))
                    if (DoCastSpell(pTarget, m_spells.priest.pAbolishDisease) == SPELL_CAST_OK)
                        return;
        }

        return;
    }

    if (!needsResurrectionOrDisease)
        m_shadowformDispelWaitUntil = 0;

    // The priest can not refresh the group's Holy buffs in Shadowform. Leave the form while a buff is still owed,
    // and put it back once the buff routine has nothing left to do.
    if (me->GetShapeshiftForm() == FORM_SHADOW && PriestHasOutOfCombatBuffTarget())
        me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

    if (m_spells.priest.pPrayerofFortitude)
    {
        if (PriestFortitudeBuffDelayPassed())
        {
            uint8 missingStamina = 0;
            Player* pTarget = SelectMissingGroupBuffTarget(m_spells.priest.pPowerWordFortitude,
                m_spells.priest.pPrayerofFortitude, false, missingStamina);
            if (pTarget)
            {
                SpellEntry const* pBuffSpell = missingStamina >= 3 ?
                    m_spells.priest.pPrayerofFortitude : m_spells.priest.pPowerWordFortitude;
                if (pBuffSpell == m_spells.priest.pPrayerofFortitude &&
                    m_spells.priest.pInnerFocus &&
                    CanTryToCastSpell(me, m_spells.priest.pInnerFocus))
                {
                    if (DoCastSpell(me, m_spells.priest.pInnerFocus) == SPELL_CAST_OK)
                    {
                        m_isBuffing = true;
                        me->ClearTarget();
                        return;
                    }
                }

                if (pBuffSpell && CanTryToCastSpell(pTarget, pBuffSpell))
                {
                    if (DoCastSpell(pTarget, pBuffSpell) == SPELL_CAST_OK)
                    {
                        m_isBuffing = true;
                        me->ClearTarget();
                        return;
                    }
                }
            }
        }
    }
    else if (m_spells.priest.pPowerWordFortitude)
    {
        uint8 missingStamina = 0;
        if (Player* pTarget = SelectMissingGroupBuffTarget(m_spells.priest.pPowerWordFortitude,
                nullptr, false, missingStamina))
        {
            if (CanTryToCastSpell(pTarget, m_spells.priest.pPowerWordFortitude))
            {
                if (DoCastSpell(pTarget, m_spells.priest.pPowerWordFortitude) == SPELL_CAST_OK)
                {
                    m_isBuffing = true;
                    me->ClearTarget();
                    return;
                }
            }
        }
    }

    Map* pMap = me->GetMap();
    bool const inDungeon = pMap && pMap->IsDungeon();
    if (!inDungeon)
    {
        if (m_spells.priest.pDivineSpirit)
        {
            uint8 missingSpirit = 0;
            if (Player* pTarget = SelectMissingGroupBuffTarget(m_spells.priest.pDivineSpirit,
                    nullptr, true, missingSpirit))
            {
                if (CanTryToCastSpell(pTarget, m_spells.priest.pDivineSpirit))
                {
                    if (DoCastSpell(pTarget, m_spells.priest.pDivineSpirit) == SPELL_CAST_OK)
                    {
                        m_isBuffing = true;
                        me->ClearTarget();
                        return;
                    }
                }
            }
        }
    }
    else if (m_spells.priest.pPrayerofSpirit)
    {
        Group* pGroup = me->GetGroup();
        if (pGroup && pGroup->GetMembersCount() >= 5)
        {
            uint8 missingSpirit = 0;
            Player* pTarget = SelectMissingGroupBuffTarget(m_spells.priest.pDivineSpirit,
                m_spells.priest.pPrayerofSpirit, true, missingSpirit);
            if (pTarget && missingSpirit >= 3 &&
                CanTryToCastSpell(pTarget, m_spells.priest.pPrayerofSpirit))
            {
                if (DoCastSpell(pTarget, m_spells.priest.pPrayerofSpirit) == SPELL_CAST_OK)
                {
                    m_isBuffing = true;
                    me->ClearTarget();
                    return;
                }
            }
        }
    }

    if (m_spells.priest.pShadowform &&
        GetRole() != ROLE_HEALER &&
        me->GetShapeshiftForm() != FORM_SHADOW &&
        !PriestNeedsResurrectionOrDiseaseDispel() &&
        !PriestHasOutOfCombatBuffTarget() &&
        CanTryToCastSpell(me, m_spells.priest.pShadowform))
    {
        if (DoCastSpell(me, m_spells.priest.pShadowform) == SPELL_CAST_OK)
        {
            m_isBuffing = true;
            me->ClearTarget();
            return;
        }
    }

    if (m_spells.priest.pInnerFire &&
        !me->HasAura(m_spells.priest.pInnerFire->Id) &&
        CanTryToCastSpell(me, m_spells.priest.pInnerFire))
    {
        if (DoCastSpell(me, m_spells.priest.pInnerFire) == SPELL_CAST_OK)
        {
            m_isBuffing = true;
            me->ClearTarget();
            return;
        }
    }

    if (m_isBuffing &&
       (!m_spells.priest.pPowerWordFortitude ||
        !me->HasGCD(m_spells.priest.pPowerWordFortitude)))
    {
        m_isBuffing = false;
    }

    if (m_spells.priest.pPowerWordShield &&
        !me->HasAura(m_spells.priest.pPowerWordShield->Id) &&
        CanTryToCastSpell(me, m_spells.priest.pPowerWordShield))
    {
        if (DoCastSpell(me, m_spells.priest.pPowerWordShield) == SPELL_CAST_OK)
        {
            m_isBuffing = true;
            me->ClearTarget();
            return;
        }
    }

    if (m_role == ROLE_HEALER &&
        FindAndHealInjuredAlly())
        return;

    if (me->GetVictim())
        UpdateInCombatAI_Priest();
}

void PartyBotAI::UpdateInCombatAI_Priest()
{
    if (!me->IsMounted())
    {
        if (m_spells.priest.pPowerWordShield &&
            !me->HasAura(m_spells.priest.pPowerWordShield->Id) &&
            CanTryToCastSpell(me, m_spells.priest.pPowerWordShield))
        {
            if (DoCastSpell(me, m_spells.priest.pPowerWordShield) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.priest.pInnerFire &&
            !me->HasAura(m_spells.priest.pInnerFire->Id) &&
            CanTryToCastSpell(me, m_spells.priest.pInnerFire))
        {
            if (DoCastSpell(me, m_spells.priest.pInnerFire) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.priest.pShadowform &&
            GetRole() != ROLE_HEALER &&
            me->GetShapeshiftForm() != FORM_SHADOW &&
            CanTryToCastSpell(me, m_spells.priest.pShadowform))
        {
            if (DoCastSpell(me, m_spells.priest.pShadowform) == SPELL_CAST_OK)
                return;
        }
    }

    if (GetRole() == ROLE_HEALER || (!me->GetVictim() && me->GetShapeshiftForm() == FORM_NONE))
    {
        // Three injured party members turn a single target heal into Prayer of Healing.
        if (SpellEntry const* pPrayerOfHealing = SelectPrayerOfHealing())
        {
            uint8 injuredMembers = 0;
            Player* pTarget = SelectPrayerOfHealingTarget(injuredMembers);
            if (injuredMembers >= 3 &&
                pTarget &&
                CanTryToCastSpell(pTarget, pPrayerOfHealing))
            {
                if (DoCastSpell(pTarget, pPrayerOfHealing) == SPELL_CAST_OK)
                    return;
            }
        }

        // Shield allies being attacked.
        if (m_spells.priest.pPowerWordShield)
        {
            if (Player* pTarget = SelectShieldTarget())
            {
                if (CanTryToCastSpell(pTarget, m_spells.priest.pPowerWordShield))
                {
                    if (DoCastSpell(pTarget, m_spells.priest.pPowerWordShield) == SPELL_CAST_OK)
                        return;
                }
            }
        }

        // Direct heal more seriously injured.
        if (Unit* pTarget = SelectHealTarget(60.0f, 80.0f))
            if (HealInjuredTargetDirect(pTarget))
                return;

        // Apply HoT aura for small injuries.
        if (Unit* pTarget = SelectPeriodicHealTarget(80.0f, 90.0f))
            if (HealInjuredTargetPeriodic(pTarget))
                return;

        if (GetRole() == ROLE_HEALER && FindAndPreHealTarget())
            return;

        if (GetRole() == ROLE_HEALER &&
            GroupHealthAbove(90.0f) &&
            me->HasSpell(PB_SPELL_SHOOT_WAND) &&
            !me->IsMounted() &&
            !me->IsMoving() &&
            !me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
        {
            Unit* pTarget = SelectPartyAttackTarget();
            if (pTarget && pTarget->IsInCombat())
                me->CastSpell(pTarget, PB_SPELL_SHOOT_WAND, false);
        }
    }
    else if (Unit* pVictim = me->GetVictim())
    {
        if (HasMeleeAttackerInRange() &&
            me->GetMotionMaster()->GetCurrentMovementGeneratorType() != DISTANCING_MOTION_TYPE &&
            !me->HasUnitState(UNIT_STATE_CAN_NOT_MOVE) &&
            CanRunAwayFrom(pVictim) &&
            RunAwayFromTarget(pVictim))
        {
            me->SetCasterChaseDistance(m_spells.priest.pMindFlay ?
                GetSpellMaxRangeForBot(m_spells.priest.pMindFlay) : GetChaseDistance());
            return;
        }

        bool const enemyHeld =
            pVictim->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED) ||
            pVictim->HasAuraType(SPELL_AURA_MOD_ROOT) ||
            pVictim->HasAuraType(SPELL_AURA_MOD_STUN) ||
            pVictim->HasAuraType(SPELL_AURA_MOD_CONFUSE);
        bool const hasEnoughDistance = me->GetCombatDistance(pVictim) >= PB_RUN_AWAY_DISTANCE;
        if (enemyHeld && hasEnoughDistance &&
            (me->IsMoving() ||
             me->GetMotionMaster()->GetCurrentMovementGeneratorType() == DISTANCING_MOTION_TYPE))
        {
            if (!me->IsStopped())
                me->StopMoving();
            me->GetMotionMaster()->Clear(false, true);
            me->GetMotionMaster()->MoveIdle();
        }

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == DISTANCING_MOTION_TYPE)
            return;

        // Mind Blast opens the fight before the maintained debuffs or the Mind Flay filler.
        if (m_priestMindBlastOpener)
        {
            if (!m_spells.priest.pMindBlast)
                m_priestMindBlastOpener = false;
            else
            {
                if (m_spells.priest.pInnerFocus &&
                    CanTryToCastSpell(me, m_spells.priest.pInnerFocus))
                {
                    if (DoCastSpell(me, m_spells.priest.pInnerFocus) == SPELL_CAST_OK)
                        return;
                }

                if (CanTryToCastSpell(pVictim, m_spells.priest.pMindBlast))
                {
                    if (DoCastSpell(pVictim, m_spells.priest.pMindBlast) == SPELL_CAST_OK)
                    {
                        m_priestMindBlastOpener = false;
                        return;
                    }
                }
            }
        }

        // Inner Focus is consumed by the next spell, so Mind Blast takes it before either maintained debuff.
        if (m_spells.priest.pInnerFocus &&
            me->HasAura(m_spells.priest.pInnerFocus->Id))
        {
            if (m_spells.priest.pMindBlast &&
                CanTryToCastSpell(pVictim, m_spells.priest.pMindBlast))
            {
                if (DoCastSpell(pVictim, m_spells.priest.pMindBlast) == SPELL_CAST_OK)
                    return;
            }

            return;
        }

        // The two maintained debuffs go back up before another damage spell once the current cast has ended.
        if (!me->IsNonMeleeSpellCasted(false, false, true))
        {
            if (m_spells.priest.pShadowWordPain &&
                CanTryToCastSpell(pVictim, m_spells.priest.pShadowWordPain))
            {
                if (DoCastSpell(pVictim, m_spells.priest.pShadowWordPain) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.priest.pVampiricEmbrace &&
                !me->HasAura(m_spells.priest.pVampiricEmbrace->Id) &&
                CanTryToCastSpell(pVictim, m_spells.priest.pVampiricEmbrace))
            {
                if (DoCastSpell(pVictim, m_spells.priest.pVampiricEmbrace) == SPELL_CAST_OK)
                    return;
            }
        }

        if (m_spells.priest.pInnerFocus &&
            m_spells.priest.pMindBlast &&
            CanTryToCastSpell(me, m_spells.priest.pInnerFocus) &&
            CanTryToCastSpell(pVictim, m_spells.priest.pMindBlast))
        {
            if (DoCastSpell(me, m_spells.priest.pInnerFocus) == SPELL_CAST_OK)
                return;
        }

        bool const mindFlayInRange =
            m_spells.priest.pMindFlay && IsTargetInSpellRange(pVictim, m_spells.priest.pMindFlay);
        if (m_spells.priest.pMindFlay && !mindFlayInRange)
        {
            bool const canMove =
                !me->HasUnitState(UNIT_STATE_NO_FREE_MOVE) &&
                !me->HasAuraType(SPELL_AURA_MOD_ROOT) &&
                !me->HasAuraType(SPELL_AURA_MOD_STUN) &&
                !me->HasAuraType(SPELL_AURA_MOD_CONFUSE) &&
                !me->HasAuraType(SPELL_AURA_MOD_FEAR);
            if (canMove)
            {
                MovementGeneratorType const movementType =
                    me->GetMotionMaster()->GetCurrentMovementGeneratorType();
                if (movementType == IDLE_MOTION_TYPE || movementType == FOLLOW_MOTION_TYPE)
                    BeginChasing(pVictim);
                return;
            }
        }

        if (m_spells.priest.pMindFlay &&
            mindFlayInRange &&
            CanTryToCastSpell(pVictim, m_spells.priest.pMindFlay))
        {
            if (DoCastSpell(pVictim, m_spells.priest.pMindFlay) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.priest.pMindBlast &&
            CanTryToCastSpell(pVictim, m_spells.priest.pMindBlast))
        {
            if (DoCastSpell(pVictim, m_spells.priest.pMindBlast) == SPELL_CAST_OK)
                return;
        }

        // Do not turn a global cooldown into a wand filler. Auto-repeat would otherwise keep it selected after the
        // next Mind Flay is ready.
        SpellEntry const* pGcdSpell = m_spells.priest.pMindFlay ?
            m_spells.priest.pMindFlay :
            (m_spells.priest.pMindBlast ? m_spells.priest.pMindBlast : m_spells.priest.pSmite);
        if (pGcdSpell && me->HasGCD(pGcdSpell))
            return;

        if (me->HasSpell(PB_SPELL_SHOOT_WAND) &&
           !me->IsMoving() &&
           !me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
            me->CastSpell(pVictim, PB_SPELL_SHOOT_WAND, false);
    }
}

void PartyBotAI::UpdateOutOfCombatAI_Warlock()
{
    if (m_spells.warlock.pDetectInvisibility)
    {
        if (Player* pTarget = SelectBuffTarget(m_spells.warlock.pDetectInvisibility))
        {
            if (CanTryToCastSpell(pTarget, m_spells.warlock.pDetectInvisibility))
            {
                if (DoCastSpell(pTarget, m_spells.warlock.pDetectInvisibility) == SPELL_CAST_OK)
                {
                    m_isBuffing = true;
                    me->ClearTarget();
                    return;
                }
            }
        }
    }

    if (m_spells.warlock.pDemonArmor &&
        CanTryToCastSpell(me, m_spells.warlock.pDemonArmor))
    {
        if (DoCastSpell(me, m_spells.warlock.pDemonArmor) == SPELL_CAST_OK)
        {
            m_isBuffing = true;
            me->ClearTarget();
            return;
        }
    }

    if (m_isBuffing &&
       (!m_spells.warlock.pDetectInvisibility ||
        !me->HasGCD(m_spells.warlock.pDetectInvisibility)))
    {
        m_isBuffing = false;
    }

    if (Unit* pVictim = me->GetVictim())
    {
        if (Pet* pPet = me->GetPet())
        {
            if (!pPet->GetVictim())
            {
                pPet->GetCharmInfo()->SetIsCommandAttack(true);
                pPet->AI()->AttackStart(pVictim);
            }
        }

        UpdateInCombatAI_Warlock();
    }
    // The pet of a bot that is coming down over a ledge stays dismissed, as it is for the hunter above.
    else if (!m_petDismissedForDrop)
        SummonPetIfNeeded();
}

void PartyBotAI::UpdateInCombatAI_Warlock()
{
    if (Unit* pVictim = me->GetVictim())
    {
        if (m_spells.warlock.pDeathCoil &&
           (pVictim->CanReachWithMeleeAutoAttack(me) || pVictim->IsNonMeleeSpellCasted()) &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pDeathCoil))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pDeathCoil) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pShadowburn &&
           (pVictim->GetHealthPercent() < 10.0f) &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pShadowburn))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pShadowburn) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pSearingPain &&
           (pVictim->GetHealthPercent() < 20.0f) &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pSearingPain))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pSearingPain) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pBanish &&
            me->GetAttackers().size() > 1)
        {
            Unit* pAttacker = *me->GetAttackers().begin();
            if ((pAttacker->GetHealth() > me->GetHealth()) &&
                CanTryToCastSpell(pAttacker, m_spells.warlock.pBanish))
            {
                if (DoCastSpell(pAttacker, m_spells.warlock.pBanish) == SPELL_CAST_OK)
                    return;
            }
        }

        if (m_spells.warlock.pRainOfFire &&
           (me->GetEnemyCountInRadiusAround(pVictim, 10.0f) > 2) &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pRainOfFire))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pRainOfFire) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pDemonicSacrifice)
        {
            if (Pet* pPet = me->GetPet())
            {
                if (pPet->IsAlive() &&
                    CanTryToCastSpell(pPet, m_spells.warlock.pDemonicSacrifice))
                {
                    if (DoCastSpell(pPet, m_spells.warlock.pDemonicSacrifice) == SPELL_CAST_OK)
                        return;
                }
            }
        }

        if (m_spells.warlock.pImmolate &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pImmolate))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pImmolate) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pConflagrate &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pConflagrate))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pConflagrate) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pCorruption &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pCorruption))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pCorruption) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pSiphonLife &&
           (me->GetHealthPercent() < 80.0f) &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pSiphonLife))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pSiphonLife) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pDrainLife &&
           (me->GetHealthPercent() < 30.0f) &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pDrainLife))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pDrainLife) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pFear &&
            pVictim->GetVictim() == me &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pFear))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pFear) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pCurseofAgony &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pCurseofAgony))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pCurseofAgony) == SPELL_CAST_OK)
                return;
        }

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE
            && me->GetDistance(pVictim) > 30.0f)
        {
            BeginChasing(pVictim);
        }

        if (m_spells.warlock.pHowlofTerror &&
            GetAttackersInRangeCount(10.0f) > 1 &&
            CanTryToCastSpell(me, m_spells.warlock.pHowlofTerror))
        {
            if (DoCastSpell(me, m_spells.warlock.pHowlofTerror) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pShadowBolt &&
            CanTryToCastSpell(pVictim, m_spells.warlock.pShadowBolt))
        {
            if (DoCastSpell(pVictim, m_spells.warlock.pShadowBolt) == SPELL_CAST_OK)
                return;
        }

        if (m_spells.warlock.pLifeTap &&
           (me->GetPowerPercent(POWER_MANA) < 10.0f) &&
           (me->GetHealthPercent() > 70.0f) &&
            CanTryToCastSpell(me, m_spells.warlock.pLifeTap))
        {
            if (DoCastSpell(me, m_spells.warlock.pLifeTap) == SPELL_CAST_OK)
                return;
        }

        if (me->HasSpell(PB_SPELL_SHOOT_WAND) &&
           !me->IsMoving() &&
           (me->GetPowerPercent(POWER_MANA) < 5.0f) &&
           !me->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
            me->CastSpell(pVictim, PB_SPELL_SHOOT_WAND, false);
    }
}

void PartyBotAI::UpdateOutOfCombatAI_Warrior()
{
    m_warriorPrimaryTargetGuid.Clear();
    m_warriorSecondaryTargetGuid.Clear();
    m_warriorSecondaryHoldTimer.Reset(0);

    // The stance that a warrior wears while it waits with its party is the stance that its rules ask for: the
    // Defensive Stance for a shield tank, and the stance that the talents of a damage warrior ask for. The stance
    // of a charge is not asked for here: the opening of a charge is the same for every warrior and belongs to
    // PartyBotAI::AttackStart, which takes the Battle Stance with it, keeps the target through it, and refuses the
    // charge of a bot that is in an instance.
    SpellEntry const* pStance = IsProtectionWarriorTank() ?
        m_spells.warrior.pDefensiveStance : GetWarriorHomeStanceSpell();
    ShapeshiftForm const desiredForm = GetStanceSpellForm(pStance);

    if (pStance && me->GetShapeshiftForm() != desiredForm)
    {
        if (TrySwitchWarriorStance(pStance))
            return;
    }

    // The shout is made up outside a fight only out of rage that the bot has already and does not need, ten points
    // of it, and never with the health that Bloodrage costs: a bot that waits with its group has no fight ahead of
    // it that would pay such a price back. The shout of a fight is left to the rotation of the fight, see
    // PartyBotAI::CollectWarriorDamageWants, and to the same rule for the shield tank.
    if (m_spells.warrior.pBattleShout &&
       !me->HasAura(m_spells.warrior.pBattleShout->Id) &&
       (me->GetPower(POWER_RAGE) >= 10 * PB_RAGE_PER_POINT))
    {
        if (CanTryToCastSpell(me, m_spells.warrior.pBattleShout))
            DoCastSpell(me, m_spells.warrior.pBattleShout);
    }
}

void PartyBotAI::UpdateProtectionWarriorTankCombat()
{
    Unit* pVictim = me->GetVictim();
    if (!pVictim)
    {
        if (m_spells.warrior.pBattleShout &&
            CanTryToCastSpell(me, m_spells.warrior.pBattleShout))
            DoCastSpell(me, m_spells.warrior.pBattleShout);
        return;
    }

    // Bloodrage is the rage source of the shield tank. It is used before a stance
    // or attack when no rotation skill is affordable, so the bot does not idle
    // until its next white swing.
    if (me->GetPower(POWER_RAGE) < 10 * PB_RAGE_PER_POINT &&
        m_spells.warrior.pBloodrage &&
        CanTryToCastSpell(me, m_spells.warrior.pBloodrage))
    {
        if (DoCastSpell(me, m_spells.warrior.pBloodrage) == SPELL_CAST_OK)
            return;
    }

    if (pVictim->IsNonMeleeSpellCasted(false, false, true) &&
        m_spells.warrior.pShieldBash &&
        IsWearingShield(me) &&
        CanTryToCastSpell(pVictim, m_spells.warrior.pShieldBash))
    {
        if (DoCastSpell(pVictim, m_spells.warrior.pShieldBash) == SPELL_CAST_OK)
            return;
    }

    // The survival of the shield tank, at the thresholds of the maintainer: Last Stand at thirty per cent of the
    // health of the bot or below, Shield Wall at twenty per cent or below, both only in combat. Neither of them
    // ends the update: they cost no rage and no stance, so a tank that puts one of them up keeps its target, its
    // taunt, its rotation and its chase, exactly as it would without the rule here. Last Stand is asked for first
    // because its threshold is the higher one, which is also the order in which a player keeps the longer cooldown
    // for later.
    if (me->IsInCombat() &&
        me->GetHealthPercent() <= 30.0f &&
        m_spells.warrior.pLastStand &&
        CanTryToCastSpell(me, m_spells.warrior.pLastStand))
        DoCastSpell(me, m_spells.warrior.pLastStand);

    if (me->IsInCombat() &&
        me->GetHealthPercent() <= 20.0f &&
        m_spells.warrior.pShieldWall &&
        CanTryToCastSpell(me, m_spells.warrior.pShieldWall))
        DoCastSpell(me, m_spells.warrior.pShieldWall);

    // The opening of a charge asked for nothing but the charge, and the Battle Stance of that opening is what the
    // Thunder Clap of the pack is asked in. The ask is over as soon as the tank stands in another stance: the
    // rotation asks the Defensive Stance back when it has nothing left to cast in this one, and the opening of a
    // fight is not repeated.
    if (m_warriorChargeOpeningThunderClap && me->GetShapeshiftForm() != FORM_BATTLESTANCE)
        m_warriorChargeOpeningThunderClap = false;

    // Intercept is how a shield tank gets hold of a mob that has walked out of a swing: the jump crosses that
    // ground in one spell, stuns the mob, and costs less than the walk it replaces. The gates of the spell, of the
    // rage and of the ground are the ones that a damage warrior is asked too (IsWarriorInterceptWanted()); only
    // the answer is written here, because the routine of a shield tank is not the wants of a damage warrior.
    //
    // The jump is allowed in the Berserker Stance, which is what a shield tank that wants to make it is asked to
    // change to. That change is bought with rage, so it is asked for only where the talent that keeps the rage
    // through a change was taken: the Defensive Stance is asked back by the rule below on the update after the
    // jump, and a tank without that talent would pay for the change twice and stand beside the mob with nothing
    // left to hold it by. With the talent this is the same shape of ask that the Battle Stance of a Mocking Blow
    // is, and the rule of the tank's own stance takes the Defensive Stance back on its own. Where the bot already
    // wears the stance that the spell wants, the ask is the spell itself.
    if (IsWarriorInterceptWanted(pVictim))
    {
        SpellEntry const* pIntercept = m_spells.warrior.pIntercept;
        if (me->GetShapeshiftForm() == FORM_BERSERKERSTANCE)
        {
            if (CanTryToCastSpell(pVictim, pIntercept) &&
                DoCastSpell(pVictim, pIntercept) == SPELL_CAST_OK)
                return;
        }
        else if (GetTacticalMasteryRank())
        {
            // The change and the jump are paid for together, as everywhere else here. Where the rage for both is
            // not there the tank leaves the jump out and goes on with its routine, which is the answer that a want
            // of a damage rotation gets too; its own rule of Bloodrage above is what brings the rage back.
            SpellEntry const* pBerserkerStance = GetWarriorStanceSpell(FORM_BERSERKERSTANCE);
            if (pBerserkerStance &&
                !IsWarriorRageShort(pIntercept, pBerserkerStance) &&
                TrySwitchWarriorStance(pBerserkerStance))
                return;
        }
    }

    bool const needMockingBlow = IsWarriorMockingBlowWanted(pVictim);

    // What the two rules of the clap have in common: the spell, its cooldown, the rage for its cost and for the
    // stance change, and a target that the spell is not refused by.
    bool const canPayThunderClap =
        m_spells.warrior.pThunderClap &&
        me->IsSpellReady(m_spells.warrior.pThunderClap) &&
        me->GetPower(POWER_RAGE) >= 20 * PB_RAGE_PER_POINT &&
        !pVictim->IsImmuneToSpell(m_spells.warrior.pThunderClap, false);

    // The opening of a charge is the one case that asks how many mobs are coming, and the count is only walked
    // while that opening is still asking. The red name is the filter of the count, as the maintainer asks, because
    // the mobs of that pack are the ones that the tank is about to hold.
    // The same opening looks for a neutral at peace within the area that the clap covers, and holds the clap back
    // when one is there, because the debuff of the clap would wake it into a second fight that the tank did not ask
    // for. A neutral that is fighting already is in that fight whatever the clap does to it, so it is no reason to
    // hold the clap back, and it is not counted by the red-name filter either: the pack of three is the pack of
    // mobs that the tank goes to hold. That look is asked at both of the points about which the core may draw the
    // area of a spell of this kind, the tank and its target, because which of the two a spell uses is in the spell
    // data of the running patch and not here; a neutral that the clap could wake at either point is reason enough
    // to hold it back. The look for neutrals is asked after the pack is counted because it walks the cells of the
    // map again, and the pack of three is the rarer answer of the two.
    bool needOpeningThunderClap = false;
    if (m_warriorChargeOpeningThunderClap && canPayThunderClap)
    {
        std::list<Unit*> nearbyEnemies;
        me->GetEnemyListInRadiusAround(pVictim, 10.0f, nearbyEnemies);
        uint8 redNamePackCount = 0;
        for (Unit* pEnemy : nearbyEnemies)
            if (pEnemy && me->IsHostileTo(pEnemy) && IsValidHostileTarget(pEnemy))
                ++redNamePackCount;

        needOpeningThunderClap = redNamePackCount >= PB_WARRIOR_THUNDER_CLAP_PACK_COUNT &&
            !HasWakingNeutralNear(pVictim, PB_WARRIOR_THUNDER_CLAP_NEUTRAL_WATCH_DIST) &&
            !HasWakingNeutralNear(me, PB_WARRIOR_THUNDER_CLAP_NEUTRAL_WATCH_DIST);
    }

    bool const needThunderClap = canPayThunderClap &&
        (needOpeningThunderClap ||
         (IsSkullConEliteTarget(pVictim) && !HasThunderClapDebuff(pVictim)));

    // The clap is asked for in Battle Stance, and the tank is brought back to the Defensive Stance by the rule of
    // that stance on the next update, when the clap has been cast or has stopped asking. Neither rule is held to
    // the rank of a talent that keeps rage through a stance change: the charge left the tank in the stance that the
    // clap needs, and the mob whose swings the debuff of the clap lowers is not waited for. Every ordinary skill
    // of the tank stays in Defensive Stance.
    bool const needBattleStance = needMockingBlow || needThunderClap;

    if (needBattleStance)
    {
        if (me->GetShapeshiftForm() != FORM_BATTLESTANCE &&
            m_spells.warrior.pBattleStance)
        {
            if (TrySwitchWarriorStance(m_spells.warrior.pBattleStance))
                return;
        }
    }
    else if (me->GetShapeshiftForm() != FORM_DEFENSIVESTANCE &&
             m_spells.warrior.pDefensiveStance)
    {
        if (TrySwitchWarriorStance(m_spells.warrior.pDefensiveStance))
            return;
    }

    // The Thunder Clap rule is a trigger, not an idle fallback: Shield Slam or
    // Sunder Armor would otherwise spend the rage that the rule asked to keep for
    // the stance switch and the Thunder Clap itself.
    if (needThunderClap &&
        me->GetShapeshiftForm() == FORM_BATTLESTANCE &&
        CanTryToCastSpell(pVictim, m_spells.warrior.pThunderClap))
    {
        if (DoCastSpell(pVictim, m_spells.warrior.pThunderClap) == SPELL_CAST_OK)
        {
            m_warriorChargeOpeningThunderClap = false;
            return;
        }
    }

    // The same holds for the emergency grab: the Battle Stance was switched to for this spell, and the rage that the
    // switch cost is spent on the grab. The cast gate carries the stance rule that the spell itself carries, so a
    // Mocking Blow that is allowed in the stance of the tank is cast without waiting for a switch, and one that is
    // not is cast on the update in which the switch has been made. The taunt fallback does not wait for any of it,
    // and the next update of the rotation asks the Defensive Stance back by itself, since this trigger is answered
    // as soon as the spell is on its cooldown.
    if (needMockingBlow && CanTryToCastSpell(pVictim, m_spells.warrior.pMockingBlow))
    {
        if (DoCastSpell(pVictim, m_spells.warrior.pMockingBlow) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.warrior.pRevenge &&
        CanTryToCastSpell(pVictim, m_spells.warrior.pRevenge))
    {
        if (DoCastSpell(pVictim, m_spells.warrior.pRevenge) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.warrior.pShieldBlock &&
        IsWarriorShieldBlockWanted(pVictim) &&
        CanTryToCastSpell(me, m_spells.warrior.pShieldBlock))
    {
        if (DoCastSpell(me, m_spells.warrior.pShieldBlock) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.warrior.pShieldSlam &&
        CanTryToCastSpell(pVictim, m_spells.warrior.pShieldSlam))
    {
        if (DoCastSpell(pVictim, m_spells.warrior.pShieldSlam) == SPELL_CAST_OK)
            return;
    }

    // Demoralizing Shout outranks Sunder Armor here, as the maintainer orders: the debuff of the shout lowers
    // the damage that the whole party takes, and it is worth more than one more stack of Sunder Armor on a mob
    // that has no shout on it yet. The number of mobs nearby is no condition, because one mob holding the debuff
    // is the whole of the point, and the only thing that stops the shout is a neutral at peace within its reach,
    // which the shout would wake into the fight. The shout is let off at the tank itself, as Battle Shout is,
    // because the area of the spell is the tank and the cast gate would otherwise look for a range that the spell
    // does not have. Nothing here asks for a stance: the stance rule of the spell is applied by the cast gate, and
    // a shout that a stance forbids is left to the stance that the tank stands in. The readiness of the spell is
    // looked at before the mobs around the tank are, because the walk of that look is not worth spending while the
    // cooldown of the shout is running.
    if (m_spells.warrior.pDemoralizingShout &&
        me->IsSpellReady(m_spells.warrior.pDemoralizingShout) &&
        !HasDemoralizingShoutDebuff(pVictim) &&
        !HasWakingNeutralNear(me, PB_WARRIOR_SHOUT_NEUTRAL_WATCH_DIST) &&
        CanTryToCastSpell(me, m_spells.warrior.pDemoralizingShout))
    {
        if (DoCastSpell(me, m_spells.warrior.pDemoralizingShout) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.warrior.pSunderArmor &&
        me->GetPower(POWER_RAGE) < Spell::CalculatePowerCost(m_spells.warrior.pSunderArmor, me) &&
        m_spells.warrior.pBloodrage &&
        CanTryToCastSpell(me, m_spells.warrior.pBloodrage))
    {
        if (DoCastSpell(me, m_spells.warrior.pBloodrage) == SPELL_CAST_OK)
            return;
    }

    if (TryCastForcedWarriorSunder(pVictim))
        return;

    MovementGeneratorType const movementType =
        me->GetMotionMaster()->GetCurrentMovementGeneratorType();
    if (!me->CanReachWithMeleeAutoAttack(pVictim) &&
        (movementType == IDLE_MOTION_TYPE ||
         movementType == FOLLOW_MOTION_TYPE))
        BeginChasing(pVictim);
}

// Whether a unit stands in the arc that the bot faces: what this file calls the front of a character, which the
// core has no test for because the side that a swing lands on is decided on the client. A warrior that stands in
// the middle of a pack does not hit all of it - the weapon of a melee character goes where the character looks -
// and the copy that a Sweeping Strikes makes lands on the mobs that the swing itself could reach. The mobs at the
// back of the bot are therefore not the second target that the spell was asked for, which is the difference this
// reading makes: the pack of a warrior with one mob in front of it and two behind is a pack that the copy of a
// hit never touches.
static bool IsInFrontOfWarrior(Player const* me, Unit const* pUnit)
{
    // Both angles are the 0..2pi of the core, so the difference of them is one turn at most and is brought into
    // the signed half turn by one correction either way.
    float angle = me->GetAngle(pUnit) - me->GetOrientation();
    if (angle > M_PI_F)
        angle -= 2.0f * M_PI_F;
    else if (angle < -M_PI_F)
        angle += 2.0f * M_PI_F;

    return angle <= PB_WARRIOR_FRONT_HALF_ANGLE && angle >= -PB_WARRIOR_FRONT_HALF_ANGLE;
}

// What the area about a warrior that a spell of several mobs would step on holds: the red-name mobs of the pack,
// a neutral at peace that the area would wake, and a mob held by a control that damage breaks. A Whirlwind and a
// bank of rage for a Sweeping Strikes are both asked for at the pack of the melee, and the two looks that the
// shield tank already makes before it lets off a shout are the two reasons not to ask for them: the neutral would
// join a second fight that the party did not start, and the control that another party member is keeping would be
// spoiled by the damage that the spell puts on the held mob. The count is asked only of the red names, because a
// neutral that has joined a fight of its own is not a mob that the spell of the warrior was asked for.
//
// The third count is for the one of those spells that copies a hit rather than drawing its area round the
// warrior: the mobs that a swing of this bot could really land on are the reach of its weapon as the core
// measures it for its own auto attack, in the front that the bot stands in. A Whirlwind is asked at the mobs
// about the bot as it always was; the copy of a Sweeping Strikes is not.
PartyBotAI::WarriorMeleePackRead PartyBotAI::ReadWarriorMeleePack(Unit const* pCenter) const
{
    WarriorMeleePackRead read;
    if (!pCenter)
        return read;

    std::list<Unit*> nearbyUnits;
    me->GetEnemyListInRadiusAround(pCenter, PB_WARRIOR_MULTI_MOB_DIST, nearbyUnits);

    for (Unit* pUnit : nearbyUnits)
    {
        if (!pUnit)
            continue;

        if (IsNeutralAtPeace(pUnit))
            read.neutralAtPeace = true;

        if (IsHeldByDamageBreakingControl(pUnit))
            read.breakableControl = true;

        if (me->GetReactionTo(pUnit) == REP_HOSTILE)
        {
            ++read.redCount;

            if (me->CanReachWithMeleeAutoAttack(pUnit) && IsInFrontOfWarrior(me, pUnit))
                ++read.reachCount;
        }
    }

    return read;
}

// The rules of the rotation of a damage warrior, in the order the maintainer ranks them. What the bot has learned
// is part of the rules: a warrior that has neither of its two talent abilities works with the abilities of its
// weapon and with the armor debuff, and the interrupt of a damage warrior is not its work, so it stands below the
// abilities of the rotation rather than above them. The gates that the core puts in front of a cast are not
// repeated here: which rank the bot knows, whether the mob may be hit at all, and whether the spell is off its
// cooldown belong to the cast gate and to the update below.
void PartyBotAI::CollectWarriorDamageWants(Unit* pVictim, std::vector<WarriorRotationWant>& wants) const
{
    auto AddWant = [&wants](SpellEntry const* pSpell, Unit* pTarget, ShapeshiftForm askedForm,
        bool canChangeStance, bool keepsRage, bool useForcedCast)
    {
        if (!pSpell || !pTarget)
            return;

        WarriorRotationWant want;
        want.pSpell = pSpell;
        want.pTarget = pTarget;
        want.askedForm = askedForm;
        want.canChangeStance = canChangeStance;
        want.keepsRage = keepsRage;
        want.useForcedCast = useForcedCast;
        wants.push_back(want);
    };

    // Berserker Rage is the answer to a fear that holds the bot in the stance that makes its own rage: it takes
    // the fear off and puts rage on the warrior. It is asked for in that stance only, which is the rule of the
    // maintainer and not a gate of the spell.
    if (me->GetShapeshiftForm() == FORM_BERSERKERSTANCE && me->HasAuraType(SPELL_AURA_MOD_FEAR))
        AddWant(m_spells.warrior.pBerserkerRage, me, FORM_NONE, false, false, false);

    // Death Wish is the burst of the warrior, and it is spent on the mob that is long enough to need it: the same
    // reading of an elite that the shield tank uses for its Thunder Clap, which is what the client shows as the
    // skull of a boss and not every mob with an elite frame.
    if (IsSkullConEliteTarget(pVictim))
        AddWant(m_spells.warrior.pDeathWish, me, FORM_NONE, false, false, false);

    // Intercept is the reach of a warrior that has no mount, and it is asked for at the one moment in which
    // nothing else of the rotation has anything to say: the target is beyond a swing of the bot and inside the
    // range that the spell covers, which IsWarriorInterceptWanted() reads from the data of that spell. The place
    // of the want in the rank of the rotation is therefore worth little - the abilities below it are refused by
    // the same distance.
    //
    // The stance that the jump is allowed in is named here rather than read from the data of the spell, because
    // the rule is about what the change of stance costs: a bot that wears that stance jumps where it stands, and
    // a bot in another one is changed only with the talent that keeps its rage through the change. Without that
    // talent the jump is not worth the rage and the global cooldown of a change that the rotation asks back a
    // moment later, which is the same reading that Overpower and the rest of this list carry.
    if (IsWarriorInterceptWanted(pVictim))
        AddWant(m_spells.warrior.pIntercept, pVictim, FORM_BERSERKERSTANCE, GetTacticalMasteryRank() > 0,
            false, false);

    WarriorMeleePackRead const pack = ReadWarriorMeleePack(me);

    // Sweeping Strikes is the hit of the warrior over the pack of the melee. Where the pack is there, the bot
    // takes the Battle Stance for it and waits for the rage that the spell costs, spending nothing of that rage on
    // the wants below: the pack is at its closest in the seconds in which the warrior stands in the middle of it,
    // and the sweep that never came is what the rage spent elsewhere would have bought. The rage that the spell
    // wants is the cost that its own data names; the bot does not spend below it until the rage is there, which is
    // what makes the want a bank rather than an attempt.
    //
    // The pack that the spell is asked for is the pack of the swings and not the pack of the walk: as many mobs
    // as the copied hits would land on, which is the reach of this bot's weapon in the front that the bot faces.
    // A count of the mobs about the warrior was the rule before, and a warrior standing with one mob in front of
    // it and two behind spent its banked rage on a copy that had nothing to hit.
    if (pack.reachCount >= PB_WARRIOR_SWEEPING_STRIKE_TARGETS &&
        !pack.neutralAtPeace && !pack.breakableControl)
        AddWant(m_spells.warrior.pSweepingStrikes, me, FORM_BATTLESTANCE, true, true, false);

    // Overpower is the answer to a dodge of the mob, and it is only asked for in the Battle Stance and never with
    // a change of stance: the rage and the global cooldown of a change cost more than the hit of the ability.
    // Whether the mob is open for it is the state that the data of the spell asks for, and the cast gate refuses
    // the want where the dodge has not happened.
    AddWant(m_spells.warrior.pOverpower, pVictim, FORM_BATTLESTANCE, false, false, false);

    // Execute is the finisher of the mob that is going. The threshold below is the one that the rotation has
    // always asked for; the data of the spell adds its own gate for the ranks that want the mob low. Which stance
    // the spell belongs to is read from that data by the update below, so a bot that keeps the Berserker Stance
    // reaches the finisher by a change of stance and takes its own stance back after the cast.
    if (pVictim->GetHealthPercent() < 20.0f)
        AddWant(m_spells.warrior.pExecute, pVictim, FORM_NONE, true, false, false);

    // The interrupt stands here rather than at the top of the rotation: the damage that a warrior is late for costs
    // the party less than the ability that the same global cooldown would have bought. In the Berserker Stance
    // Pummel is there for nothing; a bot that stands in the Battle Stance changes to the other one only where the
    // rage for both the change and the spell is there and is not wanted by the abilities of the Battle Stance,
    // which is what the window of rage below is for.
    if (pVictim->IsNonMeleeSpellCasted(false, false, true))
    {
        uint32 const rage = me->GetPower(POWER_RAGE);
        if (me->GetShapeshiftForm() == FORM_BERSERKERSTANCE)
            AddWant(m_spells.warrior.pPummel, pVictim, FORM_NONE, false, false, false);
        else if (me->GetShapeshiftForm() == FORM_BATTLESTANCE &&
                 m_spells.warrior.pTacticalMastery &&
                 rage > uint32(PB_WARRIOR_PUMMEL_STANCE_RAGE_MIN * PB_RAGE_PER_POINT) &&
                 rage < uint32(PB_WARRIOR_PUMMEL_STANCE_RAGE_MAX * PB_RAGE_PER_POINT))
            AddWant(m_spells.warrior.pPummel, pVictim, FORM_BERSERKERSTANCE, true, false, false);
    }

    // The shout of attack strength is for the party and for the fight, so it is asked for while the fight lasts and
    // not only before it: a party that has been fighting for a minute is a party that lost the shout on the way.
    // It is never worth a change of stance, and the rage that it costs is the least of the rage the bot spends.
    if (m_spells.warrior.pBattleShout && !me->HasAura(m_spells.warrior.pBattleShout->Id))
        AddWant(m_spells.warrior.pBattleShout, me, FORM_NONE, false, false, false);

    // The two abilities of the talent of the bot stand above the rest of its hits, and neither of them asks for a
    // stance of its own: the stance of the talent was chosen so that the ability of the talent goes off in it, and
    // a change of stance for one hit of the other stance is the rage of two changes.
    AddWant(m_spells.warrior.pMortalStrike, pVictim, FORM_NONE, false, false, false);
    AddWant(m_spells.warrior.pBloodthirst, pVictim, FORM_NONE, false, false, false);

    // Whirlwind is the most damage that the rage of a melee warrior buys at once, and it is asked for where it can
    // be paid for: the rage of fifty points of it rather than the cost of the spell, because the half of a
    // Whirlwind is a stance change spent on nothing. The stance is the one the spell wants, and the pack about the
    // bot must be a pack that the party wants, for the area of the spell takes the neutral at peace into the
    // fight and puts damage on a mob that another player is holding.
    if (me->GetPower(POWER_RAGE) >= uint32(PB_WARRIOR_WHIRLWIND_RAGE * PB_RAGE_PER_POINT) &&
        !pack.neutralAtPeace && !pack.breakableControl)
        AddWant(m_spells.warrior.pWhirlwind, me, FORM_BERSERKERSTANCE, true, false, false);

    bool const hasTalentAbility = m_spells.warrior.pMortalStrike || m_spells.warrior.pBloodthirst;

    // The bleed and the armor debuff are the work of a warrior that has neither of the two abilities above, and
    // they are not limited by what the mob already carries: without a talent ability the bot has nothing else to
    // spend its rage on, and the stacks of the armor debuff are the damage that the party does. The bleed is the
    // work of the Battle Stance; the debuff belongs to the stance that its own data names, which the bot reaches
    // by a change of stance and leaves again in the same way.
    if (!hasTalentAbility)
    {
        AddWant(m_spells.warrior.pRend, pVictim, FORM_BATTLESTANCE, false, false, false);
        AddWant(m_spells.warrior.pSunderArmor, pVictim, FORM_NONE, true, false, true);
    }

    // The rage that is left over is put into the swing of the bot rather than kept: the rage of a warrior that it
    // cannot spend on the abilities above is rage that the next swing would have taken anyway. The armor debuff
    // is the better spend where the bot has it, so the swing is only asked for without that ability.
    if (!m_spells.warrior.pSunderArmor)
        AddWant(m_spells.warrior.pHeroicStrike, pVictim, FORM_NONE, false, false, false);
}

// The rotation of a damage warrior: the wants of the collector above, answered one at a time in their order. The
// answer is not always a cast. Where the stance that a want asks for is not worn, the answer is the stance; where
// the rage that it costs is not there, the answer is Bloodrage, which is what makes the bank of a Sweeping
// Strikes a bank instead of an attempt; and where a want belongs to the stance of the talent, the stance that the
// bot changed into for one earlier spell is taken back before the want is answered. That last part is what
// "keeping" a stance means: the change is made for the spell, and the bot does not go on wearing it.
void PartyBotAI::UpdateWarriorDamageRotation(Unit* pVictim)
{
    std::vector<WarriorRotationWant> wants;
    CollectWarriorDamageWants(pVictim, wants);

    ShapeshiftForm const wornForm = me->GetShapeshiftForm();
    ShapeshiftForm const homeForm = GetWarriorHomeStanceForm();

    for (auto const& want : wants)
    {
        // A want on its own cooldown is not a rule of the stance or of the rage: the bank of a Sweeping Strikes
        // keeps the rage of the bot only while the spell of the bank is ready to spend it.
        if (!me->IsSpellReady(want.pSpell))
            continue;

        // The stance that this want is to be cast in: the one its rule names, and otherwise the one that the data
        // of the spell asks for when the stance that is worn is refused by that data.
        ShapeshiftForm askedForm = want.askedForm;
        if (askedForm == FORM_NONE && want.pSpell->GetErrorAtShapeshiftedCast(wornForm) != SPELL_CAST_OK)
            askedForm = GetSpellStanceRequirement(want.pSpell);

        if (askedForm != FORM_NONE && askedForm != wornForm)
        {
            if (!want.canChangeStance)
                continue;

            SpellEntry const* pStance = GetWarriorStanceSpell(askedForm);
            if (!pStance)
                continue;

            // The change and the spell are paid for together, so that a stance that the bot changed into is not a
            // stance it stands in with nothing to cast.
            if (IsWarriorRageShort(want.pSpell, pStance))
            {
                if (TryWarriorBloodrage())
                    return;

                // The rage of an ordinary want is not worth standing on: the wants below are answered in the
                // stance that the bot wears. The rage that the rotation keeps for one spell is different, and the
                // bank stands until that rage is there.
                if (!want.keepsRage)
                    continue;
            }

            TrySwitchWarriorStance(pStance);
            return;
        }

        // A spell that names no stance is not cast in a stance that the bot changed into for another spell: the
        // stance of the talent is asked back first, which is the other half of the rule that a change of stance is
        // made for one spell and taken back after it.
        // The ask is made for this want only where the want could really be cast in the stance of the talent: a
        // spell that the data of the spell keeps to the other stance would buy a change of stance that ends with
        // itself refused, and the next update would change back again. Two stance asks a second, neither of them
        // with a spell behind it, is worse than a bot that works in the stance it is standing in.
        if (askedForm == FORM_NONE && homeForm != FORM_NONE && wornForm != homeForm &&
            want.pSpell->GetErrorAtShapeshiftedCast(homeForm) == SPELL_CAST_OK)
        {
            // A refused ask does not end the update: a routine that waits for a stance that is not coming never
            // runs at all, which is the lesson the shield tank rules already carry. The want below is answered in
            // the stance that is worn, and the next update asks the stance of the talent back again.
            if (TrySwitchWarriorStance(GetWarriorHomeStanceSpell()))
                return;
        }

        if (IsWarriorRageShort(want.pSpell, nullptr))
        {
            if (TryWarriorBloodrage())
                return;

            if (want.keepsRage)
                return;

            continue;
        }

        // The armor debuff is the one spell of the rotation that adds a stack by being cast again while the mob
        // still carries it, which is what the ordinary gate of the core refuses; the forced cast of the tank is
        // the same instruction, and the rotation of a damage warrior without a talent ability needs it too.
        if (want.useForcedCast)
        {
            if (TryCastForcedWarriorSunder(want.pTarget))
                return;

            continue;
        }

        if (CanTryToCastSpell(want.pTarget, want.pSpell) &&
            DoCastSpell(want.pTarget, want.pSpell) == SPELL_CAST_OK)
            return;
    }

    // Nothing of the rotation is to be done this update: keep the stance of the talent, and stand where a swing of
    // the bot can reach the target.
    if (homeForm != FORM_NONE && wornForm != homeForm)
    {
        if (TrySwitchWarriorStance(GetWarriorHomeStanceSpell()))
            return;
    }

    if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE &&
        !me->CanReachWithMeleeAutoAttack(pVictim))
        BeginChasing(pVictim);
}

void PartyBotAI::UpdateInCombatAI_Warrior()
{
    if (IsProtectionWarriorTank())
    {
        UpdateProtectionWarriorTankCombat();
        return;
    }

    // What is left below is the routine of a warrior that neither carries the tank role nor wears a shield, since
    // a warrior that does is answered by UpdateProtectionWarriorTankCombat() above. No rule here asks for the role
    // or for a shield: the skills of a shield tank have one owner, and a rule that waited for either of them here
    // could never be reached.
    if (Unit* pVictim = me->GetVictim())
        UpdateWarriorDamageRotation(pVictim);
    else // no victim
    {
        if (m_spells.warrior.pBattleShout &&
            CanTryToCastSpell(me, m_spells.warrior.pBattleShout))
        {
            if (DoCastSpell(me, m_spells.warrior.pBattleShout) == SPELL_CAST_OK)
                return;
        }
    }
}

bool PartyBotAI::ShouldEnterStealth() const
{
    if (me->IsMounted())
        return false;

    if (m_rogueVanishRetreat)
        return true;

    if (!m_rogueStealthTargetGuid.IsEmpty())
        return true;

    if (me->GetVictim() || me->InBattleGround() || me->IsFFAPvP())
        return true;

    // A party member that starts a fight has put the group in combat even before this bot has a victim of its own.
    if (GroupInCombat())
        return true;

    if (me->GetHealthPercent() < 10.0f)
        return true;

    if (Player* pLeader = GetPartyLeader())
    {
        if (pLeader->IsDead() || pLeader->IsFeigningDeathSuccessfully() ||
            pLeader->HasAuraType(SPELL_AURA_MOD_STEALTH) ||
            pLeader->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
            return true;
    }

    // Keep a stealth that has already begun until combat or another game rule ends it. In particular, a rogue that
    // is following its leader does not cancel stealth merely because no hostile target has been selected yet.
    return me->HasAuraType(SPELL_AURA_MOD_STEALTH);
}

bool PartyBotAI::EnterStealthIfNeeded(SpellEntry const* pStealthSpell)
{
    if (pStealthSpell)
    {
        bool const shouldStealth = ShouldEnterStealth();

        if (me->HasAura(pStealthSpell->Id))
        {
            if (!shouldStealth)
                me->RemoveAurasDueToSpellByCancel(pStealthSpell->Id);
        }
        else
        {
            if (shouldStealth &&
                CanTryToCastSpell(me, pStealthSpell) &&
                DoCastSpell(me, pStealthSpell) == SPELL_CAST_OK)
                return true;
        }
    }

    return false;
}

bool PartyBotAI::PrepareStealthOpener()
{
    if (me->IsInCombat())
        return false;

    if (me->GetClass() == CLASS_ROGUE &&
        m_spells.rogue.pStealth &&
        (m_spells.rogue.pCheapShot || m_spells.rogue.pGarrote) &&
        !me->HasAuraType(SPELL_AURA_MOD_STEALTH))
    {
        if (me->HasGCD(m_spells.rogue.pStealth))
            return true;

        return CanTryToCastSpell(me, m_spells.rogue.pStealth) &&
            DoCastSpell(me, m_spells.rogue.pStealth) == SPELL_CAST_OK;
    }

    if (me->GetClass() == CLASS_DRUID &&
        GetRole() == ROLE_MELEE_DPS &&
        m_spells.druid.pCatForm &&
        m_spells.druid.pProwl)
    {
        ShapeshiftForm const form = me->GetShapeshiftForm();
        if (form == FORM_NONE)
        {
            if (me->HasGCD(m_spells.druid.pCatForm))
                return true;

            return EnterCombatDruidForm();
        }

        if (form == FORM_CAT && !me->HasAuraType(SPELL_AURA_MOD_STEALTH))
        {
            if (me->HasGCD(m_spells.druid.pProwl))
                return true;

            return CanTryToCastSpell(me, m_spells.druid.pProwl) &&
                DoCastSpell(me, m_spells.druid.pProwl) == SPELL_CAST_OK;
        }
    }

    return false;
}

void PartyBotAI::UpdateOutOfCombatAI_Rogue()
{
    if (m_spells.rogue.pMainHandPoison &&
        CanTryToCastSpell(me, m_spells.rogue.pMainHandPoison))
    {
        if (CastWeaponBuff(m_spells.rogue.pMainHandPoison, EQUIPMENT_SLOT_MAINHAND) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.rogue.pOffHandPoison &&
        CanTryToCastSpell(me, m_spells.rogue.pOffHandPoison))
    {
        if (CastWeaponBuff(m_spells.rogue.pOffHandPoison, EQUIPMENT_SLOT_OFFHAND) == SPELL_CAST_OK)
            return;
    }

    if (EnterStealthIfNeeded(m_spells.rogue.pStealth))
        return;

    if (me->GetVictim())
        UpdateInCombatAI_Rogue();
}

void PartyBotAI::UpdateInCombatAI_Rogue()
{
    Unit* pVictim = me->GetVictim();
    if (!pVictim && !m_rogueStealthTargetGuid.IsEmpty())
        pVictim = me->GetMap()->GetUnit(m_rogueStealthTargetGuid);

    if (!pVictim || !IsValidHostileTarget(pVictim))
    {
        bool const hadPendingTarget = !m_rogueStealthTargetGuid.IsEmpty();
        m_rogueStealthTargetGuid.Clear();
        if (hadPendingTarget)
            me->GetMotionMaster()->Clear(false, true);
        return;
    }

    if (!me->HasAuraType(SPELL_AURA_MOD_STEALTH) && !m_rogueStealthTargetGuid.IsEmpty())
    {
        // Stealth was lost before the opener could be cast, so the pending target becomes a normal attack.
        m_rogueStealthTargetGuid.Clear();
        me->GetMotionMaster()->Clear(false, true);
        me->Attack(pVictim, true);
        BeginChasing(pVictim);
    }

    if (me->HasAuraType(SPELL_AURA_MOD_STEALTH))
    {
        SpellEntry const* pOpener = m_spells.rogue.pCheapShot ?
            m_spells.rogue.pCheapShot : m_spells.rogue.pGarrote;

        if (pOpener)
        {
            if (CanTryToCastSpell(pVictim, pOpener) &&
                DoCastSpell(pVictim, pOpener) == SPELL_CAST_OK)
            {
                // The opener was held back from the auto attack so that stealth could still be used.
                m_rogueStealthTargetGuid.Clear();
                me->GetMotionMaster()->Clear(false, true);
                me->Attack(pVictim, true);
                BeginChasing(pVictim);
                return;
            }

            // The bot is already moving behind the target if Garrote needs it. Do not break stealth with an auto
            // attack while the opener is waiting for range or position.
            return;
        }

        // With neither opener learned, begin the ordinary attack and let stealth break.
        m_rogueStealthTargetGuid.Clear();
        me->Attack(pVictim, true);
    }
    else if (m_spells.rogue.pVanish &&
            (me->GetHealthPercent() < 15.0f) &&
            CanTryToCastSpell(me, m_spells.rogue.pVanish))
    {
        if (DoCastSpell(me, m_spells.rogue.pVanish) == SPELL_CAST_OK)
        {
            m_rogueVanishRetreat = true;
            me->AttackStop();
            RunAwayFromTarget(pVictim);
            return;
        }
    }

    uint32 const comboPoints = me->GetComboPoints();
    float const healthPercent = pVictim->GetHealthPercent();

    if (m_spells.rogue.pSliceAndDice &&
        comboPoints >= 2 &&
        healthPercent > 10.0f &&
        !me->HasAura(m_spells.rogue.pSliceAndDice->Id) &&
        CanTryToCastSpell(pVictim, m_spells.rogue.pSliceAndDice))
    {
        if (DoCastSpell(pVictim, m_spells.rogue.pSliceAndDice) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.rogue.pEviscerate &&
        comboPoints >= 3 &&
        healthPercent < 10.0f &&
        CanTryToCastSpell(pVictim, m_spells.rogue.pEviscerate))
    {
        if (DoCastSpell(pVictim, m_spells.rogue.pEviscerate) == SPELL_CAST_OK)
            return;
    }

    if (comboPoints >= 5 && healthPercent > 10.0f)
    {
        SpellAuraHolder* pSliceAndDice = m_spells.rogue.pSliceAndDice ?
            me->GetSpellAuraHolder(m_spells.rogue.pSliceAndDice->Id) : nullptr;

        if (m_spells.rogue.pSliceAndDice &&
            (!pSliceAndDice || pSliceAndDice->GetAuraDuration() < 10000) &&
            CanTryToCastSpell(pVictim, m_spells.rogue.pSliceAndDice))
        {
            if (DoCastSpell(pVictim, m_spells.rogue.pSliceAndDice) == SPELL_CAST_OK)
                return;
        }
        else if (m_spells.rogue.pEviscerate &&
                 pSliceAndDice &&
                 pSliceAndDice->GetAuraDuration() >= 10000 &&
                 CanTryToCastSpell(pVictim, m_spells.rogue.pEviscerate))
        {
            if (DoCastSpell(pVictim, m_spells.rogue.pEviscerate) == SPELL_CAST_OK)
                return;
        }
    }

    if (m_spells.rogue.pBlind)
    {
        if (Unit* pTarget = SelectAttackerDifferentFrom(pVictim))
        {
            if (CanTryToCastSpell(pTarget, m_spells.rogue.pBlind) &&
                CanUseCrowdControl(m_spells.rogue.pBlind, pTarget))
            {
                if (DoCastSpell(pTarget, m_spells.rogue.pBlind) == SPELL_CAST_OK)
                {
                    me->AttackStop();
                    AttackStart(pVictim);
                    return;
                }
            }
        }
    }

    Creature* pCreature = pVictim->ToCreature();
    bool const dangerousElite =
        pCreature && pCreature->IsElite() &&
        pVictim->GetLevel() >= me->GetLevel() + 2;
    if (m_spells.rogue.pAdrenalineRush &&
        dangerousElite &&
        me->GetPower(POWER_ENERGY) < 20 &&
        CanTryToCastSpell(me, m_spells.rogue.pAdrenalineRush))
    {
        if (DoCastSpell(me, m_spells.rogue.pAdrenalineRush) == SPELL_CAST_OK)
            return;
    }

    if (pVictim->IsNonMeleeSpellCasted() &&
        m_spells.rogue.pKick &&
        CanTryToCastSpell(pVictim, m_spells.rogue.pKick))
    {
        if (DoCastSpell(pVictim, m_spells.rogue.pKick) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.rogue.pBladeFlurry &&
        CountUnmarkedEnemiesInRange(5.0f) >= 2 &&
        CanTryToCastSpell(me, m_spells.rogue.pBladeFlurry))
    {
        if (DoCastSpell(me, m_spells.rogue.pBladeFlurry) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.rogue.pColdBlood &&
        CanTryToCastSpell(me, m_spells.rogue.pColdBlood))
    {
        DoCastSpell(me, m_spells.rogue.pColdBlood);
    }

    if (m_spells.rogue.pEvasion &&
        me->GetHealthPercent() < 80.0f &&
        CountNonRangedAttackersInRange(5.0f) >= 2 &&
        CanTryToCastSpell(me, m_spells.rogue.pEvasion))
    {
        if (DoCastSpell(me, m_spells.rogue.pEvasion) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.rogue.pRiposte &&
        CanTryToCastSpell(pVictim, m_spells.rogue.pRiposte))
    {
        if (DoCastSpell(pVictim, m_spells.rogue.pRiposte) == SPELL_CAST_OK)
            return;
    }

    if (m_spells.rogue.pSinisterStrike &&
        CanTryToCastSpell(pVictim, m_spells.rogue.pSinisterStrike))
    {
        if (DoCastSpell(pVictim, m_spells.rogue.pSinisterStrike) == SPELL_CAST_OK)
            return;
    }

    if (me->IsInCombat() &&
        m_spells.rogue.pSprint &&
        !me->HasUnitState(UNIT_STATE_ROOT) &&
        me->GetCombatDistance(pVictim) > 20.0f &&
        CanTryToCastSpell(me, m_spells.rogue.pSprint))
    {
        if (DoCastSpell(me, m_spells.rogue.pSprint) == SPELL_CAST_OK)
            return;
    }
}

bool PartyBotAI::EnterCombatDruidForm()
{
    if (m_spells.druid.pCatForm &&
        GetRole() == ROLE_MELEE_DPS &&
        CanTryToCastSpell(me, m_spells.druid.pCatForm))
    {
        if (DoCastSpell(me, m_spells.druid.pCatForm) == SPELL_CAST_OK)
            return true;
    }

    if (m_spells.druid.pBearForm &&
       (m_role == ROLE_TANK || GetRole() == ROLE_MELEE_DPS) &&
        CanTryToCastSpell(me, m_spells.druid.pBearForm))
    {
        if (DoCastSpell(me, m_spells.druid.pBearForm) == SPELL_CAST_OK)
            return true;
    }

    if (m_spells.druid.pMoonkinForm &&
        GetRole() == ROLE_RANGE_DPS &&
        CanTryToCastSpell(me, m_spells.druid.pMoonkinForm))
    {
        if (DoCastSpell(me, m_spells.druid.pMoonkinForm) == SPELL_CAST_OK)
            return true;
    }

    return false;
}

void PartyBotAI::UpdateOutOfCombatAI_Druid()
{
    // Make sure bot leaves combat form if his role is changed to healer.
    if (GetRole() == ROLE_HEALER && me->GetShapeshiftForm() != FORM_NONE &&
        me->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
    {
        me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
        return;
    }

    SpellEntry const* pBuffSpell = nullptr;
    if (Player* pTarget = SelectBuffTarget(m_spells.druid.pMarkoftheWild, m_spells.druid.pGiftoftheWild, pBuffSpell))
    {
        if (CanTryToCastSpell(pTarget, pBuffSpell))
        {
            if (DoCastSpell(pTarget, pBuffSpell) == SPELL_CAST_OK)
            {
                m_isBuffing = true;
                me->ClearTarget();
                return;
            }
        }
    }

    if (m_spells.druid.pThorns)
    {
        if (Player* pTarget = SelectBuffTarget(m_spells.druid.pThorns))
        {
            if (CanTryToCastSpell(pTarget, m_spells.druid.pThorns))
            {
                if (DoCastSpell(pTarget, m_spells.druid.pThorns) == SPELL_CAST_OK)
                {
                    m_isBuffing = true;
                    me->ClearTarget();
                    return;
                }
            }
        }
    }

    if (m_spells.druid.pNaturesGrasp &&
        CanTryToCastSpell(me, m_spells.druid.pNaturesGrasp))
    {
        if (DoCastSpell(me, m_spells.druid.pNaturesGrasp) == SPELL_CAST_OK)
            return;
    }

    if (m_isBuffing &&
       (!m_spells.druid.pMarkoftheWild ||
        !me->HasGCD(m_spells.druid.pMarkoftheWild)))
    {
        m_isBuffing = false;
    }

    if (me->GetShapeshiftForm() == FORM_NONE)
    {
        if (EnterCombatDruidForm())
            return;

        if ((me->GetPowerPercent(POWER_MANA) > 80.0f) &&
            FindAndHealInjuredAlly())
            return;
    }
    else if (me->GetShapeshiftForm() == FORM_CAT)
    {
        if (EnterStealthIfNeeded(m_spells.druid.pProwl))
            return;
    }

    if (me->GetVictim())
        UpdateInCombatAI_Druid();
}

void PartyBotAI::UpdateInCombatAI_Druid()
{
    ShapeshiftForm const form = me->GetShapeshiftForm();

    if (m_spells.druid.pBarkskin &&
        (form == FORM_NONE || form == FORM_MOONKIN) &&
        (me->GetHealthPercent() < 50.0f) &&
        CanTryToCastSpell(me, m_spells.druid.pBarkskin))
    {
        if (DoCastSpell(me, m_spells.druid.pBarkskin) == SPELL_CAST_OK)
            return;
    }

    if (form == FORM_NONE)
    {
        if (m_spells.druid.pHibernate &&
            m_role != ROLE_TANK &&
            !me->GetAttackers().empty())
        {
            Unit* pAttacker = *me->GetAttackers().begin();
            if (CanTryToCastSpell(pAttacker, m_spells.druid.pHibernate) &&
                CanUseCrowdControl(m_spells.druid.pHibernate, pAttacker))
            {
                if (DoCastSpell(pAttacker, m_spells.druid.pHibernate) == SPELL_CAST_OK)
                    return;
            }
        }

        // Prioritize applying HoTs.
        if (Unit* pTarget = SelectPeriodicHealTarget(80.0f, 90.0f))
            if (HealInjuredTargetPeriodic(pTarget))
                return;

        // Direct heal.
        if (Unit* pTarget = SelectHealTarget(60.0f, 70.0f))
            if (HealInjuredTargetDirect(pTarget))
                return;

        if (m_spells.druid.pInnervate &&
           (me->GetHealthPercent() > 40.0f) &&
           (me->GetPowerPercent(POWER_MANA) < 10.0f) &&
            CanTryToCastSpell(me, m_spells.druid.pInnervate))
        {
            if (DoCastSpell(me, m_spells.druid.pInnervate) == SPELL_CAST_OK)
                return;
        }

        if (GetRole() == ROLE_HEALER && FindAndPreHealTarget())
            return;

        if (EnterCombatDruidForm())
            return;
    }

    Unit* pVictim = me->GetVictim();
    if (!pVictim)
        return;

    if (form != FORM_NONE &&
        me->HasUnitState(UNIT_STATE_ROOT) &&
        me->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT) &&
        (m_role != ROLE_TANK || !me->CanReachWithMeleeAutoAttack(pVictim)))
        me->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

    if (GetRole() == ROLE_HEALER)
        return;

    switch (form)
    {
        case FORM_CAT:
        {
            if (me->HasDistanceCasterMovement())
                me->SetCasterChaseDistance(0.0f);

            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE
                && !me->CanReachWithMeleeAutoAttack(pVictim))
            {
                BeginChasing(pVictim);
            }

            if (me->HasAuraType(SPELL_AURA_MOD_STEALTH))
            {
                if (m_spells.druid.pPounce &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pPounce))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pPounce) == SPELL_CAST_OK)
                        return;
                }
                if (m_spells.druid.pRavage &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pRavage))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pRavage) == SPELL_CAST_OK)
                        return;
                }
                if (m_spells.druid.pTigersFury &&
                    CanTryToCastSpell(me, m_spells.druid.pTigersFury))
                {
                    if (DoCastSpell(me, m_spells.druid.pTigersFury) == SPELL_CAST_OK)
                        return;
                }
                return;
            }

            if (m_spells.druid.pCower &&
                GetAttackersInRangeCount(8.0f))
            {
                Unit* pAttacker = *me->GetAttackers().begin();
                if (CanTryToCastSpell(me, m_spells.druid.pCower))
                {
                    if (DoCastSpell(me, m_spells.druid.pCower) == SPELL_CAST_OK)
                        return;
                }
            }

            if (me->GetComboPoints() > 4)
            {
                if (m_spells.druid.pFerociousBite &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pFerociousBite))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pFerociousBite) == SPELL_CAST_OK)
                        return;
                }

                if (m_spells.druid.pRip &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pRip))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pRip) == SPELL_CAST_OK)
                        return;
                }
            }

            if (!me->CanReachWithMeleeAutoAttack(pVictim))
            {
                if (m_spells.druid.pFaerieFireFeral &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pFaerieFireFeral))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pFaerieFireFeral) == SPELL_CAST_OK)
                        return;
                }

                if (m_spells.druid.pDash &&
                    pVictim->IsMoving() &&
                    CanTryToCastSpell(me, m_spells.druid.pDash))
                {
                    if (DoCastSpell(me, m_spells.druid.pDash) == SPELL_CAST_OK)
                        return;
                }
            }

            if (m_spells.druid.pShred &&
                CanTryToCastSpell(pVictim, m_spells.druid.pShred))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pShred) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pRake &&
                CanTryToCastSpell(pVictim, m_spells.druid.pRake))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pRake) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pClaw &&
                CanTryToCastSpell(pVictim, m_spells.druid.pClaw))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pClaw) == SPELL_CAST_OK)
                    return;
            }

            break;
        }
        case FORM_BEAR:
        case FORM_DIREBEAR:
        {
            if (me->HasDistanceCasterMovement())
                me->SetCasterChaseDistance(0.0f);

            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE
                && !me->CanReachWithMeleeAutoAttack(pVictim))
            {
                BeginChasing(pVictim);
            }

            if (m_spells.druid.pFeralCharge &&
                CanTryToCastSpell(pVictim, m_spells.druid.pFeralCharge))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pFeralCharge) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pBash &&
                CanTryToCastSpell(pVictim, m_spells.druid.pBash))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pBash) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pFrenziedRegeneration &&
                (me->GetHealthPercent() < 30.0f) &&
                CanTryToCastSpell(me, m_spells.druid.pFrenziedRegeneration))
            {
                if (DoCastSpell(me, m_spells.druid.pFrenziedRegeneration) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pFaerieFireFeral &&
                CanTryToCastSpell(pVictim, m_spells.druid.pFaerieFireFeral))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pFaerieFireFeral) == SPELL_CAST_OK)
                    return;
            }

            if ((me->GetPower(POWER_RAGE) > 800) ||
                (GetAttackersInRangeCount(10.0f) > 1))
            {
                if (m_spells.druid.pDemoralizingRoar &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pDemoralizingRoar))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pDemoralizingRoar) == SPELL_CAST_OK)
                        return;
                }

                if (m_spells.druid.pSwipe &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pSwipe))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pSwipe) == SPELL_CAST_OK)
                        return;
                }
            }

            if (m_spells.druid.pMaul &&
                CanTryToCastSpell(pVictim, m_spells.druid.pMaul))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pMaul) == SPELL_CAST_OK)
                    return;
            }
            break;
        }
        case FORM_NONE:
        case FORM_MOONKIN:
        {
            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE &&
                me->GetDistance(pVictim) > 30.0f)
            {
                BeginChasing(pVictim);
            }
            else if (pVictim->CanReachWithMeleeAutoAttack(me) &&
                    !me->HasUnitState(UNIT_STATE_ROOT) &&
                    (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != DISTANCING_MOTION_TYPE))
            {
                if (m_spells.druid.pEntanglingRoots &&
                    CanTryToCastSpell(pVictim, m_spells.druid.pEntanglingRoots))
                {
                    if (DoCastSpell(pVictim, m_spells.druid.pEntanglingRoots) == SPELL_CAST_OK)
                        return;
                }
                // The roots are for an enemy that is not held yet, so they are asked for before the bot asks
                // itself whether walking away is worth it.
                if (CanRunAwayFrom(pVictim))
                {
                    me->SetCasterChaseDistance(GetChaseDistance());
                    if (RunAwayFromTarget(pVictim))
                        return;
                }
            }

            if (m_spells.druid.pFaerieFire &&
               (pVictim->GetClass() == CLASS_ROGUE) &&
                CanTryToCastSpell(pVictim, m_spells.druid.pFaerieFire))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pFaerieFire) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pInsectSwarm &&
                CanTryToCastSpell(pVictim, m_spells.druid.pInsectSwarm))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pInsectSwarm) == SPELL_CAST_OK)
                    return;
            }

            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == DISTANCING_MOTION_TYPE)
                return;

            if (m_spells.druid.pHurricane &&
               (me->GetEnemyCountInRadiusAround(pVictim, 10.0f) > 2) &&
                CanTryToCastSpell(pVictim, m_spells.druid.pHurricane))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pHurricane) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pMoonfire &&
                CanTryToCastSpell(pVictim, m_spells.druid.pMoonfire))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pMoonfire) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pStarfire &&
               (pVictim->GetHealthPercent() > 50.0f) &&
                CanTryToCastSpell(pVictim, m_spells.druid.pStarfire))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pStarfire) == SPELL_CAST_OK)
                    return;
            }

            if (m_spells.druid.pWrath &&
                CanTryToCastSpell(pVictim, m_spells.druid.pWrath))
            {
                if (DoCastSpell(pVictim, m_spells.druid.pWrath) == SPELL_CAST_OK)
                    return;
            }

            break;
        }
    }
}
