#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "clsmes.hpp"
#include "gametext.hpp"
#include "gameutil.hpp"

namespace {

std::vector<s16> Encode(std::string_view text, int *missing = nullptr) {
    std::vector<s16> codes;
    const int        replaced = GameTextEncode(text, codes);
    if (missing != nullptr) {
        *missing = replaced;
    }
    return codes;
}

} // namespace

// Place names as the PAL disc's meswin/system_N.mes spells them (messages 10, 11, 13 and 66).
TEST(GameText, EncodesAsTheDiscDoes) {
    EXPECT_EQ(Encode("Norune Village"), (std::vector<s16>{-0x2D2, -0x2B7, -0x2B4, -0x2B1, -0x2B8, -0x2C1, -0xFE, -0x2CA,
                                                          -0x2BD, -0x2BA, -0x2BA, -0x2C5, -0x2BF, -0x2C1, -0xFF}));
    EXPECT_EQ(Encode("Tanière de la bête"),
              (std::vector<s16>{-0x2CC, -0x2C5, -0x2B8, -0x2BD, -0x278, -0x2B4, -0x2C1, -0xFE, -0x2C2, -0x2C1, -0xFE,
                                -0x2BA, -0x2C5, -0xFE, -0x2C4, -0x276, -0x2B2, -0x2C1, -0xFF}));
    EXPECT_EQ(Encode("Götterbiest-Höhle"),
              (std::vector<s16>{-0x2D9, -0x26C, -0x2B2, -0x2B2, -0x2C1, -0x2B4, -0x2C4, -0x2BD, -0x2C1, -0x2B3, -0x2B2,
                                -0x2A3, -0x2D8, -0x26C, -0x2BE, -0x2BA, -0x2C1, -0xFF}));
    EXPECT_EQ(Encode("Più Ricco"),
              (std::vector<s16>{-0x2D0, -0x2BD, -0x26B, -0xFE, -0x2CE, -0x2BD, -0x2C3, -0x2C3, -0x2B7, -0xFF}));
}

// MakeMesWinTbl_value's own codes for the characters of a number.
TEST(GameText, NumbersMatchTheGame) {
    EXPECT_EQ(GameTextCode(U'+'), -0x2A4);
    EXPECT_EQ(GameTextCode(U'-'), -0x2A3);
    for (int digit = 0; digit < 10; digit++) {
        EXPECT_EQ(GameTextCode(U'0' + digit), -0x291 + digit);
    }
}

TEST(GameText, EveryCharacterRoundTrips) {
    int characters = 0;
    for (int code = -0x300; code < -0x251; code++) {
        const char32_t ch = GameTextChar(static_cast<s16>(code));
        if (ch != 0) {
            characters++;
            EXPECT_EQ(GameTextCode(ch), code) << code;
        }
    }
    EXPECT_EQ(characters, 88 + 49);
    for (int code : {-0x263, -0x262, -0x261, -0x253, -0x252}) {
        EXPECT_EQ(GameTextChar(static_cast<s16>(code)), 0U) << code;
    }
}

TEST(GameText, ReplacesWhatTheFontLacks) {
    int missing = 0;
    EXPECT_EQ(Encode("a~b;\xFF", &missing), (std::vector<s16>{-0x2C5, -0x2A7, -0x2C4, -0x2A7, -0x2A7, -0xFF}));
    EXPECT_EQ(missing, 3);
    EXPECT_EQ(Encode("\xE2\x80\x99\xE2\x80\x9C\xE2\x80\x94", &missing), Encode("'\"-"));
    EXPECT_EQ(missing, 0);
}

TEST(GameText, EscapesRoundTrip) {
    const std::vector<s16> codes = {-0x2DF, MES_CODE_NEWLINE, -0x2F0, MES_CODE_PAGE, -0x299, -0x298, -0x2DE, MES_CODE_END};
    const std::string      text = GameTextDecode(codes.data());
    EXPECT_EQ(text, "A\n{red-x}{page}{{}B");
    EXPECT_EQ(Encode(text), codes);
    EXPECT_EQ(Encode("{x}"), Encode("{{x}"));
}

// The file reads back through the game's own lookup.
TEST(GameText, FileReadsAsAMessageFile) {
    GameTextFile file;
    file.Set(0x15E, "PC Settings");
    file.Set(3, "Mouse sensitivity\n0.10");

    ClsMes mes;
    mes.SetBuff(file.Data());
    EXPECT_EQ(GameTextDecode(mes.GetTextLineDataTop(0x15E)), "PC Settings");
    EXPECT_EQ(GameTextDecode(mes.GetTextLineDataTop(3)), "Mouse sensitivity\n0.10");
    EXPECT_EQ(mes.GetTextLineDataTop(4), nullptr);
}

