#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Dialog {

enum class BubbleKind : uint8_t {
    StandardWindow = 0,
    SpeechTail = 1,
    SystemNotice = 2,
};

enum class ChoiceColor : uint8_t {
    White = 0,
    Yellow = 1,
    Cyan = 2,
    Green = 3,
    Gold = 4,
    Grey = 5,
    Highlight = 6,
};

struct NpcContext {
    int32_t     map_no = -1;
    int32_t     npc_id = -1;
    int32_t     mes_id = -1;
    std::string mes_file_key;
    // A line above the NPC's menu (a key, as ResolveKey takes one); empty for none, the choices alone.
    std::string hub_greeting_key;
    BubbleKind  bubble = BubbleKind::SpeechTail;
    bool        has_anchor = false;
    float       anchor_x = 320.0f;
    float       anchor_y = 220.0f;
    // The host shows the NPC's own message itself (the retail ClsMes window): "Talk" and
    // PlayOriginalMessage end the session with SessionEnd::PlayOriginal instead of pushing the text,
    // and an NPC no provider adds choices to is never intercepted.
    bool host_plays_original = false;
};

// Overrides for the game's own tokens. An empty entry leaves its tag ("{name1}", "{insert3}",
// "{value}") in the text, for the game's text renderer to fill in as it does for retail messages:
// the player can rename Toan, so only the game knows his name.
struct TokenContext {
    std::array<std::string, 6>  party_names = {};
    std::array<std::string, 10> inserts = {};
    std::array<std::string, 8>  values = {};
};

// How the last session ended, for a host that has to carry on from it.
enum class SessionEnd : uint8_t {
    None = 0,         // Still open, or already taken.
    Closed = 1,       // Left, cancelled, or a choice with nothing more to show.
    PlayOriginal = 2, // "Talk" with NpcContext::host_plays_original: the host shows the NPC's message.
};

class ISessionController;

struct ChoiceItem {
    std::string                               id;
    std::string                               label_key;
    std::string                               fallback_label;
    std::vector<std::string>                  label_args;
    ChoiceColor                               color = ChoiceColor::White;
    bool                                      enabled = true;
    std::function<void(ISessionController &)> on_select;
};

struct PageSlice {
    std::string              raw_text;
    std::vector<std::string> wrapped_markup_lines;
    std::vector<std::string> wrapped_visible_lines;
};

struct LayoutConfig {
    float   min_box_width = 180.0f;
    float   max_box_width = 520.0f;
    float   max_inner_width = 440.0f;
    float   padding_x = 20.0f;
    float   padding_y = 14.0f;
    float   body_line_stride = 20.0f;
    float   divider_total_gap = 12.0f;
    float   choice_row_stride = 22.0f;
    float   choice_indent_width = 28.0f;
    float   scrollbar_gutter = 14.0f;
    float   safe_left = 24.0f;
    float   safe_right = 616.0f;
    float   safe_top = 20.0f;
    float   safe_bottom = 448.0f;
    float   tail_height = 16.0f;
    int32_t max_visible_choices = 5;
};

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

struct ActiveView {
    NpcContext               context;
    std::string              speaker_text;
    PageSlice                current_slice;
    int32_t                  slice_index = 0;
    int32_t                  slice_count = 1;
    std::vector<ChoiceItem>  choices;
    std::vector<std::string> formatted_choice_labels;
    int32_t                  selected_choice = 0;
    int32_t                  choice_scroll_offset = 0;
    ComputedLayout           layout;
    bool                     show_choices_on_current_slice = false;
};

class ISessionController {
public:
    virtual ~ISessionController() = default;

    virtual const NpcContext &GetContext() const noexcept = 0;

    virtual void PushMessageKey(std::string_view         text_key,
                                std::vector<std::string> format_args = {}) = 0;

    virtual void PushRawMessage(std::string_view         raw_utf8_text,
                                std::vector<std::string> format_args = {}) = 0;

    virtual void PushChoicePage(std::string_view         prompt_key,
                                std::vector<ChoiceItem>  choices,
                                std::vector<std::string> format_args = {}) = 0;

    // A page of text that ends the conversation once read, where PushRawMessage's goes back to the
    // page under it: for a choice that leads somewhere once the conversation is over.
    virtual void PushClosingMessage(std::string_view         raw_utf8_text,
                                    std::vector<std::string> format_args = {}) = 0;

