/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Research.h"

#include "../Context.h"
#include "../Date.h"
#include "../Diagnostic.h"
#include "../Game.h"
#include "../GameState.h"
#include "../OpenRCT2.h"
#include "../actions/ParkSetResearchFundingAction.h"
#include "../config/Config.h"
#include "../core/BitSet.hpp"
#include "../core/EnumUtils.hpp"
#include "../core/Guard.hpp"
#include "../core/Memory.hpp"
#include "../localisation/Formatter.h"
#include "../localisation/StringIds.h"
#include "../object/ObjectEntryManager.h"
#include "../object/ObjectLimits.h"
#include "../object/ObjectList.h"
#include "../object/RideObject.h"
#include "../object/SceneryGroupEntry.h"
#include "../profiling/Profiling.h"
#include "../ride/Ride.h"
#include "../ride/RideData.h"
#include "../ride/RideEntry.h"
#include "../ride/TrackData.h"
#include "../ui/WindowManager.h"
#include "../windows/Intent.h"
#include "../world/Park.h"
#include "../world/Scenery.h"
#include "Finance.h"
#include "NewsItem.h"

#include <iterator>

using namespace OpenRCT2;

static constexpr int32_t _researchRate[] = {
    0,
    160,
    250,
    400,
};

static bool _researchedRideTypes[RIDE_TYPE_COUNT];
static bool _researchedRideEntries[kMaxRideObjects];
static bool _researchedSceneryItems[SCENERY_TYPE_COUNT][UINT16_MAX];

bool gSilentResearch = false;

// clang-format off
const StringId kResearchFundingLevelNames[] = {
    STR_RESEARCH_FUNDING_NONE,
    STR_RESEARCH_FUNDING_MINIMUM,
    STR_RESEARCH_FUNDING_NORMAL,
    STR_RESEARCH_FUNDING_MAXIMUM,
};
// clang-format on

/**
 *
 *  rct2: 0x006671AD, part of 0x00667132
 */
void ResearchResetItems(GameState_t& gameState)
{
    gameState.research.itemsUninvented.clear();
    gameState.research.itemsInvented.clear();
}

/**
 *
 *  rct2: 0x00684BAE
 */
void ResearchUpdateUncompletedTypes()
{
    auto& gameState = getGameState();
    int32_t uncompletedResearchTypes = 0;

    for (auto const& researchItem : gameState.research.itemsUninvented)
    {
        uncompletedResearchTypes |= EnumToFlag(researchItem.category);
    }

    gameState.research.uncompletedCategories = uncompletedResearchTypes;
}

/**
 *
 *  rct2: 0x00684D2A
 */
static void ResearchCalculateExpectedDate()
{
    auto& gameState = getGameState();
    if (gameState.research.progressStage == RESEARCH_STAGE_INITIAL_RESEARCH
        || gameState.research.fundingLevel == RESEARCH_FUNDING_NONE)
    {
        gameState.research.expectedDay = 255;
    }
    else
    {
        auto& date = GetDate();

        int32_t progressRemaining = gameState.research.progressStage == RESEARCH_STAGE_COMPLETING_DESIGN ? 0x10000 : 0x20000;
        progressRemaining -= gameState.research.progress;
        int32_t daysRemaining = (progressRemaining / _researchRate[gameState.research.fundingLevel]) * 128;

        int32_t expectedDay = date.GetMonthTicks() + (daysRemaining & 0xFFFF);
        int32_t dayQuotient = expectedDay / 0x10000;
        int32_t dayRemainder = expectedDay % 0x10000;

        int32_t expectedMonth = DateGetMonth(date.GetMonthsElapsed() + dayQuotient + (daysRemaining >> 16));
        expectedDay = (dayRemainder * Date::GetDaysInMonth(expectedMonth)) >> 16;

        gameState.research.expectedDay = expectedDay;
        gameState.research.expectedMonth = expectedMonth;
    }
}

static void ResearchInvalidateRelatedWindows()
{
    auto* windowMgr = Ui::GetWindowManager();
    windowMgr->InvalidateByClass(WindowClass::ConstructRide);
    windowMgr->InvalidateByClass(WindowClass::Research);
}

static void ResearchMarkAsFullyCompleted()
{
    auto& gameState = getGameState();
    gameState.research.progress = 0;
    gameState.research.progressStage = RESEARCH_STAGE_FINISHED_ALL;
    ResearchInvalidateRelatedWindows();
    // Reset funding to 0 if no more rides.
    auto gameAction = ParkSetResearchFundingAction(gameState.research.priorities, 0);
    GameActions::Execute(&gameAction);
}

