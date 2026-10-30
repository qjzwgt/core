#include "World.h"
#include "CombatBotBaseAI.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Group.h"
#include "PlayerBotMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Chat.h"
#include "CharacterDatabaseCache.h"
#include "BotGearScoring.h"
#include "BotTalentBuild.h"
#include "Utilities/Random.h"

#include <random>

enum CombatBotSpells
{
    SPELL_MAIL_PROFICIENCY = 8737,
    SPELL_PLATE_PROFICIENCY = 750,

    SPELL_SHIELD_SLAM = 23922,
    SPELL_HOLY_SHIELD = 20925,
    SPELL_SANCTITY_AURA = 20218,
    SPELL_SHADOWFORM = 15473,
    SPELL_ELEMENTAL_MASTERY = 16166,
    SPELL_STORMSTRIKE = 17364,
    SPELL_MOONKIN_FORM = 24858,
    SPELL_LEADER_OF_THE_PACK = 17007,

    SPELL_SUMMON_IMP = 688,
    SPELL_SUMMON_VOIDWALKER = 697,
    SPELL_SUMMON_FELHUNTER = 691,
    SPELL_SUMMON_SUCCUBUS = 712,
    SPELL_TAME_BEAST = 13481,
    SPELL_REVIVE_PET = 982,
    SPELL_CALL_PET = 883,

    PET_WOLF    = 565,
    PET_CAT     = 681,
    PET_BEAR    = 822,
    PET_CRAB    = 831,
    PET_GORILLA = 1108,
    PET_BIRD    = 1109,
    PET_BOAR    = 1190,
    PET_BAT     = 1554,
    PET_CROC    = 1693,
    PET_SPIDER  = 1781,
    PET_OWL     = 1997,
    PET_STRIDER = 2322,
    PET_SCORPID = 3127,
    PET_SERPENT = 3247,
    PET_RAPTOR  = 3254,
    PET_TURTLE  = 3461,
    PET_HYENA   = 4127,
};

void CombatBotBaseAI::AutoAssignRole()
{
    switch (me->GetClass())
    {
        case CLASS_WARRIOR:
        {
            if (me->HasSpell(SPELL_SHIELD_SLAM))
                m_role = ROLE_TANK;
            else
                m_role = ROLE_MELEE_DPS;
            return;
        }
        case CLASS_ROGUE:
        {
            m_role = ROLE_MELEE_DPS;
            return;
        }
        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        {
            m_role = ROLE_RANGE_DPS;
            return;
        }
        case CLASS_PALADIN:
        {
            if (me->HasSpell(SPELL_HOLY_SHIELD))
                m_role = ROLE_TANK;
            else if (me->HasSpell(SPELL_SANCTITY_AURA))
                m_role = ROLE_MELEE_DPS;
            else
                m_role = ROLE_HEALER;
            return;
        }
        case CLASS_PRIEST:
        {
            if (me->HasSpell(SPELL_SHADOWFORM))
                m_role = ROLE_RANGE_DPS;
            else
                m_role = ROLE_HEALER;
            return;
        }
        case CLASS_SHAMAN:
        {
            if (me->HasSpell(SPELL_ELEMENTAL_MASTERY))
                m_role = ROLE_RANGE_DPS;
            else if (me->HasSpell(SPELL_STORMSTRIKE))
                m_role = ROLE_MELEE_DPS;
            else
                m_role = ROLE_HEALER;
            return;
        }
        case CLASS_DRUID:
        {
            if (me->HasSpell(SPELL_MOONKIN_FORM))
                m_role = ROLE_RANGE_DPS;
            else if (me->HasSpell(SPELL_LEADER_OF_THE_PACK))
                m_role = ROLE_MELEE_DPS;
            else
                m_role = ROLE_HEALER;
            return;
        }
    }

    m_role = ROLE_MELEE_DPS;
}

void CombatBotBaseAI::ResetSpellData()
{
    for (auto& ptr : m_spells.raw.spells)
        ptr = nullptr;

    m_resurrectionSpell = nullptr;
    m_spellListDirectHeal.clear();
    m_spellListPeriodicHeal.clear();
    m_spellListTaunt.clear();
}