    // As PushChoicePage, with the prompt's text itself rather than a key for it.
    virtual void PushRawChoicePage(std::string_view         raw_utf8_prompt,
                                   std::vector<ChoiceItem>  choices,
                                   std::vector<std::string> format_args = {}) = 0;

    // Asks the providers again for the NPC's menu (the bottom page, where TryInterceptOpen opened
    // one), for a choice that changed what they offer: a quest that moved on.
    virtual void RefreshHub() = 0;

    virtual void PlayOriginalMessage() = 0;
    virtual void PopPage() = 0;
    virtual void Close() noexcept = 0;
};

using OptionProviderFn = std::function<void(const NpcContext &ctx, std::vector<ChoiceItem> &out_choices)>;
using MeasureTextWidthFn = std::function<float(std::string_view visible_utf8)>;
using ExternalLocalizeFn = std::function<std::string_view(std::string_view key)>;
using RenderSessionFn = std::function<void(const ActiveView &view)>;

void Init();
void Shutdown();

// Localization & token utilities. Keys resolve through the external lookup, then LocalizeText: the
// "dialog." strings are translated in lang/<language>.json beside the port's other text, and their
// English is built in (docs/DIALOG_FRAMEWORK.md).
void                SetExternalLocalizeLookup(ExternalLocalizeFn lookup_fn);
void                SetTokenContext(const TokenContext &token_ctx);
const TokenContext &GetTokenContext() noexcept;

std::string ResolveKey(std::string_view key_or_raw);
std::string SubstituteTokens(std::string_view             text,
                             std::span<const std::string> positional_args = {},
                             const TokenContext          *token_ctx = nullptr);
std::string StripControlCodes(std::string_view markup_text);
/**
 * Lists the framework localization keys and their English text.
 */
std::vector<std::pair<std::string, std::string>> Strings();
std::vector<std::string>                         SplitRawPages(std::string_view markup_text);

// Layout & measurement
void                SetTextMeasurer(MeasureTextWidthFn measurer_fn);
void                SetLayoutConfig(const LayoutConfig &cfg);
const LayoutConfig &GetLayoutConfig() noexcept;

float          MeasureMarkupLineWidth(std::string_view markup_line);
PageSlice      BuildWrappedSlice(std::string_view single_page_markup, float max_inner_width);
ComputedLayout ComputeExpandedLayout(const PageSlice             &slice,
                                     std::span<const std::string> formatted_choice_labels,
                                     const NpcContext            &ctx,
                                     const LayoutConfig          &cfg = GetLayoutConfig());

// Extension registration
void RegisterOptionProvider(OptionProviderFn provider);
void ClearOptionProviders();
void QueryOptionProviders(const NpcContext &ctx, std::vector<ChoiceItem> &out_choices);

// Session lifecycle & input navigation
void SetRenderCallback(RenderSessionFn render_fn);
// Off by default: only an NPC a provider adds choices to is intercepted. On, every NPC's message is
// shown by the framework, unless the context has host_plays_original.
void SetInterceptAllNpcs(bool intercept_all) noexcept;
bool GetInterceptAllNpcs() noexcept;
void UpdateAnchor(float anchor_x, float anchor_y, bool has_anchor = true);

// decoded_original_message is the NPC's own text, as GameTextDecode gives it; unused (and may be
// empty) with ctx.host_plays_original.
bool              TryInterceptOpen(const NpcContext &ctx, std::string_view decoded_original_message);
void              OpenCustomSession(const NpcContext       &ctx,
                                    std::string_view        prompt_raw_or_key,
                                    std::vector<ChoiceItem> choices,
                                    bool                    resolve_prompt_as_key = false);
bool              IsSessionActive() noexcept;
const ActiveView *GetActiveView() noexcept;
// How the session that last closed ended; reading it resets it to SessionEnd::None.
SessionEnd TakeSessionEnd() noexcept;

// Once a game tick while a session is open (animation timers), and once a frame to draw it.
void StepSession() noexcept;
void DrawSession();
void ForceCloseSession() noexcept;

// Input drivers (callable by pad step or mouse hit-test)
bool    InputMoveUp();
bool    InputMoveDown();
bool    InputHoverChoice(int32_t visible_row_index);
int32_t HitTestChoiceRow(float px, float py) noexcept;
bool    InputScrollWheel(int32_t wheel_notches);
bool    InputConfirm();
bool    InputCancel();

} // namespace Dialog