/**
 *
 *  rct2: 0x00684BE5
 */
static void ResearchNextDesign()
{
    auto& gameState = getGameState();
    if (gameState.research.itemsUninvented.empty())
    {
        ResearchMarkAsFullyCompleted();
        return;
    }

    // Try to find a research item of a matching type, if none found, use any first item
    auto it = std::find_if(
        gameState.research.itemsUninvented.begin(), gameState.research.itemsUninvented.end(),
        [&gameState](const auto& e) { return (gameState.research.priorities & EnumToFlag(e.category)) != 0; });
    if (it == gameState.research.itemsUninvented.end())
    {
        it = gameState.research.itemsUninvented.begin();
    }

    gameState.research.nextItem = *it;
    gameState.research.progress = 0;
    gameState.research.progressStage = RESEARCH_STAGE_DESIGNING;

    ResearchInvalidateRelatedWindows();
}

static void MarkResearchItemInvented(const ResearchItem& researchItem)
{
    auto& gameState = getGameState();
    gameState.research.itemsUninvented.erase(
        std::remove(gameState.research.itemsUninvented.begin(), gameState.research.itemsUninvented.end(), researchItem),
        gameState.research.itemsUninvented.end());

    if (std::find(gameState.research.itemsInvented.begin(), gameState.research.itemsInvented.end(), researchItem)
        == gameState.research.itemsInvented.end())
    {
        gameState.research.itemsInvented.push_back(researchItem);
    }
}

/**
 *
 *  rct2: 0x006848D4
 */
void ResearchFinishItem(const ResearchItem& researchItem)
{
    auto& gameState = getGameState();
    gameState.research.lastItem = researchItem;
    ResearchInvalidateRelatedWindows();

    if (researchItem.type == Research::EntryType::Ride)
    {
        // Ride
        auto base_ride_type = researchItem.baseRideType;
        ObjectEntryIndex rideEntryIndex = researchItem.entryIndex;
        const auto* rideEntry = GetRideEntryByIndex(rideEntryIndex);

        if (rideEntry != nullptr && base_ride_type != kRideTypeNull)
        {
            if (!RideTypeIsValid(base_ride_type))
            {
                LOG_WARNING("Invalid ride type: %d", base_ride_type);
                base_ride_type = rideEntry->GetFirstNonNullRideType();
            }

            StringId availabilityString;
            RideTypeSetInvented(base_ride_type);
            RideEntrySetInvented(rideEntryIndex);

            bool seenRideEntry[kMaxRideObjects]{};
            for (auto const& researchItem3 : gameState.research.itemsUninvented)
            {
                ObjectEntryIndex index = researchItem3.entryIndex;
                seenRideEntry[index] = true;
            }

            // RCT2 made non-separated vehicles available at once, by removing all but one from research.
            // To ensure old files keep working, look for ride entries not in research, and make them available as well.
            for (int32_t i = 0; i < kMaxRideObjects; i++)
            {
                if (!seenRideEntry[i])
                {
                    const auto* rideEntry2 = GetRideEntryByIndex(i);
                    if (rideEntry2 != nullptr)
                    {
                        for (uint8_t j = 0; j < RCT2::ObjectLimits::kMaxRideTypesPerRideEntry; j++)
                        {
                            if (rideEntry2->ride_type[j] == base_ride_type)
                            {
                                RideEntrySetInvented(i);
                                ResearchInsertRideEntry(i, true);
                                break;
                            }
                        }
                    }
                }
            }

            Formatter ft;

            // If a vehicle is the first to be invented for its ride type, show the ride type/group name.
            // Independently listed vehicles (like all flat rides and shops) should always be announced as such.
            if (GetRideTypeDescriptor(base_ride_type).HasFlag(RtdFlag::listVehiclesSeparately)
                || researchItem.flags & RESEARCH_ENTRY_FLAG_FIRST_OF_TYPE)
            {
                RideNaming naming = GetRideNaming(base_ride_type, *rideEntry);
                availabilityString = STR_NEWS_ITEM_RESEARCH_NEW_RIDE_AVAILABLE;
                ft.Add<StringId>(naming.Name);
            }
            // If the vehicle should not be listed separately and it isn't the first to be invented for its ride group,
            // report it as a new vehicle for the existing ride group.
            else
            {
                availabilityString = STR_NEWS_ITEM_RESEARCH_NEW_VEHICLE_AVAILABLE;
                RideNaming baseRideNaming = GetRideNaming(base_ride_type, *rideEntry);

                ft.Add<StringId>(baseRideNaming.Name);
                ft.Add<StringId>(rideEntry->naming.Name);
            }

            if (!gSilentResearch)
            {
                if (Config::Get().notifications.RideResearched)
                {
                    News::AddItemToQueue(News::ItemType::Research, availabilityString, researchItem.rawValue, ft);
                }
            }

            ResearchInvalidateRelatedWindows();
        }
    }
    else
    {
        // Scenery
        const auto* sceneryGroupEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(researchItem.entryIndex);
        if (sceneryGroupEntry != nullptr)
        {
            SceneryGroupSetInvented(researchItem.entryIndex);

            Formatter ft;
            ft.Add<StringId>(sceneryGroupEntry->name);

            if (!gSilentResearch)
            {
                if (Config::Get().notifications.RideResearched)
                {
                    News::AddItemToQueue(
                        News::ItemType::Research, STR_NEWS_ITEM_RESEARCH_NEW_SCENERY_SET_AVAILABLE, researchItem.rawValue, ft);
                }
            }

            ResearchInvalidateRelatedWindows();

            auto intent = Intent(INTENT_ACTION_INIT_SCENERY);
            ContextBroadcastIntent(&intent);
        }
    }
}

