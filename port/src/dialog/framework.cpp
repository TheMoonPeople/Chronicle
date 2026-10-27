#include "dialog/framework.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "localize/localize.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "ui/framework/internal.hpp"

namespace dc::dialog {

using namespace dc::game;
using namespace dc::localize;
using namespace dc::render;

namespace {

// What a custom text measurer's caller counts an icon or name tag as.
constexpr float kGlyphCellWidthPx = 14.0f;

// English for the framework's own keys; lang/<language>.json translates them under the same keys.
constexpr auto kEnglish = std::to_array<std::pair<std::string_view, std::string_view>>({
    {"dialog.common.talk",        "Talk"                        },
    {"dialog.common.leave",       "Never mind"                  },
    {"dialog.common.back",        "Back"                        },
    {"dialog.common.yes",         "Yes"                         },
    {"dialog.common.no",          "No"                          },
    {"dialog.common.accept",      "Accept"                      },
    {"dialog.common.decline",     "Decline"                     },
    {"dialog.common.next_page",   "{cross} Next"                },
    {"dialog.hub.default_prompt", "Is there something you need?"},
    {"dialog.hub.anything_else",  "Was there anything else?"    },
});

struct FrameNode {
    std::vector<PageSlice>   slices;
    int32_t                  current_slice_idx = 0;
    std::vector<ChoiceItem>  choices;
    std::vector<std::string> formatted_choice_labels;
    int32_t                  selected_choice = 0;
    int32_t                  choice_scroll_offset = 0;
    bool                     hub = false;             // TryInterceptOpen's menu, which RefreshHub builds again.
    bool                     close_when_read = false; // PushClosingMessage's: confirming its last slice ends the session.
};

// The widgets look their textures up when made, so the default renderer makes them when it first
// draws rather than with g_state, before the texture manager exists.
struct Widgets {
    ui::Cursor    cursor;
    ui::Brackets  brackets;
    ui::ScrollBar scrollbar;
};

struct FrameworkState {
    TokenContext                  tokens;
    LayoutConfig                  layout_cfg;
    MeasureTextWidthFn            text_measurer;
    ExternalLocalizeFn            external_localize;
    RenderSessionFn               render_callback;
    std::vector<OptionProviderFn> providers;
    bool                          intercept_all_npcs = false;

    bool                   active = false;
    SessionEnd             last_end = SessionEnd::None;
    NpcContext             active_ctx;
    std::string            original_message;
    std::vector<FrameNode> stack;
    ActiveView             cached_view;

    std::optional<Widgets> widgets;
    bool                   snap_cursor = false;
    uint32_t               blink_counter = 0;

