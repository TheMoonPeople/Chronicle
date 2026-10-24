#include "credits_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "caption_text.hpp"
#include "captionfont.hpp"
#include "draw2d_port.hpp"
#include "rect.hpp"
#include "textwipe.hpp"

namespace credits {

namespace {

struct CreditLine {
    int         page;
    const char *text;
    int         left, right, top, bottom;
};

struct CreditBar {
    int page;
    int left, right, top, bottom;
};

// The four credit pages' lines (endname1..4, 640 by 960), as painted: each line's ink from left to right and from top to
// bottom of its page, accents and descenders included; then the underlines.
constexpr CreditLine kCreditLines[] = {
    {1, "Dark Cloud Credits",                               149, 492, 28,  56 },
    {1, "Producer",                                         109, 211, 89,  114},
    {1, "Testers",                                          432, 534, 92,  116},
    {1, "Sean Kelly",                                       103, 217, 126, 153},
    {1, "Ian Cunliffe",                                     400, 553, 128, 152},
    {1, "Ian McEvoy",                                       407, 545, 159, 186},
    {1, "Assistant Producer",                               47,  271, 170, 195},
    {1, "Michael Kennedy",                                  373, 578, 190, 217},
    {1, "Florence Kum",                                     87,  233, 206, 231},
    {1, "Carl Seddon",                                      399, 552, 221, 245},
    {1, "Software Licensing Co-ordinator",                  6,   314, 249, 276},
    {1, "Darren Perkins",                                   377, 574, 252, 276},
    {1, "Vincent Tse",                                      399, 552, 283, 308},
    {1, "Shiho de Ruiter-Miyazaki",                         23,  295, 285, 312},
    {1, "Senior Product Manager",                           23,  297, 332, 360},
    {1, "Localisation Co-ordinator",                        326, 633, 333, 358},
    {1, "Birgit Zich",                                      99,  220, 369, 396},
    {1, "Britta Kuhnen",                                    389, 572, 369, 393},
    {1, "Manual and Packaging Design",                      16,  306, 412, 440},
    {1, "Localisation Lead Tester",                         331, 628, 413, 438},
    {1, "Richard Turner",                                   74,  246, 449, 473},
    {1, "Gaëlle Leysour",                                   388, 578, 449, 476},
    {1, "Manual and Packaging Text",                        15,  298, 491, 519},
    {1, "Localisation Testers French",                      329, 630, 493, 518},
    {1, "Ellie Gibson",                                     89,  233, 527, 551},
    {1, "Nadège Josa",                                      395, 556, 529, 556},
    {1, "Yannick Paulet",                                   388, 577, 565, 590},
    {1, "Head of QA",                                       87,  234, 571, 595},
    {1, "Geoff Rens",                                       87,  234, 607, 631},
    {1, "Localisation Testers German",                      332, 627, 650, 675},
    {1, "QA Submissions Managers",                          26,  295, 651, 679},
    {1, "Nadine Martin",                                    387, 563, 686, 710},
    {1, "Mark Pittam & Dave Bennett",                       13,  307, 687, 711},
    {1, "Sandra Raue",                                      396, 556, 722, 746},
    {1, "Internal QA Manager",                              30,  293, 731, 758},
    {1, "Katharina Tropf",                                  362, 575, 758, 785},
    {1, "Dave Parkinson",                                   55,  252, 767, 791},
    {1, "Internal QA Supervisor",                           28,  294, 811, 838},
    {1, "Localisation Testers Italian",                     329, 632, 813, 837},
    {1, "Jim McCabe",                                       87,  226, 847, 872},
    {1, "Monica Dalla Valle",                               361, 587, 849, 873},
    {1, "Rosetta Luca",                                     386, 562, 882, 906},
    {1, "Team Leader",                                      79,  239, 890, 914},
    {1, "Domenico Visone",                                  379, 585, 915, 939},
    {1, "Phil Green",                                       93,  224, 926, 950},
    {2, "Special Projects Supervisors",                     328, 636, 10,  38 },
    {2, "Localisation Testers Spanish",                     6,   314, 13,  40 },
    {2, "John Conway & Miles Rotherham",                    323, 634, 47,  74 },
    {2, "José Flores",                                      76,  229, 48,  73 },
    {2, "Vanessa Ibáñez",                                   53,  252, 85,  109},
    {2, "TRC Testers",                                      403, 563, 89,  113},
    {2, "Susana Paredes",                                   57,  262, 120, 145},
    {2, "John Hall",                                        423, 538, 128, 152},
    {2, "Lorna Croasdale",                                  376, 587, 161, 185},
    {2, "TRC Auditors",                                     89,  234, 174, 198},
    {2, "Mike Bygrave",                                     397, 566, 194, 221},
    {2, "Paul French",                                      93,  226, 209, 234},
    {2, "Carl McQuillian",                                  390, 572, 227, 251},
    {2, "John Hale",                                        97,  221, 243, 267},
    {2, "James Warburton",                                  369, 589, 260, 284},
    {2, "Phil Bramhill",                                    405, 557, 292, 317},
    {2, "Rob Knipe",                                        418, 543, 326, 353},
    {2, "Submission Supervisors",                           8,   309, 333, 360},
    {2, "Russell Power",                                    67,  241, 369, 394},
    {2, "Dave McMahon & Paul McCartan",                     7,   315, 402, 427},
    {2, "TRC Supervisor",                                   65,  262, 490, 517},
    {2, "Dave Baker",                                       89,  235, 526, 550},
    {2, "Content Supervisor",                               36,  293, 568, 595},
    {2, "Stephen Griffiths",                                47,  282, 604, 631},
    {2, "Helpline Supervisor",                              41,  289, 650, 677},
    {2, "Neil Dudley",                                      97,  235, 686, 713},
    {2, "Special thanks to",                                213, 446, 733, 760},
    {2, "Fleur Breteau",                                    120, 297, 778, 802},
    {2, "Flora Collingwood",                                340, 553, 780, 808},
    {2, "Anne Carroll",                                     142, 296, 815, 838},
    {2, "Andrew Hamilton",                                  338, 538, 817, 841},
    {2, "Martin Pearce",                                    119, 297, 850, 875},
    {2, "Vanessa Wood",                                     340, 509, 853, 877},
    {2, "all internal QA and external localisation houses", 65,  573, 888, 912},
    {3, "Music Compose & Sound Effects",                    326, 634, 12,  39 },
    {3, "LEVEL5 Inc.",                                      103, 219, 29,  55 },
    {3, "Tomohito Nishiura",                                386, 574, 48,  72 },
    {3, "Dungeon Map Design",                               368, 591, 88,  115},
    {3, "Product Producer",                                 64,  255, 90,  115},
    {3, "Kazunari Matsuo",                                  387, 572, 124, 148},
    {3, "Akihiro Hino",                                     96,  221, 127, 151},
    {3, "Art Director",                                     90,  228, 171, 195},
    {3, "Field Map Design",                                 387, 572, 195, 223},
    {3, "Takeshi Majima",                                   83,  237, 207, 234},
    {3, "Hidenobu Sasaki",                                  390, 569, 231, 255},
    {3, "Character Animation Director",                     11,  309, 251, 275},
    {3, "Yuka Katayama",                                    397, 562, 266, 293},
    {3, "Yoshiaki Kusuda",                                  76,  243, 287, 311},
    {3, "Takayuki Sameshima",                               372, 589, 301, 328},
    {3, "Story & Game Design",                              49,  270, 331, 358},
    {3, "Kengo Okabe",                                      410, 549, 336, 363},
    {3, "Akihiro Hino",                                     96,  221, 367, 391},
    {3, "Jun Maeda",                                        423, 537, 371, 395},
    {3, "Monster Design",                                   394, 566, 410, 437},
    {3, "Character Design",                                 67,  252, 411, 438},
    {3, "Takeshi Akasaka",                                  388, 572, 446, 470},
    {3, "Miho Tanaka",                                      96,  222, 447, 471},
    {3, "Main Program",                                     90,  228, 490, 518},
    {3, "Character Model & Motion",                         333, 626, 490, 514},
    {3, "Kenji Matsusue",                                   79,  240, 526, 554},
    {3, "Tomoe Ueda",                                       416, 543, 526, 550},
    {3, "Takeshi Akasaka",                                  388, 572, 561, 585},
    {3, "Battle Program",                                   79,  239, 570, 598},
    {3, "Miyuki Sumiyoshi",                                 390, 569, 596, 623},
    {3, "Yasuhiro Akasaka",                                 67,  252, 607, 631},
    {3, "Jun Sonobe",                                       416, 543, 631, 655},
    {3, "Menu System Program",                              45,  273, 650, 678},
    {3, "Tomohiro Misei",                                   85,  232, 687, 711},
    {3, "Motion System Program",                            37,  282, 730, 758},
    {3, "Title & Menu Design",                              368, 591, 730, 757},
    {3, "Ryouichi Takeyama",                                378, 582, 766, 793},
    {3, "Makoto Shikasyo",                                  72,  246, 767, 794},
    {3, "Editors & Visual Program",                         25,  293, 810, 838},
    {3, "Management",                                       415, 543, 810, 837},
    {3, "Shin-ichi Minato",                                 391, 569, 846, 870},
    {3, "Masahiro Noda",                                    84,  234, 847, 871},
    {3, "Opening & Visual Program",                         25,  293, 890, 918},
    {3, "Management Support",                               365, 595, 891, 918},
    {3, "Hisako Ichiyama",                                  393, 566, 926, 953},
    {3, "Tetsu Konno",                                      95,  225, 927, 951},
    {4, "Voice Actor & Actress",                            30,  292, 9,   33 },
    {4, "Actor & Actress",                                  385, 577, 10,  34 },
    {4, "Aki Akiha",                                        109, 212, 45,  69 },
    {4, "Osamu Nagata",                                     405, 558, 46,  73 },
    {4, "Koji Ishii",                                       110, 211, 80,  107},
    {4, "Kaori Harada",                                     407, 554, 81,  105},
    {4, "Kenta Katagiri",                                   78,  243, 115, 142},
    {4, "Megumi Kubota",                                    81,  241, 150, 177},
    {4, "Illustration",                                     417, 543, 170, 194},
    {4, "Chie Koujiro",                                     91,  231, 185, 212},
    {4, "Hitoshi Morita",                                   400, 560, 206, 230},
    {4, "Motion Capture",                                   75,  247, 268, 295},
    {4, "Special Thanks",                                   398, 563, 273, 300},
    {4, "Choreographer",                                    78,  243, 329, 356},
    {4, "Raphael digit & Studio",                           352, 607, 333, 361},
    {4, "Tooru Miura",                                      95,  228, 365, 389},
    {4, "DancingOffice Saijo",                              368, 592, 370, 397},
    {4, "Shinsaku Tanaka",                                  328, 499, 406, 431},
    {4, "(Tohokushinsha)",                                  503, 629, 411, 431},
    {4, "Sony Computer Entertainment Japan.",               109, 531, 508, 535},
    {4, "Producer",                                         104, 206, 565, 590},
    {4, "Executive Producer",                               368, 591, 568, 593},
    {4, "Yasuhide Kobayashi",                               46,  263, 602, 629},
    {4, "Akira Sato",                                       419, 540, 605, 629},
    {4, "Fumiya Takeno",                                    400, 560, 640, 668},
    {4, "Assistant Producer",                               42,  266, 645, 670},
    {4, "Masatsuka Saeki",                                  387, 571, 677, 701},
    {4, "Kentaro Motomura",                                 53,  257, 682, 706},
    {4, "Developed by",                                     407, 554, 727, 754},
    {4, "Special Thanks",                                   73,  238, 728, 755},
    {4, "LEVEL5 Inc.",                                      424, 540, 763, 789},
    {4, "Masaaki Doi",                                      90,  217, 764, 788},
    {4, "Presented By",                                     243, 396, 807, 835},
    {4, "Sony Computer Entertainment Europe",               104, 537, 844, 871},
};

constexpr CreditBar kCreditBars[] = {
    {1, 140, 501, 59,  62 },
    {1, 92,  224, 116, 119},
    {1, 415, 547, 118, 121},
    {1, 39,  280, 198, 201},
    {1, 5,   313, 277, 280},
    {1, 14,  305, 361, 364},
    {1, 324, 632, 362, 365},
    {1, 5,   313, 441, 444},
    {1, 324, 632, 441, 444},
    {1, 5,   313, 519, 522},
    {1, 324, 632, 521, 524},
    {1, 69,  251, 598, 601},
    {1, 11,  310, 679, 682},
    {1, 324, 632, 679, 682},
    {1, 16,  299, 760, 763},
    {1, 20,  297, 840, 843},
    {1, 326, 634, 841, 844},
    {1, 62,  251, 916, 919},
    {2, 5,   313, 40,  43 },
    {2, 324, 635, 40,  43 },
    {2, 388, 575, 118, 121},
    {2, 74,  247, 201, 204},
    {2, 4,   316, 361, 364},
    {2, 53,  273, 518, 521},
    {2, 30,  297, 596, 599},
    {2, 33,  293, 679, 682},
    {2, 202, 452, 761, 764},
    {3, 322, 636, 41,  44 },
    {3, 360, 598, 116, 119},
    {3, 51,  262, 119, 122},
    {3, 73,  243, 199, 202},
    {3, 380, 578, 224, 227},
    {3, 6,   312, 279, 282},
    {3, 39,  277, 359, 362},
    {3, 379, 577, 438, 441},
    {3, 53,  265, 439, 442},
    {3, 326, 633, 518, 521},
    {3, 72,  244, 519, 522},
    {3, 60,  256, 599, 602},
    {3, 35,  281, 679, 682},
    {3, 360, 598, 758, 761},
    {3, 27,  289, 759, 762},
    {3, 384, 574, 838, 841},
    {3, 17,  299, 839, 842},
    {3, 17,  301, 919, 922},
    {3, 359, 599, 919, 922},
    {4, 19,  296, 37,  40 },
    {4, 372, 584, 37,  40 },
    {4, 393, 562, 198, 201},
    {4, 70,  248, 358, 361},
    {4, 39,  269, 594, 597},
    {4, 357, 598, 596, 599},
    {4, 33,  274, 674, 677},
    {4, 64,  247, 755, 758},
    {4, 400, 558, 755, 758},
    {4, 94,  547, 836, 839},
};

// The lettering's colour, and its shadow: black right behind the letters, fading away from them, measured round the
// painted letters as 0.75 strong a page pixel out, then 0.5, 0.32, 0.14 and 0.05. Each word's shadow is painted as one,
// at the page's own resolution (it is soft, so no finer is needed), and the letters over it at the screen's.
constexpr unsigned char kInk[3] = {217, 204, 130};
constexpr float         kShadowAlpha = 0.7f;
constexpr float         kShadowGrow = 0.3f;
constexpr float         kShadowSoft = 6.5f;

// The regular dialogue set, as the credits are lettered, its letters kGap ems apart and a space kSpace ems.
constexpr int   kSlot = 1;
constexpr float kGap = 0.06f;
constexpr float kSpace = 0.5f;

// Letters that reach below the line, and marks that stand above it; the others sit on it, as painted, where the font's
// vary a little.
constexpr std::u32string_view kOffTheLine = U"gjpqyQ,;()[]{}|-'\"`^~*=+";

// The accented letters the credits have: the font composes them from a letter and a mark, which the shadow takes apart.
struct Accent {
    char32_t ch, base, mark;
};

constexpr Accent kAccents[] = {
    {U'é', U'e', U'\''},
    {U'è', U'e', U'`' },
    {U'ë', U'e', U'"' },
    {U'á', U'a', U'\''},
    {U'ñ', U'n', U'~' },
};

std::u32string Decode(std::string_view utf8) {
    std::u32string out;
    for (size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        const int           n = c < 0x80 ? 1 : c < 0xE0 ? 2
                                           : c < 0xF0   ? 3
                                                        : 4;
        char32_t            ch = n == 1 ? c : c & (0x3F >> (n - 1));
        for (int k = 1; k < n && i + static_cast<size_t>(k) < utf8.size(); k++) {
            ch = (ch << 6) | (static_cast<unsigned char>(utf8[i + static_cast<size_t>(k)]) & 0x3F);
        }
        out.push_back(ch);
        i += static_cast<size_t>(n);
    }
    return out;
}

float Snap(float value, float scale, float offset) {
    return (std::round(value * scale + offset) - offset) / scale;
}

// A line's letters as the font draws them at em_px, stretched: where each one's ink starts (pixels), which word it is
// in, and the ink's extents.
struct Laid {
    std::vector<capfont::Glyph> fills;
    std::vector<float>          inks;
    std::vector<char32_t>       chars;
    std::vector<int>            words;
    float                       left = 1e9f, right = -1e9f, top = 1e9f, bottom = -1e9f;
};

Laid Lay(const std::u32string &text, int em_px, float stretch) {
    Laid  laid;
    float pen = 0.0f;
    int   word = 0;
    for (char32_t ch : text) {
        capfont::Glyph glyph;
        if (ch == U' ' || !capfont::GetStretchedGlyph(ch, em_px, 0.0f, stretch, false, glyph) || glyph.width <= 0) {
            pen += static_cast<float>(em_px) * kSpace * stretch;
            word += ch == U' ' ? 1 : 0;
            continue;
        }
        // Letters set close, as painted: each one's ink starts a little after the last one's ends.
        laid.fills.push_back(glyph);
        laid.inks.push_back(pen);
        laid.chars.push_back(ch);
        laid.words.push_back(word);
        laid.left = std::min(laid.left, pen);
        laid.right = std::max(laid.right, pen + glyph.width);
        laid.top = std::min(laid.top, glyph.y_offset);
        laid.bottom = std::max(laid.bottom, glyph.y_offset + glyph.height);
        pen += glyph.width + static_cast<float>(em_px) * kGap * stretch;
    }
    return laid;
}

struct Layers {
    std::vector<gfx::Vertex2D> shadow, ink;
};

void Quad(std::vector<gfx::Vertex2D> &out, const capfont::Glyph &glyph, float x, float y, float w, float h, const unsigned char (&rgb)[3],
          int alpha) {
    const u_char r = static_cast<u_char>(rgb[0] / 2), g = static_cast<u_char>(rgb[1] / 2), b = static_cast<u_char>(rgb[2] / 2);
    const u_char a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
    out.push_back(draw2d::Vertex(x, y, 0.0f, glyph.u0, glyph.v0, r, g, b, a));
    out.push_back(draw2d::Vertex(x + w, y, 0.0f, glyph.u1, glyph.v0, r, g, b, a));
    out.push_back(draw2d::Vertex(x + w, y + h, 0.0f, glyph.u1, glyph.v1, r, g, b, a));
    out.push_back(draw2d::Vertex(x, y + h, 0.0f, glyph.u0, glyph.v1, r, g, b, a));
}

// One line, its ink's top left at origin_x, origin_y (logical), where the painted line's lands on the screen.
void LayLine(const CreditLine &line, float origin_x, float origin_y, int alpha, Layers &layers) {
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const std::u32string      text = Decode(line.text);
    const float               want_w = static_cast<float>(line.right - line.left) * mapping.scale_x;
    const float               want_h = static_cast<float>(line.bottom - line.top) * rows * mapping.scale_y;
    // The em that makes the ink as tall as the painted line's, then the stretch that makes it as wide.
    const Laid probe = Lay(text, 64, 1.0f);
    if (probe.fills.empty() || probe.bottom <= probe.top || probe.right <= probe.left) {
        return;
    }
    const int   em_px = std::clamp(static_cast<int>(std::lround(64.0f * want_h / (probe.bottom - probe.top))), 4, 512);
    const Laid  natural = Lay(text, em_px, 1.0f);
    const float stretch = std::clamp(want_w / std::max(natural.right - natural.left, 1.0f), 0.25f, 3.9f);
    const float stretch64 = std::round(stretch * 64.0f) / 64.0f;
    const Laid  laid = Lay(text, em_px, stretch64);
    if (laid.fills.empty()) {
        return;
    }
    // The line the letters sit on: where most of those that sit on it end.
    std::vector<float> bottoms;
    for (size_t i = 0; i < laid.fills.size(); i++) {
        if (kOffTheLine.find(laid.chars[i]) == std::u32string_view::npos) {
            bottoms.push_back(laid.fills[i].y_offset + laid.fills[i].height);
        }
    }
    std::sort(bottoms.begin(), bottoms.end());
    const float baseline = bottoms.empty() ? laid.bottom : bottoms[bottoms.size() / 2];
    auto        lift = [&](size_t i) {
        const bool descends = kOffTheLine.find(laid.chars[i]) != std::u32string_view::npos;
        return descends ? 0.0f : baseline - (laid.fills[i].y_offset + laid.fills[i].height);
    };

    // What the whole pixels of the font leave over, taken up by the quads.
    const float kx = want_w / std::max(laid.right - laid.left, 1.0f);
    const float ky = want_h / std::max(laid.bottom - laid.top, 1.0f);
    // A letter's ink rectangle in logical units, before snapping.
    auto ink_x = [&](size_t i) { return origin_x + (laid.inks[i] - laid.left) * kx / mapping.scale_x; };
    auto ink_y = [&](size_t i, float y_offset) { return origin_y * rows + (y_offset + lift(i) - laid.top) * ky / mapping.scale_y; };

    for (size_t i = 0; i < laid.fills.size(); i++) {
        const capfont::Glyph &fill = laid.fills[i];
        const float           x = Snap(ink_x(i), mapping.scale_x, mapping.offset_x);
        const float           y = Snap(ink_y(i, fill.y_offset), mapping.scale_y, mapping.offset_y);
        Quad(layers.ink, fill, x, y, fill.width * kx / mapping.scale_x, fill.height * ky / mapping.scale_y, kInk, alpha);
    }

    // Each word's shadow, its letters' boxes in page pixels from the word's first letter's top left.
    capfont::Paint paint = {0.0f, 0.0f, 0.0f, 0, 0.0f, {}, {}, {}};
    paint.shadow_only = true;
    paint.shadow_alpha = kShadowAlpha;
    paint.shadow_grow = kShadowGrow;
    paint.shadow_soft = kShadowSoft;
    paint.slot = kSlot;
    const float page_x = mapping.scale_x, page_y = mapping.scale_y;
    for (size_t first = 0; first < laid.fills.size();) {
        size_t last = first;
        while (last < laid.fills.size() && laid.words[last] == laid.words[first]) {
            last++;
        }
        const float                        word_x = ink_x(first), word_y = origin_y * rows;
        std::vector<capfont::PlacedLetter> letters;
        for (size_t i = first; i < last; i++) {
            const capfont::Glyph &fill = laid.fills[i];
            const float           x = (ink_x(i) - word_x), y = (ink_y(i, fill.y_offset) - word_y) / rows;
            const float           w = fill.width * kx / page_x, h = fill.height * ky / page_y / rows;
            const Accent         *accent = nullptr;
            for (const Accent &a : kAccents) {
                accent = a.ch == laid.chars[i] ? &a : accent;
            }
            capfont::Glyph base;
            if (accent != nullptr && capfont::GetStretchedGlyph(accent->base, em_px, 0.0f, stretch64, false, base) && base.height > 0) {
                // The letter at the bottom of the composed one's ink, its mark in the rest above it.
                const float base_h = base.height * ky / page_y / rows;
                letters.push_back({accent->base, x, y + h - base_h, w, base_h});
                const float mark_w = w * 0.6f;
                letters.push_back({accent->mark, x + (w - mark_w) / 2, y, mark_w, std::max(h - base_h - 1.0f, 1.0f)});
            } else {
                letters.push_back({laid.chars[i], x, y, w, h});
            }
        }
        capfont::Glyph shadow;
        if (!letters.empty() && capfont::GetPaintedWord(letters, paint, shadow)) {
            static constexpr unsigned char kGrey[3] = {255, 255, 255};
            Quad(layers.shadow, shadow, word_x + shadow.x_offset, word_y + shadow.y_offset * rows, shadow.width, shadow.height * rows,
                 kGrey, alpha);
        }
        first = last;
    }
}

// An untextured rectangle in a colour, for an underline and its halo.
void Rect(std::vector<gfx::Vertex2D> &out, float x0, float y0, float x1, float y1, const unsigned char (&rgb)[3], int alpha) {
    const u_char r = rgb[0], g = rgb[1], b = rgb[2], a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
    out.push_back(draw2d::Vertex(x0, y0, 0.0f, 0, 0, r, g, b, a));
    out.push_back(draw2d::Vertex(x1, y0, 0.0f, 0, 0, r, g, b, a));
    out.push_back(draw2d::Vertex(x1, y1, 0.0f, 0, 0, r, g, b, a));
    out.push_back(draw2d::Vertex(x0, y1, 0.0f, 0, 0, r, g, b, a));
}

// Whether a band holds a line (or a bar): the band its middle is in draws all of it. The bands of a column are each put
// on the screen to their own whole pixel, so they meet a pixel or two apart, and a line drawn a part in each would show
// the seam.
bool Holds(const CRect_i_ &texel, int left, int right, int top, int bottom) {
    const int x = (left + right) / 2, y = (top + bottom) / 2;
    return x >= texel.x && x < texel.x + texel.width && y >= texel.y && y < texel.y + texel.height;
}

} // namespace

bool DrawBand(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || std::strncmp(texture, "endname", 7) != 0 || texture[7] < '1' || texture[7] > '4' || texture[8] != 0 ||
        texel.width <= 0 || texel.height <= 0 || !caption::ThinReady()) {
        return false;
    }
    const int   page = texture[7] - '0';
    const float sx = static_cast<float>(screen.width) / static_cast<float>(texel.width);
    const float sy = static_cast<float>(screen.height) / static_cast<float>(texel.height);
    auto        at_x = [&](float page_x) { return static_cast<float>(screen.x) + (page_x - static_cast<float>(texel.x)) * sx; };
    auto        at_y = [&](float page_y) { return static_cast<float>(screen.y) + (page_y - static_cast<float>(texel.y)) * sy; };