/**
 *
 *  rct2: 0x00684C7A
 */
void ResearchUpdate()
{
    PROFILED_FUNCTION();

    int32_t researchLevel, currentResearchProgress;

    if (isInEditorMode())
    {
        return;
    }

    auto& gameState = getGameState();
    if (gameState.currentTicks % 32 != 0)
    {
        return;
    }

    if ((gameState.park.Flags & PARK_FLAGS_NO_MONEY) && gameState.research.fundingLevel == RESEARCH_FUNDING_NONE)
    {
        researchLevel = RESEARCH_FUNDING_NORMAL;
    }
    else
    {
        researchLevel = gameState.research.fundingLevel;
    }

    currentResearchProgress = gameState.research.progress;
    currentResearchProgress += _researchRate[researchLevel];
    if (currentResearchProgress <= 0xFFFF)
    {
        gameState.research.progress = currentResearchProgress;
    }
    else
    {
        switch (gameState.research.progressStage)
        {
            case RESEARCH_STAGE_INITIAL_RESEARCH:
                ResearchNextDesign();
                ResearchCalculateExpectedDate();
                break;
            case RESEARCH_STAGE_DESIGNING:
                gameState.research.progress = 0;
                gameState.research.progressStage = RESEARCH_STAGE_COMPLETING_DESIGN;
                ResearchCalculateExpectedDate();
                ResearchInvalidateRelatedWindows();
                break;
            case RESEARCH_STAGE_COMPLETING_DESIGN:
                MarkResearchItemInvented(*gameState.research.nextItem);
                ResearchFinishItem(*gameState.research.nextItem);
                gameState.research.progress = 0;
                gameState.research.progressStage = RESEARCH_STAGE_INITIAL_RESEARCH;
                ResearchCalculateExpectedDate();
                ResearchUpdateUncompletedTypes();
                ResearchInvalidateRelatedWindows();
                break;
            case RESEARCH_STAGE_FINISHED_ALL:
                gameState.research.fundingLevel = RESEARCH_FUNDING_NONE;
                break;
        }
    }
}

/**
 *
 *  rct2: 0x00684AC3
 */
void ResearchResetCurrentItem()
{
    auto& gameState = getGameState();
    SetEveryRideTypeNotInvented();
    SetEveryRideEntryNotInvented();

    // The following two instructions together make all items not tied to a scenery group available.
    SetAllSceneryItemsInvented();
    SetAllSceneryGroupsNotInvented();

    for (const auto& researchItem : gameState.research.itemsInvented)
    {
        ResearchFinishItem(researchItem);
    }

    gameState.research.lastItem = std::nullopt;
    gameState.research.progressStage = RESEARCH_STAGE_INITIAL_RESEARCH;
    gameState.research.progress = 0;
}

/**
 *
 *  rct2: 0x006857FA
 */
static void ResearchInsertUnresearched(ResearchItem&& item)
{
    auto& gameState = getGameState();
    // First check to make sure that entry is not already accounted for
    if (item.Exists())
    {
        return;
    }

    gameState.research.itemsUninvented.push_back(std::move(item));
}

/**
 *
 *  rct2: 0x00685826
 */
static void ResearchInsertResearched(ResearchItem&& item)
{
    auto& gameState = getGameState();
    // First check to make sure that entry is not already accounted for
    if (item.Exists())
    {
        return;
    }

    gameState.research.itemsInvented.push_back(std::move(item));
}