void CombatBotBaseAI::PopulateSpellData()
{
    // Paladin Seals
    SpellEntry const* pSealOfRighteousness = nullptr;
    SpellEntry const* pSealOfCommand = nullptr;
    SpellEntry const* pSealOfFury = nullptr;

    // Paladin Blessings
    SpellEntry const* pBlessingOfLight = nullptr;
    SpellEntry const* pBlessingOfMight = nullptr;
    SpellEntry const* pBlessingOfWisdom = nullptr;
    SpellEntry const* pBlessingOfKings = nullptr;
    SpellEntry const* pBlessingOfSanctuary = nullptr;

    // Paladin Auras
    SpellEntry const* pDevotionAura = nullptr;
    SpellEntry const* pConcentrationAura = nullptr;
    SpellEntry const* pRetributionAura = nullptr;
    SpellEntry const* pSanctityAura = nullptr;
    SpellEntry const* pShadowResistanceAura = nullptr;
    SpellEntry const* pFrostResistanceAura = nullptr;
    SpellEntry const* pFireResistanceAura = nullptr;

    // Air Totems
    SpellEntry const* pGraceOfAirTotem = nullptr;
    SpellEntry const* pNatureResistanceTotem = nullptr;
    SpellEntry const* pWindfuryTotem = nullptr;
    SpellEntry const* pWindwallTotem = nullptr;
    SpellEntry const* pTranquilAirTotem = nullptr;

    // Earth Totems
    SpellEntry const* pEarthbindTotem = nullptr;
    SpellEntry const* pStoneclawtotem = nullptr;
    SpellEntry const* pStoneskinTotem = nullptr;
    SpellEntry const* pStrengthOfEarthTotem = nullptr;
    SpellEntry const* pTremorTotem = nullptr;

    // Fire Totems
    SpellEntry const* pFireNovaTotem = nullptr;
    SpellEntry const* pMagmaTotem = nullptr;
    SpellEntry const* pSearingTotem = nullptr;
    SpellEntry const* pFlametongueTotem = nullptr;
    SpellEntry const* pFrostResistanceTotem = nullptr;

    // Water Totems
    SpellEntry const* pFireResistanceTotem = nullptr;
    SpellEntry const* pDiseaseCleansingTotem = nullptr;
    SpellEntry const* pHealingStreamTotem = nullptr;
    SpellEntry const* pManaSpringTotem = nullptr;
    SpellEntry const* pPoisonCleansingTotem = nullptr;

    // Shaman Weapon Buffs
    SpellEntry const* pFrostbrandWeapon = nullptr;
    SpellEntry const* pRockbiterWeapon = nullptr;
    SpellEntry const* pWindfuryWeapon = nullptr;

    // Mage Polymorph
    SpellEntry const* pPolymorphSheep = nullptr;
    SpellEntry const* pPolymorphCow = nullptr;
    SpellEntry const* pPolymorphPig = nullptr;
    SpellEntry const* pPolymorphTurtle = nullptr;

    // Mage Frost Armor (to replace ice armor at low level)
    SpellEntry const* pFrostArmor = nullptr;

    bool hasDeadlyPoison = false;
    bool hasInstantPoison = false;
    bool hasCripplingPoison = false;
    bool hasWoundPoison = false;
    bool HasMindNumbingPoison = false;

    for (const auto& spell : me->GetSpellMap())
    {
        if (spell.second.disabled)
            continue;

        if (spell.second.state == PLAYERSPELL_REMOVED)
            continue;

        SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(spell.first);
        if (!pSpellEntry)
            continue;

        if (pSpellEntry->HasAttribute(SPELL_ATTR_PASSIVE))
            continue;

        if (pSpellEntry->HasAttribute(SPELL_ATTR_DO_NOT_DISPLAY))
            continue;

        auto IsHigherRankSpell = [pSpellEntry](SpellEntry const* pOldSpell)
        {
            if (!pOldSpell)
                return true;

            uint32 newRank = pSpellEntry->GetRank();
            if (newRank)
                return newRank > pOldSpell->GetRank();

            return pSpellEntry->Id > pOldSpell->Id;
        };

        switch (me->GetClass())
        {
            case CLASS_PALADIN:
            {
                if (pSpellEntry->SpellName[0].find("Seal of Righteousness") != std::string::npos)
                {
                    if (IsHigherRankSpell(pSealOfRighteousness))
                        pSealOfRighteousness = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Seal of Command") != std::string::npos)
                {
                    if (IsHigherRankSpell(pSealOfCommand))
                        pSealOfCommand = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Judgement") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pJudgement))
                        m_spells.paladin.pJudgement = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hammer of Justice") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pHammerOfJustice))
                        m_spells.paladin.pHammerOfJustice = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Sacrifice") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pBlessingOfSacrifice))
                        m_spells.paladin.pBlessingOfSacrifice = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Freedom") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pBlessingOfFreedom))
                        m_spells.paladin.pBlessingOfFreedom = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Protection") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pBlessingOfProtection))
                        m_spells.paladin.pBlessingOfProtection = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Sanctuary") != std::string::npos)
                {
                    if (IsHigherRankSpell(pBlessingOfSanctuary))
                        pBlessingOfSanctuary = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Kings") != std::string::npos)
                {
                    if (IsHigherRankSpell(pBlessingOfKings))
                        pBlessingOfKings = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Wisdom") != std::string::npos)
                {
                    if (IsHigherRankSpell(pBlessingOfWisdom))
                        pBlessingOfWisdom = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Might") != std::string::npos)
                {
                    if (IsHigherRankSpell(pBlessingOfMight))
                        pBlessingOfMight = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blessing of Light") != std::string::npos)
                {
                    if (IsHigherRankSpell(pBlessingOfLight))
                        pBlessingOfLight = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Devotion Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pDevotionAura))
                        pDevotionAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Retribution Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pRetributionAura))
                        pRetributionAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Concentration Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pConcentrationAura))
                        pConcentrationAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Sanctity Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pSanctityAura))
                        pSanctityAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadow Resistance Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pShadowResistanceAura))
                        pShadowResistanceAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frost Resistance Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFrostResistanceAura))
                        pFrostResistanceAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fire Resistance Aura") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFireResistanceAura))
                        pFireResistanceAura = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Exorcism") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pExorcism))
                        m_spells.paladin.pExorcism = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Consecration") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pConsecration))
                        m_spells.paladin.pConsecration = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hammer of Wrath") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pHammerOfWrath))
                        m_spells.paladin.pHammerOfWrath = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cleanse") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pCleanse))
                        m_spells.paladin.pCleanse = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Divine Shield") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pDivineShield))
                        m_spells.paladin.pDivineShield = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Lay on Hands") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pLayOnHands))
                        m_spells.paladin.pLayOnHands = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Righteous Fury") != std::string::npos) // post 1.9
                {
                    if (IsHigherRankSpell(m_spells.paladin.pRighteousFury))
                        m_spells.paladin.pRighteousFury = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Seal of Fury") != std::string::npos) // pre 1.9
                {
                    if (IsHigherRankSpell(pSealOfFury))
                        pSealOfFury = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Holy Shock") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pHolyShock))
                        m_spells.paladin.pHolyShock = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Divine Favor") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pDivineFavor))
                        m_spells.paladin.pDivineFavor = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Holy Wrath") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pHolyWrath))
                        m_spells.paladin.pHolyWrath = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Turn Evil") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pTurnEvil))
                        m_spells.paladin.pTurnEvil = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Holy Shield") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.paladin.pHolyShield))
                        m_spells.paladin.pHolyShield = pSpellEntry;
                }
                break;
            }
            case CLASS_SHAMAN:
            {
                if (pSpellEntry->SpellName[0].find("Lightning Bolt") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pLightningBolt))
                        m_spells.shaman.pLightningBolt = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Chain Lightning") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pChainLightning))
                        m_spells.shaman.pChainLightning = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Earth Shock") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pEarthShock))
                        m_spells.shaman.pEarthShock = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Flame Shock") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pFlameShock))
                        m_spells.shaman.pFlameShock = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frost Shock") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pFrostShock))
                        m_spells.shaman.pFrostShock = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Purge") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pPurge))
                        m_spells.shaman.pPurge = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Stormstrike") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pStormstrike))
                        m_spells.shaman.pStormstrike = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Elemental Mastery") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pElementalMastery))
                        m_spells.shaman.pElementalMastery = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Lightning Shield") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pLightningShield))
                        m_spells.shaman.pLightningShield = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ghost Wolf") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pGhostWolf))
                        m_spells.shaman.pGhostWolf = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frostbrand Weapon") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFrostbrandWeapon))
                        pFrostbrandWeapon = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rockbiter Weapon") != std::string::npos)
                {
                    if (IsHigherRankSpell(pRockbiterWeapon))
                        pRockbiterWeapon = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Windfury Weapon") != std::string::npos)
                {
                    if (IsHigherRankSpell(pWindfuryWeapon))
                        pWindfuryWeapon = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Grace of Air Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pGraceOfAirTotem))
                        pGraceOfAirTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Nature Resistance Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pNatureResistanceTotem))
                        pNatureResistanceTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Windfury Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pWindfuryTotem))
                        pWindfuryTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Windwall Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pWindwallTotem))
                        pWindwallTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Tranquil Air Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pTranquilAirTotem))
                        pTranquilAirTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Earthbind Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pEarthbindTotem))
                        pEarthbindTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Stoneclaw Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pStoneclawtotem))
                        pStoneclawtotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Stoneskin Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pStoneskinTotem))
                        pStoneskinTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Strength of Earth Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pStrengthOfEarthTotem))
                        pStrengthOfEarthTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Tremor Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pTremorTotem))
                        pTremorTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fire Nova Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFireNovaTotem))
                        pFireNovaTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Magma Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pMagmaTotem))
                        pMagmaTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Searing Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pSearingTotem))
                        pSearingTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Flametongue Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFlametongueTotem))
                        pFlametongueTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frost Resistance Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFrostResistanceTotem))
                        pFrostResistanceTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fire Resistance Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFireResistanceTotem))
                        pFireResistanceTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Disease Resistance Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pDiseaseCleansingTotem))
                        pDiseaseCleansingTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Healing Stream Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pHealingStreamTotem))
                        pHealingStreamTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mana Spring Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pManaSpringTotem))
                        pManaSpringTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Poison Cleansing Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(pPoisonCleansingTotem))
                        pPoisonCleansingTotem = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mana Tide Totem") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.shaman.pManaTideTotem))
                        m_spells.shaman.pManaTideTotem = pSpellEntry;
                }
                break;
            }
            case CLASS_HUNTER:
            {
                if (pSpellEntry->SpellName[0].find("Aspect of the Cheetah") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pAspectOfTheCheetah))
                        m_spells.hunter.pAspectOfTheCheetah = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Aspect of the Hawk") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pAspectOfTheHawk))
                        m_spells.hunter.pAspectOfTheHawk = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Aspect of the Monkey") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pAspectOfTheMonkey))
                        m_spells.hunter.pAspectOfTheMonkey = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Serpent Sting") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pSerpentSting))
                        m_spells.hunter.pSerpentSting = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Arcane Shot") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pArcaneShot))
                        m_spells.hunter.pArcaneShot = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Aimed Shot") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pAimedShot))
                        m_spells.hunter.pAimedShot = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Multi-Shot") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pMultiShot))
                        m_spells.hunter.pMultiShot = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Concussive Shot") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pConcussiveShot))
                        m_spells.hunter.pConcussiveShot = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Wing Clip") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pWingClip))
                        m_spells.hunter.pWingClip = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hunter's Mark") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pHuntersMark))
                        m_spells.hunter.pHuntersMark = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mongoose Bite") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pMongooseBite))
                        m_spells.hunter.pMongooseBite = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Raptor Strike") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pRaptorStrike))
                        m_spells.hunter.pRaptorStrike = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Disengage") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pDisengage))
                        m_spells.hunter.pDisengage = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Feign Death") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pFeignDeath))
                        m_spells.hunter.pFeignDeath = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Scare Beast") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pScareBeast))
                        m_spells.hunter.pScareBeast = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Volley") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.hunter.pVolley))
                        m_spells.hunter.pVolley = pSpellEntry;
                }
                break;
            }
            case CLASS_MAGE:
            {
                if (pSpellEntry->SpellName[0].find("Ice Armor") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pIceArmor))
                        m_spells.mage.pIceArmor = pSpellEntry;
                }
                if (pSpellEntry->SpellName[0].find("Frost Armor") != std::string::npos)
                {
                    if (IsHigherRankSpell(pFrostArmor))
                        pFrostArmor = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ice Barrier") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pIceBarrier))
                        m_spells.mage.pIceBarrier = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mana Shield") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pManaShield))
                        m_spells.mage.pManaShield = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Arcane Intellect") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pArcaneIntellect))
                        m_spells.mage.pArcaneIntellect = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Arcane Brilliance") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pArcaneBrilliance))
                        m_spells.mage.pArcaneBrilliance = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frostbolt") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pFrostbolt))
                        m_spells.mage.pFrostbolt = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fire Blast") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pFireBlast))
                        m_spells.mage.pFireBlast = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fireball") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pFireball))
                        m_spells.mage.pFireball = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Arcane Explosion") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pArcaneExplosion))
                        m_spells.mage.pArcaneExplosion = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frost Nova") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pFrostNova))
                        m_spells.mage.pFrostNova = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cone of Cold") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pConeofCold))
                        m_spells.mage.pConeofCold = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blink") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pBlink))
                        m_spells.mage.pBlink = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0] == "Polymorph") // Sheep
                {
                    if (IsHigherRankSpell(pPolymorphSheep))
                        pPolymorphSheep = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Polymorph: Cow") != std::string::npos)
                {
                    if (IsHigherRankSpell(pPolymorphCow))
                        pPolymorphCow = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Polymorph: Pig") != std::string::npos)
                {
                    if (IsHigherRankSpell(pPolymorphPig))
                        pPolymorphPig = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Polymorph: Turtle") != std::string::npos)
                {
                    if (IsHigherRankSpell(pPolymorphTurtle))
                        pPolymorphTurtle = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Counterspell") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pCounterspell))
                        m_spells.mage.pCounterspell = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Presence of Mind") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pPresenceOfMind))
                        m_spells.mage.pPresenceOfMind = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Arcane Power") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pArcanePower))
                        m_spells.mage.pArcanePower = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Remove Lesser Curse") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pRemoveLesserCurse))
                        m_spells.mage.pRemoveLesserCurse = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Scorch") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pScorch))
                        m_spells.mage.pScorch = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Pyroblast") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pPyroblast))
                        m_spells.mage.pPyroblast = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Evocation") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pEvocation))
                        m_spells.mage.pEvocation = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ice Block") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pIceBlock))
                        m_spells.mage.pIceBlock = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blizzard") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pBlizzard))
                        m_spells.mage.pBlizzard = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blast Wave") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pBlastWave))
                        m_spells.mage.pBlastWave = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Combustion") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.mage.pCombustion))
                        m_spells.mage.pCombustion = pSpellEntry;
                }
                break;
            }
            case CLASS_PRIEST:
            {
                if (pSpellEntry->SpellName[0].find("Power Word: Fortitude") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pPowerWordFortitude))
                        m_spells.priest.pPowerWordFortitude = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Divine Spirit") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pDivineSpirit))
                        m_spells.priest.pDivineSpirit = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Prayer of Spirit") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pPrayerofSpirit))
                        m_spells.priest.pPrayerofSpirit = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Prayer of Fortitude") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pPrayerofFortitude))
                        m_spells.priest.pPrayerofFortitude = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Prayer of Shadow Protection") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pPrayerofShadowProtection))
                        m_spells.priest.pPrayerofShadowProtection = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Inner Fire") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pInnerFire))
                        m_spells.priest.pInnerFire = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadow Protection") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pShadowProtection))
                        m_spells.priest.pShadowProtection = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Power Word: Shield") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pPowerWordShield))
                        m_spells.priest.pPowerWordShield = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Holy Nova") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pHolyNova))
                        m_spells.priest.pHolyNova = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Holy Fire") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pHolyFire))
                        m_spells.priest.pHolyFire = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mind Blast") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pMindBlast))
                        m_spells.priest.pMindBlast = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mind Flay") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pMindFlay))
                        m_spells.priest.pMindFlay = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadow Word: Pain") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pShadowWordPain))
                        m_spells.priest.pShadowWordPain = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Inner Focus") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pInnerFocus))
                        m_spells.priest.pInnerFocus = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Abolish Disease") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pAbolishDisease))
                        m_spells.priest.pAbolishDisease = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Dispel Magic") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pDispelMagic))
                        m_spells.priest.pDispelMagic = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mana Burn") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pManaBurn))
                        m_spells.priest.pManaBurn = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Devouring Plague") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pDevouringPlague))
                        m_spells.priest.pDevouringPlague = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Psychic Scream") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pPsychicScream))
                        m_spells.priest.pPsychicScream = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadowform") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pShadowform))
                        m_spells.priest.pShadowform = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Vampiric Embrace") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pVampiricEmbrace))
                        m_spells.priest.pVampiricEmbrace = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Silence") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pSilence))
                        m_spells.priest.pSilence = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fade") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pFade))
                        m_spells.priest.pFade = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shackle Undead") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pShackleUndead))
                        m_spells.priest.pShackleUndead = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Smite") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.priest.pSmite))
                        m_spells.priest.pSmite = pSpellEntry;
                }
                break;
            }
            case CLASS_WARLOCK:
            {
                if (pSpellEntry->SpellName[0].find("Demon Armor") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pDemonArmor))
                        m_spells.warlock.pDemonArmor = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Death Coil") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pDeathCoil))
                        m_spells.warlock.pDeathCoil = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Detect Invisibility") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pDetectInvisibility))
                        m_spells.warlock.pDetectInvisibility = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadow Ward") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pShadowWard))
                        m_spells.warlock.pShadowWard = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadow Bolt") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pShadowBolt))
                        m_spells.warlock.pShadowBolt = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Corruption") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pCorruption))
                        m_spells.warlock.pCorruption = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Conflagrate") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pConflagrate))
                        m_spells.warlock.pConflagrate = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shadowburn") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pShadowburn))
                        m_spells.warlock.pShadowburn = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Searing Pain") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pSearingPain))
                        m_spells.warlock.pSearingPain = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Immolate") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pImmolate))
                        m_spells.warlock.pImmolate = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rain of Fire") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pRainOfFire))
                        m_spells.warlock.pRainOfFire = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Demonic Sacrifice") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pDemonicSacrifice))
                        m_spells.warlock.pDemonicSacrifice = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Drain Life") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pDrainLife))
                        m_spells.warlock.pDrainLife = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Siphon Life") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pSiphonLife))
                        m_spells.warlock.pSiphonLife = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Banish") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pBanish))
                        m_spells.warlock.pBanish = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Fear") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pFear))
                        m_spells.warlock.pFear = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Howl of Terror") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pHowlofTerror))
                        m_spells.warlock.pHowlofTerror = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Curse of Agony") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pCurseofAgony))
                        m_spells.warlock.pCurseofAgony = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Curse of the Elements") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pCurseoftheElements))
                        m_spells.warlock.pCurseoftheElements = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Curse of Shadow") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pCurseofShadow))
                        m_spells.warlock.pCurseofShadow = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Curse of Recklessness") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pCurseofRecklessness))
                        m_spells.warlock.pCurseofRecklessness = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Curse of Tongues") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pCurseofTongues))
                        m_spells.warlock.pCurseofTongues = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Life Tap") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warlock.pLifeTap))
                        m_spells.warlock.pLifeTap = pSpellEntry;
                }
                break;
            }
            case CLASS_WARRIOR:
            {
                if (pSpellEntry->SpellName[0].find("Battle Stance") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pBattleStance))
                        m_spells.warrior.pBattleStance = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Berserker Stance") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pBerserkerStance))
                        m_spells.warrior.pBerserkerStance = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Defensive Stance") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pDefensiveStance))
                        m_spells.warrior.pDefensiveStance = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0] == "Charge")
                {
                    if (IsHigherRankSpell(m_spells.warrior.pCharge))
                        m_spells.warrior.pCharge = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Intercept") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pIntercept))
                        m_spells.warrior.pIntercept = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Overpower") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pOverpower))
                        m_spells.warrior.pOverpower = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Heroic Strike") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pHeroicStrike))
                        m_spells.warrior.pHeroicStrike = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cleave") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pCleave))
                        m_spells.warrior.pCleave = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Execute") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pExecute))
                        m_spells.warrior.pExecute = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mortal Strike") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pMortalStrike))
                        m_spells.warrior.pMortalStrike = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Bloodthirst") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pBloodthirst))
                        m_spells.warrior.pBloodthirst = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Bloodrage") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pBloodrage))
                        m_spells.warrior.pBloodrage = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Berserker Rage") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pBerserkerRage))
                        m_spells.warrior.pBerserkerRage = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Recklessness") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pRecklessness))
                        m_spells.warrior.pRecklessness = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Retaliation") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pRetaliation))
                        m_spells.warrior.pRetaliation = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Death Wish") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pDeathWish))
                        m_spells.warrior.pDeathWish = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Intimidating Shout") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pIntimidatingShout))
                        m_spells.warrior.pIntimidatingShout = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Pummel") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pPummel))
                        m_spells.warrior.pPummel = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rend") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pRend))
                        m_spells.warrior.pRend = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Disarm") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pDisarm))
                        m_spells.warrior.pDisarm = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Whirlwind") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pWhirlwind))
                        m_spells.warrior.pWhirlwind = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Battle Shout") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pBattleShout))
                        m_spells.warrior.pBattleShout = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Demoralizing Shout") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pDemoralizingShout))
                        m_spells.warrior.pDemoralizingShout = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hamstring") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pHamstring))
                        m_spells.warrior.pHamstring = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Thunder Clap") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pThunderClap))
                        m_spells.warrior.pThunderClap = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Sweeping Strikes") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pSweepingStrikes))
                        m_spells.warrior.pSweepingStrikes = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Last Stand") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pLastStand))
                        m_spells.warrior.pLastStand = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shield Block") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pShieldBlock))
                        m_spells.warrior.pShieldBlock = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shield Wall") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pShieldWall))
                        m_spells.warrior.pShieldWall = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shield Bash") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pShieldBash))
                        m_spells.warrior.pShieldBash = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shield Slam") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pShieldSlam))
                        m_spells.warrior.pShieldSlam = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Sunder Armor") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pSunderArmor))
                        m_spells.warrior.pSunderArmor = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Concussion Blow") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pConcussionBlow))
                        m_spells.warrior.pConcussionBlow = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Piercing Howl") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.warrior.pPiercingHowl))
                        m_spells.warrior.pPiercingHowl = pSpellEntry;
                }
                break;
            }
            case CLASS_ROGUE:
            {
                if (pSpellEntry->SpellName[0].find("Slice and Dice") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pSliceAndDice))
                        m_spells.rogue.pSliceAndDice = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Sinister Strike") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pSinisterStrike))
                        m_spells.rogue.pSinisterStrike = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Adrenaline Rush") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pAdrenalineRush))
                        m_spells.rogue.pAdrenalineRush = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Eviscerate") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pEviscerate))
                        m_spells.rogue.pEviscerate = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Stealth") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pStealth))
                        m_spells.rogue.pStealth = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Garrote") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pGarrote))
                        m_spells.rogue.pGarrote = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ambush") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pAmbush))
                        m_spells.rogue.pAmbush = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cheap Shot") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pCheapShot))
                        m_spells.rogue.pCheapShot = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Premeditation") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pPremeditation))
                        m_spells.rogue.pPremeditation = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Backstab") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pBackstab))
                        m_spells.rogue.pBackstab = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hemorrhage") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pHemorrhage))
                        m_spells.rogue.pHemorrhage = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ghostly Strike") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pGhostlyStrike))
                        m_spells.rogue.pGhostlyStrike = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Gouge") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pGouge))
                        m_spells.rogue.pGouge = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rupture") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pRupture))
                        m_spells.rogue.pRupture = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Expose Armor") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pExposeArmor))
                        m_spells.rogue.pExposeArmor = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Kidney Shot") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pKidneyShot))
                        m_spells.rogue.pKidneyShot = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cold Blood") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pColdBlood))
                        m_spells.rogue.pColdBlood = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Blade Flurry") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pBladeFlurry))
                        m_spells.rogue.pBladeFlurry = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Vanish") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pVanish))
                        m_spells.rogue.pVanish = pSpellEntry;
                }
                else if (pSpellEntry->IsFitToFamily<SPELLFAMILY_ROGUE, CF_ROGUE_BLIND>())
                {
                    if (IsHigherRankSpell(m_spells.rogue.pBlind))
                        m_spells.rogue.pBlind = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Preparation") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pPreparation))
                        m_spells.rogue.pPreparation = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Evasion") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pEvasion))
                        m_spells.rogue.pEvasion = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Riposte") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pRiposte))
                        m_spells.rogue.pRiposte = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Kick") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pKick))
                        m_spells.rogue.pKick = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Sprint") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.rogue.pSprint))
                        m_spells.rogue.pSprint = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Deadly Poison") != std::string::npos)
                {
                    hasDeadlyPoison = true;
                }
                else if (pSpellEntry->SpellName[0].find("Instant Poison") != std::string::npos)
                {
                    hasInstantPoison = true;
                }
                else if (pSpellEntry->SpellName[0].find("Crippling Poison") != std::string::npos)
                {
                    hasCripplingPoison = true;
                }
                else if (pSpellEntry->SpellName[0].find("Wound Poison") != std::string::npos)
                {
                    hasWoundPoison = true;
                }
                else if (pSpellEntry->SpellName[0].find("Mind-numbing Poison") != std::string::npos)
                {
                    HasMindNumbingPoison = true;
                }
                break;
            }
            case CLASS_DRUID:
            {
                if (pSpellEntry->SpellName[0].find("Bear Form") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pBearForm))
                        m_spells.druid.pBearForm = pSpellEntry;
                }
                else if (pSpellEntry->Id == (768)) // Cat Form
                {
                    if (IsHigherRankSpell(m_spells.druid.pCatForm))
                        m_spells.druid.pCatForm = pSpellEntry;
                }
                else if (pSpellEntry->Id == (783)) // Travel Form
                {
                    if (IsHigherRankSpell(m_spells.druid.pTravelForm))
                        m_spells.druid.pTravelForm = pSpellEntry;
                }
                else if (pSpellEntry->Id == (1066)) // Aquatic Form
                {
                    if (IsHigherRankSpell(m_spells.druid.pAquaticForm))
                        m_spells.druid.pAquaticForm = pSpellEntry;
                }
                else if (pSpellEntry->Id == (24858)) // Moonkin Form
                {
                    if (IsHigherRankSpell(m_spells.druid.pMoonkinForm))
                        m_spells.druid.pMoonkinForm = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Wrath") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pWrath))
                        m_spells.druid.pWrath = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Moonfire") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pMoonfire))
                        m_spells.druid.pMoonfire = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Starfire") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pStarfire))
                        m_spells.druid.pStarfire = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hurricane") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pHurricane))
                        m_spells.druid.pHurricane = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Insect Swarm") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pInsectSwarm))
                        m_spells.druid.pInsectSwarm = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Barkskin") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pBarkskin))
                        m_spells.druid.pBarkskin = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Nature's Grasp") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pNaturesGrasp))
                        m_spells.druid.pNaturesGrasp = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Mark of the Wild") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pMarkoftheWild))
                        m_spells.druid.pMarkoftheWild = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Gift of the Wild") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pGiftoftheWild))
                        m_spells.druid.pGiftoftheWild = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Thorns") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pThorns))
                        m_spells.druid.pThorns = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Remove Curse") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pRemoveCurse))
                        m_spells.druid.pRemoveCurse = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cure Poison") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pCurePoison))
                        m_spells.druid.pCurePoison = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Abolish Poison") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pAbolishPoison))
                        m_spells.druid.pAbolishPoison = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rebirth") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pRebirth))
                        m_spells.druid.pRebirth = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Innervate") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pInnervate))
                        m_spells.druid.pInnervate = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Nature's Swiftness") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pNaturesSwiftness))
                        m_spells.druid.pNaturesSwiftness = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Entangling Roots") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pEntanglingRoots))
                        m_spells.druid.pEntanglingRoots = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Hibernate") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pHibernate))
                        m_spells.druid.pHibernate = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Pounce") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pPounce))
                        m_spells.druid.pPounce = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ravage") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pRavage))
                        m_spells.druid.pRavage = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Claw") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pClaw))
                        m_spells.druid.pClaw = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Shred") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pShred))
                        m_spells.druid.pShred = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rake") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pRake))
                        m_spells.druid.pRake = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Rip") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pRip))
                        m_spells.druid.pRip = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Ferocious Bite") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pFerociousBite))
                        m_spells.druid.pFerociousBite = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Tiger's Fury") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pTigersFury))
                        m_spells.druid.pTigersFury = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Dash") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pDash))
                        m_spells.druid.pDash = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Cower") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pCower))
                        m_spells.druid.pCower = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Faerie Fire (Feral)") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pFaerieFireFeral))
                        m_spells.druid.pFaerieFireFeral = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Faerie Fire") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pFaerieFire))
                        m_spells.druid.pFaerieFire = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Growl") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pGrowl))
                        m_spells.druid.pGrowl = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Challenging Roar") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pChallengingRoar))
                        m_spells.druid.pChallengingRoar = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Demoralizing Roar") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pDemoralizingRoar))
                        m_spells.druid.pDemoralizingRoar = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Enrage") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pEnrage))
                        m_spells.druid.pEnrage = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Frenzied Regeneration") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pFrenziedRegeneration))
                        m_spells.druid.pFrenziedRegeneration = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Swipe") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pSwipe))
                        m_spells.druid.pSwipe = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Maul") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pMaul))
                        m_spells.druid.pMaul = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Bash") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pBash))
                        m_spells.druid.pBash = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Feral Charge") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pFeralCharge))
                        m_spells.druid.pFeralCharge = pSpellEntry;
                }
                else if (pSpellEntry->SpellName[0].find("Prowl") != std::string::npos)
                {
                    if (IsHigherRankSpell(m_spells.druid.pProwl))
                        m_spells.druid.pProwl = pSpellEntry;
                }
                break;
            }
        }

        for (uint32 i = 0; i < MAX_SPELL_EFFECTS; i++)
        {
            switch (pSpellEntry->Effect[i])
            {
                case SPELL_EFFECT_HEAL:
                    m_spellListDirectHeal.insert(pSpellEntry);
                    break;
                case SPELL_EFFECT_ATTACK_ME:
                    m_spellListTaunt.push_back(pSpellEntry);
                    break;
                case SPELL_EFFECT_RESURRECT:
                case SPELL_EFFECT_RESURRECT_NEW:
                    m_resurrectionSpell = pSpellEntry;
                    break;
                case SPELL_EFFECT_APPLY_AURA:
                {
                    switch (pSpellEntry->EffectApplyAuraName[i])
                    {
                        case SPELL_AURA_PERIODIC_HEAL:
                            m_spellListPeriodicHeal.insert(pSpellEntry);
                            break;
                        case SPELL_AURA_MOD_TAUNT:
                            m_spellListTaunt.push_back(pSpellEntry);
                            break;
                    }
                    break;
                }
            }
        }
    }

    switch (me->GetClass())
    {
        case CLASS_PALADIN:
        {
            if (pSealOfFury && m_role == ROLE_TANK)
                m_spells.paladin.pSeal = pSealOfFury;
            else if (pSealOfCommand)
                m_spells.paladin.pSeal = pSealOfCommand;
            else
                m_spells.paladin.pSeal = pSealOfRighteousness;

            if (pBlessingOfSanctuary && m_role == ROLE_TANK)
                m_spells.paladin.pBlessingBuff = pBlessingOfSanctuary;
            else
            {
                std::vector<SpellEntry const*> blessings;
                if (pBlessingOfLight)
                    blessings.push_back(pBlessingOfLight);
                if (pBlessingOfMight)
                    blessings.push_back(pBlessingOfMight);
                if (pBlessingOfWisdom)
                    blessings.push_back(pBlessingOfWisdom);
                if (pBlessingOfKings)
                    blessings.push_back(pBlessingOfKings);
                if (pBlessingOfSanctuary)
                    blessings.push_back(pBlessingOfSanctuary);
                if (!blessings.empty())
                    m_spells.paladin.pBlessingBuff = SelectRandomContainerElement(blessings);
            }

            std::vector<SpellEntry const*> auras;
            if (pDevotionAura)
                auras.push_back(pDevotionAura);
            if (pConcentrationAura)
                auras.push_back(pConcentrationAura);
            if (pRetributionAura)
                auras.push_back(pRetributionAura);
            if (pSanctityAura)
                auras.push_back(pSanctityAura);
            if (pShadowResistanceAura)
                auras.push_back(pShadowResistanceAura);
            if (pFrostResistanceAura)
                auras.push_back(pFrostResistanceAura);
            if (pFireResistanceAura)
                auras.push_back(pFireResistanceAura);
            if (!auras.empty())
                m_spells.paladin.pAura = SelectRandomContainerElement(auras);

            break;
        }
        case CLASS_SHAMAN:
        {
            std::vector<SpellEntry const*> airTotems;
            if (pGraceOfAirTotem)
                airTotems.push_back(pGraceOfAirTotem);
            if (pNatureResistanceTotem)
                airTotems.push_back(pNatureResistanceTotem);
            if (pWindfuryTotem)
                airTotems.push_back(pWindfuryTotem);
            if (pWindwallTotem)
                airTotems.push_back(pWindwallTotem);
            if (pTranquilAirTotem)
                airTotems.push_back(pTranquilAirTotem);
            if (!airTotems.empty())
                m_spells.shaman.pAirTotem = SelectRandomContainerElement(airTotems);

            std::vector<SpellEntry const*> earthTotems;
            if (pEarthbindTotem)
                earthTotems.push_back(pEarthbindTotem);
            if (pStoneclawtotem)
                earthTotems.push_back(pStoneclawtotem);
            if (pStoneskinTotem)
                earthTotems.push_back(pStoneskinTotem);
            if (pStrengthOfEarthTotem)
                earthTotems.push_back(pStrengthOfEarthTotem);
            if (pTremorTotem)
                earthTotems.push_back(pTremorTotem);
            if (!earthTotems.empty())
                m_spells.shaman.pEarthTotem = SelectRandomContainerElement(earthTotems);

            std::vector<SpellEntry const*> fireTotems;
            if (pFireNovaTotem)
                fireTotems.push_back(pFireNovaTotem);
            if (pMagmaTotem)
                fireTotems.push_back(pMagmaTotem);
            if (pSearingTotem)
                fireTotems.push_back(pSearingTotem);
            if (pFlametongueTotem)
                fireTotems.push_back(pFlametongueTotem);
            if (pFrostResistanceTotem)
                fireTotems.push_back(pFrostResistanceTotem);
            if (!fireTotems.empty())
                m_spells.shaman.pFireTotem = SelectRandomContainerElement(fireTotems);

            std::vector<SpellEntry const*> waterTotems;
            if (pFireResistanceTotem)
                waterTotems.push_back(pFireResistanceTotem);
            if (pDiseaseCleansingTotem)
                waterTotems.push_back(pDiseaseCleansingTotem);
            if (pHealingStreamTotem)
                waterTotems.push_back(pHealingStreamTotem);
            if (pManaSpringTotem)
                waterTotems.push_back(pManaSpringTotem);
            if (pPoisonCleansingTotem)
                waterTotems.push_back(pPoisonCleansingTotem);
            if (!waterTotems.empty())
                m_spells.shaman.pWaterTotem = SelectRandomContainerElement(waterTotems);

            if (pWindfuryWeapon && m_role == ROLE_MELEE_DPS)
                m_spells.shaman.pWeaponBuff = pWindfuryWeapon;
            else
            {
                std::vector<SpellEntry const*> weaponBuffs;
                if (pWindfuryWeapon)
                    weaponBuffs.push_back(pWindfuryWeapon);
                if (pRockbiterWeapon)
                    weaponBuffs.push_back(pRockbiterWeapon);
                if (pFrostbrandWeapon)
                    weaponBuffs.push_back(pFrostbrandWeapon);
                if (!weaponBuffs.empty())
                    m_spells.shaman.pWeaponBuff = SelectRandomContainerElement(weaponBuffs);
            }

            break;
        }
        case CLASS_MAGE:
        {
            if (!m_spells.mage.pIceArmor && pFrostArmor)
                m_spells.mage.pIceArmor = pFrostArmor;

            std::vector<SpellEntry const*> polymorph;
            if (pPolymorphSheep)
                polymorph.push_back(pPolymorphSheep);
            if (pPolymorphCow)
                polymorph.push_back(pPolymorphCow);
            if (pPolymorphPig)
                polymorph.push_back(pPolymorphPig);
            if (pPolymorphTurtle)
                polymorph.push_back(pPolymorphTurtle);
            if (!polymorph.empty())
                m_spells.mage.pPolymorph = SelectRandomContainerElement(polymorph);

            break;
        }
        case CLASS_ROGUE:
        {
            // Rogues can only craft an item that applies the poison, they don't know the actual poison enchant.
            auto GetHighestRankOfPoisonByName = [](std::string name, uint32 level)
            {
                SpellEntry const* pHighestRank = nullptr;
                for (uint32 i = 0; i < sSpellMgr.GetMaxSpellId(); i++)
                {
                    if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(i))
                    {
                        if (pSpellEntry->Effect[0] == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY &&
                            pSpellEntry->SpellName[0] == name && pSpellEntry->spellLevel <= level &&
                           (!pHighestRank || pHighestRank->spellLevel < pSpellEntry->spellLevel))
                        {
                            pHighestRank = pSpellEntry;
                        }
                    }
                }
                return pHighestRank;
            };

            SpellEntry const* pPoisonSpell = nullptr;
            std::vector<SpellEntry const*> vPoisons;
            if (hasDeadlyPoison && (pPoisonSpell = GetHighestRankOfPoisonByName("Deadly Poison", me->GetLevel())))
                vPoisons.push_back(pPoisonSpell);
            if (hasInstantPoison && (pPoisonSpell = GetHighestRankOfPoisonByName("Instant Poison", me->GetLevel())))
                vPoisons.push_back(pPoisonSpell);
            if (hasCripplingPoison && (pPoisonSpell = GetHighestRankOfPoisonByName("Crippling Poison", me->GetLevel())))
                vPoisons.push_back(pPoisonSpell);
            if (hasWoundPoison && (pPoisonSpell = GetHighestRankOfPoisonByName("Wound Poison", me->GetLevel())))
                vPoisons.push_back(pPoisonSpell);
            if (HasMindNumbingPoison && (pPoisonSpell = GetHighestRankOfPoisonByName("Mind-numbing Poison", me->GetLevel())))
                vPoisons.push_back(pPoisonSpell);

            if (!vPoisons.empty())
            {
                m_spells.rogue.pMainHandPoison = SelectRandomContainerElement(vPoisons);
                m_spells.rogue.pOffHandPoison = SelectRandomContainerElement(vPoisons);
            }

            break;
        }
    }
}

