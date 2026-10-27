#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "dialog/framework.hpp"
#include "mainselect.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::dialog;

namespace {

// Ten units a byte, so widths are easy to reckon; the game's font is not loaded in a test.
float TenPerByte(std::string_view text) {
    return 10.0f * static_cast<float>(text.size());
}

class DialogFramework : public ::testing::Test {
protected:
    void SetUp() override {
        LanguageCode = 2; // en_gb: no language file, so the built-in English.
        dialog::Shutdown();
        dialog::Init();
        dialog::SetTextMeasurer(TenPerByte);
        dialog::SetLayoutConfig(dialog::LayoutConfig{});
        dialog::SetTokenContext(dialog::TokenContext{});
        dialog::SetInterceptAllNpcs(false);
    }

    void TearDown() override { dialog::Shutdown(); }

    // A villager context as the town's conversations make one.
    static dialog::NpcContext Villager() {
        dialog::NpcContext ctx;
        ctx.map_no = 0;
        ctx.npc_id = 3;
        ctx.mes_id = 12;
        ctx.host_plays_original = true;
        return ctx;
    }

    static dialog::ChoiceItem Item(std::string label, bool enabled = true) {
        dialog::ChoiceItem item;
        item.id = label;
        item.fallback_label = std::move(label);
        item.enabled = enabled;
        return item;
    }

    static const dialog::ActiveView &View() {
        const dialog::ActiveView *view = dialog::GetActiveView();
        EXPECT_NE(view, nullptr);
        static const dialog::ActiveView none;
        return view != nullptr ? *view : none;
    }
};

TEST_F(DialogFramework, SplitsPagesOnPageTags) {
    EXPECT_EQ(dialog::SplitRawPages("One{page}\nTwo{page}Three"),
              (std::vector<std::string>{"One", "Two", "Three"}));
    // "{{" is a brace, not the start of a tag.
    EXPECT_EQ(dialog::SplitRawPages("{{page}"), (std::vector<std::string>{"{{page}"}));
}

TEST_F(DialogFramework, SubstitutesArgumentsAndLeavesGameTokens) {
    const std::vector<std::string> args = {"A", "B"};
    EXPECT_EQ(dialog::SubstituteTokens("Hi %1, %2 and %3", args), "Hi A, B and %3");
    // The game fills these in itself: the player can rename Toan.
    EXPECT_EQ(dialog::SubstituteTokens("{name1} has {value}"), "{name1} has {value}");

    dialog::TokenContext tokens;
    tokens.party_names[0] = "Rua";
    EXPECT_EQ(dialog::SubstituteTokens("{name1} has {value}", {}, &tokens), "Rua has {value}");
    EXPECT_EQ(dialog::SubstituteTokens("{{name1}", {}, &tokens), "{{name1}");
}

TEST_F(DialogFramework, StripsColoursAndMarksIcons) {
    EXPECT_EQ(dialog::StripControlCodes("{cyan}Hi{/color} {cross}{{"), "Hi @{");
}

TEST_F(DialogFramework, MeasuresTagsAsZeroWidthOrOneCell) {
    // A custom measurer sees the text; an icon counts as one 14-unit cell, a colour as nothing.
    EXPECT_FLOAT_EQ(dialog::MeasureMarkupLineWidth("{cyan}ab{/color}{cross}"), 34.0f);
}

TEST_F(DialogFramework, WrapsAtWordsAndKeepsBlankLines) {
    const dialog::PageSlice slice = dialog::BuildWrappedSlice("aaa bbb ccc", 70.0f);
    EXPECT_EQ(slice.wrapped_markup_lines, (std::vector<std::string>{"aaa bbb", "ccc"}));

    const dialog::PageSlice lines = dialog::BuildWrappedSlice("a\n\nb", 440.0f);
    EXPECT_EQ(lines.wrapped_markup_lines, (std::vector<std::string>{"a", "", "b"}));
}

TEST_F(DialogFramework, LaysOutChoicesBelowTheTextWithAScrollbar) {
    const dialog::PageSlice  slice = dialog::BuildWrappedSlice("hello", 440.0f);
    std::vector<std::string> labels(7, "x");
    dialog::NpcContext       ctx;
    ctx.has_anchor = false;
    const dialog::ComputedLayout lay = dialog::ComputeExpandedLayout(slice, labels, ctx);

    // The widest row is a choice (indent 28 + 10 + scrollbar 14) under the minimum width.
    EXPECT_FLOAT_EQ(lay.box_w, 180.0f);
    // Padding 14 twice, one line of 20, the divider's 12 and five rows of 22.
    EXPECT_FLOAT_EQ(lay.box_h, 170.0f);
    EXPECT_FLOAT_EQ(lay.box_x, 230.0f);
    EXPECT_FLOAT_EQ(lay.box_y, 266.0f);
    EXPECT_TRUE(lay.has_divider);
    EXPECT_FLOAT_EQ(lay.choices_y, 312.0f);
    EXPECT_EQ(lay.visible_choice_count, 5);
    EXPECT_TRUE(lay.needs_scrollbar);
}

TEST_F(DialogFramework, LeavesAVillagerNoProviderAddsTo) {
    EXPECT_FALSE(dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_FALSE(dialog::IsSessionActive());

    // Intercepting every NPC still leaves one whose host shows the message itself.
    dialog::SetInterceptAllNpcs(true);
    EXPECT_FALSE(dialog::TryInterceptOpen(Villager(), {}));

    dialog::NpcContext ctx = Villager();
    ctx.host_plays_original = false;
    ASSERT_TRUE(dialog::TryInterceptOpen(ctx, "Hello{page}Bye"));
    EXPECT_EQ(View().slice_count, 2);
    EXPECT_FALSE(View().show_choices_on_current_slice);
}

TEST_F(DialogFramework, OpensTheHubWithTalkAndLeaveAroundProvidedChoices) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &ctx, std::vector<dialog::ChoiceItem> &out) {
        if (ctx.npc_id == 3) {
            out.push_back(Item("Quest"));
        }
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));

    const dialog::ActiveView &view = View();
    // Only the choices: no line above them unless the NPC has a greeting.
    EXPECT_EQ(view.current_slice.wrapped_markup_lines, (std::vector<std::string>{""}));
    EXPECT_EQ(view.formatted_choice_labels, (std::vector<std::string>{"Talk", "Quest", "Never mind"}));
    EXPECT_TRUE(view.show_choices_on_current_slice);
    EXPECT_EQ(view.selected_choice, 0);
    EXPECT_EQ(view.context.npc_id, 3);
}