TEST(GameText, WindowCapacity) {
    GameText text;
    EXPECT_EQ(text.Set(std::string(MES_WIN_LINE_MAX - 1, 'A')), 0);
    EXPECT_EQ(text.Width(), (MES_WIN_LINE_MAX - 1) * 11);
    EXPECT_EQ(text.Set(std::string(MES_WIN_LINE_MAX, 'A')), -1);
    EXPECT_EQ(text.Width(), 0);
    EXPECT_LT(text.Mes().mes_made, 0);
    EXPECT_EQ(text.Mes().win_line_num, MES_WIN_LINE_MAX);

    const std::string value = std::string(MES_WIN_LINE_MAX - 3, 'A') + "{-1025}";
    EXPECT_EQ(text.Set(value), 0);
    text.Mes().value = 1000;
    text.SetColour(FONT_COLOR_YELLOW);
    EXPECT_EQ(text.Set(value), -1);
    text.Mes().value = 1;
    EXPECT_EQ(text.Set(value), 0);
    EXPECT_GT(text.Width(), 0);

    std::string lines = "A";
    for (int line = 1; line < 11; line++) {
        lines += "\nA";
    }
    EXPECT_EQ(text.Set(lines), 0);
    EXPECT_EQ(text.Mes().text_rows, 11);
    text.Draw(0, 0);
}

TEST(GameText, FileLimits) {
    GameTextFile file;
    EXPECT_EQ(file.Set(-0x8001, "A"), -1);
    EXPECT_EQ(file.Set(0x8000, "A"), -1);
    EXPECT_EQ(file.Set(-0x8000, "low"), 0);
    EXPECT_EQ(file.Set(0x7FFF, "high"), 0);

    EXPECT_EQ(file.Set(0, std::string(0x7FF7, 'A')), -1);
    EXPECT_EQ(file.Data()[0], 2);
    EXPECT_EQ(file.Set(0, std::string(0x7FF6, 'A')), 0);

    ClsMes mes;
    mes.SetBuff(file.Data());
    EXPECT_EQ(mes.buff[0], 3);
    EXPECT_EQ(GameTextDecode(mes.GetTextLineDataTop(-0x8000)), "low");
    EXPECT_EQ(GameTextDecode(mes.GetTextLineDataTop(0x7FFF)), "high");
    EXPECT_EQ(GameTextDecode(mes.GetTextLineDataTop(0)).size(), 0x7FF6U);
}

// A control code is written by its name where it has one, and by its number where it does not; either way
// the same code comes back.
TEST(GameText, ControlCodesHaveNames) {
    EXPECT_EQ(GameTextDecode(Encode("{page}{/color}{cyan}{L1}{R1}{cross}{circle}{square}{triangle}").data()),
              "{page}{/color}{cyan}{L1}{R1}{cross}{circle}{square}{triangle}");
    EXPECT_EQ(Encode("{-253}{-1024}{-1021}{-766}{-760}"), Encode("{page}{/color}{cyan}{L1}{cross}"));
    EXPECT_EQ(Encode("{value}{value1}{value8}{insert1}{insert10}{name1}{name6}"),
              (std::vector<s16>{-0x401, -0x406, -0x40D, -0x402, -0x413, -0x506, -0x501, MES_CODE_END}));
    EXPECT_EQ(Encode("{wait 12}{gap 8}{spacing 10}{justify 3}{bubble 5}{color 9}{icon 32}"),
              (std::vector<s16>{-0x200 + 12, -0x700 + 8, -0x800 + 10, -0x900 + 3, -0xA00 + 5, -0x401 + 9, -0x2E0, MES_CODE_END}));
    // Names are not case sensitive, and a name the game has no code for is left as the text it is.
    EXPECT_EQ(Encode("{PAGE}{l1}"), Encode("{page}{L1}"));
    int missing = 0;
    EXPECT_EQ(Encode("{nonsense}", &missing).size(), 11U);
    EXPECT_EQ(Encode("{value9}{insert11}{name7}{wait 300}", &missing).front(), Encode("{").front());
}

TEST(GameText, EveryCodeSurvivesBeingWrittenAndReadBack) {
    int named = 0;
    for (int code = INT16_MIN; code <= INT16_MAX; code++) {
        if (code == MES_CODE_END) {
            continue;
        }
        const std::vector<s16> codes = {static_cast<s16>(code), MES_CODE_END};
        const std::string      text = GameTextDecode(codes.data());
        ASSERT_EQ(Encode(text), codes) << code << " written as " << text;
        if (text.size() > 2 && text[0] == '{' && text[1] != '-' && !(text[1] >= '0' && text[1] <= '9') && text[1] != '{') {
            named++;
        }
    }
    // Every code the PAL game's text uses, and more, has a name: 33 icons less the one unnamed, the families,
    // the colours and the page break.
    EXPECT_GT(named, 300);
}

// No control code the game's text uses is left as a number.
TEST(GameText, TheCodesThePalTextUsesAreAllNamed) {
    for (int code : {-0xFD, -0x400, -0x3FF, -0x3FE, -0x3FD, -0x3FC, -0x3FB, -0x3FA, -0x301, -0x401, -0x402, -0x405, -0x406, -0x40D,
                     -0x506, -0x501, -0x6F8, -0x7F6, -0x2F8, -0x2EE, -0x2E1, -0x1FF}) {
        const std::vector<s16> codes = {static_cast<s16>(code), MES_CODE_END};
        const std::string      text = GameTextDecode(codes.data());
        EXPECT_FALSE(text.size() > 2 && (text[1] == '-' || (text[1] >= '0' && text[1] <= '9'))) << code << " is " << text;
    }
}