    // The default renderer's labels for cached_view, laid out again only when view_generation moves on:
    // a Label lays its text out when it is set, and that is too slow to do every frame.
    uint32_t                                view_generation = 0;
    uint32_t                                drawn_generation = 0;
    std::vector<std::unique_ptr<ui::Label>> body_labels;
    std::vector<std::unique_ptr<ui::Label>> choice_labels;
    std::unique_ptr<ui::Label>              measure_label;
};

FrameworkState g_state;

Widgets &GetWidgets() {
    if (!g_state.widgets) {
        g_state.widgets.emplace();
    }
    return *g_state.widgets;
}

// The scrollbar shows a while after the selection moves; only the default renderer has one.
void NoteSelectionMoved() {
    if (g_state.widgets) {
        g_state.widgets->scrollbar.Update(true);
    }
}

// A Label that leaves the game's random numbers alone: laying text out otherwise draws on rand(), and
// the dialog lays its text out at times the retail game does not.
std::unique_ptr<ui::Label> MakeLabel(std::string_view text) {
    auto label = std::make_unique<ui::Label>();
    label->KeepGameRandom(true);
    label->SetText(text);
    return label;
}

// A markup line's width in the message font, tags and all, as the game's text renderer lays it out.
float MeasureWithGameFont(std::string_view markup_line) {
    if (!g_state.measure_label) {
        g_state.measure_label = MakeLabel("");
    }
    g_state.measure_label->SetText(markup_line);
    const int width = g_state.measure_label->GetWidth();
    // GameText lays out nothing past MES_WIN_LINE_MAX characters, and gives 0: never let that fit.
    if (width <= 0 && !StripControlCodes(markup_line).empty()) {
        return std::numeric_limits<float>::max();
    }
    return static_cast<float>(width);
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix) noexcept {
    if (text.size() < prefix.size()) {
        return false;
    }
    return EqualsIgnoreCase(std::string_view(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(prefix.size())), prefix);
}

bool IsZeroWidthControlTag(std::string_view tag_content) noexcept {
    if (tag_content.empty()) {
        return true;
    }
    static constexpr std::array<std::string_view, 10> kZeroWidthNames = {
        "page", "white", "yellow", "cyan", "green", "dark",
        "gold", "grey", "highlight", "/color"};
    if (std::ranges::any_of(kZeroWidthNames, [tag_content](std::string_view name) {
            return EqualsIgnoreCase(tag_content, name);
        })) {
        return true;
    }
    static constexpr std::array<std::string_view, 6> kZeroWidthPrefixes = {
        "wait ", "color ", "gap ", "spacing ", "justify ", "bubble "};
    return std::ranges::any_of(kZeroWidthPrefixes, [tag_content](std::string_view prefix) {
        return StartsWithIgnoreCase(tag_content, prefix);
    });
}

u32 MapChoiceColor(ChoiceColor color, bool enabled) noexcept {
    if (!enabled) {
        return FONT_COLOR_GREY;
    }
    switch (color) {
        case ChoiceColor::White:
            return ui::ColorWhite;
        case ChoiceColor::Yellow:
            return ui::ColorYellow;
        case ChoiceColor::Cyan:
            return FONT_COLOR_CYAN;
        case ChoiceColor::Green:
            return FONT_COLOR_GREEN;
        case ChoiceColor::Gold:
            return ui::ColorGold;
        case ChoiceColor::Grey:
            return FONT_COLOR_GREY;
        case ChoiceColor::Highlight:
            return ui::ColorYellow;
    }
    return ui::ColorWhite;
}

int32_t FindFirstEnabledChoice(std::span<const ChoiceItem> choices) noexcept {
    for (size_t i = 0; i < choices.size(); ++i) {
        if (choices[i].enabled) {
            return static_cast<int32_t>(i);
        }
    }
    return 0;
}

void ClampChoiceScroll(FrameNode &node, int32_t max_visible) noexcept {
    const auto total = static_cast<int32_t>(node.choices.size());
    if (total <= max_visible || max_visible <= 0) {
        node.choice_scroll_offset = 0;
        return;
    }
    if (node.selected_choice < node.choice_scroll_offset) {
        node.choice_scroll_offset = node.selected_choice;
    } else if (node.selected_choice >= node.choice_scroll_offset + max_visible) {
        node.choice_scroll_offset = node.selected_choice - max_visible + 1;
    }
    const int32_t max_offset = total - max_visible;
    node.choice_scroll_offset = std::clamp(node.choice_scroll_offset, 0, max_offset);
}

void EndSession(SessionEnd how) noexcept {
    if (g_state.active) {
        g_state.last_end = how;
    }
    g_state.active = false;
    g_state.stack.clear();
}

// frame_changed: a page was pushed, popped or turned, so the hand jumps to the new selection rather
// than gliding there from where the last page had it.
void RebuildActiveView(bool frame_changed = false) {
    if (!g_state.active || g_state.stack.empty()) {
        EndSession(SessionEnd::Closed);
        return;
    }
    ++g_state.view_generation;
    if (frame_changed) {
        g_state.snap_cursor = true;
    }

    FrameNode &top = g_state.stack.back();
    if (top.slices.empty()) {
        top.slices.push_back(BuildWrappedSlice("", g_state.layout_cfg.max_inner_width));
    }
    top.current_slice_idx = std::clamp(
        top.current_slice_idx, 0, static_cast<int32_t>(top.slices.size()) - 1);

    const bool is_final_slice =
        (top.current_slice_idx == static_cast<int32_t>(top.slices.size()) - 1);
    const bool show_choices = is_final_slice && !top.choices.empty();

    ClampChoiceScroll(top, g_state.layout_cfg.max_visible_choices);

    ActiveView &view = g_state.cached_view;
    view.context = g_state.active_ctx;
    view.current_slice = top.slices[static_cast<size_t>(top.current_slice_idx)];
    view.slice_index = top.current_slice_idx;
    view.slice_count = static_cast<int32_t>(top.slices.size());
    view.show_choices_on_current_slice = show_choices;

    if (show_choices) {
        view.choices = top.choices;
        view.formatted_choice_labels = top.formatted_choice_labels;
        view.selected_choice = top.selected_choice;
        view.choice_scroll_offset = top.choice_scroll_offset;
        view.layout = ComputeExpandedLayout(
            view.current_slice, view.formatted_choice_labels, g_state.active_ctx, g_state.layout_cfg);
    } else {
        view.choices.clear();
        view.formatted_choice_labels.clear();
        view.selected_choice = 0;
        view.choice_scroll_offset = 0;
        view.layout = ComputeExpandedLayout(
            view.current_slice, {}, g_state.active_ctx, g_state.layout_cfg);
    }
}

FrameNode MakeFrameNode(std::string_view             raw_or_key,
                        std::span<const std::string> format_args,
                        std::vector<ChoiceItem>      choices,
                        bool                         resolve_as_key) {
    const std::string              base_text = resolve_as_key ? ResolveKey(raw_or_key) : std::string(raw_or_key);
    const std::string              expanded_text = SubstituteTokens(base_text, format_args, &g_state.tokens);
    const std::vector<std::string> raw_pages = SplitRawPages(expanded_text);

    FrameNode node;
    node.slices.reserve(raw_pages.size());
    for (const std::string &page_str : raw_pages) {
        node.slices.push_back(BuildWrappedSlice(page_str, g_state.layout_cfg.max_inner_width));
    }
    if (node.slices.empty()) {
        node.slices.push_back(BuildWrappedSlice("", g_state.layout_cfg.max_inner_width));
    }

    node.choices = std::move(choices);
    node.formatted_choice_labels.reserve(node.choices.size());
    for (const ChoiceItem &item : node.choices) {
        std::string label = !item.label_key.empty() ? ResolveKey(item.label_key) : item.fallback_label;
        if (label == item.label_key && !item.fallback_label.empty()) {
            label = item.fallback_label;
        }
        node.formatted_choice_labels.push_back(
            SubstituteTokens(label, item.label_args, &g_state.tokens));
    }
    node.selected_choice = FindFirstEnabledChoice(node.choices);
    node.choice_scroll_offset = 0;
    return node;
}

// The menu an NPC opens on: "Talk", what the providers added, "Never mind".
FrameNode MakeHubNode(const NpcContext &ctx, std::vector<ChoiceItem> injected_choices) {
    std::vector<ChoiceItem> hub_choices;
    hub_choices.reserve(injected_choices.size() + 2);

    ChoiceItem talk_item;
    talk_item.id = "builtin.talk";
    talk_item.label_key = "dialog.common.talk";
    talk_item.fallback_label = "Talk";
    talk_item.color = ChoiceColor::White;
    talk_item.on_select = [](ISessionController &ctrl) {
        ctrl.PlayOriginalMessage();
    };
    hub_choices.push_back(std::move(talk_item));

    for (ChoiceItem &item : injected_choices) {
        hub_choices.push_back(std::move(item));
    }

    ChoiceItem leave_item;
    leave_item.id = "builtin.leave";
    leave_item.label_key = "dialog.common.leave";
    leave_item.fallback_label = "Never mind";
    leave_item.color = ChoiceColor::White;
    leave_item.on_select = [](ISessionController &ctrl) {
        ctrl.Close();
    };
    hub_choices.push_back(std::move(leave_item));

    // Only the choices, as retail's own choice prompts are, unless the NPC has a greeting above them.
    FrameNode node = ctx.hub_greeting_key.empty()
                         ? MakeFrameNode("", {}, std::move(hub_choices), false)
                         : MakeFrameNode(ctx.hub_greeting_key, {}, std::move(hub_choices), true);
    node.hub = true;
    return node;
}

void RefreshLabels(const ActiveView &view) {
    if (g_state.drawn_generation == g_state.view_generation) {
        return;
    }
    g_state.drawn_generation = g_state.view_generation;
    g_state.body_labels.clear();
    for (const std::string &line : view.current_slice.wrapped_markup_lines) {
        g_state.body_labels.push_back(MakeLabel(line));
    }
    g_state.choice_labels.clear();
    for (const std::string &label : view.formatted_choice_labels) {
        g_state.choice_labels.push_back(MakeLabel(label));
    }
}

void DefaultRenderSession(const ActiveView &view) {
    constexpr int alpha = 0x80;
    Widgets      &widgets = GetWidgets();
    RefreshLabels(view);

    const ComputedLayout &lay = view.layout;
    const ui::Rect        box_rect(
        static_cast<int>(std::lround(lay.box_x)),
        static_cast<int>(std::lround(lay.box_y)),
        static_cast<int>(std::lround(lay.box_w)),
        static_cast<int>(std::lround(lay.box_h)));

    MenuTextureReload(ui::detail::kMessageTextureBlock);
    ui::TrackerWindow::Draw(box_rect, alpha, ui::WindowPreset::BlueStone);

    MenuTextureReload(ui::detail::kMessageTextureBlock);
    const int text_x = static_cast<int>(std::lround(lay.text_x));
    const int text_y = static_cast<int>(std::lround(lay.text_y));
    const int stride = static_cast<int>(std::lround(g_state.layout_cfg.body_line_stride));

    for (size_t i = 0; i < g_state.body_labels.size(); ++i) {
        g_state.body_labels[i]->SetColor(ui::ColorWhite);
        g_state.body_labels[i]->Draw(text_x, text_y + static_cast<int>(i) * stride, alpha);
    }

    if (view.show_choices_on_current_slice && !view.choices.empty()) {
        if (lay.has_divider) {
            ui::Divider::DrawHorizontal(
                text_x,
                text_x + static_cast<int>(std::lround(lay.text_w)),
                static_cast<int>(std::lround(lay.divider_y)),
                alpha);
        }

        const int choices_x = static_cast<int>(std::lround(lay.choices_x + g_state.layout_cfg.choice_indent_width));
        const int choices_y = static_cast<int>(std::lround(lay.choices_y));
        const int row_stride = static_cast<int>(std::lround(g_state.layout_cfg.choice_row_stride));
        const int row_w = static_cast<int>(std::lround(
            lay.choices_w - g_state.layout_cfg.choice_indent_width -
            (lay.needs_scrollbar ? g_state.layout_cfg.scrollbar_gutter : 0.0f)));

        const int total_choices = static_cast<int>(view.choices.size());
        const int vis_count = std::min(lay.visible_choice_count, total_choices - view.choice_scroll_offset);

        MenuTextureReload(ui::detail::kMessageTextureBlock);
        for (int s = 0; s < vis_count; ++s) {
            const int         idx = view.choice_scroll_offset + s;
            const ChoiceItem &item = view.choices[static_cast<size_t>(idx)];
            ui::Label        &label = *g_state.choice_labels[static_cast<size_t>(idx)];
            const u32         color = (idx == view.selected_choice && item.enabled)
                                          ? ui::ColorYellow
                                          : MapChoiceColor(item.color, item.enabled);
            const int         row_y = choices_y + s * row_stride;
            label.SetColor(color);
            label.Draw(choices_x, row_y, item.enabled ? alpha : (alpha >> 1));
        }

        if (lay.needs_scrollbar) {
            widgets.scrollbar.Draw(
                box_rect.Right() - 16,
                choices_y,
                static_cast<int>(std::lround(lay.choices_h)),
                total_choices,
                lay.visible_choice_count,
                view.choice_scroll_offset,
                alpha);
        }

        const int sel_slot = view.selected_choice - view.choice_scroll_offset;
        if (sel_slot >= 0 && sel_slot < vis_count) {
            const int      sel_y = choices_y + sel_slot * row_stride;
            const ui::Rect sel_rect(choices_x - 4, sel_y - 1, std::max(40, row_w + 8), row_stride);
            widgets.brackets.Draw(sel_rect, alpha);
            const auto hand_x = static_cast<float>(choices_x - 28);
            const auto hand_y = static_cast<float>(sel_y - 2);
            if (g_state.snap_cursor) {
                widgets.cursor.SnapTo(hand_x, hand_y);
                g_state.snap_cursor = false;
            } else {
                widgets.cursor.SetTarget(hand_x, hand_y);
            }
            widgets.cursor.Draw(alpha);
        }
    } else if (view.slice_index + 1 < view.slice_count && ((g_state.blink_counter >> 4) & 1u) != 0u) {
        // Flashing next-page arrow matching ClsMes::DrawMesWin's syst04 indicator.
        CTexture *arrow_tex = draw2d::Get().find_texture("syst04");
        if (arrow_tex != nullptr) {
            const int arrow_x = box_rect.CenterX() - 8;
            const int arrow_y = box_rect.Bottom() - 14;
            set2DSprite(GetVif1Packet(), arrow_tex,
                        CRect_i_(arrow_x, arrow_y, 16, 16),
                        CRect_i_(0, 0, 16, 16), alpha);
        }
    }
}

class SessionControllerImpl final : public ISessionController {
public:
    [[nodiscard]] const NpcContext &GetContext() const noexcept override {
        return g_state.active_ctx;
    }