TEST_F(DialogFramework, AGreetingGoesAboveTheChoicesWhereTheNpcHasOne) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        out.push_back(Item("Quest"));
    });
    dialog::NpcContext ctx = Villager();
    ctx.hub_greeting_key = "dialog.hub.default_prompt";
    ASSERT_TRUE(dialog::TryInterceptOpen(ctx, {}));
    EXPECT_EQ(View().current_slice.wrapped_markup_lines, (std::vector<std::string>{"Is there something you need?"}));
}

TEST_F(DialogFramework, TalkHandsBackToTheHost) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        out.push_back(Item("Quest"));
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));

    EXPECT_TRUE(dialog::InputConfirm());
    EXPECT_FALSE(dialog::IsSessionActive());
    EXPECT_EQ(dialog::TakeSessionEnd(), dialog::SessionEnd::PlayOriginal);
    EXPECT_EQ(dialog::TakeSessionEnd(), dialog::SessionEnd::None);
}

TEST_F(DialogFramework, NeverMindAndCancelClose) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        out.push_back(Item("Quest"));
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_TRUE(dialog::InputMoveDown());
    EXPECT_TRUE(dialog::InputMoveDown());
    EXPECT_EQ(View().selected_choice, 2);
    EXPECT_TRUE(dialog::InputConfirm());
    EXPECT_EQ(dialog::TakeSessionEnd(), dialog::SessionEnd::Closed);

    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_TRUE(dialog::InputCancel());
    EXPECT_FALSE(dialog::IsSessionActive());
    EXPECT_EQ(dialog::TakeSessionEnd(), dialog::SessionEnd::Closed);
}

