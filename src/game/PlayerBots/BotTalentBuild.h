#ifndef _BOTTALENTBUILD_H
#define _BOTTALENTBUILD_H

#include "Common.h"
#include "SharedDefines.h"

#include <string>
#include <vector>

class Player;

// One step of the build that a talent of a bot is learned with: how many ranks of one talent to take at that
// point. The steps are read in `seq` order and the free talent points of a bot are spent in that order, so a bot
// that has not reached the maximum level has the first steps of the same build, and the primary tree of the
// talent is filled before another tree is touched. The steps of one build are all keyed by the class and the
// talent page of the talent that the build belongs to, and a step may be a talent of another tree of the class.
struct BotTalentBuildStep
{
    uint32 talentId = 0;                            // the row of Talent.dbc that `rank_spell` belongs to
    uint8 ranks = 0;                                // ranks to take, from the lowest rank not learned yet
    std::string name;                               // the name the table gives the step, for the log
};

// Reads `playerbot_talent_build` again. Every `rank_spell` is resolved to the talent of Talent.dbc that has it,
// and a row whose spell is not a talent of the class that it is declared for is skipped with a word in the log,
// because a wrong spell id would otherwise spend the points of a bot on something else.
void LoadBotTalentBuilds();

// The build of one talent page of a class, nullptr when the table has no rows for that talent
std::vector<BotTalentBuildStep> const* GetBotTalentBuild(uint8 botClass, uint8 tabpage);

// How many ranks of the talent of Talent.dbc `me` already knows, which is the rank that its next talent point
// goes to. A bot that was summoned before keeps the talents that it had, and the steps of a build are written
// from the first rank, so the ranks that are known have to be counted out before the build is walked again.
uint32 GetLearnedTalentRanks(Player const* me, uint32 talentId);

#endif