    void PushMessageKey(std::string_view         text_key,
                        std::vector<std::string> format_args) override {
        g_state.stack.push_back(MakeFrameNode(text_key, format_args, {}, true));
        RebuildActiveView(true);
    }

    void PushRawMessage(std::string_view         raw_utf8_text,
                        std::vector<std::string> format_args) override {
        g_state.stack.push_back(MakeFrameNode(raw_utf8_text, format_args, {}, false));
        RebuildActiveView(true);
    }

    void PushClosingMessage(std::string_view         raw_utf8_text,
                            std::vector<std::string> format_args) override {
        FrameNode node = MakeFrameNode(raw_utf8_text, format_args, {}, false);
        node.close_when_read = true;
        g_state.stack.push_back(std::move(node));
        RebuildActiveView(true);
    }

    void PushChoicePage(std::string_view         prompt_key,
                        std::vector<ChoiceItem>  choices,
                        std::vector<std::string> format_args) override {
        g_state.stack.push_back(MakeFrameNode(prompt_key, format_args, std::move(choices), true));
        RebuildActiveView(true);
    }

    void PushRawChoicePage(std::string_view         raw_utf8_prompt,
                           std::vector<ChoiceItem>  choices,
                           std::vector<std::string> format_args) override {
        g_state.stack.push_back(MakeFrameNode(raw_utf8_prompt, format_args, std::move(choices), false));
        RebuildActiveView(true);
    }