TEST_F(DialogFramework, PushedPagesTurnThenReturnToTheHub) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        dialog::ChoiceItem item = Item("Quest");
        item.on_select = [](dialog::ISessionController &session) {
            session.PushRawMessage("One %1{page}Two", {"more"});
        };
        out.push_back(std::move(item));
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));
    ASSERT_TRUE(dialog::InputMoveDown());
    ASSERT_TRUE(dialog::InputConfirm());

    EXPECT_EQ(View().current_slice.wrapped_markup_lines, (std::vector<std::string>{"One more"}));
    EXPECT_EQ(View().slice_count, 2);
    EXPECT_FALSE(View().show_choices_on_current_slice);

    ASSERT_TRUE(dialog::InputConfirm());
    EXPECT_EQ(View().slice_index, 1);
    EXPECT_EQ(View().current_slice.wrapped_markup_lines, (std::vector<std::string>{"Two"}));

    // The last page confirmed goes back to the hub, on the choice that opened it.
    ASSERT_TRUE(dialog::InputConfirm());
    EXPECT_TRUE(View().show_choices_on_current_slice);
    EXPECT_EQ(View().selected_choice, 1);

    // Cancel backs out of a pushed page the same way.
    ASSERT_TRUE(dialog::InputConfirm());
    ASSERT_TRUE(dialog::InputCancel());
    EXPECT_TRUE(dialog::IsSessionActive());
    EXPECT_TRUE(View().show_choices_on_current_slice);
}

TEST_F(DialogFramework, AClosingMessageEndsTheConversationOnceRead) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        dialog::ChoiceItem item = Item("Goodbye");
        item.on_select = [](dialog::ISessionController &session) { session.PushClosingMessage("See you.{page}Bye!"); };
        out.push_back(std::move(item));
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));
    ASSERT_TRUE(dialog::InputMoveDown());
    ASSERT_TRUE(dialog::InputConfirm());
    EXPECT_EQ(View().slice_count, 2);
    ASSERT_TRUE(dialog::InputConfirm());
    EXPECT_TRUE(dialog::IsSessionActive());
    ASSERT_TRUE(dialog::InputConfirm());
    EXPECT_FALSE(dialog::IsSessionActive());
    EXPECT_EQ(dialog::TakeSessionEnd(), dialog::SessionEnd::Closed);
}

TEST_F(DialogFramework, CursorSkipsDisabledChoicesAndWraps) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        out.push_back(Item("Locked", false));
        out.push_back(Item("Open"));
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));
    // Talk, Locked, Open, Never mind.
    ASSERT_TRUE(dialog::InputMoveDown());
    EXPECT_EQ(View().selected_choice, 2);
    ASSERT_TRUE(dialog::InputMoveUp());
    EXPECT_EQ(View().selected_choice, 0);
    ASSERT_TRUE(dialog::InputMoveUp());
    EXPECT_EQ(View().selected_choice, 3);
}

TEST_F(DialogFramework, WheelLargeDeltasWrapAcrossEnabledChoices) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        out.push_back(Item("Locked", false));
        out.push_back(Item("Open"));
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));

    // Talk, Open, and Never mind are enabled; the locked choice is skipped.
    EXPECT_TRUE(dialog::InputScrollWheel(3));
    EXPECT_EQ(View().selected_choice, 0);
    EXPECT_TRUE(dialog::InputScrollWheel(std::numeric_limits<std::int32_t>::min()));
    EXPECT_EQ(View().selected_choice, 3);
    EXPECT_TRUE(dialog::InputScrollWheel(std::numeric_limits<std::int32_t>::max()));
    EXPECT_EQ(View().selected_choice, 2);
}

TEST_F(DialogFramework, LongListsScrollWithTheCursor) {
    dialog::RegisterOptionProvider([](const dialog::NpcContext &, std::vector<dialog::ChoiceItem> &out) {
        for (int i = 0; i < 8; ++i) {
            out.push_back(Item("Choice " + std::to_string(i)));
        }
    });
    ASSERT_TRUE(dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_EQ(View().layout.visible_choice_count, 5);
    EXPECT_TRUE(View().layout.needs_scrollbar);

    for (int i = 0; i < 6; ++i) {
        ASSERT_TRUE(dialog::InputMoveDown());
    }
    EXPECT_EQ(View().selected_choice, 6);
    EXPECT_EQ(View().choice_scroll_offset, 2);
}

} // namespace

} // namespace dc::test
