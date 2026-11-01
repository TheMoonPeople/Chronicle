#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/framework/dialogs.hpp"
#include "ui/framework/tracker.hpp"
#include "ui/framework/types.hpp"

namespace dc::ui {

/**
 * One line of a catalogue's list: its label, coloured by the entry's state.
 */
struct CatalogueEntry {
    std::string label;
    u32         color = ColorWhite;
};

/**
 * What the detail panel says about the selected entry.
 */
struct CatalogueDetail {
    std::string              heading;
    std::string              status; /**< A line under the heading, in status_color. */
    u32                      status_color = ColorWhite;
    std::string              body_heading; /**< Above the body; empty for none. */
    std::vector<std::string> body;         /**< Paragraphs, each wrapped to the panel. */
};

/**
 * What a catalogue shows, supplied by the screen that owns the entries. The catalogue knows nothing
 * of what they are: buildings, enemies and fish are all entries with a detail.
 */
struct CatalogueSource {
    std::string                         title;         /**< The banner's text. */
    std::string                         list_heading;  /**< Above the list ("BUILDINGS"). */
    std::string                         summary_label; /**< Beside the summary's tally; empty for no summary. */
    std::string                         progress;      /**< The summary's progress bar text, %1 the percentage. */
    std::string                         controls;      /**< The controls line at the foot. */
    std::function<int()>                count;
    std::function<CatalogueEntry(int)>  entry;
    std::function<CatalogueDetail(int)> detail;
    std::function<TrackerTally()>       summary; /**< Every entry's tally together; empty for none. */
};

/**
 * A catalogue screen (docs/UI_FRAMEWORK.md, section 11): a list of entries on the left and the selected
 * one's detail on the right, on paper under a stone banner as the tracker is, with an optional tally and
 * progress bar under the detail. Up and Down choose an entry, Circle goes back. Its list takes the
 * mouse's hover itself (ListView), and EntryAt says where the entries are.
 */
class Catalogue {
public:
    Catalogue();

    /**
     * Shows source, starting at the first entry.
     */
    void Open(CatalogueSource source);

    /**
     * Reads the pad. Returns true when the player backs out.
     */
    bool HandleKey();

    /**
     * Draws the catalogue.
     */
    void Draw();

    /**
     * The entry under (x, y), in logical coordinates, or -1 for none.
     */
    int EntryAt(float x, float y) const;

    /**
     * The selected entry.
     */
    int Selected() const { return m_list.SelectedIndex(); }

private:
    int Count() const;

    CatalogueSource m_source;
    ListView        m_list;
};

} // namespace dc::ui
