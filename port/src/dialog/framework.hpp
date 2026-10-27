#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dc::dialog {

/**
 * The window style a dialog session is drawn with.
 */
enum class BubbleKind : uint8_t {
    StandardWindow = 0,
    SpeechTail = 1,
    SystemNotice = 2,
};

/**
 * The text colour of a choice row.
 */
enum class ChoiceColor : uint8_t {
    White = 0,
    Yellow = 1,
    Cyan = 2,
    Green = 3,
    Gold = 4,
    Grey = 5,
    Highlight = 6,
};

/**
 * The NPC a session talks for, and where its window goes.
 */
struct NpcContext {
    int32_t     map_no = -1;
    int32_t     npc_id = -1;
    int32_t     mes_id = -1;
    std::string mes_file_key;
    std::string hub_greeting_key; /**< A line above the NPC's menu (a key, as ResolveKey takes one); empty for the choices alone. */
    BubbleKind  bubble = BubbleKind::SpeechTail;
    bool        has_anchor = false;          /**< Whether anchor_x/anchor_y place the window, rather than the screen's bottom. */
    float       anchor_x = 320.0f;           /**< Screen x the window centres on, in pixels. */
    float       anchor_y = 220.0f;           /**< Screen y the window's tail points at, in pixels. */
    bool        host_plays_original = false; /**< The host shows the NPC's own message (the retail ClsMes window): "Talk" ends the session with SessionEnd::PlayOriginal, and an NPC without provider choices is never intercepted. */
};

/**
 * Overrides for the game's own tokens. An empty entry leaves its tag ("{name1}", "{insert3}",
 * "{value}") in the text, for the game's text renderer to fill in as it does for retail messages:
 * the player can rename Toan, so only the game knows his name.
 */
struct TokenContext {
    std::array<std::string, 6>  party_names = {};
    std::array<std::string, 10> inserts = {};
    std::array<std::string, 8>  values = {};
};

/**
 * How the last session ended, for a host that has to carry on from it.
 */
enum class SessionEnd : uint8_t {
    None = 0,         /**< Still open, or already taken. */
    Closed = 1,       /**< Left, cancelled, or a choice with nothing more to show. */
    PlayOriginal = 2, /**< "Talk" with NpcContext::host_plays_original: the host shows the NPC's message. */
};

class ISessionController;

/**
 * One selectable row of a choice page.
 */
struct ChoiceItem {
    std::string                               id;             /**< Stable identity, kept across RefreshHub to keep the cursor on it. */
    std::string                               label_key;      /**< Localization key for the label. */
    std::string                               fallback_label; /**< Label used when label_key is empty or does not resolve. */
    std::vector<std::string>                  label_args;     /**< Positional %1..%9 arguments for the label. */
    ChoiceColor                               color = ChoiceColor::White;
    bool                                      enabled = true;
    std::function<void(ISessionController &)> on_select; /**< Runs when confirmed; empty closes the session. */
};

/**
 * One page of dialog text, wrapped to the window's width.
 */
struct PageSlice {
    std::string              raw_text;
    std::vector<std::string> wrapped_markup_lines;  /**< Wrapped lines with their control tags. */
    std::vector<std::string> wrapped_visible_lines; /**< The same lines as StripControlCodes gives them. */
};

/**
 * Sizes and screen bounds the dialog window is laid out with, in 640x448 screen pixels.
 */
struct LayoutConfig {
    float   min_box_width = 180.0f;
    float   max_box_width = 520.0f;
    float   max_inner_width = 440.0f;
    float   padding_x = 20.0f;
    float   padding_y = 14.0f;
    float   body_line_stride = 20.0f;
    float   divider_total_gap = 12.0f;
    float   choice_row_stride = 22.0f;
    float   choice_indent_width = 28.0f; /**< Room left of each choice for the hand cursor. */
    float   scrollbar_gutter = 14.0f;
    float   safe_left = 24.0f;
    float   safe_right = 616.0f;
    float   safe_top = 20.0f;
    float   safe_bottom = 448.0f;
    float   tail_height = 16.0f;
    int32_t max_visible_choices = 5;
};

/**
 * Where each part of the dialog window goes on screen, in pixels.
 */
struct ComputedLayout {
    float box_x = 0.0f;
    float box_y = 0.0f;
    float box_w = 0.0f;
    float box_h = 0.0f;

    float text_x = 0.0f;
    float text_y = 0.0f;
    float text_w = 0.0f;
    float text_h = 0.0f;

    bool  has_divider = false;
    float divider_y = 0.0f;

