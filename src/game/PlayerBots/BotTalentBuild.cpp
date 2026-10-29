#include "BotTalentBuild.h"

#include "BotGearScoring.h"
#include "Database/DatabaseEnv.h"
#include "DBCEnums.h"
#include "DBCStructure.h"
#include "DBCStores.h"
#include "Log.h"
#include "Player.h"

#include <algorithm>
#include <map>

namespace
{
    // A build is looked up by the class and the talent page that it belongs to
    uint32 GetTalentBuildKey(uint32 botClass, uint32 tabpage)
    {
        return botClass * MAX_GEAR_SPEC_TABS + tabpage;
    }

    std::map<uint32, std::vector<BotTalentBuildStep>> talentBuilds;

    // The talent of the class `botClass` that has `rankSpell` as one of its ranks, with the number of ranks that
    // the talent has; false when no talent of that class has that spell. Any tree of the class is searched,
    // because the steps of a build are not all in the tree of the talent that the build is for.
    bool FindTalentByRankSpell(uint32 botClass, uint32 rankSpell, uint32& talentId, uint32& maxRanks)
    {
        // the same class mask as Unit::GetClassMask()
        uint32 const classMask = 1 << (botClass - 1);

        for (uint32 id = 0; id < sTalentStore.GetNumRows(); ++id)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(id);
            if (!talentInfo)
                continue;

            TalentTabEntry const* tabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!tabInfo || (tabInfo->ClassMask & classMask) == 0)
                continue;

            uint32 ranks = 0;
            bool hasSpell = false;
            for (uint32 rank = 0; rank < MAX_TALENT_RANK && talentInfo->RankID[rank]; ++rank)
            {
                ++ranks;
                if (talentInfo->RankID[rank] == rankSpell)
                    hasSpell = true;
            }

            if (!hasSpell)
                continue;

            talentId = id;
            maxRanks = ranks;
            return true;
        }

        return false;
    }
}

void LoadBotTalentBuilds()
{
    talentBuilds.clear();

    uint32 loaded = 0;
    uint32 skipped = 0;

    if (std::unique_ptr<QueryResult> result = WorldDatabase.Query(
            "SELECT `class`, `talent_page`, `rank_spell`, `ranks`, `name`"
            " FROM `playerbot_talent_build` ORDER BY `class`, `talent_page`, `seq`"))
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 botClass = fields[0].GetUInt32();
            uint32 tabpage = fields[1].GetUInt32();
            uint32 rankSpell = fields[2].GetUInt32();
            uint32 ranks = fields[3].GetUInt32();
            char const* name = fields[4].GetString();

            if (botClass < CLASS_WARRIOR || botClass > CLASS_DRUID || tabpage >= MAX_GEAR_SPEC_TABS || !ranks)
            {
                sLog.Out(LOG_DBERROR, LOG_LVL_MINIMAL, "Wrong class %u, talent page %u or rank count %u in table"
                    " `playerbot_talent_build`, step `%s`", botClass, tabpage, ranks, name);
                ++skipped;
                continue;
            }

            // The page of the talent that the build is for. A build is asked for by the class and that page, so
            // a page that the class does not have is a row that no bot could be given.
            uint32 talentTab = 0;
            if (!GetClassTalentTab(uint8(botClass), uint8(tabpage), talentTab))
            {
                sLog.Out(LOG_DBERROR, LOG_LVL_MINIMAL, "Class %u has no talent page %u in table"
                    " `playerbot_talent_build`, step `%s`", botClass, tabpage, name);
                ++skipped;
                continue;
            }

            uint32 talentId = 0;
            uint32 maxRanks = 0;
            if (!FindTalentByRankSpell(botClass, rankSpell, talentId, maxRanks))
            {
                sLog.Out(LOG_DBERROR, LOG_LVL_MINIMAL, "Spell %u of table `playerbot_talent_build` is not a"
                    " talent of class %u, step `%s` of talent page %u", rankSpell, botClass, name, tabpage);
                ++skipped;
                continue;
            }

            // A talent does not have more ranks than the DBC gives it, and a row that asks for more is a row
            // that was written with the wrong talent in mind.
            if (ranks > maxRanks)
                sLog.Out(LOG_DBERROR, LOG_LVL_MINIMAL, "Step `%s` of table `playerbot_talent_build` asks %u"
                    " ranks of a talent that has %u", name, ranks, maxRanks);

            BotTalentBuildStep step;
            step.talentId = talentId;
            step.ranks = uint8(std::min(ranks, maxRanks));
            step.name = name;

            talentBuilds[GetTalentBuildKey(botClass, tabpage)].push_back(step);
            ++loaded;
        } while (result->NextRow());
    }

    if (skipped)
        sLog.Out(LOG_DBERROR, LOG_LVL_MINIMAL, ">> Ignored %u rows of table `playerbot_talent_build`", skipped);

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %u playerbot talent build steps", loaded);
}

std::vector<BotTalentBuildStep> const* GetBotTalentBuild(uint8 botClass, uint8 tabpage)
{
    if (tabpage >= MAX_GEAR_SPEC_TABS)
        return nullptr;

    std::map<uint32, std::vector<BotTalentBuildStep>>::const_iterator iter =
        talentBuilds.find(GetTalentBuildKey(botClass, tabpage));
    if (iter == talentBuilds.end())
        return nullptr;

    return &iter->second;
}

uint32 GetLearnedTalentRanks(Player const* me, uint32 talentId)
{
    if (!me)
        return 0;

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
    if (!talentInfo)
        return 0;

    // The same walk that Player::LearnTalent does to find the rank that a point goes to
    uint32 learned = 0;
    for (uint32 rank = 0; rank < MAX_TALENT_RANK && talentInfo->RankID[rank]; ++rank)
    {
        if (me->HasSpell(talentInfo->RankID[rank]))
            ++learned;
    }

    return learned;
}
