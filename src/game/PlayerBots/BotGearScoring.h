#ifndef _BOTGEARSCORING_H
#define _BOTGEARSCORING_H

#include "Common.h"
#include "SharedDefines.h"

class ChatHandler;
class ItemPrototype;
class Player;

// The talent that a bot is built around. The talents of a bot are learned in one talent tab, so the tab that has
// the learned spells says the talent, and the role of the bot says it when no talent is learned.
enum BotGearSpec
{
    GEAR_SPEC_NONE = 0,
    GEAR_SPEC_WARRIOR_ARMS,
    GEAR_SPEC_WARRIOR_FURY,
    GEAR_SPEC_WARRIOR_PROTECTION,
    GEAR_SPEC_PALADIN_HOLY,
    GEAR_SPEC_PALADIN_PROTECTION,
    GEAR_SPEC_PALADIN_RETRIBUTION,
    GEAR_SPEC_HUNTER,
    GEAR_SPEC_ROGUE,
    GEAR_SPEC_PRIEST_DISCIPLINE,
    GEAR_SPEC_PRIEST_HOLY,
    GEAR_SPEC_PRIEST_SHADOW,
    GEAR_SPEC_SHAMAN_ELEMENTAL,
    GEAR_SPEC_SHAMAN_ENHANCEMENT,
    GEAR_SPEC_SHAMAN_RESTORATION,
    GEAR_SPEC_MAGE,
    GEAR_SPEC_WARLOCK,
    GEAR_SPEC_DRUID_BALANCE,
    GEAR_SPEC_DRUID_FERAL_CAT,
    GEAR_SPEC_DRUID_FERAL_BEAR,
    GEAR_SPEC_DRUID_RESTORATION,
    MAX_GEAR_SPECS
};

// What an item can give to a bot, one column per the attributes that the gear of a bot is rated by.
// The percentage attributes are counted in percent, the others in the unit that the client shows.
enum BotGearStat
{
    GEAR_STAT_STRENGTH = 0,
    GEAR_STAT_AGILITY,
    GEAR_STAT_INTELLECT,
    GEAR_STAT_SPIRIT,
    GEAR_STAT_STAMINA,
    GEAR_STAT_MANA,                               // fixed mana of an item, rated as a part of the intellect
    GEAR_STAT_HEALTH,                             // fixed health of an item, rated as a part of the stamina
    GEAR_STAT_MELEE_ATTACK_POWER,
    GEAR_STAT_RANGED_ATTACK_POWER,
    GEAR_STAT_SPELL_DAMAGE,
    GEAR_STAT_HEALING,
    GEAR_STAT_PHYSICAL_CRIT,
    GEAR_STAT_PHYSICAL_HIT,
    GEAR_STAT_SPELL_CRIT,
    GEAR_STAT_SPELL_HIT,
    GEAR_STAT_MANA_REGENERATION,                  // mana regained every 5 seconds
    GEAR_STAT_RESISTANCE,                         // any resistance, the schools are not told apart
    GEAR_STAT_DODGE,
    GEAR_STAT_PARRY,
    GEAR_STAT_BLOCK,
    MAX_GEAR_STATS
};

// How a weapon slot must be filled for one talent: by the damage of the weapon or by its attributes, and which
// kinds of weapons are allowed in it
enum BotGearWeaponPolicy
{
    GEAR_WEAPON_POLICY_TWO_HANDED = 0,                // only a two handed weapon, in the main hand
    GEAR_WEAPON_POLICY_DUAL_WIELD,                    // one handed weapons in both hands
    GEAR_WEAPON_POLICY_ONE_HANDED_SHIELD,             // one handed weapon, shield in the offhand
    GEAR_WEAPON_POLICY_ONE_HANDED_OFFHAND,            // one handed weapon, any item usable in the offhand
    GEAR_WEAPON_POLICY_ONE_HANDED,                    // one handed weapon, nothing in the other hand
    GEAR_WEAPON_POLICY_TWO_HANDED_OR_ONE_HANDED_OFFHAND,  // the best of {two handed} and {one handed + offhand}
    GEAR_WEAPON_POLICY_TWO_HANDED_OR_TWO_ONE_HANDED,  // the best of {two handed} and {two one handed weapons}
    GEAR_WEAPON_POLICY_NONE                           // no weapons at all
};