/**
 *
 *  rct2: 0x006857CF
 */
void ResearchRemove(const ResearchItem& researchItem)
{
    auto& gameState = getGameState();
    gameState.research.itemsUninvented.erase(
        std::remove(gameState.research.itemsUninvented.begin(), gameState.research.itemsUninvented.end(), researchItem),
        gameState.research.itemsUninvented.end());
    gameState.research.itemsInvented.erase(
        std::remove(gameState.research.itemsInvented.begin(), gameState.research.itemsInvented.end(), researchItem),
        gameState.research.itemsInvented.end());
}

void ResearchInsert(ResearchItem&& item, bool researched)
{
    if (researched)
    {
        ResearchInsertResearched(std::move(item));
    }
    else
    {
        ResearchInsertUnresearched(std::move(item));
    }
}

/**
 *
 *  rct2: 0x00685675
 */
void ResearchPopulateListRandom()
{
    auto& gameState = getGameState();
    ResearchResetItems(gameState);

    // Rides
    for (int32_t i = 0; i < kMaxRideObjects; i++)
    {
        const auto* rideEntry = GetRideEntryByIndex(i);
        if (rideEntry == nullptr)
        {
            continue;
        }

        int32_t researched = (ScenarioRand() & 0xFF) > 128;
        for (auto rideType : rideEntry->ride_type)
        {
            if (rideType != kRideTypeNull)
            {
                ResearchCategory category = GetRideTypeDescriptor(rideType).GetResearchCategory();
                ResearchInsertRideEntry(rideType, i, category, researched);
            }
        }
    }

    // Scenery
    for (uint32_t i = 0; i < kMaxSceneryGroupObjects; i++)
    {
        const auto* sceneryGroupEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(i);
        if (sceneryGroupEntry == nullptr)
        {
            continue;
        }

        int32_t researched = (ScenarioRand() & 0xFF) > 85;
        ResearchInsertSceneryGroupEntry(i, researched);
    }
}

bool ResearchInsertRideEntry(ride_type_t rideType, ObjectEntryIndex entryIndex, ResearchCategory category, bool researched)
{
    if (rideType != kRideTypeNull && entryIndex != kObjectEntryIndexNull)
    {
        auto tmpItem = ResearchItem(Research::EntryType::Ride, entryIndex, rideType, category, 0);
        ResearchInsert(std::move(tmpItem), researched);
        return true;
    }

    return false;
}

void ResearchInsertRideEntry(ObjectEntryIndex entryIndex, bool researched)
{
    const auto* rideEntry = GetRideEntryByIndex(entryIndex);
    if (rideEntry == nullptr)
        return;

    for (auto rideType : rideEntry->ride_type)
    {
        if (rideType != kRideTypeNull)
        {
            ResearchCategory category = GetRideTypeDescriptor(rideType).GetResearchCategory();
            ResearchInsertRideEntry(rideType, entryIndex, category, researched);
        }
    }
}

bool ResearchInsertSceneryGroupEntry(ObjectEntryIndex entryIndex, bool researched)
{
    if (entryIndex != kObjectEntryIndexNull)
    {
        auto tmpItem = ResearchItem(Research::EntryType::Scenery, entryIndex, 0, ResearchCategory::SceneryGroup, 0);
        ResearchInsert(std::move(tmpItem), researched);
        return true;
    }
    return false;
}

bool ResearchIsInvented(ObjectType objectType, ObjectEntryIndex index)
{
    switch (objectType)
    {
        case ObjectType::ride:
            return RideEntryIsInvented(index);
        case ObjectType::sceneryGroup:
            return SceneryGroupIsInvented(index);
        case ObjectType::smallScenery:
            return SceneryIsInvented({ SCENERY_TYPE_SMALL, index });
        case ObjectType::largeScenery:
            return SceneryIsInvented({ SCENERY_TYPE_LARGE, index });
        case ObjectType::walls:
            return SceneryIsInvented({ SCENERY_TYPE_WALL, index });
        case ObjectType::banners:
            return SceneryIsInvented({ SCENERY_TYPE_BANNER, index });
        case ObjectType::pathAdditions:
            return SceneryIsInvented({ SCENERY_TYPE_PATH_ITEM, index });
        default:
            return true;
    }
}

bool RideTypeIsInvented(ride_type_t rideType)
{
    return RideTypeIsValid(rideType) ? _researchedRideTypes[rideType] : false;
}