    float choices_x = 0.0f;
    float choices_y = 0.0f;
    float choices_w = 0.0f;
    float choices_h = 0.0f;

    int32_t visible_choice_count = 0;
    bool    needs_scrollbar = false;

    bool  has_tail = false;
    float tail_x = 0.0f;
    float tail_y = 0.0f;
};

/**
 * Everything a renderer needs to draw the open session's current page.
 */
struct ActiveView {
    NpcContext               context;
    std::string              speaker_text;
    PageSlice                current_slice;
    int32_t                  slice_index = 0;
    int32_t                  slice_count = 1;
    std::vector<ChoiceItem>  choices;
    std::vector<std::string> formatted_choice_labels; /**< Choice labels resolved and token-substituted. */
    int32_t                  selected_choice = 0;
    int32_t                  choice_scroll_offset = 0; /**< Index of the first visible choice. */
    ComputedLayout           layout;
    bool                     show_choices_on_current_slice = false; /**< Choices show only on a page's last slice. */
};

/**
 * What a choice's on_select can do to the session it was chosen in.
 */
class ISessionController {
public:
    virtual ~ISessionController() = default;

    /**
     * Returns the NPC the session talks for.
     */
    virtual const NpcContext &GetContext() const noexcept = 0;

    /**
     * Pushes a page of text given by its localization key.
     */
    virtual void PushMessageKey(std::string_view         text_key,
                                std::vector<std::string> format_args = {}) = 0;

    /**
     * Pushes a page of text; reading it goes back to the page under it.
     */
    virtual void PushRawMessage(std::string_view         raw_utf8_text,
                                std::vector<std::string> format_args = {}) = 0;

    /**
     * Pushes a page of choices under a prompt given by its localization key.
     */
    virtual void PushChoicePage(std::string_view         prompt_key,
                                std::vector<ChoiceItem>  choices,
                                std::vector<std::string> format_args = {}) = 0;

    /**
     * Pushes a page of text that ends the conversation once read, where PushRawMessage's goes back
     * to the page under it: for a choice that leads somewhere once the conversation is over.
     */
    virtual void PushClosingMessage(std::string_view         raw_utf8_text,
                                    std::vector<std::string> format_args = {}) = 0;

    /**
     * As PushChoicePage, with the prompt's text itself rather than a key for it.
     */
    virtual void PushRawChoicePage(std::string_view         raw_utf8_prompt,
                                   std::vector<ChoiceItem>  choices,
                                   std::vector<std::string> format_args = {}) = 0;

    /**
     * Asks the providers again for the NPC's menu (the bottom page, where TryInterceptOpen opened
     * one), for a choice that changed what they offer: a quest that moved on.
     */
    virtual void RefreshHub() = 0;

    /**
     * Shows the NPC's own message, or ends the session for the host to show it.
     */
    virtual void PlayOriginalMessage() = 0;

    /**
     * Goes back to the page under the current one, closing the session from the bottom page.
     */
    virtual void PopPage() = 0;

    /**
     * Ends the session.
     */
    virtual void Close() noexcept = 0;
};

using OptionProviderFn = std::function<void(const NpcContext &ctx, std::vector<ChoiceItem> &out_choices)>;
using MeasureTextWidthFn = std::function<float(std::string_view visible_utf8)>;
using ExternalLocalizeFn = std::function<std::string_view(std::string_view key)>;
using RenderSessionFn = std::function<void(const ActiveView &view)>;

/**
 * Resets the session state.
 */
void Init();

/**
 * Closes any session and drops the registered callbacks, providers and widgets.
 */
void Shutdown();

/**
 * Sets a lookup consulted before LocalizeText for keys. The "dialog." strings are translated in
 * lang/<language>.json beside the port's other text, and their English is built in.
 */
void SetExternalLocalizeLookup(ExternalLocalizeFn lookup_fn);

/**
 * Sets the token overrides SubstituteTokens uses by default.
 */
void SetTokenContext(const TokenContext &token_ctx);

/**
 * Returns the token overrides set with SetTokenContext.
 */
const TokenContext &GetTokenContext() noexcept;

/**
 * Resolves a localization key to its text, or returns the text itself when it is not a key.
 */
std::string ResolveKey(std::string_view key_or_raw);

/**
 * Fills %1..%9 from positional_args and the {nameN}, {insertN} and {valueN} tags from token_ctx
 * (the context set with SetTokenContext when null).
 */
std::string SubstituteTokens(std::string_view             text,
                             std::span<const std::string> positional_args = {},
                             const TokenContext          *token_ctx = nullptr);

/**
 * Removes markup tags, leaving one '@' for each tag that draws something.
 */