    void RefreshHub() override {
        if (g_state.stack.empty() || !g_state.stack.front().hub) {
            return;
        }
        std::vector<ChoiceItem> injected;
        QueryOptionProviders(g_state.active_ctx, injected);
        FrameNode hub = MakeHubNode(g_state.active_ctx, std::move(injected));

        // The cursor stays on the choice it was on, where that choice is still there.
        const FrameNode &old = g_state.stack.front();
        if (old.selected_choice >= 0 && std::cmp_less(old.selected_choice, old.choices.size())) {
            const std::string &id = old.choices[static_cast<size_t>(old.selected_choice)].id;
            for (size_t i = 0; i < hub.choices.size(); ++i) {
                if (hub.choices[i].id == id && hub.choices[i].enabled) {
                    hub.selected_choice = static_cast<int32_t>(i);
                    break;
                }
            }
        }
        g_state.stack.front() = std::move(hub);
        RebuildActiveView();
    }

    void PlayOriginalMessage() override {
        if (g_state.active_ctx.host_plays_original) {
            EndSession(SessionEnd::PlayOriginal);
            return;
        }
        g_state.stack.push_back(MakeFrameNode(g_state.original_message, {}, {}, false));
        RebuildActiveView(true);
    }

    void PopPage() override {
        if (g_state.stack.size() > 1) {
            g_state.stack.pop_back();
            RebuildActiveView(true);
        } else {
            Close();
        }
    }