bool RideEntryIsInvented(ObjectEntryIndex rideEntryIndex)
{
    if (rideEntryIndex >= std::size(_researchedRideEntries))
        return false;

    return _researchedRideEntries[rideEntryIndex];
}

void RideTypeSetInvented(ride_type_t rideType)
{
    if (RideTypeIsValid(rideType))
    {
        _researchedRideTypes[rideType] = true;
    }
}

void RideEntrySetInvented(ObjectEntryIndex rideEntryIndex)
{
    if (rideEntryIndex >= std::size(_researchedRideEntries))
        LOG_ERROR("Tried setting ride entry %u as invented", rideEntryIndex);
    else
        _researchedRideEntries[rideEntryIndex] = true;
}

bool SceneryIsInvented(const ScenerySelection& sceneryItem)
{
    if (sceneryItem.SceneryType < SCENERY_TYPE_COUNT)
    {
        return _researchedSceneryItems[sceneryItem.SceneryType][sceneryItem.EntryIndex];
    }

    LOG_WARNING("Invalid Scenery Type");
    return false;
}

void ScenerySetInvented(const ScenerySelection& sceneryItem)
{
    if (sceneryItem.SceneryType < SCENERY_TYPE_COUNT)
    {
        _researchedSceneryItems[sceneryItem.SceneryType][sceneryItem.EntryIndex] = true;
    }
    else
    {
        LOG_WARNING("Invalid Scenery Type");
    }
}

void ScenerySetNotInvented(const ScenerySelection& sceneryItem)
{
    if (sceneryItem.SceneryType < SCENERY_TYPE_COUNT)
    {
        _researchedSceneryItems[sceneryItem.SceneryType][sceneryItem.EntryIndex] = false;
    }
    else
    {
        LOG_WARNING("Invalid Scenery Type");
    }
}

bool SceneryGroupIsInvented(int32_t sgIndex)
{
    auto& gameState = getGameState();
    const auto sgEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(sgIndex);
    if (sgEntry == nullptr || sgEntry->SceneryEntries.empty())
    {
        return false;
    }

    // All scenery is temporarily invented when in the scenario editor
    if (isInEditorMode())
    {
        return true;
    }

    if (getGameState().cheats.ignoreResearchStatus)
    {
        return true;
    }

    return std::none_of(
        std::begin(gameState.research.itemsUninvented), std::end(gameState.research.itemsUninvented),
        [sgIndex](const ResearchItem& item) {
            return item.type == Research::EntryType::Scenery && item.entryIndex == sgIndex;
        });
}

void SceneryGroupSetInvented(int32_t sgIndex)
{
    const auto sgEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(sgIndex);
    if (sgEntry != nullptr)
    {
        for (const auto& entry : sgEntry->SceneryEntries)
        {
            ScenerySetInvented(entry);
        }
    }
}

void SetAllSceneryGroupsNotInvented()
{
    for (int32_t i = 0; i < kMaxSceneryGroupObjects; ++i)
    {
        const auto* scenery_set = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(i);
        if (scenery_set == nullptr)
        {
            continue;
        }

        for (const auto& sceneryEntry : scenery_set->SceneryEntries)
        {
            ScenerySetNotInvented(sceneryEntry);
        }
    }
}

void SetAllSceneryItemsInvented()
{
    for (auto sceneryType = 0; sceneryType < SCENERY_TYPE_COUNT; sceneryType++)
    {
        std::fill(std::begin(_researchedSceneryItems[sceneryType]), std::end(_researchedSceneryItems[sceneryType]), true);
    }
}

void SetAllSceneryItemsNotInvented()
{
    for (auto sceneryType = 0; sceneryType < SCENERY_TYPE_COUNT; sceneryType++)
    {
        std::fill(std::begin(_researchedSceneryItems[sceneryType]), std::end(_researchedSceneryItems[sceneryType]), false);
    }
}

void SetEveryRideTypeInvented()
{
    std::fill(std::begin(_researchedRideTypes), std::end(_researchedRideTypes), true);
}

void SetEveryRideTypeNotInvented()
{
    std::fill(std::begin(_researchedRideTypes), std::end(_researchedRideTypes), false);
}

void SetEveryRideEntryInvented()
{
    std::fill(std::begin(_researchedRideEntries), std::end(_researchedRideEntries), true);
}

void SetEveryRideEntryNotInvented()
{
    std::fill(std::begin(_researchedRideEntries), std::end(_researchedRideEntries), false);
}

/**
 *
 *  rct2: 0x0068563D
 */
