#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "dialog_framework.hpp"
#include "mainselect.hpp"

namespace {

// Ten units a byte, so widths are easy to reckon; the game's font is not loaded in a test.
float TenPerByte(std::string_view text) {
    return 10.0f * static_cast<float>(text.size());
}

class DialogFramework : public ::testing::Test {
protected:
    void SetUp() override {
        LanguageCode = 2; // en_gb: no language file, so the built-in English.
        Dialog::Shutdown();
        Dialog::Init();
        Dialog::SetTextMeasurer(TenPerByte);
        Dialog::SetLayoutConfig(Dialog::LayoutConfig{});
        Dialog::SetTokenContext(Dialog::TokenContext{});
        Dialog::SetInterceptAllNpcs(false);
    }

    void TearDown() override { Dialog::Shutdown(); }

    // A villager context as the town's conversations make one.
    static Dialog::NpcContext Villager() {
        Dialog::NpcContext ctx;
        ctx.map_no = 0;
        ctx.npc_id = 3;
        ctx.mes_id = 12;
        ctx.host_plays_original = true;
        return ctx;
    }

    static Dialog::ChoiceItem Item(std::string label, bool enabled = true) {
        Dialog::ChoiceItem item;
        item.id = label;
        item.fallback_label = std::move(label);
        item.enabled = enabled;
        return item;
    }

    static const Dialog::ActiveView &View() {
        const Dialog::ActiveView *view = Dialog::GetActiveView();
        EXPECT_NE(view, nullptr);
        static const Dialog::ActiveView none;
        return view != nullptr ? *view : none;
    }
};

TEST_F(DialogFramework, SplitsPagesOnPageTags) {
    EXPECT_EQ(Dialog::SplitRawPages("One{page}\nTwo{page}Three"),
              (std::vector<std::string>{"One", "Two", "Three"}));
    // "{{" is a brace, not the start of a tag.
    EXPECT_EQ(Dialog::SplitRawPages("{{page}"), (std::vector<std::string>{"{{page}"}));
}

TEST_F(DialogFramework, SubstitutesArgumentsAndLeavesGameTokens) {
    const std::vector<std::string> args = {"A", "B"};
    EXPECT_EQ(Dialog::SubstituteTokens("Hi %1, %2 and %3", args), "Hi A, B and %3");
    // The game fills these in itself: the player can rename Toan.
    EXPECT_EQ(Dialog::SubstituteTokens("{name1} has {value}"), "{name1} has {value}");

    Dialog::TokenContext tokens;
    tokens.party_names[0] = "Rua";
    EXPECT_EQ(Dialog::SubstituteTokens("{name1} has {value}", {}, &tokens), "Rua has {value}");
    EXPECT_EQ(Dialog::SubstituteTokens("{{name1}", {}, &tokens), "{{name1}");
}

TEST_F(DialogFramework, StripsColoursAndMarksIcons) {
    EXPECT_EQ(Dialog::StripControlCodes("{cyan}Hi{/color} {cross}{{"), "Hi @{");
}

TEST_F(DialogFramework, MeasuresTagsAsZeroWidthOrOneCell) {
    // A custom measurer sees the text; an icon counts as one 14-unit cell, a colour as nothing.
    EXPECT_FLOAT_EQ(Dialog::MeasureMarkupLineWidth("{cyan}ab{/color}{cross}"), 34.0f);
}

TEST_F(DialogFramework, WrapsAtWordsAndKeepsBlankLines) {
    const Dialog::PageSlice slice = Dialog::BuildWrappedSlice("aaa bbb ccc", 70.0f);
    EXPECT_EQ(slice.wrapped_markup_lines, (std::vector<std::string>{"aaa bbb", "ccc"}));

    const Dialog::PageSlice lines = Dialog::BuildWrappedSlice("a\n\nb", 440.0f);
    EXPECT_EQ(lines.wrapped_markup_lines, (std::vector<std::string>{"a", "", "b"}));
}

TEST_F(DialogFramework, LaysOutChoicesBelowTheTextWithAScrollbar) {
    const Dialog::PageSlice  slice = Dialog::BuildWrappedSlice("hello", 440.0f);
    std::vector<std::string> labels(7, "x");
    Dialog::NpcContext       ctx;
    ctx.has_anchor = false;
    const Dialog::ComputedLayout lay = Dialog::ComputeExpandedLayout(slice, labels, ctx);

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
    EXPECT_FALSE(Dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_FALSE(Dialog::IsSessionActive());

    // Intercepting every NPC still leaves one whose host shows the message itself.
    Dialog::SetInterceptAllNpcs(true);
    EXPECT_FALSE(Dialog::TryInterceptOpen(Villager(), {}));

    Dialog::NpcContext ctx = Villager();
    ctx.host_plays_original = false;
    ASSERT_TRUE(Dialog::TryInterceptOpen(ctx, "Hello{page}Bye"));
    EXPECT_EQ(View().slice_count, 2);
    EXPECT_FALSE(View().show_choices_on_current_slice);
}

TEST_F(DialogFramework, OpensTheHubWithTalkAndLeaveAroundProvidedChoices) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &ctx, std::vector<Dialog::ChoiceItem> &out) {
        if (ctx.npc_id == 3) {
            out.push_back(Item("Quest"));
        }
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));

    const Dialog::ActiveView &view = View();
    // Only the choices: no line above them unless the NPC has a greeting.
    EXPECT_EQ(view.current_slice.wrapped_markup_lines, (std::vector<std::string>{""}));
    EXPECT_EQ(view.formatted_choice_labels, (std::vector<std::string>{"Talk", "Quest", "Never mind"}));
    EXPECT_TRUE(view.show_choices_on_current_slice);
    EXPECT_EQ(view.selected_choice, 0);
    EXPECT_EQ(view.context.npc_id, 3);
}

