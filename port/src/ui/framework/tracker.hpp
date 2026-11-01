#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/framework/types.hpp"
#include "ui/framework/widgets.hpp"

namespace dc::ui {

/**
 * How many of a set of goals are done.
 */
struct TrackerTally {
    int done = 0;
    int total = 0;
};

/**
 * One line of a group's breakdown: an item's icon (or none), a label and its tally.
 */
struct TrackerRow {
    std::string  label;
    int          item_no = -1; /**< The icon's item, or -1 for none. */
    TrackerTally tally;
};

/**
 * What a tracker shows, supplied by the screen that owns the goals. The tracker knows nothing of
 * what they are: chests, rewards and requests are all groups of rows with tallies.
 */
struct TrackerSource {
    std::string                                           title;          /**< The banner's text. */
    std::string                                           groups_heading; /**< Above the group list ("SETTLEMENTS"). */
    std::string                                           all_label;      /**< Above every group's tally together ("World Total"). */
    std::string                                           group_label;    /**< Beside the selected group's tally ("Area Total"). */
    std::string                                           breakdown;      /**< The breakdown's heading, %1 the group's name. */
    std::string                                           progress;       /**< The progress bar's text, %1 the percentage. */
    std::string                                           controls;       /**< The controls line at the foot. */
    std::string                                           filter_on;      /**< The filter line while filtered; empty for a tracker without a filter. */
    std::string                                           filter_off;     /**< The filter line while not. */
    std::function<int()>                                  group_count;
    std::function<std::string(int group)>                 group_name;
    std::function<std::vector<TrackerRow>(int, bool)>     rows;         /**< A group's rows, filtered or not. */
    std::function<TrackerTally(int group, bool filtered)> tally;        /**< A group's tally, filtered or not. */
    std::function<bool(int group)>                        group_locked; /**< Drawn grey, not yet reached; empty for none. */
};

/**
 * A tracker screen (docs/UI_FRAMEWORK.md, section 11): groups of goals with progress, on paper under a
 * stone banner. The groups are listed on the left with every group's tally beneath; the selected one's
 * breakdown, tally and progress bar are on the right. Up and Down choose a group, Cross or Square turns
 * the filter, Circle goes back. It says where its rows are, so the mouse needs no copy of them.
 */
class Tracker {
public:
    /**
     * Shows source, starting at the first group and filtered.
     */
    void Open(TrackerSource source);

    /**
     * Reads the pad. Returns true when the player backs out.
     */
    bool HandleKey();

    /**
     * Selects the group or turns the filter under a click at (x, y), in logical coordinates.
     */
    void HandlePointer(float x, float y);

    /**
     * Draws the tracker.
     */
    void Draw();

    /**
     * The row of group in the list, for hit-testing.
     */
    Rect GroupRect(int group) const;

    /**
     * The filter line, for hit-testing; empty for a tracker without a filter.
     */
    Rect FilterRect() const;

    /**
     * Whether the filter is on.
     */
    bool Filtered() const { return m_filtered; }

    /**
     * The selected group.
     */
    int Selected() const { return m_selected; }

private:
    int Groups() const;

    TrackerSource m_source;
    int           m_selected = 0;
    bool          m_filtered = true;
    Cursor        m_cursor;
    Brackets      m_brackets;
};

} // namespace dc::ui