StringId ResearchItem::GetName() const
{
    if (type == Research::EntryType::Ride)
    {
        const auto* rideEntry = GetRideEntryByIndex(entryIndex);
        if (rideEntry == nullptr)
        {
            return kStringIdEmpty;
        }

        return rideEntry->naming.Name;
    }

    const auto* sceneryEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(entryIndex);
    if (sceneryEntry == nullptr)
    {
        return kStringIdEmpty;
    }

    return sceneryEntry->name;
}

/**
 *
 *  rct2: 0x00685A79
 *  Do not use the research list outside of the inventions list window with the flags
 *  Clears flags like "always researched".
 */
void ResearchRemoveFlags()
{
    auto& gameState = getGameState();
    for (auto& researchItem : gameState.research.itemsUninvented)
    {
        researchItem.flags &= ~(RESEARCH_ENTRY_FLAG_RIDE_ALWAYS_RESEARCHED | RESEARCH_ENTRY_FLAG_SCENERY_SET_ALWAYS_RESEARCHED);
    }
    for (auto& researchItem : gameState.research.itemsInvented)
    {
        researchItem.flags &= ~(RESEARCH_ENTRY_FLAG_RIDE_ALWAYS_RESEARCHED | RESEARCH_ENTRY_FLAG_SCENERY_SET_ALWAYS_RESEARCHED);
    }
}

static void ResearchRemoveNullItems(std::vector<ResearchItem>& items)
{
    const auto it = std::remove_if(std::begin(items), std::end(items), [](const ResearchItem& researchItem) {
        if (researchItem.type == Research::EntryType::Ride)
        {
            return GetRideEntryByIndex(researchItem.entryIndex) == nullptr;
        }
        else
        {
            return OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(researchItem.entryIndex) == nullptr;
        }
    });
    items.erase(it, std::end(items));
}

static void ResearchMarkItemAsResearched(const ResearchItem& item)
{
    if (item.type == Research::EntryType::Ride)
    {
        const auto* rideEntry = GetRideEntryByIndex(item.entryIndex);
        if (rideEntry != nullptr)
        {
            RideEntrySetInvented(item.entryIndex);
            for (auto rideType : rideEntry->ride_type)
            {
                if (rideType != kRideTypeNull)
                {
                    RideTypeSetInvented(rideType);
                }
            }
        }
    }
    else if (item.type == Research::EntryType::Scenery)
    {
        const auto sgEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(item.entryIndex);
        if (sgEntry != nullptr)
        {
            for (const auto& sceneryEntry : sgEntry->SceneryEntries)
            {
                ScenerySetInvented(sceneryEntry);
            }
        }
    }
}

static void ResearchRebuildInventedTables()
{
    auto& gameState = getGameState();
    SetEveryRideTypeNotInvented();
    SetEveryRideEntryInvented();
    SetEveryRideEntryNotInvented();
    SetAllSceneryItemsNotInvented();
    for (const auto& item : gameState.research.itemsInvented)
    {
        // Ignore item, if the research of it is in progress
        if (gameState.research.progressStage == RESEARCH_STAGE_DESIGNING
            || gameState.research.progressStage == RESEARCH_STAGE_COMPLETING_DESIGN)
        {
            if (item == gameState.research.nextItem)
            {
                continue;
            }
        }

        ResearchMarkItemAsResearched(item);
    }
    MarkAllUnrestrictedSceneryAsInvented();
}