TEST_F(DialogFramework, AGreetingGoesAboveTheChoicesWhereTheNpcHasOne) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        out.push_back(Item("Quest"));
    });
    Dialog::NpcContext ctx = Villager();
    ctx.hub_greeting_key = "dialog.hub.default_prompt";
    ASSERT_TRUE(Dialog::TryInterceptOpen(ctx, {}));
    EXPECT_EQ(View().current_slice.wrapped_markup_lines, (std::vector<std::string>{"Is there something you need?"}));
}

TEST_F(DialogFramework, TalkHandsBackToTheHost) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        out.push_back(Item("Quest"));
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));

    EXPECT_TRUE(Dialog::InputConfirm());
    EXPECT_FALSE(Dialog::IsSessionActive());
    EXPECT_EQ(Dialog::TakeSessionEnd(), Dialog::SessionEnd::PlayOriginal);
    EXPECT_EQ(Dialog::TakeSessionEnd(), Dialog::SessionEnd::None);
}

TEST_F(DialogFramework, NeverMindAndCancelClose) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        out.push_back(Item("Quest"));
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_TRUE(Dialog::InputMoveDown());
    EXPECT_TRUE(Dialog::InputMoveDown());
    EXPECT_EQ(View().selected_choice, 2);
    EXPECT_TRUE(Dialog::InputConfirm());
    EXPECT_EQ(Dialog::TakeSessionEnd(), Dialog::SessionEnd::Closed);

    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_TRUE(Dialog::InputCancel());
    EXPECT_FALSE(Dialog::IsSessionActive());
    EXPECT_EQ(Dialog::TakeSessionEnd(), Dialog::SessionEnd::Closed);
}

