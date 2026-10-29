#include "BotGearScoring.h"

#include "Chat.h"
#include "Database/DatabaseEnv.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "ItemPrototype.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "SpellEntry.h"
#include "SpellMgr.h"

#include <cstring>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace
{
    // The weight of every attribute for every talent, in the order of the values of BotGearStat:
    // strength agility intellect spirit stamina mana health melee ap ranged ap spell damage healing physical crit
    // physical hit spell crit spell hit mana per 5 resistance dodge parry block.
    // The talents that were rated by hand keep the values they were given, the other ones follow the same scale,
    // and every value can be changed in the table `playerbot_gear_stat_weights` without compiling again.
    float const defaultGearStatWeights[MAX_GEAR_SPECS][MAX_GEAR_STATS] =
    {
        /*         STR AGI INT SPI STA MAN HEA MAP RAP SPD HEL PCR PHR SCR SHR M5 RES DDG PAR BLK */
        /* none*/  {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
        /* arms*/  {2.0,1.0,  0,  0,0.5,  0,0.05,1.0,  0,  0,  0,20,20,  0,  0,  0,  0,  0,  0,  0 },
        /* fury*/  {2.0,1.0,  0,  0,0.5,  0,0.05,1.0,  0,  0,  0,20,20,  0,  0,  0,  0,  0,  0,  0 },
        /* protw*/{1.0,1.5,  0,  0,3.0,  0, 0.3,1.5,  0,  0,  0, 5,15,  0,  0,  0,0.5,  0,  0,  0 },
        /* holy*/  {0.5,  0,3.0,0.5,1.0,0.2, 0.1, 0,  0,  0,2.0, 0, 0,15, 5,2.0, 0, 0, 0, 0 },
        /* protp*/{1.0,1.5,0.5,0.25,3.0,0.03,0.3,1.5,0,0.25,0, 5,15, 0, 0,1.0,0.5,0, 0, 0 },
        /* ret*/   {2.0,1.5,1.0,0.25,1.0,0.07,0.1,1.0,0,0.5, 0,20,20, 0, 0,1.0, 0, 0, 0, 0 },
        /* hnt*/   {  0,2.5,  0,  0,0.5,0,0.05,0,1.5, 0, 0,20,20, 0, 0,0.5, 0, 0, 0, 0 },
        /* rg*/    {1.0,2.0,  0,  0,0.5,  0,0.05,1.0, 0,  0,  0,20,20, 0,  0,  0,  0,  0,  0,  0 },
        /* dis*/   {  0,  0,3.0,0.5,1.0,0.2,0.1,  0,  0,  0,2.5, 0, 0,20,10,2.0, 0, 0, 0, 0 },
        /* holyp*/ {  0,  0,3.0,0.5,1.0,0.2,0.1,  0,  0,  0,2.5, 0, 0,20, 5,2.0, 0, 0, 0, 0 },
        /* shd*/   {  0,  0,1.0,0.5,0.5,0.07,0.1, 0,  0,1.0, 0,  0, 0,20,20,1.0, 0, 0, 0, 0 },
        /* ele*/   {  0,  0,1.0,0.5,0.5,0.07,0.1, 0,  0,1.0, 0,  0, 0,20,20,1.0, 0, 0, 0, 0 },
        /* enh*/   {2.0,1.5,1.0,  0,0.5,0.07,0.05,1.0,0,0.5,0.5,15,15,0, 0,  0,  0, 0, 0, 0 },
        /* shm*/   {  0,  0,3.0,1.0,1.0,0.2,0.1,  0,  0,  0,2.5, 0, 0,15, 5,2.0, 0, 0, 0, 0 },
        /* mge*/   {  0,  0,1.0,  0,0.5,0.07,0.05,0,  0,1.0, 0,  0, 0,20,20, 0,  0, 0, 0, 0 },
        /* wlk*/   {  0,  0,1.0,0.25,1.0,0.07,0.1,0,  0,1.0, 0,  0, 0,20,20,0.5, 0, 0, 0, 0 },
        /* bal*/   {  0,  0,1.0,0.5,1.0,0.07,0.1, 0,  0,1.0, 0,  0, 0,20,20,1.0, 0, 0, 0, 0 },
        /* cat*/   {1.0,2.0,0.5,0.25,1.0,0.03,0.1,1.5,0, 0,  0,20,20, 0,  0,0.5, 0, 0, 0, 0 },
        /* ber*/   {0.5,2.0,0.5,  0,3.0,0.03,0.3,1.0, 0,  0,  0, 5,10, 0,  0,  0,0.5, 0, 0, 0 },
        /* drd*/   {  0,  0,3.0,1.0,1.0,0.2,0.1,  0,  0,  0,2.5, 0, 0,15, 5,2.0, 0, 0, 0, 0 },
    };

    // The weights in use, the defaults above until the table is read
    float gearStatWeights[MAX_GEAR_SPECS][MAX_GEAR_STATS];

    char const* const gearSpecNames[MAX_GEAR_SPECS] =
    {
        "none",
        "arms warrior",
        "fury warrior",
        "protection warrior",
        "holy paladin",
        "protection paladin",
        "retribution paladin",
        "hunter",
        "rogue",
        "discipline priest",
        "holy priest",
        "shadow priest",
        "elemental shaman",
        "enhancement shaman",
        "restoration shaman",
        "mage",
        "warlock",
        "balance druid",
        "feral cat druid",
        "feral bear druid",
        "restoration druid",
    };

    char const* const gearStatNames[MAX_GEAR_STATS] =
    {
        "strength",
        "agility",
        "intelligence",
        "spirit",
        "stamina",
        "mana",
        "health",
        "melee attack power",
        "ranged attack power",
        "spell damage",
        "healing",
        "physical critical strike %",
        "physical hit %",
        "spell critical strike %",
        "spell hit %",
        "mana per 5 sec",
        "resistance",
        "dodge %",
        "parry %",
        "block %",
    };

    // The attributes that are a percentage are stored in hundredths of percent
    constexpr float AURA_PERCENT_FACTOR = 0.01f;

    void AddStat(BotGearItemValue& value, BotGearStat stat, float amount)
    {
        value.stats[stat] += amount;
    }

    // One of the stats of an item, both the ones of the item and the ones that a random property adds with
    // ITEM_ENCHANTMENT_TYPE_STAT
    void AddItemMod(BotGearItemValue& value, uint32 itemMod, float amount)
    {
        switch (itemMod)
        {
            case ITEM_MOD_MANA:
                AddStat(value, GEAR_STAT_MANA, amount);
                break;
            case ITEM_MOD_HEALTH:
                AddStat(value, GEAR_STAT_HEALTH, amount);
                break;
            case ITEM_MOD_AGILITY:
                AddStat(value, GEAR_STAT_AGILITY, amount);
                break;
            case ITEM_MOD_STRENGTH:
                AddStat(value, GEAR_STAT_STRENGTH, amount);
                break;
            case ITEM_MOD_INTELLECT:
                AddStat(value, GEAR_STAT_INTELLECT, amount);
                break;
            case ITEM_MOD_SPIRIT:
                AddStat(value, GEAR_STAT_SPIRIT, amount);
                break;
            case ITEM_MOD_STAMINA:
                AddStat(value, GEAR_STAT_STAMINA, amount);
                break;
            default:
                break;
        }
    }

    // The attributes that the spells of an item add while it is equipped. The spells that the item casts on use,
    // the spells that its weapon procs cast and the periodic effects are not counted, like the effects of the
    // trinkets, because a bot does not use them on purpose.
    void AddSpellValue(Player* me, SpellEntry const* spell, BotGearItemValue& value)
    {
        int32 level = int32(me->GetLevel());
        if (spell->maxLevel && uint32(level) > spell->maxLevel)
            level = int32(spell->maxLevel);
        else if (uint32(level) < spell->baseLevel)
            level = int32(spell->baseLevel);
        level -= int32(spell->spellLevel);

        for (uint32 effect = 0; effect < MAX_EFFECT_INDEX; ++effect)
        {
            if (!Spells::IsEffectAppliesAura(spell->Effect[effect]))
                continue;

            float amount = float(spell->EffectBasePoints[effect]) +
                float(level) * spell->EffectRealPointsPerLevel[effect];
            if (amount == 0.0f)
                continue;

            switch (spell->EffectApplyAuraName[effect])
            {
                case SPELL_AURA_MOD_ATTACK_POWER:
                    AddStat(value, GEAR_STAT_MELEE_ATTACK_POWER, amount);
                    break;
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS:
                    AddStat(value, GEAR_STAT_RANGED_ATTACK_POWER, amount);
                    break;
                case SPELL_AURA_MOD_DAMAGE_DONE:
                    // The auras of the items raise both the damage and the healing of the spells
                    AddStat(value, GEAR_STAT_SPELL_DAMAGE, amount);
                    AddStat(value, GEAR_STAT_HEALING, amount);
                    break;
                case SPELL_AURA_MOD_HEALING_DONE:
                    AddStat(value, GEAR_STAT_HEALING, amount);
                    break;
                case SPELL_AURA_MOD_CRIT_PERCENT:
                case SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE:
                case SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE:
                    AddStat(value, GEAR_STAT_PHYSICAL_CRIT, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_HIT_CHANCE:
                case SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE:
                case SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE:
                    AddStat(value, GEAR_STAT_PHYSICAL_HIT, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
                case SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL:
                case SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE:
                    AddStat(value, GEAR_STAT_SPELL_CRIT, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_SPELL_HIT_CHANCE:
                case SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE:
                    AddStat(value, GEAR_STAT_SPELL_HIT, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_POWER_REGEN:
                    AddStat(value, GEAR_STAT_MANA_REGENERATION, amount);
                    break;
                case SPELL_AURA_MOD_RESISTANCE:
                case SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE:
                    AddStat(value, GEAR_STAT_RESISTANCE, amount);
                    break;
                case SPELL_AURA_MOD_DODGE_PERCENT:
                    AddStat(value, GEAR_STAT_DODGE, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_PARRY_PERCENT:
                    AddStat(value, GEAR_STAT_PARRY, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_BLOCK_PERCENT:
                    AddStat(value, GEAR_STAT_BLOCK, amount * AURA_PERCENT_FACTOR);
                    break;
                case SPELL_AURA_MOD_INCREASE_ENERGY:
                    AddStat(value, GEAR_STAT_MANA, amount);
                    break;
                case SPELL_AURA_MOD_INCREASE_HEALTH:
                    AddStat(value, GEAR_STAT_HEALTH, amount);
                    break;
                default:
                    break;
            }
        }
    }

    // The attributes that one random property of an item adds
    void AddEnchantmentValue(Player* me, uint32 enchantId, BotGearItemValue& value,
        std::unordered_set<uint32>& castSpells)
    {
        SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!enchant)
            return;

        for (uint32 effect = 0; effect < 3; ++effect)
        {
            switch (enchant->type[effect])
            {
                case ITEM_ENCHANTMENT_TYPE_STAT:
                    AddItemMod(value, enchant->spellid[effect], float(enchant->amount[effect]));
                    break;
                case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                    AddStat(value, GEAR_STAT_RESISTANCE, float(enchant->amount[effect]));
                    break;
                case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
                    if (enchant->spellid[effect] && castSpells.insert(enchant->spellid[effect]).second)
                    {
                        if (SpellEntry const* spell = sSpellMgr.GetSpellEntry(enchant->spellid[effect]))
                            AddSpellValue(me, spell, value);
                    }
                    break;
                default:
                    // The damage that the enchantments of a weapon add is not rated, because the damage of a weapon
                    // is what its tooltip shows, and the combat spells and the totems are not rated either
                    break;
            }
        }
    }

    float ScoreValue(BotGearSpec spec, BotGearItemValue const& value)
    {
        float score = 0.0f;
        for (uint32 stat = 0; stat < MAX_GEAR_STATS; ++stat)
            score += value.stats[stat] * gearStatWeights[spec][stat];
        return score;
    }

    // The tab of the class where the player has learned the most talents, -1 when it has learned none
    int GetLearnedTalentTabPage(Player const* me)
    {
        uint32 const classMask = me->GetClassMask();
        int learned[3] = { 0, 0, 0 };

        for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
            if (!talentInfo)
                continue;

            TalentTabEntry const* tabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!tabInfo || !(tabInfo->ClassMask & classMask) || tabInfo->tabpage > 2)
                continue;

            for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
            {
                uint32 spellId = talentInfo->RankID[rank];
                if (spellId && me->HasSpell(spellId))
                {
                    learned[tabInfo->tabpage] += rank + 1;
                    break;
                }
            }
        }

        int bestTab = -1;
        int bestCount = 0;
        for (int tab = 0; tab < 3; ++tab)
        {
            if (learned[tab] > bestCount)
            {
                bestCount = learned[tab];
                bestTab = tab;
            }
        }

        return bestTab;
    }

    // True when the player can use the two handed axes and maces that the enhancement talents teach
    bool HasTwoHandedWeaponProficiency(Player const* me)
    {
        if (!me)
            return false;

        return me->GetSkillValue(SKILL_2H_AXES) != 0 || me->GetSkillValue(SKILL_2H_MACES) != 0;
    }
}

char const* GetBotGearSpecName(BotGearSpec spec)
{
    if (spec < GEAR_SPEC_NONE || spec >= MAX_GEAR_SPECS)
        return "unknown";
    return gearSpecNames[spec];
}

char const* GetBotGearStatName(BotGearStat stat)
{
    if (stat < GEAR_STAT_STRENGTH || stat >= MAX_GEAR_STATS)
        return "unknown";
    return gearStatNames[stat];
}

float GetBotGearStatWeight(BotGearSpec spec, BotGearStat stat)
{
    if (spec < GEAR_SPEC_NONE || spec >= MAX_GEAR_SPECS || stat < GEAR_STAT_STRENGTH || stat >= MAX_GEAR_STATS)
        return 0.0f;
    return gearStatWeights[spec][stat];
}

// Reads again the weights that change the defaults. The rows of the table are the only way to change a value
// without compiling again, so a missing row means that the default value of that talent and attribute is used.
void LoadBotGearStatWeights()
{
    std::memcpy(gearStatWeights, defaultGearStatWeights, sizeof(gearStatWeights));

    uint32 count = 0;

    if (std::unique_ptr<QueryResult> result = WorldDatabase.Query(
            "SELECT `spec`, `stat`, `weight` FROM `playerbot_gear_stat_weights`"))
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 spec = fields[0].GetUInt32();
            uint32 stat = fields[1].GetUInt32();
            float weight = fields[2].GetFloat();

            if (spec >= MAX_GEAR_SPECS || stat >= MAX_GEAR_STATS)
            {
                sLog.Out(LOG_DBERROR, LOG_LVL_MINIMAL,
                    "Wrong talent %u or attribute %u in table `playerbot_gear_stat_weights`", spec, stat);
                continue;
            }

            gearStatWeights[spec][stat] = weight;
            ++count;
        } while (result->NextRow());
    }

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %u playerbot gear attribute weights", count);
}

// The talent of a bot: the talents it has learned say it, and its role says it when it has learned none
BotGearSpec GetBotGearSpec(Player const* me, CombatBotRoles role)
{
    if (!me)
        return GEAR_SPEC_NONE;

    int const tab = GetLearnedTalentTabPage(me);

    switch (me->GetClass())
    {
        case CLASS_WARRIOR:
            if (tab == 2)
                return GEAR_SPEC_WARRIOR_PROTECTION;
            if (tab == 1)
                return GEAR_SPEC_WARRIOR_FURY;
            if (tab == 0)
                return GEAR_SPEC_WARRIOR_ARMS;
            return role == ROLE_TANK ? GEAR_SPEC_WARRIOR_PROTECTION : GEAR_SPEC_WARRIOR_ARMS;

        case CLASS_PALADIN:
            if (tab == 0)
                return GEAR_SPEC_PALADIN_HOLY;
            if (tab == 1)
                return GEAR_SPEC_PALADIN_PROTECTION;
            if (tab == 2)
                return GEAR_SPEC_PALADIN_RETRIBUTION;
            switch (role)
            {
                case ROLE_TANK:       return GEAR_SPEC_PALADIN_PROTECTION;
                case ROLE_MELEE_DPS:  return GEAR_SPEC_PALADIN_RETRIBUTION;
                default:              return GEAR_SPEC_PALADIN_HOLY;
            }

        case CLASS_HUNTER:
            return GEAR_SPEC_HUNTER;

        case CLASS_ROGUE:
            return GEAR_SPEC_ROGUE;

        case CLASS_PRIEST:
            if (tab == 0)
                return GEAR_SPEC_PRIEST_DISCIPLINE;
            if (tab == 1)
                return GEAR_SPEC_PRIEST_HOLY;
            if (tab == 2)
                return GEAR_SPEC_PRIEST_SHADOW;
            return role == ROLE_RANGE_DPS ? GEAR_SPEC_PRIEST_SHADOW : GEAR_SPEC_PRIEST_DISCIPLINE;

        case CLASS_SHAMAN:
            if (tab == 0)
                return GEAR_SPEC_SHAMAN_ELEMENTAL;
            if (tab == 1)
                return GEAR_SPEC_SHAMAN_ENHANCEMENT;
            if (tab == 2)
                return GEAR_SPEC_SHAMAN_RESTORATION;
            switch (role)
            {
                case ROLE_RANGE_DPS:  return GEAR_SPEC_SHAMAN_ELEMENTAL;
                case ROLE_MELEE_DPS:  return GEAR_SPEC_SHAMAN_ENHANCEMENT;
                default:              return GEAR_SPEC_SHAMAN_RESTORATION;
            }

        case CLASS_MAGE:
            return GEAR_SPEC_MAGE;

        case CLASS_WARLOCK:
            return GEAR_SPEC_WARLOCK;

        case CLASS_DRUID:
            if (tab == 1)
                return role == ROLE_TANK ? GEAR_SPEC_DRUID_FERAL_BEAR : GEAR_SPEC_DRUID_FERAL_CAT;
            if (tab == 2)
                return GEAR_SPEC_DRUID_RESTORATION;
            switch (role)
            {
                case ROLE_TANK:       return GEAR_SPEC_DRUID_FERAL_BEAR;
                case ROLE_MELEE_DPS:  return GEAR_SPEC_DRUID_FERAL_CAT;
                case ROLE_RANGE_DPS:  return GEAR_SPEC_DRUID_BALANCE;
                default:              return GEAR_SPEC_DRUID_RESTORATION;
            }
    }

    return GEAR_SPEC_NONE;
}

// How the weapon slots of each talent are dressed, as a player of that talent dresses them
BotGearWeaponRules GetBotWeaponRules(Player const* me, BotGearSpec spec)
{
    BotGearWeaponRules rules;

    switch (spec)
    {
        case GEAR_SPEC_WARRIOR_ARMS:
            rules.policy = GEAR_WEAPON_POLICY_TWO_HANDED;
            rules.byWeaponDamage = true;
            break;

        case GEAR_SPEC_WARRIOR_FURY:
            // A warrior that has not learned to dual wield yet dresses as a protection warrior
            rules.policy = me && me->CanDualWield() ? GEAR_WEAPON_POLICY_DUAL_WIELD
                : GEAR_WEAPON_POLICY_ONE_HANDED_SHIELD;
            rules.byWeaponDamage = true;
            break;

        case GEAR_SPEC_WARRIOR_PROTECTION:
        case GEAR_SPEC_PALADIN_PROTECTION:
            rules.policy = GEAR_WEAPON_POLICY_ONE_HANDED_SHIELD;
            rules.byWeaponDamage = true;
            break;

        case GEAR_SPEC_PALADIN_RETRIBUTION:
            rules.policy = GEAR_WEAPON_POLICY_TWO_HANDED;
            rules.byWeaponDamage = true;
            break;

        case GEAR_SPEC_ROGUE:
            // Without the dual wield of its trainer a rogue only dresses a one handed weapon
            rules.policy = me && me->CanDualWield() ? GEAR_WEAPON_POLICY_DUAL_WIELD : GEAR_WEAPON_POLICY_ONE_HANDED;
            rules.byWeaponDamage = true;
            break;

        case GEAR_SPEC_HUNTER:
            // The melee weapons of a hunter are only used when it has to fight close, so they are chosen for the
            // attributes that they give, not for the little damage that they do. A hunter learns to dual wield at
            // the same time as a warrior, and then two one handed weapons can give more than a two handed one.
            rules.policy = me && me->CanDualWield() ? GEAR_WEAPON_POLICY_TWO_HANDED_OR_TWO_ONE_HANDED
                : GEAR_WEAPON_POLICY_TWO_HANDED;
            rules.rangedWeapon = true;
            break;

        case GEAR_SPEC_PALADIN_HOLY:
            rules.policy = GEAR_WEAPON_POLICY_ONE_HANDED_OFFHAND;
            break;

        case GEAR_SPEC_SHAMAN_ENHANCEMENT:
            // Enhancement learns the two handed axes and maces with its talents, and until then it dresses a one
            // handed weapon and a shield
            rules.policy = HasTwoHandedWeaponProficiency(me) ? GEAR_WEAPON_POLICY_TWO_HANDED
                : GEAR_WEAPON_POLICY_ONE_HANDED_SHIELD;
            rules.byWeaponDamage = true;
            break;

        case GEAR_SPEC_DRUID_FERAL_CAT:
        case GEAR_SPEC_DRUID_FERAL_BEAR:
            // The feral forms hit with the damage of the form, the weapon only adds its attributes
            rules.policy = GEAR_WEAPON_POLICY_TWO_HANDED;
            break;

        case GEAR_SPEC_SHAMAN_ELEMENTAL:
        case GEAR_SPEC_SHAMAN_RESTORATION:
        case GEAR_SPEC_PRIEST_DISCIPLINE:
        case GEAR_SPEC_PRIEST_HOLY:
        case GEAR_SPEC_PRIEST_SHADOW:
        case GEAR_SPEC_MAGE:
        case GEAR_SPEC_WARLOCK:
        case GEAR_SPEC_DRUID_BALANCE:
        case GEAR_SPEC_DRUID_RESTORATION:
            rules.policy = GEAR_WEAPON_POLICY_TWO_HANDED_OR_ONE_HANDED_OFFHAND;
            break;

        default:
            rules.policy = GEAR_WEAPON_POLICY_NONE;
            break;
    }

    return rules;
}

void EvaluateBotGearItem(Player* me, ItemPrototype const* proto, BotGearSpec spec, BotGearItemValue& value)
{
    value = BotGearItemValue();

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        if (proto->ItemStat[i].ItemStatType != 0)
            AddItemMod(value, proto->ItemStat[i].ItemStatType, float(proto->ItemStat[i].ItemStatValue));

    // The spells that the item casts while it is equipped. The ones of a random property that cast the same spell
    // are not counted twice.
    std::unordered_set<uint32> castSpells;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP || !proto->Spells[i].SpellId)
            continue;

        if (!castSpells.insert(proto->Spells[i].SpellId).second)
            continue;

        if (SpellEntry const* spell = sSpellMgr.GetSpellEntry(proto->Spells[i].SpellId))
            AddSpellValue(me, spell, value);
    }

    BotGearItemValue best = value;
    int32 bestProperty = 0;

    // Of all the random properties that an item can use, the one that gives the most to the talent of the bot is
    // the one that the item is created with, because the real values of the properties are the ones of the table
    // of the enchantments and they grow with the level of the item.
    if (proto->RandomProperty)
    {
        std::vector<uint32> const mods = GetItemEnchantMods(proto->RandomProperty);
        for (uint32 enchantId : mods)
        {
            ItemRandomPropertiesEntry const* randomProp = sItemRandomPropertiesStore.LookupEntry(enchantId);
            if (!randomProp)
                continue;

            BotGearItemValue withProperty = value;
            std::unordered_set<uint32> propertySpells = castSpells;
            for (uint32 i = 0; i < 3; ++i)
                if (randomProp->enchant_id[i])
                    AddEnchantmentValue(me, randomProp->enchant_id[i], withProperty, propertySpells);

            if (ScoreValue(spec, withProperty) > ScoreValue(spec, best))
            {
                best = withProperty;
                bestProperty = int32(randomProp->ID);
            }
        }
    }

    value = best;
    value.randomPropertyId = bestProperty;
    value.score = ScoreValue(spec, value);
}