void CombatBotBaseAI::AddAllSpellReagents()
{
    for (const auto& pSpell : m_spells.raw.spells)
    {
        if (pSpell)
        {
            for (const auto& reagent : pSpell->Reagent)
            {
                if (reagent && !me->HasItemCount(reagent, 1))
                    AddItemToInventory(reagent);
            }
            for (const auto& totem : pSpell->Totem)
            {
                if (totem && !me->HasItemCount(totem, 1))
                    AddItemToInventory(totem);
            }
        }
    }
}

bool CombatBotBaseAI::AreOthersOnSameTarget(ObjectGuid guid, bool checkMelee, bool checkSpells) const
{
    Group* pGroup = me->GetGroup();
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* pMember = itr->getSource())
        {
            // Not self.
            if (pMember == me)
                continue;

            // Not the target itself.
            if (pMember->GetObjectGuid() == guid)
                continue;

            if (pMember->GetTargetGuid() == guid)
            {
                if (checkMelee && pMember->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
                    return true;

                if (checkSpells && pMember->IsNonMeleeSpellCasted())
                    return true;
            }
        }
    }
    return false;
}

bool CombatBotBaseAI::FindAndHealInjuredAlly(float selfHealPercent, float groupHealPercent)
{
    Unit* pTarget = SelectHealTarget(selfHealPercent, groupHealPercent);
    if (!pTarget)
        return false;

    return HealInjuredTarget(pTarget);
}

template <class T>
SpellEntry const* CombatBotBaseAI::SelectMostEfficientHealingSpell(Unit const* pTarget, std::set<SpellEntry const*, T>& spellList) const
{
    return SelectMostEfficientHealingSpell(pTarget, pTarget->GetMaxHealth() - pTarget->GetHealth(), spellList);
}

template <class T>
SpellEntry const* CombatBotBaseAI::SelectMostEfficientHealingSpell(Unit const* pTarget, int32 missingHealth, std::set<SpellEntry const*, T>& spellList) const
{
    SpellEntry const* pHealSpell = nullptr;
    int32 healthDiff = INT32_MAX;

    // Find most efficient healing spell.
    for (const auto pSpellEntry : spellList)
    {
        if (CanTryToCastSpell(pTarget, pSpellEntry))
        {
            int32 basePoints = 0;
            for (uint32 i = 0; i < MAX_SPELL_EFFECTS; i++)
            {
                switch (pSpellEntry->Effect[i])
                {
                    case SPELL_EFFECT_HEAL:
                        basePoints += pSpellEntry->EffectBasePoints[i];
                        break;
                    case SPELL_EFFECT_APPLY_AURA:
                    case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                    case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                        if (pSpellEntry->EffectApplyAuraName[i] == SPELL_AURA_PERIODIC_HEAL)
                            basePoints += ((pSpellEntry->GetDuration() / pSpellEntry->EffectAmplitude[i]) * pSpellEntry->EffectBasePoints[i]);
                        break;
                }
            }

            int32 const diff = basePoints - missingHealth;
            if (std::abs(diff) < healthDiff)
            {
                healthDiff = diff;
                pHealSpell = pSpellEntry;
            }

            // Healing spells are sorted from strongest to weakest.
            if (diff < 0)
                break;
        }
    }

    return pHealSpell;
}

int32 CombatBotBaseAI::GetIncomingdamage(Unit const* pTarget) const
{
    int32 damage = 0;
    for (auto const& pAttacker : pTarget->GetAttackers())
        if (pAttacker->CanReachWithMeleeAutoAttack(pTarget))
            damage += int32((pAttacker->GetFloatValue(UNIT_FIELD_MINDAMAGE) + pAttacker->GetFloatValue(UNIT_FIELD_MAXDAMAGE)) / 2);
    return damage;
}

bool CombatBotBaseAI::HealInjuredTarget(Unit* pTarget)
{
    // Put a HoT on the target if only missing a little health.
    if (pTarget->GetHealthPercent() >= 80.0f &&
       !pTarget->HasAuraType(SPELL_AURA_PERIODIC_HEAL))
    {
        if (HealInjuredTargetPeriodic(pTarget))
            return true;
    }

    if (HealInjuredTargetDirect(pTarget))
        return true;

    return false;
}

bool CombatBotBaseAI::HealInjuredTargetPeriodic(Unit* pTarget)
{
    if (SpellEntry const* pHealSpell = SelectMostEfficientHealingSpell(pTarget, m_spellListPeriodicHeal))
    {
        if (CanTryToCastSpell(pTarget, pHealSpell))
        {
            if (DoCastSpell(pTarget, pHealSpell) == SPELL_CAST_OK)
                return true;
        }
    }

    return false;
}