    capfont::UseFontSlot(kSlot);
    Layers                     layers;
    std::vector<gfx::Vertex2D> bars;
    // The underlines: a bar with round ends, a dot of its height at each, over the same soft shadow as the words'.
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    capfont::Paint            shade = {0.0f, 0.0f, 0.0f, 0, 0.0f, {}, {}, {}};
    shade.shadow_only = true;
    shade.shadow_alpha = kShadowAlpha;
    shade.shadow_grow = kShadowGrow;
    shade.shadow_soft = kShadowSoft;
    shade.slot = kSlot;
    capfont::Paint dot = {0.0f, 0.0f, 0.0f, 0, 0.0f, {}, {}, {}};
    for (unsigned char (*colour)[3] : {&dot.fill, &dot.body, &dot.ring}) {
        std::copy_n(kInk, 3, *colour);
    }
    dot.slot = kSlot;
    static constexpr unsigned char kWhite[3] = {255, 255, 255};
    // Made again, once, if the atlas started over while the letters were fetched.
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        layers = {};
        bars.clear();
        for (const CreditLine &line : kCreditLines) {
            if (line.page != page || !Holds(texel, line.left, line.right, line.top, line.bottom)) {
                continue;
            }
            LayLine(line, at_x(static_cast<float>(line.left)), at_y(static_cast<float>(line.top)), alpha, layers);
        }
        for (const CreditBar &bar : kCreditBars) {
            if (bar.page != page || !Holds(texel, bar.left, bar.right, bar.top, bar.bottom)) {
                continue;
            }
            const float    x0 = at_x(static_cast<float>(bar.left)), x1 = at_x(static_cast<float>(bar.right));
            const float    y0 = at_y(static_cast<float>(bar.top)), y1 = at_y(static_cast<float>(bar.bottom));
            const float    h = y1 - y0;
            capfont::Glyph glyph;
            if (capfont::GetPaintedGlyph(U'-', -1, x1 - x0, h, shade, glyph)) {
                Quad(layers.shadow, glyph, x0 + glyph.x_offset, y0 * rows + glyph.y_offset * rows, glyph.width, glyph.height * rows, kWhite,
                     alpha);
            }
            Rect(bars, x0 + h / 2, y0 * rows, x1 - h / 2, y1 * rows, kInk, alpha);
            if (capfont::GetPaintedGlyph(U'.', -1, h * mapping.scale_x, h * rows * mapping.scale_y, dot, glyph)) {
                for (float end : {x0, x1 - h}) {
                    Quad(layers.ink, glyph, end + glyph.x_offset / mapping.scale_x, y0 * rows + glyph.y_offset / mapping.scale_y,
                         glyph.width / mapping.scale_x, glyph.height / mapping.scale_y, kWhite, alpha);
                }
            }
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    capfont::UseFontSlot(0);

    const gfx::DrawState      state = draw2d::SpriteState();
    const gfx::TextureBinding binding = capfont::Binding();
    if (!layers.shadow.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, layers.shadow, binding, state);
    }
    if (!bars.empty()) {
        // Already in the target's rows.
        textwipe::Draw2D(gfx::Primitive::Quads, bars, {}, state);
    }
    if (!layers.ink.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, layers.ink, binding, state);
    }
    draw2d::RestoreTestZbuf();
    return true;
}

} // namespace credits
