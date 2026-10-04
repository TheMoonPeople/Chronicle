#include <gtest/gtest.h>

// SDL's headers name parameters A and B, which libgraph.h defines as macros: SDL goes first.
#include <SDL3/SDL.h>
#include <libgraph.h>

#include <string>

#include "draw3d_fixture.hpp"
#include "edit.hpp"
#include "rect.hpp"
#include "tex_fixture.hpp"
#include "texture.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "title/title_port.hpp"

using namespace dc::test;
using namespace texfix;

// The title units' own rectangle and their MoveImageTest spelling (port/src/clothread.cpp).
template <class T>
class CRect {
public:
    T x;
    T y;
    T w;
    T h;
};

void MoveImageTest(sceVif1Packet *packet, int sbp, int sbw, int spsm, const CRect<int> &rect, int dbp, int dbw,
                   int dpsm, int dsax, int dsay, int dir);
void FaceChangeD(int actor_no);
void EdDMoveCameraRef(float *position, float *reference);
void EdSaveFrameImage(CTexture texture);

namespace {

// The game's renderer state with a fresh texture manager over the fixture's staging buffer.
struct TailBFixture : Draw3DFixture {
    TailBFixture() {
        TexManager.Initialize(16352);
        TexManager.SetBuffer(staging, static_cast<int>(std::size(staging)));
    }

    ~TailBFixture() { TexManager.Initialize(16352); }

    void Enter(Bytes &img) {
        TexManager.BeginEnterTextureBlock(1);
        TexManager.EnterIMGFile(img.data(), 1, 0, 0);
        TexManager.EndEnterTextureBlock(1);
    }

    std::vector<uint8_t> Texels(CTexture *texture, uint32_t &width, uint32_t &height) {
        std::vector<uint8_t> texels;
        EXPECT_TRUE(texture != nullptr);
        EXPECT_TRUE(gfx::ReadbackTexture(PortTextureFromCTexture(texture).binding.texture, texels, width, height));
        return texels;
    }
};

CTexture *Named(const char *name) {
    std::string mutable_name(name);
    return TexManager.GetTexture(mutable_name.data(), -1);
}

unsigned Tbp(const CTexture *texture) { return static_cast<unsigned>(texture->tex0 & 0x3FFF); }

} // namespace

// TitleDraw's trail: the previous frame at alpha 35 through mgAlpha (Cs - Cd) * As + Cd, over the
// whole logical frame, the bottom rows included.
TEST(TailBRender, FeedbackBlendsPreviousFrame) {
    TailBFixture fixture;
    MGSetBGColor(200.0f, 0.0f, 0.0f, 128.0f);
    fixture.Frame([] {});
    MGSetBGColor(0.0f, 0.0f, 200.0f, 128.0f);
    fixture.Frame([] { TitlePortFeedback(35); });

    const int red = 200 * 35 / 128;
    const int blue = 200 - 200 * 35 / 128;
    ASSERT_TRUE(fixture.PixelNear(320, 240, red, 0, blue, 3));
    ASSERT_TRUE(fixture.PixelNear(10, 470, red, 0, blue, 3));
    ASSERT_TRUE(fixture.PixelNear(630, 5, red, 0, blue, 3));
}

// setTexScroll's two transfers with the seam at row 40: the plate shows the strip scrolled up 40 rows.
TEST(TailBRender, ScrollMovesTextureRows) {
    TailBFixture fixture;
    Bytes        strip_texels;
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            Append(strip_texels, Rgba32({Gs(static_cast<uint8_t>(y), static_cast<uint8_t>(x), 7)}));
        }
    }
    Bytes img = Img({
        {"cloud",   Tim2({TIM2_RGB32, 128, 128, {Bytes(128 * 128 * 4, 0)}, {}, 0})},
        {"cloudan", Tim2({TIM2_RGB32, 128, 128, {strip_texels}, {}, 0})           },
    });
    fixture.Enter(img);
    CTexture *plate = Named("cloud");
    CTexture *strip = Named("cloudan");
    ASSERT_TRUE(plate != nullptr && strip != nullptr);

    const int seam = 40;
    fixture.Frame([&] {
        MoveImageTest(Vif1Packet, Tbp(strip), 2, SCE_GS_PSMCT32, CRect<int>{0, seam, 128, 128 - seam}, Tbp(plate), 2,
                      SCE_GS_PSMCT32, 0, 0, 0);
        MoveImageTest(Vif1Packet, Tbp(strip), 2, SCE_GS_PSMCT32, CRect<int>{0, 0, 128, seam}, Tbp(plate), 2,
                      SCE_GS_PSMCT32, 0, 128 - seam, 0);
    });

    uint32_t             width = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> texels = fixture.Texels(plate, width, height);
    ASSERT_TRUE(width == 128 && height == 128);
    for (int y : {0, 1, 50, 87, 88, 100, 127}) {
        for (int x : {0, 63, 127}) {
            const uint8_t *texel = &texels[(static_cast<size_t>(y) * width + x) * 4];
            ASSERT_TRUE(texel[0] == (y + seam) % 128);
            ASSERT_TRUE(texel[1] == x);
            ASSERT_TRUE(texel[2] == 7);
        }
    }
}