// The damage per second that the tooltip of a weapon shows, without the damage that its random property adds
float GetItemWeaponDamagePerSecond(ItemPrototype const* proto)
{
    if (!proto || proto->Class != ITEM_CLASS_WEAPON)
        return 0.0f;

    float damage = 0.0f;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        if (proto->Damage[i].DamageMin <= 0.0f || proto->Damage[i].DamageMax <= 0.0f)
            continue;

        damage += (proto->Damage[i].DamageMin + proto->Damage[i].DamageMax) * 0.5f;
    }

    if (damage <= 0.0f)
        return 0.0f;

    float const speed = proto->Delay ? float(proto->Delay) / 1000.0f : 1.0f;
    return damage / speed;
}

// All the attributes that one item gives to a bot of one talent and the score of each one, to rate the weights
void SendBotGearItemScore(Player* me, ChatHandler& handler, BotGearSpec spec, uint32 itemEntry)
{
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemEntry);
    if (!proto)
    {
        handler.SendSysMessage("Error: invalid item entry.");
        return;
    }

    BotGearItemValue value;
    EvaluateBotGearItem(me, proto, spec, value);

    std::ostringstream out;
    out << "Score of " << proto->Name1 << " (" << itemEntry << ") for a " << GetBotGearSpecName(spec)
        << " of level " << me->GetLevel() << ": " << value.score;

    for (uint32 stat = 0; stat < MAX_GEAR_STATS; ++stat)
    {
        if (value.stats[stat] == 0.0f)
            continue;

        float const weight = GetBotGearStatWeight(spec, BotGearStat(stat));
        out << "\n    " << value.stats[stat] << " " << GetBotGearStatName(BotGearStat(stat))
            << " x " << weight << " = " << value.stats[stat] * weight;
    }

    if (value.randomPropertyId)
        if (ItemRandomPropertiesEntry const* randomProp = sItemRandomPropertiesStore.LookupEntry(
                uint32(value.randomPropertyId)))
            out << "\n    random property: " << randomProp->internalName;

    float const damagePerSecond = GetItemWeaponDamagePerSecond(proto);
    if (damagePerSecond > 0.0f)
        out << "\n    " << damagePerSecond << " damage per second of the weapon";

    handler.SendSysMessage(out.str().c_str());
}