bool CombatBotBaseAI::HealInjuredTargetDirect(Unit* pTarget)
{
    if (SpellEntry const* pHealSpell = SelectMostEfficientHealingSpell(pTarget, m_spellListDirectHeal))
        if (DoCastSpell(pTarget, pHealSpell) == SPELL_CAST_OK)
            return true;

    return false;
}

bool CombatBotBaseAI::IsValidHealTarget(Unit const* pTarget, float healthPercent) const
{
    return (pTarget->GetHealthPercent() < healthPercent) &&
            me->IsValidHelpfulTarget(pTarget) &&
            me->IsWithinLOSInMap(pTarget) &&
            me->IsWithinDist(pTarget, 30.0f);
}

Unit* CombatBotBaseAI::SelectHealTarget(float selfHealPercent, float groupHealPercent) const
{
    if (me->GetHealthPercent() < selfHealPercent)
        return me;

    if (IsInDuel())
        return nullptr;

    Unit* pTarget = nullptr;
    float healthPercent = 100.0f;

    if (Group* pGroup = me->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Unit* pMember = itr->getSource())
            {
                // We already checked self.
                if (pMember == me)
                    continue;

                // Avoid all healers picking same target.
                if (pTarget && !IsTankClass(pMember->GetClass()) && AreOthersOnSameTarget(pMember->GetObjectGuid(), false, true))
                    continue;

                // Check if we should heal party member.
                if ((IsValidHealTarget(pMember, groupHealPercent) &&
                    healthPercent > pMember->GetHealthPercent()) ||
                    // Or a pet if there are no injured players.
                    (!pTarget && (pMember = pMember->GetPet()) &&
                      IsValidHealTarget(pMember, groupHealPercent)))
                {
                    healthPercent = pMember->GetHealthPercent();
                    pTarget = pMember;
                }
            }
        }
    }

    if (healthPercent == 100.0f)
        return nullptr;

    return pTarget;
}

Unit* CombatBotBaseAI::SelectPeriodicHealTarget(float selfHealPercent, float groupHealPercent) const
{
    if (me->GetHealthPercent() < selfHealPercent &&
       !me->HasAuraType(SPELL_AURA_PERIODIC_HEAL))
        return me;

    if (IsInDuel())
        return nullptr;

    if (Group* pGroup = me->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Unit* pMember = itr->getSource())
            {
                // We already checked self.
                if (pMember == me)
                    continue;

                // Check if we should heal party member.
                if (IsValidHealTarget(pMember, groupHealPercent) &&
                   !pMember->HasAuraType(SPELL_AURA_PERIODIC_HEAL))
                    return pMember;
            }
        }
    }

    return nullptr;
}

bool CombatBotBaseAI::FindAndPreHealTarget()
{
    Unit* pTarget = me;
    int32 maxIncomingDamage = GetIncomingdamage(me);

    if (!IsInDuel())
    {
        if (Group* pGroup = me->GetGroup())
        {
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                if (Unit* pMember = itr->getSource())
                {
                    // We already checked self.
                    if (pMember == me)
                        continue;

                    // Avoid all healers picking same target.
                    if (pTarget && !IsTankClass(pMember->GetClass()) && AreOthersOnSameTarget(pMember->GetObjectGuid(), false, true))
                        continue;

                    int32 incomingDamage = GetIncomingdamage(pMember);
                    if (!incomingDamage)
                        continue;

                    // Check if we should heal party member.
                    if (incomingDamage > maxIncomingDamage &&
                        IsValidHealTarget(pMember))
                    {
                        maxIncomingDamage = incomingDamage;
                        pTarget = pMember;
                    }
                }
            }
        }
    }

    if (!maxIncomingDamage)
        return false;

    // Add currently missing health too.
    maxIncomingDamage += int32(pTarget->GetMaxHealth() - pTarget->GetHealth());
    if (maxIncomingDamage < int32(pTarget->GetMaxHealth() / 2))
        return false;

    if (SpellEntry const* pHealSpell = SelectMostEfficientHealingSpell(pTarget, maxIncomingDamage, m_spellListDirectHeal))
    {
        if (pHealSpell->GetCastTime(me) > 1000 && CanTryToCastSpell(pTarget, pHealSpell))
        {
            if (DoCastSpell(pTarget, pHealSpell) == SPELL_CAST_OK)
                return true;
        }
    }

    return pTarget;
}

bool CombatBotBaseAI::IsValidHostileTarget(Unit const* pTarget) const
{
    return me->IsValidAttackTarget(pTarget) &&
           pTarget->IsVisibleForOrDetect(me, me, false) &&
           !pTarget->HasBreakableByDamageCrowdControlAura() &&
           !pTarget->IsTotalImmune() &&
           pTarget->GetTransport() == me->GetTransport();
}

bool CombatBotBaseAI::IsValidDispelTarget(Unit const* pTarget, SpellEntry const* pSpellEntry) const
{
    uint32 dispelMask = 0;
    bool bFoundOneDispell = false;
    // Compute Dispel Mask
    for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (pSpellEntry->Effect[i] != SPELL_EFFECT_DISPEL)
            continue;

        // Create dispel mask by dispel type
        uint32 dispel_type = pSpellEntry->EffectMiscValue[i];
        dispelMask |= Spells::GetDispellMask(DispelType(dispel_type));
    }
    bool friendly_dispel = pTarget && pTarget->IsFriendlyTo(me);

    if (pTarget &&
        // Check immune for offensive dispel
        (!pTarget->IsImmuneToSchoolMask(pSpellEntry->GetSpellSchoolMask()) ||
            friendly_dispel))
    {
        if (!friendly_dispel && !me->IsValidAttackTarget(pTarget))
            return false;

        auto const& auras = pTarget->GetSpellAuraHolderMap();
        for (const auto& aura : auras)
        {
            SpellAuraHolder* holder = aura.second;
            if ((1 << holder->GetSpellProto()->Dispel) & dispelMask)
            {
                if (holder->GetSpellProto()->Dispel == DISPEL_MAGIC ||
                    holder->GetSpellProto()->Dispel == DISPEL_DISEASE ||
                    holder->GetSpellProto()->Dispel == DISPEL_POISON)
                {
                    bool positive = holder->IsPositive();
                    // do not remove positive auras if friendly target
                    // do not remove negative auras if non-friendly target
                    // when removing charm auras ignore hostile reaction from the charm
                    if (!friendly_dispel && !positive && holder->GetSpellProto()->IsCharmSpell())
                        if (CharmInfo *charm = pTarget->GetCharmInfo())
                            if (FactionTemplateEntry const* ft = charm->GetOriginalFactionTemplate())
                                if (FactionTemplateEntry const* ft2 = me->GetFactionTemplateEntry())
                                    if (charm->GetOriginalFactionTemplate()->IsFriendlyTo(*ft2))
                                        bFoundOneDispell = true;
                    if (positive == friendly_dispel)
                        continue;
                }
                bFoundOneDispell = true;
                break;
            }
        }
    }

    if (!bFoundOneDispell)
        return false;

    return true;
}

uint8 CombatBotBaseAI::GetAttackersInRangeCount(float range) const
{
    uint8 count = 0;
    for (const auto& pTarget : me->GetAttackers())
    {
        if (me->GetCombatDistance(pTarget) <= range)
            count++;
    }

    return count;
}

Unit* CombatBotBaseAI::SelectAttackerDifferentFrom(Unit const* pExcept) const
{
    for (const auto& pTarget : me->GetAttackers())
    {
        if (pTarget != pExcept)
            return pTarget;
    }

    return nullptr;
}

bool CombatBotBaseAI::IsValidBuffTarget(Unit const* pTarget, SpellEntry const* pSpellEntry) const
{
    std::vector<uint32> morePowerfulSpells;
    sSpellMgr.ListMorePowerfulSpells(pSpellEntry->Id, morePowerfulSpells);

    for (const auto& i : pTarget->GetSpellAuraHolderMap())
    {
        if (i.first == pSpellEntry->Id)
            return false;

        if (sSpellMgr.IsRankSpellDueToSpell(pSpellEntry, i.first))
            return false;

        for (const auto& it : morePowerfulSpells)
            if (it == i.first)
                return false;
    }

    return true;
}

Player* CombatBotBaseAI::SelectBuffTarget(SpellEntry const* pSpellEntry) const
{
    Group* pGroup = me->GetGroup();
    if (pGroup)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
            {
                if (me->IsValidHelpfulTarget(pMember) &&
                   !pMember->IsGameMaster() &&
                    IsValidBuffTarget(pMember, pSpellEntry) &&
                    me->IsWithinLOSInMap(pMember) &&
                    me->IsWithinDist(pMember, 30.0f))
                    return pMember;
            }
        }
    }

    return nullptr;
}

Player* CombatBotBaseAI::SelectBuffTarget(SpellEntry const* pSingleSpellEntry, SpellEntry const* pGroupSpellEntry, SpellEntry const*& pSelectedSpellEntry) const
{
    pSelectedSpellEntry = nullptr;

    if (!pSingleSpellEntry && !pGroupSpellEntry)
        return nullptr;

    if (!pSingleSpellEntry)
    {
        pSelectedSpellEntry = pGroupSpellEntry;
        return SelectBuffTarget(pGroupSpellEntry);
    }

    if (!pGroupSpellEntry)
    {
        pSelectedSpellEntry = pSingleSpellEntry;
        return SelectBuffTarget(pSingleSpellEntry);
    }

    Player* pFirstMissingMember = nullptr;
    uint8 missingMemberCount = 0;
    Group* pGroup = me->GetGroup();
    if (pGroup)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
            {
                if (!me->IsValidHelpfulTarget(pMember) ||
                    pMember->IsGameMaster() ||
                    !me->IsWithinLOSInMap(pMember) ||
                    !me->IsWithinDist(pMember, 30.0f) ||
                    !IsValidBuffTarget(pMember, pSingleSpellEntry) ||
                    !IsValidBuffTarget(pMember, pGroupSpellEntry))
                    continue;

                if (!pFirstMissingMember)
                    pFirstMissingMember = pMember;

                ++missingMemberCount;
                if (missingMemberCount > 1)
                {
                    pSelectedSpellEntry = pGroupSpellEntry;
                    return pFirstMissingMember;
                }
            }
        }
    }

    if (missingMemberCount == 1)
        pSelectedSpellEntry = pSingleSpellEntry;

    return pFirstMissingMember;
}

Player* CombatBotBaseAI::SelectDispelTarget(SpellEntry const* pSpellEntry) const
{
    Group* pGroup = me->GetGroup();
    if (pGroup)
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Player* pMember = itr->getSource())
            {
                if (me->IsValidHelpfulTarget(pMember) &&
                   !pMember->IsGameMaster() &&
                    IsValidDispelTarget(pMember, pSpellEntry) &&
                    me->IsWithinLOSInMap(pMember) &&
                    me->IsWithinDist(pMember, 30.0f))
                    return pMember;
            }
        }
    }

    return nullptr;
}

void CombatBotBaseAI::SummonPetIfNeeded()
{
    if (me->GetClass() == CLASS_HUNTER)
    {
        if (me->GetCharmGuid())
            return;

        if (me->GetLevel() < 10)
            return;

        if (me->GetPetGuid() || sCharacterDatabaseCache.GetCharacterPetByOwner(me->GetGUIDLow()))
        {
            if (Pet* pPet = me->GetPet())
            {
                if (!pPet->IsAlive())
                    me->CastSpell(pPet, SPELL_REVIVE_PET, true);
            }
            else
                me->CastSpell(me, SPELL_CALL_PET, true);

            return;
        }

        uint32 petId = PickRandomValue( PET_WOLF, PET_CAT, PET_BEAR, PET_CRAB, PET_GORILLA, PET_BIRD,
                                        PET_BOAR, PET_BAT, PET_CROC, PET_SPIDER, PET_OWL, PET_STRIDER,
                                        PET_SCORPID, PET_SERPENT, PET_RAPTOR, PET_TURTLE, PET_HYENA );
        if (Creature* pCreature = me->SummonCreature(petId,
            me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f,
            TEMPSUMMON_TIMED_COMBAT_OR_DEAD_DESPAWN, 3000, false, 3000))
        {
            pCreature->SetLevel(me->GetLevel());
            me->CastSpell(pCreature, SPELL_TAME_BEAST, true);
        }
    }
    else if (me->GetClass() == CLASS_WARLOCK)
    {
        if (me->GetPetGuid() || me->GetCharmGuid())
            return;

        std::vector<uint32> vSummons;
        if (me->HasSpell(SPELL_SUMMON_IMP))
            vSummons.push_back(SPELL_SUMMON_IMP);
        if (me->HasSpell(SPELL_SUMMON_VOIDWALKER))
            vSummons.push_back(SPELL_SUMMON_VOIDWALKER);
        if (me->HasSpell(SPELL_SUMMON_FELHUNTER))
            vSummons.push_back(SPELL_SUMMON_FELHUNTER);
        if (me->HasSpell(SPELL_SUMMON_SUCCUBUS))
            vSummons.push_back(SPELL_SUMMON_SUCCUBUS);
        if (!vSummons.empty())
            me->CastSpell(me, SelectRandomContainerElement(vSummons), true);
    }
}

void CombatBotBaseAI::LearnArmorProficiencies()
{
    switch (me->GetClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        {
            if (me->GetLevel() >= 40 && !me->HasSpell(SPELL_PLATE_PROFICIENCY))
                me->LearnSpell(SPELL_PLATE_PROFICIENCY, false, false);
            break;
        }
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
        {
            if (me->GetLevel() >= 40 && !me->HasSpell(SPELL_MAIL_PROFICIENCY))
                me->LearnSpell(SPELL_MAIL_PROFICIENCY, false, false);
            break;
        }
    }
}

void CombatBotBaseAI::LearnPremadeSpecForClass()
{
    // The talent of the bot: the one that `.partybot add <abbreviation>` asked for, or the one that the role of
    // the bot has for its class. The premade templates are chosen by role and not by talent, and the gear is
    // rated for the talents that the bot has, so a bot of a known talent learns the build of that talent
    // instead: the steps of `playerbot_talent_build` are spent in the order of the table, which fills the
    // primary tree of the talent before another tree is touched and gives a bot that has not reached the maximum
    // level the first steps of the same build. The role was set when the bot was created, which tells apart the
    // two talents that share a tree (the cat and the bear of a druid).
    uint8 talentPage = m_requestedTalentPage;
    if (talentPage >= MAX_GEAR_SPEC_TABS)
    {
        uint8 rolePage = MAX_GEAR_SPEC_TABS;
        if (GetSpecTalentPageForRole(me->GetClass(), m_role, rolePage))
            talentPage = rolePage;
    }

    if (talentPage < MAX_GEAR_SPEC_TABS)
    {
        bool learned = LearnTalentsFromBuild(talentPage);

        // Without a row in the table the tree of the asked talent is filled in the order the tree lists it, as
        // it was before the table existed.
        if (!learned && m_requestedTalentPage < MAX_GEAR_SPEC_TABS)
        {
            uint32 talentTab = 0;
            if (GetClassTalentTab(me->GetClass(), m_requestedTalentPage, talentTab))
            {
                LearnTalentsInTab(talentTab, false);
                learned = true;
            }
        }

        if (learned)
        {
            // The rows of the table are the whole build, but a step can not be taken at all: Talent.dbc gives a
            // talent fewer ranks than the row asks for, or the talent that a step asks for is not in the build.
            // The points that such a row leaves behind go to the tree of the talent, in the order the tree lists
            // its talents, rather than staying unspent; see LearnTalentsFromBuild() for the steps that are only
            // locked for the moment they come.
            if (me->GetFreeTalentPoints())
            {
                uint32 leftTab = 0;
                if (GetClassTalentTab(me->GetClass(), talentPage, leftTab))
                    LearnTalentsInTab(leftTab, false);
            }

            // The spells of the trainers and of the items come after the talents, as in the fallback below.
            char trainerArgs[] = "";
            char itemArgs[] = "";
            ChatHandler(me).HandleLearnAllTrainerCommand(trainerArgs);
            ChatHandler(me).HandleLearnAllItemsCommand(itemArgs);
            return;
        }
    }

    std::vector<PlayerPremadeSpecTemplate const*> vSpecs;
    for (const auto& itr : sObjectMgr.GetPlayerPremadeSpecTemplates())
    {
        if (itr.second.requiredClass == me->GetClass() &&
            itr.second.level == me->GetLevel())
            vSpecs.push_back(&itr.second);
    }
    // Use lower level spec template if there are no templates for the current level.
    if (vSpecs.empty())
    {
        for (const auto& itr : sObjectMgr.GetPlayerPremadeSpecTemplates())
        {
            if (itr.second.requiredClass == me->GetClass() &&
                itr.second.level < me->GetLevel())
                vSpecs.push_back(&itr.second);
        }
    }
    if (!vSpecs.empty())
    {
        PlayerPremadeSpecTemplate const* pSpec = nullptr;
        // Try to find a role appropriate gear template.
        if (m_role != ROLE_INVALID)
        {
            for (const auto itr : vSpecs)
            {
                if (itr->role == m_role &&
                   (!pSpec || pSpec->level < itr->level))
                {
                    pSpec = itr;
                }
            }
        }
        // There is no spec template for this role, pick randomly.
        if (!pSpec)
            pSpec = SelectRandomContainerElement(vSpecs);
        sObjectMgr.ApplyPremadeSpecTemplateToPlayer(pSpec->entry, me);
        if (m_role == ROLE_INVALID)
            m_role = pSpec->role;
    }
    else
    {
        // Use gm command to learn spells on trainers and items.
        LearnRandomTalents();
        char trainerArgs[] = "";
        char itemArgs[] = "";
        ChatHandler(me).HandleLearnAllTrainerCommand(trainerArgs);
        ChatHandler(me).HandleLearnAllItemsCommand(itemArgs);
    }
}