static void ResearchAddAllMissingItems(bool isResearched)
{
    auto& gameState = getGameState();
    // Mark base ridetypes as seen if they exist in the invented research list.
    bool seenBaseEntry[kMaxRideObjects]{};
    for (auto const& researchItem : gameState.research.itemsInvented)
    {
        ObjectEntryIndex index = researchItem.baseRideType;
        seenBaseEntry[index] = true;
    }

    // Unlock and add research entries to the invented list for ride types whose base ridetype has been seen.
    for (ObjectEntryIndex i = 0; i < kMaxRideObjects; i++)
    {
        const auto* rideEntry = GetRideEntryByIndex(i);
        if (rideEntry != nullptr)
        {
            for (uint8_t j = 0; j < RCT2::ObjectLimits::kMaxRideTypesPerRideEntry; j++)
            {
                if (seenBaseEntry[rideEntry->ride_type[j]])
                {
                    RideEntrySetInvented(i);
                    ResearchInsertRideEntry(i, true);
                    break;
                }
            }
        }
    }

    // Mark base ridetypes as seen if they exist in the uninvented research list.
    for (auto const& researchItem : gameState.research.itemsUninvented)
    {
        ObjectEntryIndex index = researchItem.baseRideType;
        seenBaseEntry[index] = true;
    }

    // Only add Rides to uninvented research that haven't had their base ridetype seen.
    // This prevents rct2 grouped rides from only unlocking the first train.
    for (ObjectEntryIndex i = 0; i < kMaxRideObjects; i++)
    {
        const auto* rideEntry = GetRideEntryByIndex(i);
        if (rideEntry != nullptr)
        {
            bool baseSeen = false;
            for (uint8_t j = 0; j < RCT2::ObjectLimits::kMaxRideTypesPerRideEntry; j++)
            {
                if (seenBaseEntry[rideEntry->ride_type[j]])
                {
                    baseSeen = true;
                    break;
                }
            }
            if (!baseSeen)
            {
                ResearchInsertRideEntry(i, isResearched);
            }
        }
    }

    for (ObjectEntryIndex i = 0; i < kMaxSceneryGroupObjects; i++)
    {
        const auto* groupEntry = OpenRCT2::ObjectManager::GetObjectEntry<SceneryGroupEntry>(i);
        if (groupEntry != nullptr)
        {
            ResearchInsertSceneryGroupEntry(i, isResearched);
        }
    }
}

void ResearchFix()
{
    auto& gameState = getGameState();
    // Remove null entries from the research list
    ResearchRemoveNullItems(gameState.research.itemsInvented);
    ResearchRemoveNullItems(gameState.research.itemsUninvented);

    // Add missing entries to the research list
    // If research is complete, mark all the missing items as available
    ResearchAddAllMissingItems(gameState.research.progressStage == RESEARCH_STAGE_FINISHED_ALL);

    // Now rebuild all the tables that say whether a ride or scenery item is invented
    ResearchRebuildInventedTables();
    ResearchUpdateUncompletedTypes();
}

void ResearchItemsMakeAllUnresearched()
{
    auto& gameState = getGameState();
    gameState.research.itemsUninvented.insert(
        gameState.research.itemsUninvented.end(), std::make_move_iterator(gameState.research.itemsInvented.begin()),
        std::make_move_iterator(gameState.research.itemsInvented.end()));
    gameState.research.itemsInvented.clear();
}

void ResearchItemsMakeAllResearched()
{
    auto& gameState = getGameState();
    gameState.research.itemsInvented.insert(
        gameState.research.itemsInvented.end(), std::make_move_iterator(gameState.research.itemsUninvented.begin()),
        std::make_move_iterator(gameState.research.itemsUninvented.end()));
    gameState.research.itemsUninvented.clear();
}

/**
 *
 *  rct2: 0x00685A93
 */
void ResearchItemsShuffle()
{
    auto& gameState = getGameState();
    std::shuffle(
        std::begin(gameState.research.itemsUninvented), std::end(gameState.research.itemsUninvented),
        std::default_random_engine{});
}

bool ResearchItem::IsAlwaysResearched() const
{
    return (flags & (RESEARCH_ENTRY_FLAG_RIDE_ALWAYS_RESEARCHED | RESEARCH_ENTRY_FLAG_SCENERY_SET_ALWAYS_RESEARCHED)) != 0;
}

bool ResearchItem::IsNull() const
{
    return entryIndex == kObjectEntryIndexNull;
}

void ResearchItem::SetNull()
{
    entryIndex = kObjectEntryIndexNull;
}

bool ResearchItem::Exists() const
{
    auto& gameState = getGameState();
    for (auto const& researchItem : gameState.research.itemsUninvented)
    {
        if (researchItem == *this)
        {
            return true;
        }
    }
    for (auto const& researchItem : gameState.research.itemsInvented)
    {
        if (researchItem == *this)
        {
            return true;
        }
    }
    return false;
}

// clang-format off
static constexpr StringId _editorInventionsResearchCategories[] = {
    STR_RESEARCH_NEW_TRANSPORT_RIDES,
    STR_RESEARCH_NEW_GENTLE_RIDES,
    STR_RESEARCH_NEW_ROLLER_COASTERS,
    STR_RESEARCH_NEW_THRILL_RIDES,
    STR_RESEARCH_NEW_WATER_RIDES,
    STR_RESEARCH_NEW_SHOPS_AND_STALLS,
    STR_RESEARCH_NEW_SCENERY_AND_THEMING,
};
// clang-format on

StringId ResearchItem::GetCategoryInventionString() const
{
    const auto categoryValue = EnumValue(category);
    Guard::Assert(categoryValue <= 6, "Unsupported category invention string");
    return _editorInventionsResearchCategories[categoryValue];
}