    void Close() noexcept override {
        EndSession(SessionEnd::Closed);
    }
};

} // namespace

void Init() {
    g_state.active = false;
    g_state.last_end = SessionEnd::None;
    g_state.stack.clear();
    g_state.blink_counter = 0;
}

void Shutdown() {
    ForceCloseSession();
    g_state.last_end = SessionEnd::None;
    g_state.providers.clear();
    g_state.text_measurer = nullptr;
    g_state.external_localize = nullptr;
    g_state.render_callback = nullptr;
    g_state.body_labels.clear();
    g_state.choice_labels.clear();
    g_state.measure_label.reset();
    g_state.widgets.reset();
}

void SetExternalLocalizeLookup(ExternalLocalizeFn lookup_fn) {
    g_state.external_localize = std::move(lookup_fn);
}

void SetTokenContext(const TokenContext &token_ctx) {
    g_state.tokens = token_ctx;
}

const TokenContext &GetTokenContext() noexcept {
    return g_state.tokens;
}

std::string ResolveKey(std::string_view key_or_raw) {
    if (g_state.external_localize) {
        const std::string_view ext = g_state.external_localize(key_or_raw);
        if (!ext.empty() && ext != key_or_raw) {
            return std::string(ext);
        }
    }
    // The current language's lang/<language>.json, as for the port's other text; else the English.
    std::string_view english = key_or_raw;
    for (const auto &[key, text] : kEnglish) {
        if (key_or_raw == key) {
            english = text;
            break;
        }
    }
    return LocalizeText(key_or_raw, english);
}

std::vector<std::pair<std::string, std::string>> Strings() {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(kEnglish.size());
    for (const auto &[key, text] : kEnglish) {
        out.emplace_back(key, text);
    }
    return out;
}

std::string SubstituteTokens(std::string_view             text,
                             std::span<const std::string> positional_args,
                             const TokenContext          *token_ctx) {
    const TokenContext &ctx = token_ctx != nullptr ? *token_ctx : g_state.tokens;
    std::string         out;
    out.reserve(text.size() + 32);

    for (size_t i = 0; i < text.size();) {
        if (text[i] == '%' && i + 1 < text.size() &&
            text[i + 1] >= '1' && text[i + 1] <= '9') {
            const auto arg_idx = static_cast<size_t>(text[i + 1] - '1');
            if (arg_idx < positional_args.size()) {
                out.append(positional_args[arg_idx]);
            } else {
                out.append(text.substr(i, 2));
            }
            i += 2;
            continue;
        }

        if (text[i] == '{' && i + 1 < text.size() && text[i + 1] == '{') {
            out.append("{{");
            i += 2;
            continue;
        }

        if (text[i] == '{') {
            const size_t close_pos = text.find('}', i + 1);
            if (close_pos != std::string_view::npos) {
                const std::string_view tag = text.substr(i + 1, close_pos - i - 1);

                if (StartsWithIgnoreCase(tag, "name") && tag.size() == 5 &&
                    tag[4] >= '1' && tag[4] <= '6') {
                    const auto idx = static_cast<size_t>(tag[4] - '1');
                    if (!ctx.party_names[idx].empty()) {
                        out.append(ctx.party_names[idx]);
                        i = close_pos + 1;
                        continue;
                    }
                }

                if (StartsWithIgnoreCase(tag, "insert")) {
                    const std::string_view num_part = tag.substr(6);
                    int                    idx = 0;
                    if (num_part == "10") {
                        idx = 10;
                    } else if (num_part.size() == 1 && num_part[0] >= '1' && num_part[0] <= '9') {
                        idx = num_part[0] - '0';
                    }
                    if (idx >= 1 && idx <= 10 && !ctx.inserts[static_cast<size_t>(idx - 1)].empty()) {
                        out.append(ctx.inserts[static_cast<size_t>(idx - 1)]);
                        i = close_pos + 1;
                        continue;
                    }
                }

                if (EqualsIgnoreCase(tag, "value")) {
                    if (!ctx.values[0].empty()) {
                        out.append(ctx.values[0]);
                        i = close_pos + 1;
                        continue;
                    }
                } else if (StartsWithIgnoreCase(tag, "value") && tag.size() == 6 &&
                           tag[5] >= '1' && tag[5] <= '8') {
                    const auto idx = static_cast<size_t>(tag[5] - '1');
                    if (!ctx.values[idx].empty()) {
                        out.append(ctx.values[idx]);
                        i = close_pos + 1;
                        continue;
                    }
                }
            }
        }

        out.push_back(text[i]);
        ++i;
    }

    return out;
}

std::string StripControlCodes(std::string_view markup_text) {
    std::string out;
    out.reserve(markup_text.size());

    for (size_t i = 0; i < markup_text.size();) {
        if (markup_text[i] == '{' && i + 1 < markup_text.size() && markup_text[i + 1] == '{') {
            out.push_back('{');
            i += 2;
            continue;
        }
        if (markup_text[i] == '{') {
            const size_t close_pos = markup_text.find('}', i + 1);
            if (close_pos != std::string_view::npos) {
                const std::string_view tag = markup_text.substr(i + 1, close_pos - i - 1);
                if (!IsZeroWidthControlTag(tag)) {
                    out.push_back('@');
                }
                i = close_pos + 1;
                continue;
            }
        }
        out.push_back(markup_text[i]);
        ++i;
    }

    return out;
}

std::vector<std::string> SplitRawPages(std::string_view markup_text) {
    std::vector<std::string> pages;
    std::string              current;

    for (size_t i = 0; i < markup_text.size();) {
        if (markup_text[i] == '{' && i + 1 < markup_text.size() && markup_text[i + 1] == '{') {
            current.append("{{");
            i += 2;
            continue;
        }
        if (markup_text[i] == '{') {
            const size_t close_pos = markup_text.find('}', i + 1);
            if (close_pos != std::string_view::npos) {
                const std::string_view tag = markup_text.substr(i + 1, close_pos - i - 1);
                if (EqualsIgnoreCase(tag, "page")) {
                    pages.push_back(std::move(current));
                    current.clear();
                    i = close_pos + 1;
                    if (i < markup_text.size() && markup_text[i] == '\n') {
                        ++i;
                    }
                    continue;
                }
            }
        }
        current.push_back(markup_text[i]);
        ++i;
    }

    pages.push_back(std::move(current));
    return pages;
}

void SetTextMeasurer(MeasureTextWidthFn measurer_fn) {
    g_state.text_measurer = std::move(measurer_fn);
}

void SetLayoutConfig(const LayoutConfig &cfg) {
    g_state.layout_cfg = cfg;
}

const LayoutConfig &GetLayoutConfig() noexcept {
    return g_state.layout_cfg;
}

float MeasureMarkupLineWidth(std::string_view markup_line) {
    if (!g_state.text_measurer) {
        return MeasureWithGameFont(markup_line);
    }

    // A custom measurer sees only text: each tag that draws something counts as one glyph cell.
    float       total_width = 0.0f;
    std::string visible_run;

    auto flush_run = [&] {
        if (!visible_run.empty()) {
            total_width += g_state.text_measurer(visible_run);
            visible_run.clear();
        }
    };

    for (size_t i = 0; i < markup_line.size();) {
        if (markup_line[i] == '{' && i + 1 < markup_line.size() && markup_line[i + 1] == '{') {
            visible_run.push_back('{');
            i += 2;
            continue;
        }
        if (markup_line[i] == '{') {
            const size_t close_pos = markup_line.find('}', i + 1);
            if (close_pos != std::string_view::npos) {
                const std::string_view tag = markup_line.substr(i + 1, close_pos - i - 1);
                flush_run();
                if (!IsZeroWidthControlTag(tag)) {
                    total_width += kGlyphCellWidthPx;
                }
                i = close_pos + 1;
                continue;
            }
        }
        visible_run.push_back(markup_line[i]);
        ++i;
    }
    flush_run();
    return total_width;
}

PageSlice BuildWrappedSlice(std::string_view single_page_markup, float max_inner_width) {
    PageSlice slice;
    slice.raw_text = std::string(single_page_markup);

    const float limit = std::max(max_inner_width, 64.0f);

    size_t para_start = 0;
    while (para_start <= single_page_markup.size()) {
        const size_t           nl_pos = single_page_markup.find('\n', para_start);
        const std::string_view paragraph =
            (nl_pos == std::string_view::npos)
                ? single_page_markup.substr(para_start)
                : single_page_markup.substr(para_start, nl_pos - para_start);

        if (paragraph.empty()) {
            slice.wrapped_markup_lines.emplace_back("");
            slice.wrapped_visible_lines.emplace_back("");
        } else if (MeasureMarkupLineWidth(paragraph) <= limit) {
            slice.wrapped_markup_lines.emplace_back(paragraph);
            slice.wrapped_visible_lines.push_back(StripControlCodes(paragraph));
        } else {
            std::string current_line;
            size_t      i = 0;
            while (i < paragraph.size()) {
                while (i < paragraph.size() && paragraph[i] == ' ' && current_line.empty()) {
                    ++i;
                }
                if (i >= paragraph.size()) {
                    break;
                }

                const size_t word_start = i;
                while (i < paragraph.size() && paragraph[i] != ' ') {
                    if (paragraph[i] == '{') {
                        const size_t close_pos = paragraph.find('}', i + 1);
                        if (close_pos != std::string_view::npos) {
                            i = close_pos + 1;
                            continue;
                        }
                    }
                    ++i;
                }
                const std::string_view word = paragraph.substr(word_start, i - word_start);
                const std::string      candidate =
                    current_line.empty() ? std::string(word) : (current_line + " " + std::string(word));

                if (!current_line.empty() && MeasureMarkupLineWidth(candidate) > limit) {
                    slice.wrapped_markup_lines.push_back(current_line);
                    slice.wrapped_visible_lines.push_back(StripControlCodes(current_line));
                    current_line = std::string(word);
                } else {
                    current_line = candidate;
                }

                if (i < paragraph.size() && paragraph[i] == ' ') {
                    ++i;
                }
            }
            if (!current_line.empty()) {
                slice.wrapped_markup_lines.push_back(current_line);
                slice.wrapped_visible_lines.push_back(StripControlCodes(current_line));
            }
        }

        if (nl_pos == std::string_view::npos) {
            break;
        }
        para_start = nl_pos + 1;
    }

    if (slice.wrapped_markup_lines.empty()) {
        slice.wrapped_markup_lines.emplace_back("");
        slice.wrapped_visible_lines.emplace_back("");
    }
    return slice;
}

ComputedLayout ComputeExpandedLayout(const PageSlice             &slice,
                                     std::span<const std::string> formatted_choice_labels,
                                     const NpcContext            &ctx,
                                     const LayoutConfig          &cfg) {
    ComputedLayout out;

    float widest_body = 0.0f;
    for (const std::string &line : slice.wrapped_markup_lines) {
        widest_body = std::max(widest_body, MeasureMarkupLineWidth(line));
    }

    float widest_choice = 0.0f;
    for (const std::string &choice_label : formatted_choice_labels) {
        widest_choice = std::max(widest_choice, MeasureMarkupLineWidth(choice_label));
    }

    const auto    total_choices = static_cast<int32_t>(formatted_choice_labels.size());
    const int32_t visible_choices = std::min(total_choices, std::max(1, cfg.max_visible_choices));
    const bool    needs_scroll = total_choices > cfg.max_visible_choices;

    float choice_block_w = 0.0f;
    if (total_choices > 0) {
        choice_block_w = cfg.choice_indent_width + widest_choice +
                         (needs_scroll ? cfg.scrollbar_gutter : 0.0f);
    }

    const float inner_w = std::max(widest_body, choice_block_w);
    out.box_w = std::clamp(inner_w + cfg.padding_x * 2.0f, cfg.min_box_width, cfg.max_box_width);

    const size_t body_line_count = std::max<size_t>(1, slice.wrapped_markup_lines.size());
    const float  body_h = static_cast<float>(body_line_count) * cfg.body_line_stride;

    float choices_h = 0.0f;
    float divider_h = 0.0f;
    if (total_choices > 0) {
        out.has_divider = true;
        divider_h = cfg.divider_total_gap;
        choices_h = static_cast<float>(visible_choices) * cfg.choice_row_stride;
    }

    out.box_h = cfg.padding_y * 2.0f + body_h + divider_h + choices_h;

    if (ctx.has_anchor) {
        out.box_x = std::round(ctx.anchor_x - out.box_w * 0.5f);
        out.box_y = std::round(ctx.anchor_y - out.box_h - cfg.tail_height);
    } else {
        out.box_x = std::round((640.0f - out.box_w) * 0.5f);
        out.box_y = std::round(cfg.safe_bottom - out.box_h - 12.0f);
    }

    out.box_x = std::clamp(out.box_x, cfg.safe_left, std::max(cfg.safe_left, cfg.safe_right - out.box_w));
    out.box_y = std::clamp(out.box_y, cfg.safe_top, std::max(cfg.safe_top, cfg.safe_bottom - out.box_h));

    out.text_x = out.box_x + cfg.padding_x;
    out.text_y = out.box_y + cfg.padding_y;
    out.text_w = out.box_w - cfg.padding_x * 2.0f;
    out.text_h = body_h;

    if (total_choices > 0) {
        out.divider_y = out.text_y + out.text_h + (cfg.divider_total_gap * 0.5f);
        out.choices_x = out.text_x;
        out.choices_y = out.text_y + out.text_h + divider_h;
        out.choices_w = out.text_w;
        out.choices_h = choices_h;
        out.visible_choice_count = visible_choices;
        out.needs_scrollbar = needs_scroll;
    }

    if (ctx.bubble == BubbleKind::SpeechTail && ctx.has_anchor) {
        out.has_tail = true;
        out.tail_x = std::clamp(ctx.anchor_x, out.box_x + 18.0f, out.box_x + out.box_w - 18.0f);
        out.tail_y = out.box_y + out.box_h + cfg.tail_height;
    }

    return out;
}

void RegisterOptionProvider(OptionProviderFn provider) {
    if (provider) {
        g_state.providers.push_back(std::move(provider));
    }
}

void ClearOptionProviders() {
    g_state.providers.clear();
}

void QueryOptionProviders(const NpcContext &ctx, std::vector<ChoiceItem> &out_choices) {
    for (const OptionProviderFn &provider : g_state.providers) {
        provider(ctx, out_choices);
    }
}

void SetRenderCallback(RenderSessionFn render_fn) {
    g_state.render_callback = std::move(render_fn);
}

void SetInterceptAllNpcs(bool intercept_all) noexcept {
    g_state.intercept_all_npcs = intercept_all;
}

bool GetInterceptAllNpcs() noexcept {
    return g_state.intercept_all_npcs;
}

void UpdateAnchor(float anchor_x, float anchor_y, bool has_anchor) {
    g_state.active_ctx.has_anchor = has_anchor;
    g_state.active_ctx.anchor_x = anchor_x;
    g_state.active_ctx.anchor_y = anchor_y;
    if (g_state.active) {
        RebuildActiveView();
    }
}

namespace {

void BeginSession(const NpcContext &ctx, std::string_view original_message) {
    g_state.active = true;
    g_state.last_end = SessionEnd::None;
    g_state.active_ctx = ctx;
    g_state.original_message = std::string(original_message);
    g_state.stack.clear();
    g_state.blink_counter = 0;
}

} // namespace

bool TryInterceptOpen(const NpcContext &ctx, std::string_view decoded_original_message) {
    std::vector<ChoiceItem> injected_choices;
    QueryOptionProviders(ctx, injected_choices);

    if (injected_choices.empty() && (!g_state.intercept_all_npcs || ctx.host_plays_original)) {
        return false;
    }

    BeginSession(ctx, decoded_original_message);

    if (injected_choices.empty()) {
        g_state.stack.push_back(MakeFrameNode(g_state.original_message, {}, {}, false));
        RebuildActiveView(true);
        return true;
    }

    g_state.stack.push_back(MakeHubNode(ctx, std::move(injected_choices)));
    RebuildActiveView(true);
    return true;
}

void OpenCustomSession(const NpcContext       &ctx,
                       std::string_view        prompt_raw_or_key,
                       std::vector<ChoiceItem> choices,
                       bool                    resolve_prompt_as_key) {
    BeginSession(ctx, prompt_raw_or_key);
    g_state.stack.push_back(MakeFrameNode(prompt_raw_or_key, {}, std::move(choices), resolve_prompt_as_key));
    RebuildActiveView(true);
}

bool IsSessionActive() noexcept {
    return g_state.active;
}

const ActiveView *GetActiveView() noexcept {
    if (!g_state.active) {
        return nullptr;
    }
    return &g_state.cached_view;
}

SessionEnd TakeSessionEnd() noexcept {
    return std::exchange(g_state.last_end, SessionEnd::None);
}

void StepSession() noexcept {
    if (!g_state.active) {
        return;
    }
    ++g_state.blink_counter;
    if (g_state.widgets) {
        g_state.widgets->scrollbar.Update(false);
    }
}

void DrawSession() {
    if (!g_state.active) {
        return;
    }
    if (g_state.render_callback) {
        g_state.render_callback(g_state.cached_view);
    } else {
        DefaultRenderSession(g_state.cached_view);
    }
}

void ForceCloseSession() noexcept {
    EndSession(SessionEnd::Closed);
}

bool InputMoveUp() {
    if (!g_state.active || g_state.stack.empty()) {
        return false;
    }
    FrameNode &top = g_state.stack.back();
    if (!g_state.cached_view.show_choices_on_current_slice || top.choices.empty()) {
        return false;
    }

    const auto count = static_cast<int32_t>(top.choices.size());
    for (int32_t step = 1; step <= count; ++step) {
        const int32_t idx = (top.selected_choice - step + count) % count;
        if (top.choices[static_cast<size_t>(idx)].enabled) {
            if (idx != top.selected_choice) {
                top.selected_choice = idx;
                NoteSelectionMoved();
                RebuildActiveView();
                return true;
            }
            break;
        }
    }
    return false;
}

bool InputMoveDown() {
    if (!g_state.active || g_state.stack.empty()) {
        return false;
    }
    FrameNode &top = g_state.stack.back();
    if (!g_state.cached_view.show_choices_on_current_slice || top.choices.empty()) {
        return false;
    }

    const auto count = static_cast<int32_t>(top.choices.size());
    for (int32_t step = 1; step <= count; ++step) {
        const int32_t idx = (top.selected_choice + step) % count;
        if (top.choices[static_cast<size_t>(idx)].enabled) {
            if (idx != top.selected_choice) {
                top.selected_choice = idx;
                NoteSelectionMoved();
                RebuildActiveView();
                return true;
            }
            break;
        }
    }
    return false;
}

bool InputHoverChoice(int32_t visible_row_index) {
    if (!g_state.active || g_state.stack.empty() || !g_state.cached_view.show_choices_on_current_slice) {
        return false;
    }
    FrameNode    &top = g_state.stack.back();
    const int32_t target = top.choice_scroll_offset + visible_row_index;
    if (target < 0 || std::cmp_greater_equal(target, top.choices.size())) {
        return false;
    }
    if (!top.choices[static_cast<size_t>(target)].enabled || target == top.selected_choice) {
        return false;
    }
    top.selected_choice = target;
    NoteSelectionMoved();
    RebuildActiveView();
    return true;
}

int32_t HitTestChoiceRow(float px, float py) noexcept {
    if (!g_state.active || !g_state.cached_view.show_choices_on_current_slice) {
        return -1;
    }
    const ComputedLayout &lay = g_state.cached_view.layout;
    if (px < lay.choices_x || px > lay.choices_x + lay.choices_w ||
        py < lay.choices_y || py >= lay.choices_y + lay.choices_h) {
        return -1;
    }
    const auto slot = static_cast<int32_t>((py - lay.choices_y) / g_state.layout_cfg.choice_row_stride);
    if (slot < 0 || slot >= lay.visible_choice_count) {
        return -1;
    }
    return slot;
}

bool InputScrollWheel(int32_t wheel_notches) {
    if (wheel_notches == 0 || !g_state.active || g_state.stack.empty() ||
        !g_state.cached_view.show_choices_on_current_slice) {
        return false;
    }
    const int64_t notch_count = std::abs(static_cast<int64_t>(wheel_notches));
    const auto   &choices = g_state.stack.back().choices;
    const size_t  enabled_count = static_cast<size_t>(std::ranges::count(choices, true, &ChoiceItem::enabled));
    if (enabled_count <= 1) {
        return false;
    }
    const auto   remainder = static_cast<size_t>(notch_count % static_cast<int64_t>(enabled_count));
    const size_t steps = (std::cmp_greater_equal(notch_count, enabled_count) ? enabled_count : 0) + remainder;
    bool         moved = false;
    for (size_t i = 0; i < steps; ++i) {
        if (wheel_notches > 0) {
            moved |= InputMoveUp();
        } else {
            moved |= InputMoveDown();
        }
    }
    return moved;
}

bool InputConfirm() {
    if (!g_state.active || g_state.stack.empty()) {
        return false;
    }

    FrameNode    &top = g_state.stack.back();
    const int32_t last_slice = static_cast<int32_t>(top.slices.size()) - 1;

    if (top.current_slice_idx < last_slice) {
        ++top.current_slice_idx;
        RebuildActiveView(true);
        return true;
    }

    if (top.choices.empty()) {
        if (!top.close_when_read && g_state.stack.size() > 1) {
            g_state.stack.pop_back();
            RebuildActiveView(true);
        } else {
            ForceCloseSession();
        }
        return true;
    }

    if (top.selected_choice < 0 || std::cmp_greater_equal(top.selected_choice, top.choices.size())) {
        return false;
    }

    const ChoiceItem chosen = top.choices[static_cast<size_t>(top.selected_choice)];
    if (!chosen.enabled) {
        return false;
    }

    if (chosen.on_select) {
        SessionControllerImpl controller;
        chosen.on_select(controller);
    } else {
        ForceCloseSession();
    }
    return true;
}

bool InputCancel() {
    if (!g_state.active || g_state.stack.empty()) {
        return false;
    }

    if (g_state.stack.size() > 1) {
        g_state.stack.pop_back();
        RebuildActiveView(true);
        return true;
    }

    ForceCloseSession();
    return true;
}

} // namespace dc::dialog