std::string StripControlCodes(std::string_view markup_text);

/**
 * Lists the framework localization keys and their English text.
 */
std::vector<std::pair<std::string, std::string>> Strings();

/**
 * Splits markup text into pages at each {page} tag.
 */
std::vector<std::string> SplitRawPages(std::string_view markup_text);

/**
 * Sets a measurer for visible text; without one, lines are measured in the game's message font.
 */
void SetTextMeasurer(MeasureTextWidthFn measurer_fn);

/**
 * Sets the layout sizes and bounds used for new pages.
 */
void SetLayoutConfig(const LayoutConfig &cfg);

/**
 * Returns the layout sizes and bounds set with SetLayoutConfig.
 */
const LayoutConfig &GetLayoutConfig() noexcept;

/**
 * Returns the width of a markup line in pixels, tags included.
 */
float MeasureMarkupLineWidth(std::string_view markup_line);

/**
 * Word-wraps one page of markup to max_inner_width pixels.
 */
PageSlice BuildWrappedSlice(std::string_view single_page_markup, float max_inner_width);

/**
 * Lays out the window for a page and its choices, around the context's anchor.
 */
ComputedLayout ComputeExpandedLayout(const PageSlice             &slice,
                                     std::span<const std::string> formatted_choice_labels,
                                     const NpcContext            &ctx,
                                     const LayoutConfig          &cfg = GetLayoutConfig());

/**
 * Registers a provider that adds choices to an NPC's menu.
 */
void RegisterOptionProvider(OptionProviderFn provider);

/**
 * Removes every registered option provider.
 */
void ClearOptionProviders();

/**
 * Appends every provider's choices for an NPC to out_choices, in registration order.
 */
void QueryOptionProviders(const NpcContext &ctx, std::vector<ChoiceItem> &out_choices);

/**
 * Sets a renderer to draw sessions with in place of the default window.
 */
void SetRenderCallback(RenderSessionFn render_fn);

/**
 * Off by default: only an NPC a provider adds choices to is intercepted. On, every NPC's message
 * is shown by the framework, unless the context has host_plays_original.
 */
void SetInterceptAllNpcs(bool intercept_all) noexcept;

/**
 * Returns the setting made with SetInterceptAllNpcs.
 */
bool GetInterceptAllNpcs() noexcept;

/**
 * Moves the open session's window to a new anchor point.
 */
void UpdateAnchor(float anchor_x, float anchor_y, bool has_anchor = true);

/**
 * Opens a session for an NPC when a provider adds choices to it (or every NPC is intercepted),
 * returning whether it did. decoded_original_message is the NPC's own text, as GameTextDecode gives
 * it; unused (and may be empty) with ctx.host_plays_original.
 */
bool TryInterceptOpen(const NpcContext &ctx, std::string_view decoded_original_message);

/**
 * Opens a session with a prompt and choices of the caller's own.
 */
void OpenCustomSession(const NpcContext       &ctx,
                       std::string_view        prompt_raw_or_key,
                       std::vector<ChoiceItem> choices,
                       bool                    resolve_prompt_as_key = false);

/**
 * Returns whether a session is open.
 */
bool IsSessionActive() noexcept;

/**
 * Returns the open session's view, or nullptr when none is open.
 */
const ActiveView *GetActiveView() noexcept;

/**
 * Returns how the session that last closed ended; reading it resets it to SessionEnd::None.
 */
SessionEnd TakeSessionEnd() noexcept;

/**
 * Advances the session's animation timers; called once a game tick while a session is open.
 */
void StepSession() noexcept;

/**
 * Draws the open session; called once a frame.
 */
void DrawSession();

/**
 * Closes the open session, if any.
 */
void ForceCloseSession() noexcept;

/**
 * Moves the selection to the previous enabled choice, returning whether it moved.
 */
bool InputMoveUp();

/**
 * Moves the selection to the next enabled choice, returning whether it moved.
 */
bool InputMoveDown();

/**
 * Selects the choice in a visible row, returning whether the selection changed.
 */
bool InputHoverChoice(int32_t visible_row_index);

/**
 * Returns the visible choice row at a screen point, or -1 for none.
 */
int32_t HitTestChoiceRow(float px, float py) noexcept;

/**
 * Moves the selection by mouse wheel notches (positive up), returning whether it moved.
 */
bool InputScrollWheel(int32_t wheel_notches);

/**
 * Turns the page or confirms the selected choice, returning whether the input was used.
 */
bool InputConfirm();

/**
 * Goes back a page or closes the session, returning whether the input was used.
 */
bool InputCancel();

} // namespace dc::dialog