void CombatBotBaseAI::LearnRandomTalents()
{
    std::vector<uint32> talentTabsForClass;
    for (uint32 talentTab = 0; talentTab < sTalentTabStore.GetNumRows(); ++talentTab)
    {
        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentTab);
        if (!talentTabInfo)
            continue;

        if ((me->GetClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        talentTabsForClass.push_back(talentTab);
    }

    if (talentTabsForClass.empty())
        return;

    LearnTalentsInTab(SelectRandomContainerElement(talentTabsForClass), true);
}

void CombatBotBaseAI::LearnTalentsInTab(uint32 talentTab, bool randomOrder)
{
    if (!me->GetFreeTalentPoints())
        return;

    std::map<uint32 /*row*/, std::vector<std::pair<uint32 /*talent id*/, uint32 /*ranks*/>>> possibleTalents;
    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
        if (!talentInfo)
            continue;

        if (talentInfo->TalentTab != talentTab)
            continue;

        uint32 ranks;
        for (ranks = 0; ranks < MAX_TALENT_RANK && talentInfo->RankID[ranks]; ++ranks);
        possibleTalents[talentInfo->Row].push_back({ talentId, ranks });
    }

    if (possibleTalents.empty())
        return;

    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    for (auto& itrRow : possibleTalents)
    {
        // The talents of an asked tree are taken in the order the tree lists them, so that two bots of the same
        // talent are dressed and rated the same way.
        if (randomOrder)
            std::shuffle(itrRow.second.begin(), itrRow.second.end(), std::default_random_engine(seed));

        for (auto const& itrTalent: itrRow.second)
        {
            for (uint32 rank = 0; rank < itrTalent.second; ++rank)
            {
                if (me->LearnTalent(itrTalent.first, rank))
                {
                    if (!me->GetFreeTalentPoints())
                        return;
                }
                else
                    break;
            }
        }
    }
}

bool CombatBotBaseAI::LearnTalentsFromBuild(uint8 talentPage)
{
    std::vector<BotTalentBuildStep> const* steps = GetBotTalentBuild(me->GetClass(), talentPage);
    if (!steps || steps->empty())
        return false;

    // The steps of a build start at the first rank of a talent, so the ranks that this bot already has are
    // counted out first: a bot that was summoned before keeps the talents that it had when it was saved, and a
    // bot that levels up spends the points that it gains from the rank that it stopped at.
    std::map<uint32, uint32> ranksAsked;
    for (BotTalentBuildStep const& step : *steps)
    {
        if (ranksAsked.find(step.talentId) == ranksAsked.end())
            ranksAsked[step.talentId] = GetLearnedTalentRanks(me, step.talentId);
    }

    // Walking the steps once is enough when the order of the table is the order of the build, but a step can not
    // be taken at the moment it comes: the spell that the talent asks for is learned by the trainer after the
    // talents, or the tree does not yet have the points that the row of the talent asks for. Those steps are
    // asked for again after the points that follow them are spent, as a player who finds a talent still locked
    // comes back to it later, and a pass that adds nothing ends the walk.
    bool again = true;
    while (again && me->GetFreeTalentPoints())
    {
        again = false;

        for (BotTalentBuildStep const& step : *steps)
        {
            uint32& asked = ranksAsked[step.talentId];

            for (uint8 rank = 0; rank < step.ranks; ++rank)
            {
                if (!me->GetFreeTalentPoints())
                    break;

                if (!me->LearnTalent(step.talentId, asked))
                    break;

                ++asked;
                again = true;
            }

            if (!me->GetFreeTalentPoints())
                break;
        }
    }

    return true;
}

void CombatBotBaseAI::EquipPremadeGearTemplate()
{
    std::vector<PlayerPremadeGearTemplate const*> vGear;
    for (const auto& itr : sObjectMgr.GetPlayerPremadeGearTemplates())
    {
        if (itr.second.requiredClass == me->GetClass() &&
            itr.second.level <= me->GetLevel())
        {
            if (!vGear.empty())
            {
                if (vGear.front()->level < itr.second.level)
                    vGear.clear();
                else if (vGear.front()->level > itr.second.level)
                    continue;
            }
            vGear.push_back(&itr.second);
        }
    }

    if (!vGear.empty())
    {
        std::vector<PlayerPremadeGearTemplate const*> vGear2;

        // Try to find a role appropriate gear template.
        if (m_role != ROLE_INVALID)
        {
            for (const auto itr : vGear)
            {
                if (itr->role == m_role)
                    vGear2.push_back(itr);
            }
        }

        PlayerPremadeGearTemplate const* pGear;
        if (vGear2.empty())
            pGear = SelectRandomContainerElement(vGear);
        else
            pGear = SelectRandomContainerElement(vGear2);

        sObjectMgr.ApplyPremadeGearTemplateToPlayer(pGear->entry, me);
    }
}

namespace
{
    // The trainers of the cities teach the dual wield with this spell
    constexpr uint32 SPELL_DUAL_WIELD = 674;

    // What says that one candidate of a slot is better than another: the score of its attributes, the damage that
    // a weapon does, or the armor that a shield gives
    enum GearCandidateKey
    {
        GEAR_KEY_SCORE = 0,
        GEAR_KEY_WEAPON_DAMAGE,
        GEAR_KEY_ARMOR,
    };

    struct BotGearCandidate
    {
        ItemPrototype const* proto = nullptr;
        BotGearItemValue value;
        float weaponDamage = 0.0f;
    };

    float GetGearCandidateKey(BotGearCandidate const& candidate, GearCandidateKey key)
    {
        switch (key)
        {
            case GEAR_KEY_WEAPON_DAMAGE:
                return candidate.weaponDamage;
            case GEAR_KEY_ARMOR:
                return float(candidate.proto->Armor);
            default:
                return candidate.value.score;
        }
    }

    bool IsTwoHandedWeapon(ItemPrototype const* proto)
    {
        return proto->InventoryType == INVTYPE_2HWEAPON;
    }

    bool IsOneHandedWeapon(ItemPrototype const* proto)
    {
        return proto->InventoryType == INVTYPE_WEAPON || proto->InventoryType == INVTYPE_WEAPONMAINHAND;
    }

    bool IsShield(ItemPrototype const* proto)
    {
        return proto->InventoryType == INVTYPE_SHIELD;
    }

    bool IsOffHandWeapon(ItemPrototype const* proto)
    {
        return proto->InventoryType == INVTYPE_WEAPON || proto->InventoryType == INVTYPE_WEAPONOFFHAND;
    }

    bool IsAnyOffHandItem(ItemPrototype const* proto)
    {
        return IsShield(proto) || IsOffHandWeapon(proto) || proto->InventoryType == INVTYPE_HOLDABLE;
    }

    bool IsRangedWeapon(ItemPrototype const* proto)
    {
        return proto->InventoryType == INVTYPE_RANGED &&
            (proto->SubClass == ITEM_SUBCLASS_WEAPON_BOW || proto->SubClass == ITEM_SUBCLASS_WEAPON_GUN ||
                proto->SubClass == ITEM_SUBCLASS_WEAPON_CROSSBOW);
    }

    bool IsWand(ItemPrototype const* proto)
    {
        return proto->InventoryType == INVTYPE_RANGEDRIGHT && proto->SubClass == ITEM_SUBCLASS_WEAPON_WAND;
    }

    bool IsAnyGear(ItemPrototype const*)
    {
        return true;
    }

    // The candidate that gives the most of what the slot is rated by and that the bot does not wear yet. When
    // more than one give the same, one of them is chosen, because two players do not choose the same item.
    template<class Predicate>
    BotGearCandidate const* SelectGearCandidate(std::vector<BotGearCandidate> const& candidates,
        GearCandidateKey key, std::set<uint32> const& wornEntries, Predicate allowed)
    {
        float best = 0.0f;
        bool found = false;

        for (auto const& candidate : candidates)
        {
            if (wornEntries.count(candidate.proto->ItemId) || !allowed(candidate.proto))
                continue;

            float const value = GetGearCandidateKey(candidate, key);
            if (!found || value > best)
            {
                best = value;
                found = true;
            }
        }

        if (!found)
            return nullptr;

        // A weapon that does no damage or a shield without armor is not a weapon or a shield for its slot
        if (key != GEAR_KEY_SCORE && best <= 0.0f)
            return nullptr;

        std::vector<BotGearCandidate const*> sameValue;
        for (auto const& candidate : candidates)
        {
            if (wornEntries.count(candidate.proto->ItemId) || !allowed(candidate.proto))
                continue;

            if (GetGearCandidateKey(candidate, key) >= best - 0.001f)
                sameValue.push_back(&candidate);
        }

        return SelectRandomContainerElement(sameValue);
    }

    // The score that a two handed weapon gives, against the score of a one handed weapon and of what can be worn
    // in the offhand with it, to know which of the two ways of dressing the weapon slots gives the most
    template<class PredicateOneHanded, class PredicateOffHand>
    void SelectWeaponPairByScore(std::vector<BotGearCandidate> const& mainHandCandidates,
        std::vector<BotGearCandidate> const& offHandCandidates, std::set<uint32> const& wornEntries,
        BotGearCandidate const*& mainHand, BotGearCandidate const*& offHand,
        PredicateOneHanded oneHanded, PredicateOffHand offHandItem)
    {
        BotGearCandidate const* twoHanded = SelectGearCandidate(mainHandCandidates, GEAR_KEY_SCORE, wornEntries,
            IsTwoHandedWeapon);
        BotGearCandidate const* bestOneHanded = SelectGearCandidate(mainHandCandidates, GEAR_KEY_SCORE, wornEntries,
            oneHanded);

        // The two hands can not wear two copies of the same item
        std::set<uint32> otherHandEntries = wornEntries;
        if (bestOneHanded)
            otherHandEntries.insert(bestOneHanded->proto->ItemId);

        BotGearCandidate const* bestOffHand = SelectGearCandidate(offHandCandidates, GEAR_KEY_SCORE,
            otherHandEntries, offHandItem);

        float const twoHandedScore = twoHanded ? twoHanded->value.score : 0.0f;
        float const pairScore = (bestOneHanded ? bestOneHanded->value.score : 0.0f) +
            (bestOffHand ? bestOffHand->value.score : 0.0f);

        if (twoHanded && (!bestOneHanded || twoHandedScore > pairScore))
        {
            mainHand = twoHanded;
            return;
        }

        mainHand = bestOneHanded;
        offHand = bestOffHand;
    }
}

// The trainers teach the dual wield to the rogues at level 10 and to the warriors and the hunters at level 20,
// and the effect of the spell is not applied again when the bot is logged in, so it must be applied again before
// the gear of the bot is chosen, and the other classes learn it with their talents.
bool CombatBotBaseAI::PrepareDualWield()
{
    uint32 requiredLevel = 0;
    switch (me->GetClass())
    {
        case CLASS_ROGUE:
            requiredLevel = 10;
            break;
        case CLASS_WARRIOR:
        case CLASS_HUNTER:
            requiredLevel = 20;
            break;
        default:
            break;
    }

    if (!me->HasSpell(SPELL_DUAL_WIELD) && requiredLevel && me->GetLevel() >= requiredLevel)
        me->LearnSpell(SPELL_DUAL_WIELD, false, false);

    if (me->HasSpell(SPELL_DUAL_WIELD))
    {
        me->SetCanDualWield(true);
        return true;
    }

    return me->CanDualWield();
}

