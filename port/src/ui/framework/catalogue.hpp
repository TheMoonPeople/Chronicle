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
 * One page of the detail panel's body: a title over wrapped text.
 */
struct CataloguePage {
    std::string title;
    std::string text;
    u32         color = ColorWhite; /**< The text's colour. */
};

/**
 * What the detail panel says about the selected entry: a heading, a status line, and pages that Left
 * and Right turn.
 */
struct CatalogueDetail {
    std::string                heading;
    std::string                status; /**< A line under the heading, in status_color. */
    u32                        status_color = ColorWhite;
    std::vector<CataloguePage> pages;
};

/**
 * What a catalogue shows, supplied by the screen that owns the entries. The catalogue knows nothing
 * of what they are: buildings, enemies and fish are all entries with a detail.
 */
struct CatalogueSource {
    std::string                         title;         /**< The banner's text. */
    std::string                         list_heading;  /**< Above the list ("BUILDINGS"). */
    std::string                         summary_label; /**< Above the summary's tally, under the list; empty for none. */
    std::string                         controls;      /**< The controls line at the foot. */
    std::function<int()>                count;
    std::function<CatalogueEntry(int)>  entry;
    std::function<CatalogueDetail(int)> detail;
    std::function<TrackerTally()>       summary; /**< Every entry's tally together; empty for none. */
};

/**
 * A catalogue screen (docs/UI_FRAMEWORK.md, section 11): a list of entries on the left with an optional
 * tally under it, and the selected one's detail on the right, its body in pages, on paper under a stone
 * banner as the tracker is. Up and Down choose an entry, Left and Right turn its pages, Circle goes
 * back. Its list takes the mouse's hover itself (ListView); EntryAt and PageArrowRect say where the
 * entries and the page arrows are.
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
     * The page arrow that turns back (direction -1) or on (+1), for hit-testing; empty when the selected
     * entry has one page or none.
     */
    Rect PageArrowRect(int direction) const;

    /**
     * The selected entry.
     */
    int Selected() const { return m_list.SelectedIndex(); }

private:
    int  Count() const;
    int  Pages() const;
    void TurnPage(int direction);

    CatalogueSource m_source;
    ListView        m_list;
    int             m_page = 0;
    int             m_page_entry = -1; /**< The entry m_page belongs to. */
    Rect            m_indicator;       /**< Where Draw put the page indicator, "< 1/3 >"; empty for none. */
};

} // namespace dc::ui