// clang-format off
static constexpr StringId _researchCategoryNames[] = {
    STR_RESEARCH_CATEGORY_TRANSPORT,
    STR_RESEARCH_CATEGORY_GENTLE,
    STR_RESEARCH_CATEGORY_ROLLERCOASTER,
    STR_RESEARCH_CATEGORY_THRILL,
    STR_RESEARCH_CATEGORY_WATER,
    STR_RESEARCH_CATEGORY_SHOP,
    STR_RESEARCH_CATEGORY_SCENERY_GROUP,
};
// clang-format on

StringId ResearchItem::GetCategoryName() const
{
    const auto categoryValue = EnumValue(category);
    Guard::Assert(categoryValue <= 6, "Unsupported category name");
    return _researchCategoryNames[categoryValue];
}

bool ResearchItem::operator==(const ResearchItem& rhs) const
{
    return (entryIndex == rhs.entryIndex && baseRideType == rhs.baseRideType && type == rhs.type);
}

static BitSet<RIDE_TYPE_COUNT> _seenRideType = {};

static void ResearchUpdateFirstOfType(ResearchItem* researchItem)
{
    if (researchItem->IsNull())
        return;

    if (researchItem->type != Research::EntryType::Ride)
        return;

    auto rideType = researchItem->baseRideType;
    if (rideType >= RIDE_TYPE_COUNT)
    {
        LOG_ERROR("Research item has non-existent ride type index %d", rideType);
        return;
    }

    researchItem->flags &= ~RESEARCH_ENTRY_FLAG_FIRST_OF_TYPE;
    const auto& rtd = GetRideTypeDescriptor(rideType);
    if (rtd.HasFlag(RtdFlag::listVehiclesSeparately))
    {
        researchItem->flags |= RESEARCH_ENTRY_FLAG_FIRST_OF_TYPE;
        return;
    }

    if (!_seenRideType[rideType])
        researchItem->flags |= RESEARCH_ENTRY_FLAG_FIRST_OF_TYPE;
}

static void ResearchMarkRideTypeAsSeen(const ResearchItem& researchItem)
{
    auto rideType = researchItem.baseRideType;
    if (rideType >= RIDE_TYPE_COUNT)
        return;

    _seenRideType[rideType] = true;
}

void ResearchDetermineFirstOfType()
{
    auto& gameState = getGameState();
    _seenRideType.reset();

    for (const auto& researchItem : gameState.research.itemsInvented)
    {
        if (researchItem.type != Research::EntryType::Ride)
            continue;

        auto rideType = researchItem.baseRideType;
        if (rideType >= RIDE_TYPE_COUNT)
            continue;

        const auto& rtd = GetRideTypeDescriptor(rideType);
        if (rtd.HasFlag(RtdFlag::listVehiclesSeparately))
            continue;

        // The last research item will also be present in gameState.researchItemsInvented.
        // Avoid marking its ride type as "invented" prematurely.
        if (gameState.research.lastItem.has_value() && !gameState.research.lastItem->IsNull()
            && researchItem == gameState.research.lastItem.value())
            continue;

        // The next research item is (sometimes?) also present in gameState.researchItemsInvented, even though it isn't invented
        // yet(!)
        if (gameState.research.nextItem.has_value() && !gameState.research.nextItem->IsNull()
            && researchItem == gameState.research.nextItem.value())
            continue;

        ResearchMarkRideTypeAsSeen(researchItem);
    }

    if (gameState.research.lastItem.has_value())
    {
        ResearchUpdateFirstOfType(&gameState.research.lastItem.value());
        ResearchMarkRideTypeAsSeen(gameState.research.lastItem.value());
    }
    if (gameState.research.nextItem.has_value())
    {
        ResearchUpdateFirstOfType(&gameState.research.nextItem.value());
        ResearchMarkRideTypeAsSeen(gameState.research.nextItem.value());
    }

    for (auto& researchItem : gameState.research.itemsUninvented)
    {
        // The next research item is (sometimes?) also present in gameState.researchItemsUninvented
        if (gameState.research.nextItem.has_value() && !gameState.research.nextItem->IsNull()
            && researchItem.baseRideType == gameState.research.nextItem.value().baseRideType)
        {
            // Copy the "first of type" flag.
            researchItem.flags = gameState.research.nextItem->flags;
            continue;
        }

        ResearchUpdateFirstOfType(&researchItem);
        ResearchMarkRideTypeAsSeen(researchItem);
    }
}