// Dress the empty equipment slots of the bot with the gear that a player of its talent could realistically have
// got at its level, and rate every item by the attributes that it gives to that talent.
void CombatBotBaseAI::EquipRandomGearInEmptySlots()
{
    LearnArmorProficiencies();

    bool const canDualWield = PrepareDualWield();

    BotGearSpec const spec = GetBotGearSpec(me, m_role);
    BotGearWeaponRules const weaponRules = GetBotWeaponRules(me, spec);

    std::map<uint32 /*slot*/, std::vector<BotGearCandidate>> candidatesPerSlot;

    for (auto const& itr : sObjectMgr.GetItemPrototypeMap())
    {
        ItemPrototype const* pProto = &itr.second;

        // Only the items that someone has already got, and the ones that the world drops or that a profession
        // makes, which are the gear that a player of the level of the bot can realistically have
        if (!pProto->Discovered &&
            !(pProto->SourceKinds & (GEAR_SOURCE_WORLD_LOOT | GEAR_SOURCE_CRAFTED)))
            continue;

        // Skip unobtainable items
        if (pProto->HasExtraFlag(ITEM_EXTRA_NOT_OBTAINABLE))
            continue;

        // No gear that a player has to earn with a reputation standing or an honor rank, and no gear that is
        // only available while a world event or a holiday is active
        if (pProto->SourceRestrictions)
            continue;

        // Only gear and weapons
        if (pProto->Class != ITEM_CLASS_WEAPON && pProto->Class != ITEM_CLASS_ARMOR)
            continue;

        // No tabards and shirts
        if (pProto->InventoryType == INVTYPE_TABARD || pProto->InventoryType == INVTYPE_BODY)
            continue;

        // The effects of the trinkets are not rated, and the relics of the class are not worn by the bots
        if (pProto->InventoryType == INVTYPE_TRINKET || pProto->InventoryType == INVTYPE_RELIC)
            continue;

        // The kinds of weapon that the talent of the bot refuses. The items of those kinds are out of the
        // candidates and not only out of one slot, so that neither hand of a warrior or of a combat rogue can
        // wear a light weapon.
        if (IsWeaponKindRefused(pProto, weaponRules))
            continue;

        // The gear that can only be used with a trade skill, as the engineering and the fishing gear, is not for
        // a bot, which does not learn any trade skill
        if (pProto->RequiredSkill)
            continue;

        if (pProto->SourceQuestRaces && !(pProto->SourceQuestRaces & me->GetRaceMask()))
            continue;

        // No gear of the other faction, whatever its source (vendor, quest, ...) is
        if (pProto->SourceTeams &&
           !(pProto->SourceTeams & (me->GetTeam() == ALLIANCE ? GEAR_SOURCE_TEAM_ALLIANCE : GEAR_SOURCE_TEAM_HORDE)))
            continue;

        if (pProto->SourceQuestClasses && !(pProto->SourceQuestClasses & me->GetClassMask()))
            continue;

        if (pProto->SourceQuestLevel < 0)
        {
            // Avoid higher level items with no level requirement
            if (!pProto->RequiredLevel && pProto->ItemLevel > me->GetLevel())
                continue;
        }
        else
        {
            // Item is from a high level quest
            if (uint32(pProto->SourceQuestLevel) > me->GetLevel())
                continue;
        }

        // Avoid low level items
        if ((pProto->ItemLevel + sWorld.getConfig(CONFIG_UINT32_PARTY_BOT_RANDOM_GEAR_LEVEL_DIFFERENCE)) < me->GetLevel())
            continue;

        // The gear that a player can only get by picking it up is the reward of a content that a bot does not
        // reach: no epic of that kind at all, and the uncommon (ITEM_QUALITY_UNCOMMON, the green gear) and the
        // rare (ITEM_QUALITY_RARE, the blue gear) ones that the quests and the instances drop must not be gear
        // made for a level higher than the level of the bot, which would make the game too easy for it. The gear
        // that is bound when it is equipped is limited by the level that it requires, which CanUseItem below
        // checks.
        if (pProto->Bonding == BIND_WHEN_PICKED_UP)
        {
            if (pProto->Quality >= ITEM_QUALITY_EPIC)
                continue;

            if (pProto->Quality >= ITEM_QUALITY_UNCOMMON &&
               (pProto->SourceKinds & (GEAR_SOURCE_QUEST_REWARD | GEAR_SOURCE_INSTANCE_LOOT)) &&
               pProto->ItemLevel > me->GetLevel())
                continue;
        }

        // The class, the race, the level, the trade skills and the spells that the item requires
        if (me->CanUseItem(pProto, true) != EQUIP_ERR_OK)
            continue;

        if (uint32 skill = pProto->GetProficiencySkill())
        {
            // Don't equip cloth items on warriors, etc unless bot is a healer
            if (pProto->Class == ITEM_CLASS_ARMOR &&
                pProto->InventoryType != INVTYPE_CLOAK &&
                pProto->InventoryType != INVTYPE_SHIELD &&
                skill != me->GetHighestKnownArmorProficiency() &&
                m_role != ROLE_HEALER)
                continue;

            // Fist weapons use unarmed skill calculations, but we must query fist weapon skill presence to use this item
            if (pProto->SubClass == ITEM_SUBCLASS_WEAPON_FIST)
                skill = SKILL_FIST_WEAPONS;
            if (!me->GetSkillValue(skill))
                continue;
        }

        uint8 slots[4];
        pProto->GetAllowedEquipSlots(slots, me->GetClass(), canDualWield);

        bool evaluated = false;
        BotGearCandidate candidate;
        for (uint8 slot : slots)
        {
            if (slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
                continue;

            if (me->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                continue;

            if (!evaluated)
            {
                evaluated = true;
                candidate.proto = pProto;
                EvaluateBotGearItem(me, pProto, spec, candidate.value);
                candidate.weaponDamage = GetItemWeaponDamagePerSecond(pProto);
            }

            candidatesPerSlot[slot].push_back(candidate);
        }
    }

    std::set<uint32> wornEntries;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* pWorn = me->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            wornEntries.insert(pWorn->GetEntry());

    // The weapon slots, because which weapon is worn in one hand depends on what is worn in the other
    BotGearCandidate const* mainHandItem = nullptr;
    BotGearCandidate const* offHandItem = nullptr;

    // A bot that already wears a two handed weapon has no place for anything in its other hand
    Item* wornMainHand = me->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    bool const twoHandedWorn = wornMainHand && wornMainHand->GetProto() &&
        wornMainHand->GetProto()->InventoryType == INVTYPE_2HWEAPON;

    std::vector<BotGearCandidate> const& mainHandCandidates = candidatesPerSlot[EQUIPMENT_SLOT_MAINHAND];
    std::vector<BotGearCandidate> const& offHandCandidates = candidatesPerSlot[EQUIPMENT_SLOT_OFFHAND];

    switch (twoHandedWorn ? GEAR_WEAPON_POLICY_NONE : weaponRules.policy)
    {
        case GEAR_WEAPON_POLICY_TWO_HANDED:
            mainHandItem = SelectGearCandidate(mainHandCandidates,
                weaponRules.byWeaponDamage ? GEAR_KEY_WEAPON_DAMAGE : GEAR_KEY_SCORE, wornEntries,
                IsTwoHandedWeapon);
            break;

        case GEAR_WEAPON_POLICY_DUAL_WIELD:
            mainHandItem = SelectGearCandidate(mainHandCandidates, GEAR_KEY_WEAPON_DAMAGE, wornEntries,
                IsOneHandedWeapon);
            if (mainHandItem)
                wornEntries.insert(mainHandItem->proto->ItemId);
            offHandItem = SelectGearCandidate(offHandCandidates, GEAR_KEY_WEAPON_DAMAGE, wornEntries,
                IsOffHandWeapon);
            break;

        case GEAR_WEAPON_POLICY_ONE_HANDED_SHIELD:
            mainHandItem = SelectGearCandidate(mainHandCandidates, GEAR_KEY_WEAPON_DAMAGE, wornEntries,
                IsOneHandedWeapon);
            if (mainHandItem)
                wornEntries.insert(mainHandItem->proto->ItemId);
            offHandItem = SelectGearCandidate(offHandCandidates, GEAR_KEY_ARMOR, wornEntries, IsShield);
            break;

        case GEAR_WEAPON_POLICY_ONE_HANDED_OFFHAND:
            mainHandItem = SelectGearCandidate(mainHandCandidates, GEAR_KEY_SCORE, wornEntries, IsOneHandedWeapon);
            if (mainHandItem)
                wornEntries.insert(mainHandItem->proto->ItemId);
            offHandItem = SelectGearCandidate(offHandCandidates, GEAR_KEY_SCORE, wornEntries, IsAnyOffHandItem);
            break;

        case GEAR_WEAPON_POLICY_ONE_HANDED:
            mainHandItem = SelectGearCandidate(mainHandCandidates,
                weaponRules.byWeaponDamage ? GEAR_KEY_WEAPON_DAMAGE : GEAR_KEY_SCORE, wornEntries,
                IsOneHandedWeapon);
            break;

        case GEAR_WEAPON_POLICY_TWO_HANDED_OR_ONE_HANDED_OFFHAND:
            SelectWeaponPairByScore(mainHandCandidates, offHandCandidates, wornEntries, mainHandItem, offHandItem,
                IsOneHandedWeapon, IsAnyOffHandItem);
            break;

        case GEAR_WEAPON_POLICY_TWO_HANDED_OR_TWO_ONE_HANDED:
            // The other hand of a hunter takes a weapon, nothing else
            SelectWeaponPairByScore(mainHandCandidates, offHandCandidates, wornEntries, mainHandItem, offHandItem,
                IsOneHandedWeapon, IsOffHandWeapon);
            break;

        case GEAR_WEAPON_POLICY_NONE:
        default:
            break;
    }

    if (mainHandItem)
        wornEntries.insert(mainHandItem->proto->ItemId);
    if (offHandItem)
        wornEntries.insert(offHandItem->proto->ItemId);

    // The other slots, rated by the attributes that the item of each one gives
    for (auto& itr : candidatesPerSlot)
    {
        if (itr.first == EQUIPMENT_SLOT_MAINHAND || itr.first == EQUIPMENT_SLOT_OFFHAND)
            continue;

        GearCandidateKey key = GEAR_KEY_SCORE;
        bool (*allowed)(ItemPrototype const*) = IsAnyGear;

        if (itr.first == EQUIPMENT_SLOT_RANGED)
        {
            // A hunter wears a bow, a gun or a crossbow, the others only a wand for its attributes
            if (weaponRules.rangedWeapon)
            {
                key = GEAR_KEY_WEAPON_DAMAGE;
                allowed = IsRangedWeapon;
            }
            else
            {
                if (!(me->GetClassMask() & CLASSMASK_WAND_USERS))
                    continue;

                allowed = IsWand;
            }
        }

        BotGearCandidate const* chosen = SelectGearCandidate(itr.second, key, wornEntries, allowed);
        if (!chosen)
            continue;

        wornEntries.insert(chosen->proto->ItemId);
        me->StoreNewItemInSlot(uint8(itr.first), chosen->proto->ItemId, chosen->value.randomPropertyId);
    }

    // The weapons are equipped after the armour and before the offhand, which can not be worn with two hands
    if (mainHandItem)
        me->StoreNewItemInSlot(EQUIPMENT_SLOT_MAINHAND, mainHandItem->proto->ItemId,
            mainHandItem->value.randomPropertyId);
    if (offHandItem)
        me->StoreNewItemInSlot(EQUIPMENT_SLOT_OFFHAND, offHandItem->proto->ItemId,
            offHandItem->value.randomPropertyId);
}

void CombatBotBaseAI::PrintGearItemScore(ChatHandler& handler, uint32 itemEntry)
{
    SendBotGearItemScore(me, handler, GetBotGearSpec(me, m_role), itemEntry);
}

void CombatBotBaseAI::AutoEquipGear(uint32 option)
{
    switch (option)
    {
        case PLAYER_BOT_AUTO_EQUIP_STARTING_GEAR:
            me->AddStartingItems();
            break;
        case PLAYER_BOT_AUTO_EQUIP_RANDOM_GEAR:
            EquipRandomGearInEmptySlots();
            break;
        case PLAYER_BOT_AUTO_EQUIP_PREMADE_GEAR:
            EquipPremadeGearTemplate();
            break;
    }

    UpdateVisualHonorRankBasedOnItems();
}

bool CombatBotBaseAI::CanTryToCastSpell(Unit const* pTarget, SpellEntry const* pSpellEntry) const
{
    if (m_preventCasting)
        return false;

    if (!me->IsSpellReady(pSpellEntry))
        return false;

    if (me->HasGCD(pSpellEntry))
        return false;

    if (pSpellEntry->TargetAuraState &&
       !pTarget->HasAuraState(AuraState(pSpellEntry->TargetAuraState)))
        return false;

    if (pSpellEntry->CasterAuraState &&
        !me->HasAuraState(AuraState(pSpellEntry->CasterAuraState)))
        return false;

    uint32 const powerCost = Spell::CalculatePowerCost(pSpellEntry, me);
    Powers const powerType = Powers(pSpellEntry->powerType);

    if (powerType == POWER_HEALTH)
    {
        if (me->GetHealth() <= powerCost)
            return false;
        return true;
    }

    if (me->GetPower(powerType) < powerCost)
        return false;

    if (pTarget->IsImmuneToSpell(pSpellEntry, false))
        return false;

    if (pSpellEntry->GetErrorAtShapeshiftedCast(me->GetShapeshiftForm()) != SPELL_CAST_OK)
        return false;

    if (pSpellEntry->IsSpellAppliesAura() && pTarget->HasAura(pSpellEntry->Id))
        return false;

    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(pSpellEntry->rangeIndex);
    if (me != pTarget && pSpellEntry->EffectImplicitTargetA[0] != TARGET_UNIT_CASTER)
    {
        float const dist = me->GetCombatDistance(pTarget);

        if (dist > srange->maxRange)
            return false;
        if (srange->minRange && dist < srange->minRange)
            return false;
    }

    return true;
}

SpellCastResult CombatBotBaseAI::DoCastSpell(Unit* pTarget, SpellEntry const* pSpellEntry)
{
    if (m_preventCasting)
        return SPELL_FAILED_DONT_REPORT;

    if (me != pTarget)
        me->SetFacingToObject(pTarget);

    if (me->IsMounted())
        me->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    me->SetTargetGuid(pTarget->GetObjectGuid());
    auto result = me->CastSpell(pTarget, pSpellEntry, false);

    //printf("cast %s result %u\n", pSpellEntry->SpellName[0].c_str(), result);

    if ((result == SPELL_FAILED_MOVING ||
        result == SPELL_CAST_OK) &&
        (pSpellEntry->GetCastTime(me) > 0) &&
        (me->IsMoving() || !me->IsStopped()))
        me->StopMoving();

    if ((result == SPELL_FAILED_NEED_AMMO_POUCH ||
        result == SPELL_FAILED_ITEM_NOT_READY) &&
        pSpellEntry->Reagent[0])
    {
        if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START))
            me->DestroyItem(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START, true);

        AddItemToInventory(pSpellEntry->Reagent[0]);
    }

    return result;
}

void CombatBotBaseAI::AddItemToInventory(uint32 itemId, uint32 count)
{
    ItemPosCountVec dest;
    uint8 msg = me->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count);
    if (msg == EQUIP_ERR_OK)
    {
        if (Item* pItem = me->StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId)))
            pItem->SetCount(count);
    }
}

void CombatBotBaseAI::AddHunterAmmo()
{
    if (Item* pWeapon = me->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
    {
        if (ItemPrototype const* pWeaponProto = pWeapon->GetProto())
        {
            if (pWeaponProto->Class == ITEM_CLASS_WEAPON)
            {
                uint32 ammoType;
                switch (pWeaponProto->SubClass)
                {
                    case ITEM_SUBCLASS_WEAPON_GUN:
                        ammoType = ITEM_SUBCLASS_BULLET;
                        break;
                    case ITEM_SUBCLASS_WEAPON_BOW:
                    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                        ammoType = ITEM_SUBCLASS_ARROW;
                        break;
                    default:
                        return;
                }

                ItemPrototype const* pAmmoProto = nullptr;
                for (auto const& itr : sObjectMgr.GetItemPrototypeMap())
                {
                    ItemPrototype const* pProto = &itr.second;

                    if (pProto->Class == ITEM_CLASS_PROJECTILE &&
                        pProto->SubClass == ammoType &&
                        pProto->RequiredLevel <= me->GetLevel() &&
                        (!pAmmoProto || pAmmoProto->ItemLevel < pProto->ItemLevel) &&
                        me->CanUseAmmo(pProto->ItemId) == EQUIP_ERR_OK)
                    {
                        pAmmoProto = pProto;
                    }
                }

                if (pAmmoProto)
                {
                    if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START))
                        me->DestroyItem(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START, true);

                    AddItemToInventory(pAmmoProto->ItemId, pAmmoProto->GetMaxStackSize());
                    me->SetAmmo(pAmmoProto->ItemId);
                }
            }
        }
    }
}

void CombatBotBaseAI::EquipOrUseNewItem()
{
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && !pItem->IsEquipped())
        {
            switch (pItem->GetProto()->Class)
            {
                case ITEM_CLASS_CONSUMABLE:
                {
                    SpellCastTargets targets;
                    targets.setUnitTarget(me);
                    me->CastItemUseSpell(pItem, targets);
                    break;
                }
                case ITEM_CLASS_WEAPON:
                case ITEM_CLASS_ARMOR:
                {
                    uint32 slot = me->FindEquipSlot(pItem->GetProto(), NULL_SLOT, true);
                    if (slot != NULL_SLOT)
                    {
                        if (Item* pItem2 = me->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                            me->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);

                        // Learn required proficiency
                        if (uint32 proficiencySpellId = pItem->GetProto()->GetProficiencySpell())
                            if (!me->HasSpell(proficiencySpellId))
                                me->LearnSpell(proficiencySpellId, false, false);

                        me->RemoveItem(INVENTORY_SLOT_BAG_0, i, false);
                        me->EquipItem(slot, pItem, true);
                    }
                    break;
                }
            }
        }
    }
}

uint8 CombatBotBaseAI::GetHighestHonorRankFromEquippedItems() const
{
    uint8 maxRank = 0;
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetProto()->RequiredHonorRank > maxRank)
                maxRank = pItem->GetProto()->RequiredHonorRank;
        }
    }
    return maxRank;
}

void CombatBotBaseAI::UpdateVisualHonorRankBasedOnItems()
{
    uint8 rank = GetHighestHonorRankFromEquippedItems();
    if (rank > m_visualHonorRank)
        m_visualHonorRank = rank;

    // This is purely visual.
    me->SetByteValue(PLAYER_BYTES_3, PLAYER_BYTES_3_OFFSET_HONOR_RANK, m_visualHonorRank);
    me->SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_HIGHEST_HONOR_RANK, m_visualHonorRank);
}