TEST_F(DialogFramework, PushedPagesTurnThenReturnToTheHub) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        Dialog::ChoiceItem item = Item("Quest");
        item.on_select = [](Dialog::ISessionController &session) {
            session.PushRawMessage("One %1{page}Two", {"more"});
        };
        out.push_back(std::move(item));
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));
    ASSERT_TRUE(Dialog::InputMoveDown());
    ASSERT_TRUE(Dialog::InputConfirm());

    EXPECT_EQ(View().current_slice.wrapped_markup_lines, (std::vector<std::string>{"One more"}));
    EXPECT_EQ(View().slice_count, 2);
    EXPECT_FALSE(View().show_choices_on_current_slice);

    ASSERT_TRUE(Dialog::InputConfirm());
    EXPECT_EQ(View().slice_index, 1);
    EXPECT_EQ(View().current_slice.wrapped_markup_lines, (std::vector<std::string>{"Two"}));

    // The last page confirmed goes back to the hub, on the choice that opened it.
    ASSERT_TRUE(Dialog::InputConfirm());
    EXPECT_TRUE(View().show_choices_on_current_slice);
    EXPECT_EQ(View().selected_choice, 1);

    // Cancel backs out of a pushed page the same way.
    ASSERT_TRUE(Dialog::InputConfirm());
    ASSERT_TRUE(Dialog::InputCancel());
    EXPECT_TRUE(Dialog::IsSessionActive());
    EXPECT_TRUE(View().show_choices_on_current_slice);
}

TEST_F(DialogFramework, AClosingMessageEndsTheConversationOnceRead) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        Dialog::ChoiceItem item = Item("Goodbye");
        item.on_select = [](Dialog::ISessionController &session) { session.PushClosingMessage("See you.{page}Bye!"); };
        out.push_back(std::move(item));
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));
    ASSERT_TRUE(Dialog::InputMoveDown());
    ASSERT_TRUE(Dialog::InputConfirm());
    EXPECT_EQ(View().slice_count, 2);
    ASSERT_TRUE(Dialog::InputConfirm());
    EXPECT_TRUE(Dialog::IsSessionActive());
    ASSERT_TRUE(Dialog::InputConfirm());
    EXPECT_FALSE(Dialog::IsSessionActive());
    EXPECT_EQ(Dialog::TakeSessionEnd(), Dialog::SessionEnd::Closed);
}

TEST_F(DialogFramework, CursorSkipsDisabledChoicesAndWraps) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        out.push_back(Item("Locked", false));
        out.push_back(Item("Open"));
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));
    // Talk, Locked, Open, Never mind.
    ASSERT_TRUE(Dialog::InputMoveDown());
    EXPECT_EQ(View().selected_choice, 2);
    ASSERT_TRUE(Dialog::InputMoveUp());
    EXPECT_EQ(View().selected_choice, 0);
    ASSERT_TRUE(Dialog::InputMoveUp());
    EXPECT_EQ(View().selected_choice, 3);
}

TEST_F(DialogFramework, WheelLargeDeltasWrapAcrossEnabledChoices) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        out.push_back(Item("Locked", false));
        out.push_back(Item("Open"));
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));

    // Talk, Open, and Never mind are enabled; the locked choice is skipped.
    EXPECT_TRUE(Dialog::InputScrollWheel(3));
    EXPECT_EQ(View().selected_choice, 0);
    EXPECT_TRUE(Dialog::InputScrollWheel(INT32_MIN));
    EXPECT_EQ(View().selected_choice, 3);
    EXPECT_TRUE(Dialog::InputScrollWheel(INT32_MAX));
    EXPECT_EQ(View().selected_choice, 2);
}

TEST_F(DialogFramework, LongListsScrollWithTheCursor) {
    Dialog::RegisterOptionProvider([](const Dialog::NpcContext &, std::vector<Dialog::ChoiceItem> &out) {
        for (int i = 0; i < 8; ++i) {
            out.push_back(Item("Choice " + std::to_string(i)));
        }
    });
    ASSERT_TRUE(Dialog::TryInterceptOpen(Villager(), {}));
    EXPECT_EQ(View().layout.visible_choice_count, 5);
    EXPECT_TRUE(View().layout.needs_scrollbar);

    for (int i = 0; i < 6; ++i) {
        ASSERT_TRUE(Dialog::InputMoveDown());
    }
    EXPECT_EQ(View().selected_choice, 6);
    EXPECT_EQ(View().choice_scroll_offset, 2);
}

} // namespace