struct BotGearWeaponRules
{
    BotGearWeaponPolicy policy = GEAR_WEAPON_POLICY_NONE;
    bool byWeaponDamage = false;                      // rank the weapons by their damage, not by their score
    bool rangedWeapon = false;                        // the ranged slot holds a weapon, not a wand
};

// The number of talent pages a class has (TalentTabEntry.tabpage), also used as "no talent page was asked for"
enum { MAX_GEAR_SPEC_TABS = 3 };

// A talent that a bot can be summoned with (`.partybot add <abbreviation>`). The rating reads the talent back from
// the tree that the bot learns and from its role (see GetBotGearSpec), so an entry only says which class to create,
// which role to give it, and which talent page of that class to fill; MAX_GEAR_SPEC_TABS as the page leaves the
// choice of the tree to the class, which is right for the classes rated by one talent (hunter, mage, warlock).
struct BotGearSpecAlias
{
    char const* alias;
    uint8 botClass;
    CombatBotRoles role;
    uint8 talentTabPage;
    Team team;                                        // TEAM_CROSSFACTION when both factions may summon it
};

// All that one item gives to a bot of one talent: the attributes of the item, the attributes of the spells that
// the item casts while it is equipped, and the best of the random properties that the item can use.
struct BotGearItemValue
{
    float stats[MAX_GEAR_STATS] = {};
    int32 randomPropertyId = 0;                       // the chosen random property, 0 when the item has none
    float score = 0.0f;
};

// The talent of a bot, from its learned talents and from its role when it has no learned talent
BotGearSpec GetBotGearSpec(Player const* me, CombatBotRoles role);
// How the weapon slots of a bot of that talent must be filled
BotGearWeaponRules GetBotWeaponRules(Player const* me, BotGearSpec spec);

// The talent asked by an abbreviation of `.partybot add`, nullptr when the argument is not one or when it belongs
// to the other faction than `team`
BotGearSpecAlias const* GetBotGearSpecAlias(char const* alias, Team team);
// The id of the talent tab of one page of a class (TalentTabEntry.tabpage), false when the class has no such page
bool GetClassTalentTab(uint8 botClass, uint8 tabpage, uint32& talentTab);
// The talent page that a bot of that class and role is built with, taken from the abbreviations of
// `.partybot add`: a bot summoned by a role has no asked talent, but the class and the role together say which
// tree the players of this fork build it with. False when the class and the role have no abbreviation, which is
// the case of the classes rated by one talent and of the roles that have no tree of their own.
bool GetSpecTalentPageForRole(uint8 botClass, CombatBotRoles role, uint8& tabpage);

// The value of one attribute for one talent: the compiled defaults, overridden by `playerbot_gear_stat_weights`
float GetBotGearStatWeight(BotGearSpec spec, BotGearStat stat);
// Reads the table again, over the compiled defaults
void LoadBotGearStatWeights();

// Fills all the attributes of one item for one talent and scores them, and tells which random property to use
void EvaluateBotGearItem(Player* me, ItemPrototype const* proto, BotGearSpec spec, BotGearItemValue& value);
// The damage per second shown by the tooltip of a weapon, without the damage of its random property
float GetItemWeaponDamagePerSecond(ItemPrototype const* proto);

// Names and scores of the attributes of one item, to rate the values of the weights
void SendBotGearItemScore(Player* me, ChatHandler& handler, BotGearSpec spec, uint32 itemEntry);

char const* GetBotGearSpecName(BotGearSpec spec);
char const* GetBotGearStatName(BotGearStat stat);

#endif