void CombatBotBaseAI::BeginChasing(Unit* pVictim) const
{
    if ((m_role == ROLE_RANGE_DPS || m_role == ROLE_HEALER) &&
        IsRangedDamageClass(me->GetClass()) &&
       !IsAttackSpeedOverridenForm(me->GetShapeshiftForm()) &&
       (me->GetPowerPercent(POWER_MANA) > 10.0f || me->GetWeaponForAttack(RANGED_ATTACK, true, true)))
        me->SetCasterChaseDistance(25.0f);
    else if (me->HasDistanceCasterMovement())
        me->SetCasterChaseDistance(0.0f);

    // we use dist = 1 always so we can specify angle, instead of spreading around target like mobs
    me->GetMotionMaster()->MoveChase(pVictim, 1.0f, m_role == ROLE_MELEE_DPS ? M_PI_F : 0.0f);
}

bool CombatBotBaseAI::SummonShamanTotems()
{
    if (m_spells.shaman.pAirTotem &&
        !me->GetTotem(TOTEM_SLOT_AIR) &&
        CanTryToCastSpell(me, m_spells.shaman.pAirTotem))
    {
        if (DoCastSpell(me, m_spells.shaman.pAirTotem) == SPELL_CAST_OK)
            return true;
    }

    if (m_spells.shaman.pEarthTotem &&
        !me->GetTotem(TOTEM_SLOT_EARTH) &&
        CanTryToCastSpell(me, m_spells.shaman.pEarthTotem))
    {
        if (DoCastSpell(me, m_spells.shaman.pEarthTotem) == SPELL_CAST_OK)
            return true;
    }

    if (m_spells.shaman.pFireTotem &&
        !me->GetTotem(TOTEM_SLOT_FIRE) &&
        CanTryToCastSpell(me, m_spells.shaman.pFireTotem))
    {
        if (DoCastSpell(me, m_spells.shaman.pFireTotem) == SPELL_CAST_OK)
            return true;
    }

    if (m_spells.shaman.pWaterTotem &&
        !me->GetTotem(TOTEM_SLOT_WATER) &&
        CanTryToCastSpell(me, m_spells.shaman.pWaterTotem))
    {
        if (DoCastSpell(me, m_spells.shaman.pWaterTotem) == SPELL_CAST_OK)
            return true;
    }

    return false;
}

SpellCastResult CombatBotBaseAI::CastWeaponBuff(SpellEntry const* pSpellEntry, EquipmentSlots slot)
{
    Item* pWeapon = me->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!pWeapon)
        return SPELL_FAILED_ITEM_NOT_FOUND;
    if (pWeapon->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return SPELL_FAILED_ITEM_ALREADY_ENCHANTED;

    Spell* spell = new Spell(me, pSpellEntry, true, ObjectGuid(), nullptr, nullptr, nullptr);
    SpellCastTargets targets;
    targets.setItemTarget(pWeapon);
    return spell->prepare(std::move(targets), nullptr);
}

bool CombatBotBaseAI::UseTrinketEffects(bool onlyToBreakCC)
{
    if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TRINKET1))
        if (UseItemEffect(pItem, onlyToBreakCC))
            return true;
    if (Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TRINKET2))
        if (UseItemEffect(pItem, onlyToBreakCC))
            return true;

    return false;
}

bool CombatBotBaseAI::UseItemEffect(Item* pItem, bool onlyToBreakCC)
{
    ItemPrototype const* pProto = pItem->GetProto();
    for (auto const& itr : pProto->Spells)
    {
        if (itr.SpellId && itr.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
        {
            if (SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(itr.SpellId))
            {
                if (me->IsSpellReady(pSpellEntry, pProto))
                {
                    if (onlyToBreakCC && !pSpellEntry->HasAttribute(SPELL_ATTR_EX_IMMUNITY_PURGES_EFFECT))
                        continue;

                    if (pSpellEntry->IsPositiveSpell())
                        return me->CastSpell(me, pSpellEntry, false, pItem) == SPELL_CAST_OK;
                    else if (me->GetVictim())
                        return me->CastSpell(me->GetVictim(), pSpellEntry, false, pItem) == SPELL_CAST_OK;
                }
            }
        }
    }
    return false;
}

namespace
{
    // A player carries the bandages that its first aid has the rank for: first aid is the secondary skill that a
    // player learns at level 10 and that it ranks up as it goes, and every bandage says in its `required_skill_rank`
    // which rank of that skill the bandage asks for. The levels below are where the first aid of a player that
    // adventures has commonly reached that rank, and they are the level at which a bot is given the bandage of the
    // tier. The bandages of the tribal lands (Wool Bandage and Heavy Wool Bandage) ask the ranks of the linen ones
    // and heal the same, and the bandages of the battlegrounds are carried only inside one, so none of those belong
    // here.
    struct BandageTier
    {
        uint8 level;
        uint32 itemEntry;
    };

    BandageTier const bandageTiers[] =
    {
        { 10, 1251 },      // Linen Bandage, first aid 1
        { 20, 2581 },      // Heavy Linen Bandage, first aid 20
        { 28, 6450 },      // Silk Bandage, first aid 100
        { 33, 6451 },      // Heavy Silk Bandage, first aid 125
        { 38, 8544 },      // Mageweave Bandage, first aid 150
        { 43, 8545 },      // Heavy Mageweave Bandage, first aid 175
        { 48, 14529 },     // Runecloth Bandage, first aid 200
        { 54, 14530 },     // Heavy Runecloth Bandage, first aid 225
    };

    // The victuals that a player eats are of the level that it is adventuring at, and so are the spells behind
    // them: every rank of the generic "Food" and "Drink" spells is the use spell of the victuals of one level band,
    // and the item that carries the rank says the level by its own `required_level`. The ranks are named here by the
    // victuals that carry them, with the level those victuals ask for, so that a table can be checked against the
    // item prototypes without guessing what a spell id was meant for.
    struct RecoveryTier
    {
        uint8 level;
        uint32 spellId;
    };

    // "Food": Tough Jerky and Slitherskin Mackerel (level 1), Dalaran Sharp and Conjured Bread (5), Succulent Pork
    // Ribs and Dwarven Mild (10, 15), Stormwind Brie and Wild Hog Shank (25), Fine Aged Cheddar (35), and Alterac
    // Swiss and Dried King Bolete (45). A level 20 character that ate the food of the last rank would be filled by
    // one sitting several times over, which is what no player of that level has in its bags.
    RecoveryTier const foodTiers[] =
    {
        {  1,  433 },      // 61 health over 18 sec
        {  5,  434 },      // 243 health over 21 sec
        { 15,  435 },      // 552 health over 24 sec
        { 25, 1127 },      // 874 health over 27 sec
        { 35, 1129 },      // 1392 health over 30 sec
        { 45, 1131 },      // 2148 health over 30 sec
    };

    // "Drink": Refreshing Spring Water (level 1), Ice Cold Milk (5), Melon Juice (15), Sweet Nectar (25), Moonberry
    // Juice (35), and Morning Glory Dew (45). The water of the last rank is what the bots of a battleground drink,
    // where every player is at the maximum level.
    RecoveryTier const drinkTiers[] =
    {
        {  1,  430 },      // 151 mana over 18 sec
        {  5,  431 },      // 436 mana over 21 sec
        { 15,  432 },      // 835 mana over 24 sec
        { 25, 1133 },      // 1344 mana over 27 sec
        { 35, 1135 },      // 1992 mana over 30 sec
        { 45, 1137 },      // 2934 mana over 30 sec
    };

    // The highest rank of the table that a character of this level buys. A rank whose spell the Spell.dbc of the
    // running patch does not have is passed over, which leaves the rank of the patches before. The rank that the
    // core knows to be food or drink - Spells::GetSpellSpecific() reads the seated requirement and the health or the
    // mana that comes back over time - wins over one that it does not, which keeps a rank out of the way when an id
    // means something else in a patch; a rank that the core does not name is still used, because a bot that never
    // sits down to eat is the worse outcome.
    template <size_t TIER_COUNT>
    uint32 GetHighestRecoverySpell(uint8 level, RecoveryTier const (&tiers)[TIER_COUNT], SpellSpecific specific)
    {
        uint32 unnamedRank = 0;

        for (size_t i = TIER_COUNT; i > 0; --i)
        {
            if (level < tiers[i - 1].level || !sSpellMgr.GetSpellEntry(tiers[i - 1].spellId))
                continue;

            if (Spells::GetSpellSpecific(tiers[i - 1].spellId) == specific)
                return tiers[i - 1].spellId;

            if (!unnamedRank)
                unnamedRank = tiers[i - 1].spellId;
        }

        return unnamedRank;
    }
}

// The bandage that a bot of this level carries, nothing when the bot is too young to have learned first aid.
uint32 CombatBotBaseAI::GetBandageItemEntry() const
{
    uint32 itemEntry = 0;
    for (BandageTier const& tier : bandageTiers)
        if (me->GetLevel() >= tier.level)
            itemEntry = tier.itemEntry;

    return itemEntry;
}

// The food of the level of the bot, nothing when the patch has no rank that it may eat.
uint32 CombatBotBaseAI::GetFoodSpellId() const
{
    return GetHighestRecoverySpell(me->GetLevel(), foodTiers, SPELL_FOOD);
}

// The water of the level of the bot, nothing when the patch has no rank that it may drink.
uint32 CombatBotBaseAI::GetDrinkSpellId() const
{
    return GetHighestRecoverySpell(me->GetLevel(), drinkTiers, SPELL_DRINK);
}

// The best bandage that the bot carries. The bandages are the consumables whose use asks the first aid skill, which
// tells them from every other item of the bags without naming one of them.
Item* CombatBotBaseAI::GetBandageItem() const
{
    Item* pBestItem = nullptr;
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = me->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!pItem)
            continue;

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto->Class != ITEM_CLASS_CONSUMABLE || pProto->RequiredSkill != SKILL_FIRST_AID)
            continue;

        if (!pBestItem || pBestItem->GetProto()->RequiredSkillRank < pProto->RequiredSkillRank)
            pBestItem = pItem;
    }

    return pBestItem;
}