// op_d's face table, actor 3: eye frame 1 of c09a01an lands 78 rows down the plate, mouth frame 2
// 20 rows down. Pause keeps the mouth where the script put it.
TEST(TailBRender, FaceChangeCopiesFaceRects) {
    TailBFixture fixture;
    Bytes        strip_texels(256 * 448, 0);
    auto         fill = [&](int x0, int y0, int w, int h, uint8_t index) {
        for (int y = y0; y < y0 + h; y++) {
            for (int x = x0; x < x0 + w; x++) {
                strip_texels[static_cast<size_t>(y) * 256 + x] = index;
            }
        }
    };
    fill(0, 448 - 40 * 2, 128, 40, 7);
    fill(128, 448 - 35 * 3, 128, 35, 9);
    Bytes clut = Clut256({});
    Bytes img = Img({
        {"c09a01",   Tim2({TIM2_IDTEX8, 128, 128, {Bytes(128 * 128, 1)}, clut, 256})},
        {"c09a01an", Tim2({TIM2_IDTEX8, 256, 448, {strip_texels}, clut, 256})       },
    });
    fixture.Enter(img);

    Pause = 1;
    CScript__2.obj[3].eye = 1;
    CScript__2.obj[3].mouth = 2;
    fixture.Frame([] { FaceChangeD(3); });
    Pause = 0;

    uint32_t             width = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> texels = fixture.Texels(Named("c09a01"), width, height);
    ASSERT_TRUE(width == 128 && height == 128);
    auto at = [&](int x, int y) { return texels[static_cast<size_t>(y) * width + x]; };
    ASSERT_TRUE(at(0, 78) == 7 && at(127, 117) == 7);
    ASSERT_TRUE(at(0, 77) == 1 && at(0, 118) == 1);
    ASSERT_TRUE(at(0, 20) == 9 && at(127, 54) == 9);
    ASSERT_TRUE(at(0, 19) == 1 && at(64, 55) == 1);
}

// The menu's backdrop: the next step's task copies the frame the request was made in to
// frame_image, once per request.
TEST(TailBRender, SaveFrameImageGrabsFrame) {
    TailBFixture     fixture;
    char             name[] = "#frame_image#640#480#4";
    char             end[] = "";
    LOADTEXTURE_INFO table[] = {
        {name, 3, 0},
        {end,  0, 0}
    };
    TexManager.LoadTextureBlock(-1, table, nullptr);
    CTexture *frame_image = Named("frame_image");
    ASSERT_TRUE(frame_image != nullptr);

    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    fixture.Frame([&] {
        MGFillBox(CRect_i_(0, 0, 640 * 16, SCREEN_HALF_HEIGHT * 16), 0, 160, 0, 0x80);
        EdSaveFrameImage(*frame_image);
    });
    fixture.Frame([&] {
        EdSaveFrameImageTask();
        MGFillBox(CRect_i_(0, 0, 640 * 16, SCREEN_HALF_HEIGHT * 16), 160, 0, 0, 0x80);
    });
    fixture.Frame([&] { EdSaveFrameImageTask(); });

    uint32_t             width = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> texels = fixture.Texels(frame_image, width, height);
    ASSERT_TRUE(width == 640 && height == 480);
    for (auto [x, y] : {
             std::pair{0,   0  },
             std::pair{320, 240},
             std::pair{639, 479}
    }) {
        const uint8_t *texel = &texels[(static_cast<size_t>(y) * width + x) * 4];
        ASSERT_TRUE(texel[0] == 0 && texel[1] == 160 && texel[2] == 0);
    }
}

// The free camera's reference box goes through DrawLine: a unit cube 50 units ahead, drawn in
// (128, 64, 64) at alpha 80 over black (80, 40, 40), around the centre of the frame and nowhere else.
TEST(TailBRender, DebugBoxDrawsLines) {
    TailBFixture  fixture;
    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FVECTOR reference = {0.0f, 0.0f, 50.0f, 1.0f};
    fixture.Frame([&] { EdDMoveCameraRef(position, reference); });

    int      inside = 0;
    int      outside = 0;
    uint32_t left = fixture.width;
    uint32_t right = 0;
    uint32_t top = fixture.height;
    uint32_t bottom = 0;
    for (uint32_t y = 0; y < fixture.height; y++) {
        for (uint32_t x = 0; x < fixture.width; x++) {
            std::array<uint8_t, 4> pixel = fixture.Pixel(x, y);
            if (pixel[0] < 70) {
                continue;
            }
            // Where two edges cross the colour is blended twice; it keeps the line's 2:1:1 hue.
            ASSERT_TRUE(pixel[1] == pixel[2] && std::abs(pixel[0] - 2 * pixel[1]) <= 3);
            bool near = x > 320 - 64 && x < 320 + 64 && y > 240 - 64 && y < 240 + 64;
            (near ? inside : outside)++;
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    ASSERT_TRUE(inside > 40);
    ASSERT_TRUE(outside == 0);
    // Square on screen: the field's half-height rows are doubled back into the logical frame.
    ASSERT_TRUE(right > left && std::abs(static_cast<int>(right - left) - static_cast<int>(bottom - top)) <= 2);
    ASSERT_TRUE(right - left >= 28 && right - left <= 36);
}