// A player that has lost a lot of its health puts a bandage on before it thinks of sitting down, and it can not put
// a second one on while one is working or while the cooldown of the bandages runs. The stack of the bandage of its
// level is put in the bags of the bot when the bags have none, as the ammo of a hunter is put in the quiver when the
// quiver is empty.
bool CombatBotBaseAI::BandageSelf()
{
    Item* pItem = GetBandageItem();
    if (!pItem)
    {
        uint32 const itemEntry = GetBandageItemEntry();
        if (!itemEntry)
            return false;

        ItemPrototype const* pBandageProto = sObjectMgr.GetItemPrototype(itemEntry);
        if (!pBandageProto)
            return false;

        AddItemToInventory(itemEntry, pBandageProto->GetMaxStackSize());
        pItem = GetBandageItem();
        if (!pItem)
            return false;
    }

    ItemPrototype const* pProto = pItem->GetProto();
    for (auto const& itr : pProto->Spells)
    {
        if (!itr.SpellId || itr.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            continue;

        SpellEntry const* pSpellEntry = sSpellMgr.GetSpellEntry(itr.SpellId);
        if (!pSpellEntry)
            continue;

        if (!me->IsSpellReady(pSpellEntry, pProto) || me->HasAura(pSpellEntry->Id))
            continue;

        SpellCastTargets targets;
        targets.setUnitTarget(me);
        me->CastItemUseSpell(pItem, targets);
        return true;
    }

    return false;
}

namespace
{
    // A character rides the mount of its race: the vendor of the race sells the mount of sixty per cent speed to a
    // character of forty and the one of a hundred per cent to a character of sixty, and each is a spell the
    // character learns. A paladin and a warlock ride the warhorse or the steed of their class rather than the mount
    // of the same speed of their race, because the class teaches that one to its own member. The gold that a player
    // pays for the mounts is not asked here - the money of a bot is not spent as a player spends it - only the
    // mount that a character of that race, class and level is able to have.
    struct MountTier
    {
        uint8 level;
        uint8 race;           // the race whose members ride this mount, nothing for the mount of a class
        uint8 playerClass;    // the class whose members ride this mount, nothing for the mount of a race
        uint32 spellId;
    };

    MountTier const mountTiers[] =
    {
        { 40, RACE_HUMAN, 0, 470 },               // Black Stallion
        { 40, RACE_NIGHTELF, 0, 10787 },          // Black Panther
        { 40, RACE_DWARF, 0, 6896 },              // Black Ram
        { 40, RACE_GNOME, 0, 17456 },             // Mechanostrider
        { 40, RACE_TROLL, 0, 10795 },             // Ivory Raptor
        { 40, RACE_ORC, 0, 581 },                 // Winter Wolf
        { 40, RACE_TAUREN, 0, 18363 },            // Riding Kodo
        { 40, RACE_UNDEAD, 0, 8980 },             // Skeletal Horse

        { 60, RACE_HUMAN, 0, 22717 },             // Black War Steed
        { 60, RACE_NIGHTELF, 0, 22723 },          // Black War Tiger
        { 60, RACE_DWARF, 0, 22720 },             // Black War Ram
        { 60, RACE_GNOME, 0, 22719 },             // Black Battlestrider
        { 60, RACE_TROLL, 0, 22721 },             // Black War Raptor
        { 60, RACE_ORC, 0, 22724 },               // Black War Wolf
        { 60, RACE_TAUREN, 0, 22718 },            // Black War Kodo
        { 60, RACE_UNDEAD, 0, 22722 },            // Red Skeletal Warhorse

        { 40, 0, CLASS_PALADIN, 13819 },          // Summon Warhorse
        { 60, 0, CLASS_PALADIN, 23214 },          // Summon Warhorse, the one of a hundred per cent speed
        { 40, 0, CLASS_WARLOCK, 5784 },           // Summon Felsteed
        { 60, 0, CLASS_WARLOCK, 23161 },          // Summon Felsteed, the one of a hundred per cent speed
    };

    // Patch 1.12 joined the riding of every race into one skill, and the core teaches that skill to a character that
    // reaches the patch with these two spells (Player::UpdateOldRidingSkillToNew). A bot learns the same ones, so
    // that its spellbook and its character sheet say what the patch makes a rider have; under the patches before,
    // RaiseRidingSkill() raises the riding skill of the mount of the race instead.
    enum RidingSkillSpells
    {
        SPELL_APPRENTICE_RIDING = 33388,
        SPELL_JOURNEYMAN_RIDING = 33391,
    };

    // Under the patches before 1.12 the riding was the skill of the mount of one's own race, where the rank of
    // seventy five let a character ride at sixty per cent speed and the rank of one hundred and fifty at a hundred
    // per cent. The names of those skills are the one place of this subsystem where a skill is written down, and
    // they are used only when SkillLineAbility.dbc says nothing about the mount of a race.
    struct LegacyRidingSkill
    {
        uint8 race;
        uint16 skillId;
    };

    LegacyRidingSkill const legacyRidingSkills[] =
    {
        { RACE_HUMAN, SKILL_RIDING_HORSE },
        { RACE_DWARF, SKILL_RIDING_RAM },
        { RACE_NIGHTELF, SKILL_RIDING_TIGER },
        { RACE_ORC, SKILL_RIDING_WOLF },
        { RACE_TROLL, SKILL_RIDING_RAPTOR },
        { RACE_GNOME, SKILL_RIDING_MECHANOSTRIDER },
        { RACE_UNDEAD, SKILL_RIDING_UNDEAD_HORSE },
        { RACE_TAUREN, SKILL_RIDING_KODO },
    };

    void RaiseSkillToRank(Player* pPlayer, uint16 skillId, uint16 wantedRank)
    {
        if (!skillId || !wantedRank || pPlayer->GetSkillValuePure(skillId) >= wantedRank)
            return;

        uint16 maxRank = pPlayer->GetSkillMaxPure(skillId);
        if (wantedRank > maxRank)
            maxRank = wantedRank;

        pPlayer->SetSkill(skillId, wantedRank, maxRank);
    }

    void RaiseRidingSkill(Player* pPlayer, uint32 mountSpellId, uint8 level, bool isRaceMount)
    {
        // A summon asks for the riding of the one who rides it, and SkillLineAbility.dbc says which skill and which
        // rank of it each summon asks for: the riding of the character's own race under the patches before 1.12, and
        // the one riding skill of every mount from that patch on. To raise the skill to the rank that the summon
        // asks for is what the trainer of the ride does when it teaches the mount, and under the patches before 1.12
        // it is what the quest leading a character of sixty to its epic bridle does.
        bool askedByData = false;

        SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(mountSpellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            SkillLineAbilityEntry const* pAbility = itr->second;
            if (!pAbility || !pAbility->skillId || pAbility->skillId >= MAX_SKILL_TYPE)
                continue;

            // The riding of one race is not a skill of the patches that joined them into one, and a skill that the
            // SkillLine.dbc of the running patch does not have can not be given to anybody. The summon is left
            // usable all the same, because the core does not ask the skill of a spell that it casts itself.
            if (!sSkillLineStore.LookupEntry(pAbility->skillId))
                continue;

            askedByData = true;

            uint32 wantedRank = pAbility->req_skill_value;
            if (pAbility->min_value > wantedRank)
                wantedRank = pAbility->min_value;

            RaiseSkillToRank(pPlayer, static_cast<uint16>(pAbility->skillId), static_cast<uint16>(wantedRank));
        }

        // A mount of a class is taught by the class and asks no riding of its own, so only the mount of a race is
        // measured against the skills above.
        if (askedByData || !isRaceMount)
            return;

        uint16 const wantedRank = level >= 60 ? 150 : 75;
        for (LegacyRidingSkill const& entry : legacyRidingSkills)
        {
            if (entry.race != pPlayer->GetRace() || !sSkillLineStore.LookupEntry(entry.skillId))
                continue;

            RaiseSkillToRank(pPlayer, entry.skillId, wantedRank);
        }
    }
}

// The mount that this bot rides: the highest rank of its own race or class that a character of its level has
// learned the summon of. Nothing when the bot has no mount to ride, which is also the test that says whether a
// shaman or a druid of a battleground has to move on its own legs and in its own form.
uint32 CombatBotBaseAI::GetMountSpellId() const
{
    uint32 raceMount = 0;
    uint32 classMount = 0;
    uint8 reachedLevel = 0;

    for (MountTier const& tier : mountTiers)
    {
        if (me->GetLevel() < tier.level || tier.level < reachedLevel)
            continue;

        if (tier.race && tier.race != me->GetRace())
            continue;

        if (tier.playerClass && tier.playerClass != me->GetClass())
            continue;

        if (!me->HasSpell(tier.spellId))
            continue;

        if (tier.level > reachedLevel)
        {
            reachedLevel = tier.level;
            raceMount = 0;
            classMount = 0;
        }

        if (tier.playerClass)
            classMount = tier.spellId;
        else
            raceMount = tier.spellId;
    }

    return classMount ? classMount : raceMount;
}

// The riding skill and the summons of the mounts that a character of this race, class and level would have been
// taught by now. It is asked for when the bot is created and when the level of the bot is other than the level it
// was taught at, which is where a party that plays together reaches the level of a mount: a character buys the mount
// of the rank its level has reached and learns it once. What the bot already knows is left alone, so a bot that was
// saved as a rider keeps what it had.
void CombatBotBaseAI::LearnRidingAndMounts()
{
    // A party that plays grows, and the level it grows to is asked here rather than watched anywhere else: the
    // riding and the mounts of a rank are taught at the level that reaches it, once, and nothing is looked at while
    // the level of the bot is the level it was last taught at.
    if (me->GetLevel() == m_ridingTaughtLevel)
        return;

    m_ridingTaughtLevel = me->GetLevel();

    // The riding of the patches from 1.12 on, learned from the riding trainer of the city as a character learns it.
    // The rank of the skill comes with the spell, which is what the spells of the two ranks teach.
    if (me->GetLevel() >= 40 && sSpellMgr.GetSpellEntry(SPELL_APPRENTICE_RIDING) &&
        !me->HasSpell(SPELL_APPRENTICE_RIDING))
        me->LearnSpell(SPELL_APPRENTICE_RIDING, false, false);

    // The higher rank takes the lower one out of the spellbook, as it does for a character that grows.
    if (me->GetLevel() >= 60 && sSpellMgr.GetSpellEntry(SPELL_JOURNEYMAN_RIDING) &&
        !me->HasSpell(SPELL_JOURNEYMAN_RIDING))
        me->LearnSpell(SPELL_JOURNEYMAN_RIDING, false, false);

    for (MountTier const& tier : mountTiers)
    {
        if (me->GetLevel() < tier.level)
            continue;

        if (tier.race && tier.race != me->GetRace())
            continue;

        if (tier.playerClass && tier.playerClass != me->GetClass())
            continue;

        // A mount whose summon the Spell.dbc of the running patch does not have is passed over, which leaves a patch
        // of a low maximum level with the mounts that it has.
        if (!sSpellMgr.GetSpellEntry(tier.spellId))
            continue;

        RaiseRidingSkill(me, tier.spellId, tier.level, tier.race != 0);

        if (!me->HasSpell(tier.spellId))
            me->LearnSpell(tier.spellId, false, false);
    }
}

// The mount is called the way a character calls it: the summon goes through the normal pipeline of a spell, so the
// cast bar of the bot shows the three seconds that a mount of this era takes and the mount comes under the rules
// that stop a spell. When a bot rides is left to the caller, which knows what the party or the battleground is
// doing; here only whether the bot is on foot and what it would ride are asked.
bool CombatBotBaseAI::SummonMount()
{
    if (me->IsMounted())
        return false;

    uint32 const spellId = GetMountSpellId();
    if (!spellId)
        return false;

    return me->CastSpell(me, spellId, false) == SPELL_CAST_OK;
}

void CombatBotBaseAI::BreakCrowdControlEffects()
{
    if (UseTrinketEffects(true))
        return;

    switch (me->GetClass())
    {
        case CLASS_PALADIN:
        {
            if (m_spells.paladin.pDivineShield &&
                CanTryToCastSpell(me, m_spells.paladin.pDivineShield))
            {
                if (DoCastSpell(me, m_spells.paladin.pDivineShield) == SPELL_CAST_OK)
                {
                    if (m_role != ROLE_HEALER)
                    {
                        me->m_Events.AddLambdaEventAtOffset([player = me, spellId = m_spells.paladin.pDivineShield->Id]()
                        {
                            if (player->GetHealthPercent() > 75.0f && player->GetAttackers().size() < 3)
                                player->RemoveAurasDueToSpellByCancel(spellId);
                        }, 1 * IN_MILLISECONDS);
                    }
                    return;
                }
            }
            break;
        }
        case CLASS_MAGE:
        {
            if (me->HasUnitState(UNIT_STATE_STUNNED) && m_spells.mage.pBlink &&
                CanTryToCastSpell(me, m_spells.mage.pBlink))
            {
                if (DoCastSpell(me, m_spells.mage.pBlink) == SPELL_CAST_OK)
                    return;
            }
            if (m_spells.mage.pIceBlock &&
                CanTryToCastSpell(me, m_spells.mage.pIceBlock))
            {
                if (DoCastSpell(me, m_spells.mage.pIceBlock) == SPELL_CAST_OK)
                {
                    me->m_Events.AddLambdaEventAtOffset([player = me, spellId = m_spells.mage.pIceBlock->Id]()
                    {
                        if (player->GetHealthPercent() > 75.0f && player->GetAttackers().size() < 3)
                            player->RemoveAurasDueToSpellByCancel(spellId);
                    }, 1 * IN_MILLISECONDS);
                    return;
                }
            }
            break;
        }
        case CLASS_DRUID:
        {
            bool polymorphed = false;
            auto const& auraList = me->GetAurasByType(SPELL_AURA_MOD_CONFUSE);
            for (auto const& pAura : auraList)
            {
                if (pAura->GetSpellProto()->Mechanic == MECHANIC_POLYMORPH)
                {
                    polymorphed = true;
                    break;
                }
            }

            if (polymorphed)
            {
                SpellEntry const* pShapeshift = nullptr;

                if (m_role == ROLE_TANK && m_spells.druid.pBearForm && CanTryToCastSpell(me, m_spells.druid.pBearForm))
                    pShapeshift = m_spells.druid.pBearForm;
                else if (m_role == ROLE_MELEE_DPS && m_spells.druid.pCatForm && CanTryToCastSpell(me, m_spells.druid.pCatForm))
                    pShapeshift = m_spells.druid.pCatForm;
                else if (m_role == ROLE_RANGE_DPS && m_spells.druid.pMoonkinForm && CanTryToCastSpell(me, m_spells.druid.pMoonkinForm))
                    pShapeshift = m_spells.druid.pMoonkinForm;
                else
                {
                    for (auto const& pSpell : m_spells.raw.spells)
                    {
                        if (pSpell && pSpell->HasAura(SPELL_AURA_MOD_SHAPESHIFT) && CanTryToCastSpell(me, pSpell))
                        {
                            pShapeshift = pSpell;
                            break;
                        }
                    }
                }

                if (pShapeshift && DoCastSpell(me, pShapeshift) == SPELL_CAST_OK)
                {
                    if (m_role == ROLE_HEALER)
                    {
                        me->m_Events.AddLambdaEventAtOffset([player = me, spellId = pShapeshift->Id]()
                        {
                            player->RemoveAurasDueToSpellByCancel(spellId);
                        }, 1 * IN_MILLISECONDS);
                    }
                    return;
                }
            }
            break;
        }
    }
}

bool CombatBotBaseAI::IsWearingShield(Player* pPlayer) const
{
    Item* pItem = pPlayer->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!pItem)
        return false;

    if (pItem->GetProto()->InventoryType == INVTYPE_SHIELD)
        return true;

    return false;
}

bool CombatBotBaseAI::IsInDuel() const
{
    return me->m_duel && me->m_duel->startTime != 0;
}

CombatBotRoles CombatBotBaseAI::GetRole() const
{
    if (m_role == ROLE_HEALER && IsInDuel())
    {
        if (IsMeleeDamageClass(me->GetClass()))
            return ROLE_MELEE_DPS;
        else
            return ROLE_RANGE_DPS;
    }

    return m_role;
}

void CombatBotBaseAI::SendBattlefieldPortPacket()
{
    for (uint32 i = BATTLEGROUND_QUEUE_AV; i <= BATTLEGROUND_QUEUE_AB; i++)
    {
        if (me->IsInvitedForBattleGroundQueueType(static_cast<BattleGroundQueueTypeId>(i)))
        {
            WorldPackets::Battleground::BattleFieldPort packet;
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_8_4
            packet.mapId = GetBattleGrounMapIdByTypeId(static_cast<BattleGroundTypeId>(i));
#endif
            packet.action = 1;
            me->GetSession()->HandleBattleFieldPortOpcode(packet);
            break;
        }
    }
}

void CombatBotBaseAI::SendBattlemasterJoinPacket(uint8 battlegroundId)
{
    uint32 instanceId = 0; // first available
    uint32 mapId;
    switch (battlegroundId)
    {
        case BATTLEGROUND_QUEUE_AV:
            mapId = MAP_ALTERAC_VALLEY;
            break;
        case BATTLEGROUND_QUEUE_WS:
            mapId = MAP_WARSONG_GULCH;
            break;
        case BATTLEGROUND_QUEUE_AB:
            mapId = MAP_ARATHI_BASIN;
            break;
        default:
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "BattleBot: Invalid BG queue type!");
            botEntry->requestRemoval = true;
            return;
    }

    me->GetSession()->RequestBgJoinQueue(me->GetObjectGuid(), instanceId, mapId, false);
}

void CombatBotBaseAI::SendAreaTriggerPacket(uint32 areaTriggerId)
{
    WorldPackets::Misc::AreaTrigger packet;
    packet.triggerId = areaTriggerId;
    me->GetSession()->HandleAreaTriggerOpcode(packet);
}

void CombatBotBaseAI::ActivateNearbyAreaTrigger()
{
    for (auto const& itr : sObjectMgr.GetAreaTriggersMap())
    {
        AreaTriggerEntry const* pTrigger = &itr.second;
        if (!pTrigger)
            continue;

        if (!IsPointInAreaTriggerZone(pTrigger, me->GetMapId(), me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 5.0f))
            continue;

        SendAreaTriggerPacket(pTrigger->id);
        break;
    }
}

void CombatBotBaseAI::OnPacketReceived(WorldPacket const* packet)
{
    // Must always check "me" player pointer here!
    //printf("Bot received %s\n", LookupOpcodeName(packet->GetOpcode()));
    switch (packet->GetOpcode())
    {
        case SMSG_NEW_WORLD:
        {
            if (!me)
                return;

            auto data = std::make_unique<NullClientPacket>(MSG_MOVE_WORLDPORT_ACK);
            me->GetSession()->QueuePacket(std::move(data));
            break;
        }
        case MSG_MOVE_TELEPORT_ACK:
        {
            if (!me)
                return;

            auto data = std::make_unique<WorldPackets::Movement::MoveTeleportAck>();
            data->guid = me->GetObjectGuid();
#if SUPPORTED_CLIENT_BUILD > CLIENT_BUILD_1_9_4
            data->movementCounter = me->GetLastCounterForMovementChangeType(TELEPORT);
#endif
            data->time = uint32(time(nullptr));
            me->GetSession()->QueuePacket(std::move(data));
            break;
        }
        case SMSG_LOGIN_SETTIMESPEED:
        {
            if (!me)
                return;

            UpdateVisualHonorRankBasedOnItems();
            break;
        }
        case SMSG_TRADE_STATUS:
        {
            if (!me)
                return;

            uint32 status = *((uint32*)(*packet).contents());
            if (status == TRADE_STATUS_BEGIN_TRADE)
            {
                auto data = std::make_unique<NullClientPacket>(CMSG_BEGIN_TRADE);
                me->GetSession()->QueuePacket(std::move(data));
            }
            else if (status == TRADE_STATUS_TRADE_ACCEPT)
            {
                auto data = std::make_unique<WorldPackets::Trade::AcceptTrade>();
                me->GetSession()->QueuePacket(std::move(data));
            }
            else if (status == TRADE_STATUS_TRADE_COMPLETE)
            {
                EquipOrUseNewItem();
                UpdateVisualHonorRankBasedOnItems();
            }
            break;
        }
        case SMSG_RESURRECT_REQUEST:
        {
            if (!me)
                return;

            auto data = std::make_unique<WorldPackets::Misc::ResurrectResponse>();
            data->resurrectorGuid = me->GetResurrector();
            data->accept = true;
            me->GetSession()->QueuePacket(std::move(data));
            break;
        }
        case SMSG_BATTLEFIELD_STATUS:
        {
            if (!me)
                return;

            if (me->IsBeingTeleported() || me->InBattleGround())
                m_receivedBgInvite = false;
            else
            {
                for (uint32 i = BATTLEGROUND_QUEUE_AV; i <= BATTLEGROUND_QUEUE_AB; i++)
                {
                    if (me->IsInvitedForBattleGroundQueueType(BattleGroundQueueTypeId(i)))
                    {
                        m_receivedBgInvite = true;
                        break;
                    }
                }
            }
            return;
        }
        case SMSG_LOOT_START_ROLL:
        {
            if (!me)
                return;

            uint64 guid = *((uint64*)(*packet).contents());
            uint32 slot = *(((uint32*)(*packet).contents()) + 2);

            auto data = std::make_unique<WorldPackets::Loot::LootRoll>();
            data->lootedTarget = ObjectGuid(guid);
            data->itemSlot = slot;
            data->rollType = ROLL_PASS;
            me->GetSession()->QueuePacket(std::move(data));
            return;
        }
    }
}
