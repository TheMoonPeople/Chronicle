#include <gtest/gtest.h>

#include <algorithm>
#include <set>

#include "tex_fixture.hpp"

using namespace dc::test;
using namespace texfix;

namespace {

sceGsTex0 Tex0Of(const CTexture *texture) {
    sceGsTex0 tex0;
    std::memcpy(&tex0, &texture->tex0, sizeof tex0);
    return tex0;
}

CTexture *Named(const char *name, int block = -1) {
    std::string mutable_name(name);
    return TexManager.GetTexture(mutable_name.data(), block);
}

Bytes ThreeKinds() {
    return Img({
        {"idx",  Tim2({TIM2_IDTEX8, 4, 1, {{1, 2, 3, 4}}, Clut256({{1, Gs(255, 0, 0)}}), 256})},
        {"rgba", Tim2({TIM2_RGB32, 2, 2, {Bytes(16, 0x40)}, {}, 0})                           },
        {"rgb",  Tim2({TIM2_RGB24, 2, 1, {Bytes(6, 0x20)}, {}, 0})                            }
    });
}

void Enter(Bytes &img, int block) {
    TexManager.BeginEnterTextureBlock(block);
    TexManager.EnterIMGFile(img.data(), block, 0, 0);
    TexManager.EndEnterTextureBlock(block);
}

// Every TBP0 and CBP the registry's textures hold, by block.
std::multiset<unsigned> Keys(int block) {
    std::multiset<unsigned> keys;
    for (int i = 0; i < 196; i++) {
        const CTexture &texture = TexManager.textures[i];
        if (texture.name[0] == 0 || (block != -2 && texture.block != block)) {
            continue;
        }
        sceGsTex0 tex0 = Tex0Of(&texture);
        keys.insert(tex0.TBP0);
        if (tex0.CLD) {
            keys.insert(tex0.CBP);
        }
    }
    return keys;
}

} // namespace

TEST(TexManager, PlaceholdersBecomeNamedTargets) {
    TexEnv           env;
    char             shadow[] = "#shadow_buf#640#256#4";
    char             font[] = "#fontbase#512#256#1";
    char             frame24[] = "#frame24#64#32#3";
    char             defaults[] = "#defaults#";
    char             end[] = "";
    LOADTEXTURE_INFO table[] = {
        {shadow,   3, 0},
        {font,     3, 0},
        {frame24,  4, 0},
        {defaults, 4, 0},
        {end,      0, 0}
    };
    TexManager.LoadTextureBlock(-1, table, nullptr);

    PortTextureRef shadow_ref = PortTextureFromCTexture(Named("shadow_buf"));
    ASSERT_TRUE(shadow_ref.valid && shadow_ref.width == 640 && shadow_ref.height == 256);
    ASSERT_TRUE(shadow_ref.binding.texture == gfx::FindNamedRenderTarget("shadow_buf"));
    auto shadow_info = gfx::GetTextureInfo(shadow_ref.binding.texture);
    ASSERT_TRUE(shadow_info && shadow_info->render_target && shadow_info->has_alpha && shadow_info->width == 640 &&
                shadow_info->height == 256);
    ASSERT_TRUE(Named("shadow_buf")->block == 3);

    CTexture      *font_texture = Named("fontbase");
    PortTextureRef font_ref = PortTextureFromCTexture(font_texture);
    ASSERT_TRUE(font_ref.valid && font_ref.binding.palette != gfx::kNullTexture);
    auto font_info = gfx::GetTextureInfo(font_ref.binding.texture);
    ASSERT_TRUE(font_info && font_info->format == gfx::TextureFormat::Index8 && font_info->width == 512);
    ASSERT_TRUE(Tex0Of(font_texture).PSM == SCE_GS_PSMT8 && Tex0Of(font_texture).CLD == 1);

    auto frame_info = gfx::GetTextureInfo(PortTextureFromCTexture(Named("frame24")).binding.texture);
    ASSERT_TRUE(frame_info && frame_info->render_target && !frame_info->has_alpha && frame_info->width == 64);
    auto default_info = gfx::GetTextureInfo(gfx::FindNamedRenderTarget("defaults"));
    ASSERT_TRUE(default_info && default_info->width == 32 && default_info->height == 32);

    // The game renders into a placeholder and draws from it.
    env.gfx.Frame({0, 0, 0, 0x80}, [&] {
        const uint8_t red[4] = {255, 0, 0, 0x80};
        gfx::SetRenderTarget(shadow_ref.binding.texture);
        gfx::Clear(true, red, false, 0.0f);
        gfx::SetRenderTarget(gfx::kMainTarget);
        env.Draw(shadow_ref, 0, 0, 100, 100, 0, 0, 640, 256);
    });
    ASSERT_TRUE(env.gfx.PixelNear(50, 50, 255, 0, 0));

    // The same name at another size while the first is live gets a target of its own.
    char             small[] = "#shadow_buf#320#240#4";
    LOADTEXTURE_INFO second[] = {
        {small, 5, 0},
        {end,   0, 0}
    };
    TexManager.LoadTextureBlock(5, second, nullptr);
    PortTextureRef small_ref = PortTextureFromCTexture(Named("shadow_buf", 5));
    ASSERT_TRUE(small_ref.valid && small_ref.width == 320 && small_ref.binding.texture != shadow_ref.binding.texture);
    ASSERT_TRUE(small_ref.binding.texture == gfx::FindNamedRenderTarget("#shadow_buf#320#240#4"));
    ASSERT_TRUE(PortTextureFromCTexture(Named("shadow_buf", 3)).binding.texture == shadow_ref.binding.texture);

    // Targets outlive the block, so a mode that enters them again keeps their contents.
    TexManager.DeleteTextureBlock(3);
    ASSERT_TRUE(Named("shadow_buf", 3) == nullptr);
    ASSERT_TRUE(gfx::FindNamedRenderTarget("shadow_buf") == shadow_ref.binding.texture);
    TexManager.LoadTextureBlock(3, table, nullptr);
    ASSERT_TRUE(PortTextureFromCTexture(Named("shadow_buf", 3)).binding.texture == shadow_ref.binding.texture);
}

TEST(TexManager, Tbp0KeysAreUniqueAndReleased) {
    TexEnv env;
    Bytes  first = ThreeKinds();
    Bytes  second = ThreeKinds();
    Enter(first, 1);
    Enter(second, 2);

    std::multiset<unsigned> all = Keys(-2);
    std::multiset<unsigned> block1 = Keys(1);
    std::multiset<unsigned> block2 = Keys(2);
    ASSERT_TRUE(block1.size() == 4 && block2.size() == 4);
    ASSERT_TRUE(std::set<unsigned>(all.begin(), all.end()).size() == all.size());
    for (unsigned key : all) {
        ASSERT_TRUE(key != 0 && key != 0xFFF && key < 0x4000 && PortKeyLive(key));
    }
    // The same name in two blocks is two textures.
    ASSERT_TRUE(Tex0Of(Named("idx", 1)).TBP0 != Tex0Of(Named("idx", 2)).TBP0);

    // "work", the manager's own entry, resolves to its named target.
    PortTextureRef work = PortTextureFromHandle(0);
    ASSERT_TRUE(work.valid && work.binding.texture == gfx::FindNamedRenderTarget("work"));
    ASSERT_TRUE(std::string(TexManager.textures[0].name) == "work");

    TexManager.DeleteTextureBlock(1);
    for (unsigned key : block1) {
        ASSERT_TRUE(!PortKeyLive(key));
    }
    for (unsigned key : block2) {
        ASSERT_TRUE(PortKeyLive(key));
    }
    ASSERT_TRUE(PortTextureFromCTexture(Named("rgba", 2)).valid);
    ASSERT_TRUE(PortTextureFromCTexture(Named("idx", 2)).valid);

    // A freed key is not handed out again straight away, so stale copies of a TEX0 resolve to
    // nothing rather than to the next texture.
    Bytes third = ThreeKinds();
    Enter(third, 3);
    for (unsigned key : Keys(3)) {
        ASSERT_TRUE(block1.count(key) == 0 && block2.count(key) == 0);
    }
    ASSERT_TRUE(!PortTextureFromTex0(Tex0Of(&TexManager.textures[0]).TBP0 + 0x2000, 0).valid);

    unsigned work_key = Tex0Of(&TexManager.textures[0]).TBP0;
    TexManager.DeleteTextureBlock(-1);
    ASSERT_TRUE(!PortKeyLive(work_key));
    ASSERT_TRUE(Keys(-2).empty());
}

TEST(TexManager, InitializeReleasesEverything) {
    TexEnv env;
    Bytes  img = ThreeKinds();
    Enter(img, 7);
    std::multiset<unsigned> keys = Keys(7);
    gfx::TextureHandle      handle = PortTextureFromCTexture(Named("rgba")).binding.texture;
    ASSERT_TRUE(gfx::GetTextureInfo(handle).has_value());
    TexManager.Initialize(16352);
    for (unsigned key : keys) {
        ASSERT_TRUE(!PortKeyLive(key));
    }
    ASSERT_TRUE(!gfx::GetTextureInfo(handle).has_value());
    ASSERT_TRUE(Named("rgba") == nullptr);
}

TEST(TexManager, StagingAndVramBookkeepingFollowRetail) {
    TexEnv env;
    Bytes  img = Img({
        {"square", Tim2({TIM2_RGB32, 16, 16, {Bytes(1024, 0x11)}, {}, 0})}
    });
    int    vram_start = TexManager.blocks[1].vram_end;
    Enter(img, 1);
    // 1024 bytes are four 256-byte blocks, and a block end is aligned to 32 of them.
    ASSERT_TRUE(TexManager.buffer_used == 512);
    ASSERT_TRUE(TexManager.blocks[1].vram_end == vram_start + 32);
    CTexture *texture = Named("square");
    ASSERT_TRUE(texture->image[0] == (u_int *) TexManager.buffer);
    ASSERT_TRUE(texture->width == 16 && texture->bpp == 4 && texture->block == 1);
    sceGsTex0 tex0 = Tex0Of(texture);
    ASSERT_TRUE(tex0.PSM == SCE_GS_PSMCT32 && tex0.TW == 4 && tex0.TH == 4 && tex0.TBW == 1 && tex0.TCC == 1);

    // EnterTextureEX keeps the caller's pixels and stages nothing.
    LOADTEXTURE_INFO2 extended[] = {
        {(char *) img.data(), 2, 0},
        {nullptr,             0, 0}
    };
    TexManager.LoadTextureBlockEX(2, extended);
    ASSERT_TRUE(TexManager.buffer_used == 512);
    CTexture *kept = Named("square", 2);
    ASSERT_TRUE(kept != nullptr && TexManager.blocks[2].extend);
    ASSERT_TRUE((u_char *) kept->image[0] > img.data() && (u_char *) kept->image[0] < img.data() + img.size());
    ASSERT_TRUE(PortTextureFromCTexture(kept).valid);

    TexManager.ReloadTexture(nullptr, 2);
    ASSERT_TRUE(TexManager.last_block == 2 && TexManager.blocks[2].loaded);
    TexManager.ReloadTexture(nullptr, 99);
    ASSERT_TRUE(TexManager.last_block == -1);
}

TEST(TexManager, FixedTexturesAndStayframe) {
    TexEnv env;
    Bytes  fixed = Img({
        {"fixed", Tim2({TIM2_RGB32, 16, 16, {Bytes(1024, 0x80)}, {}, 0})}
    });
    int    vram_fix = TexManager.vram_fix;
    TexManager.EnterIMGFile(fixed.data(), -1, 0, 0);
    CTexture *fixed_texture = Named("fixed");
    ASSERT_TRUE(fixed_texture && fixed_texture->block == -1 && TexManager.vram_fix == vram_fix - 32);
    ASSERT_TRUE(PortTextureFromCTexture(fixed_texture).valid);

    Bytes sheet(640 * 16, 0);
    std::fill(sheet.begin() + 640 * 8, sheet.end(), 1);
    Bytes stayframe = Img({
        {"stayframe", Tim2({TIM2_IDTEX8, 640, 16, {sheet}, Clut256({{1, Gs(0, 0, 255)}}), 256})}
    });
    TexManager.EnterFixTextureZ(stayframe.data());
    CTexture *stay = Named("stayframe");
    ASSERT_TRUE(stay != nullptr && stay->block == 73 && stay->width == 640 && stay->height == 16);
    sceGsTex0 tex0 = Tex0Of(stay);
    ASSERT_TRUE(tex0.PSM == 27 && tex0.TBW == 10 && tex0.TW == 10 && tex0.TH == 8 && tex0.CLD == 1);
    PortTextureRef ref = PortTextureFromCTexture(stay);
    ASSERT_TRUE(ref.valid && ref.width == 640 && ref.height == 16 && ref.binding.palette != gfx::kNullTexture);
    // Menus draw pieces of the sheet by texel rect.
    env.gfx.Frame({0, 0, 0, 0x80}, [&] { env.Draw(ref, 0, 0, 120, 160, 0, 0, 12, 16); });
    ASSERT_TRUE(env.gfx.PixelNear(60, 40, 0, 0, 0));
    ASSERT_TRUE(env.gfx.PixelNear(60, 120, 0, 0, 255));

    // Only a 640-wide 8-bit picture of at most a field's height is taken.
    Bytes wrong = Img({
        {"narrow", Tim2({TIM2_IDTEX8, 320, 16, {Bytes(320 * 16, 0)}, Clut256({}), 256})}
    });
    TexManager.EnterFixTextureZ(wrong.data());
    ASSERT_TRUE(Named("narrow") == nullptr);

    // A DeleteTextureBlock of every block takes the fixed ones too, as retail's does.
    TexManager.DeleteTextureBlock(-1);
    ASSERT_TRUE(Named("stayframe") == nullptr && Named("fixed") == nullptr);
    ASSERT_TRUE(!PortKeyLive(tex0.TBP0) && !PortKeyLive(tex0.CBP));
}

TEST(TexManager, FrameKeysAndClutLoads) {
    TexEnv         env;
    PortTextureRef frame = PortTextureFromTex0(SCE_GS_SET_TEX0(0, 10, 0, 10, 8, 0, 0, 0, 0, 0, 0, 0), 0);
    ASSERT_TRUE(frame.valid && frame.binding.texture == gfx::kMainTarget && frame.width == 640 && frame.height == 480);
    PortTextureRef back = PortTextureFromTex0(SCE_GS_SET_TEX0(0xFFF, 10, 0, 10, 8, 0, 0, 0, 0, 0, 0, 0), 1 << 5);
    ASSERT_TRUE(back.valid && back.binding.texture == gfx::kPreviousFrame && back.binding.filter == gfx::Filter::Linear);

    Bytes img = Img({
        {"text", Tim2({TIM2_IDTEX8, 2, 1, {{8, 9}}, Clut256({}), 256})}
    });
    Enter(img, 0);
    CTexture *text = Named("text");
    sceGsTex0 tex0 = Tex0Of(text);

    // SetClut's upload: a 16x16 PSMCT32 image in CSM1 order, GS alpha.
    std::vector<u_int> clut(256, Gs(0, 0, 0));
    clut[16] = Gs(255, 0, 0, 0x40);
    clut[17] = Gs(0, 255, 0);
    ASSERT_TRUE(PortLoadClut(tex0.CBP, clut.data()));
    uint32_t palette[256];
    ASSERT_TRUE(PortPaletteEntries(tex0.CBP, palette));
    ASSERT_TRUE(palette[8] == Rgba(255, 0, 0, 0x80) && palette[9] == Rgba(0, 255, 0));
    PortTextureRef ref = PortTextureFromCTexture(text);
    env.gfx.Frame({0, 0, 0, 0x80}, [&] { env.Draw(ref, 0, 0, 200, 100, 0, 0, 2, 1); });
    ASSERT_TRUE(env.gfx.PixelNear(150, 50, 0, 255, 0));

    // A TEX0 whose CBP names another palette draws with that one.
    Bytes other = Img({
        {"other", Tim2({TIM2_IDTEX8, 2, 1, {{8, 9}}, Clut256({{16, Gs(0, 0, 255)}, {17, Gs(0, 0, 255)}}), 256})}
    });
    Enter(other, 0);
    sceGsTex0 swapped = tex0;
    swapped.CBP = Tex0Of(Named("other")).CBP;
    u_long bits;
    std::memcpy(&bits, &swapped, sizeof bits);
    PortTextureRef recoloured = PortTextureFromTex0(bits, 0);
    ASSERT_TRUE(recoloured.valid && recoloured.binding.texture == ref.binding.texture &&
                recoloured.binding.palette != ref.binding.palette);
}

TEST(TexManager, ReplacingEffectBlockKeepsCachedBindings) {
    TexEnv env;
    Bytes  effect = Img({
        {"menu_effect", Tim2({TIM2_RGB32, 2, 2, {Rgba32({Gs(0, 255, 0), Gs(0, 255, 0), Gs(0, 255, 0), Gs(0, 255, 0)})}, {}, 0})}
    });
    Bytes  model = Img({
        {"head", Tim2({TIM2_RGB32, 2, 2, {Rgba32({Gs(255, 0, 0), Gs(255, 0, 0), Gs(255, 0, 0), Gs(255, 0, 0)})}, {}, 0})},
        {"body", Tim2({TIM2_RGB32, 2, 2, {Rgba32({Gs(0, 0, 255), Gs(0, 0, 255), Gs(0, 0, 255), Gs(0, 0, 255)})}, {}, 0})}
    });
    Enter(effect, 25);
    Enter(model, 24);
    char             water[] = "#water#32#32#4";
    char             end[] = "";
    LOADTEXTURE_INFO targets[] = {
        {water, 13, 0},
        {end,   0,  0}
    };
    TexManager.LoadTextureBlock(-1, targets, nullptr);

    CTexture          *head = Named("head");
    CTexture          *body = Named("body");
    CTexture          *target = Named("water");
    char               head_name[] = "head";
    char               body_name[] = "body";
    char               water_name[] = "water";
    char               effect_name[] = "menu_effect";
    int                effect_handle = TexManager.GetTextureHandle(effect_name, -1);
    int                head_handle = TexManager.GetTextureHandle(head_name, -1);
    int                body_handle = TexManager.GetTextureHandle(body_name, -1);
    int                water_handle = TexManager.GetTextureHandle(water_name, -1);
    gfx::TextureHandle head_image = PortTextureFromCTexture(head).binding.texture;
    gfx::TextureHandle body_image = PortTextureFromCTexture(body).binding.texture;
    PortTextureRef     water_ref = PortTextureFromCTexture(target);
    gfx::TextureHandle water_image = water_ref.binding.texture;

    TexManager.DeleteTextureBlock(25);
    ASSERT_EQ(TexManager.CleanUpTextureList(), 1);
    Enter(effect, 25);

    env.gfx.Frame({0, 0, 0, 0x80}, [&] {
        gfx::SetRenderTarget(water_image);
        env.Draw(PortTextureFromHandle(body_handle), 0, 0, 32, 32, 0, 0, 2, 2);
        gfx::SetRenderTarget(gfx::kMainTarget);
        env.Draw(PortTextureFromHandle(head_handle), 0, 0, 100, 100, 0, 0, 2, 2);
        env.Draw(PortTextureFromCTexture(body), 100, 0, 100, 100, 0, 0, 2, 2);
        env.Draw(water_ref, 200, 0, 100, 100, 0, 0, 32, 32);
    });
    EXPECT_TRUE(env.gfx.PixelNear(50, 50, 255, 0, 0));
    EXPECT_TRUE(env.gfx.PixelNear(150, 50, 0, 0, 255));
    EXPECT_TRUE(env.gfx.PixelNear(250, 50, 0, 0, 255));
    EXPECT_EQ(Named("head"), head);
    EXPECT_EQ(Named("body"), body);
    EXPECT_EQ(Named("water"), target);
    EXPECT_EQ(PortTextureFromHandle(head_handle).binding.texture, head_image);
    EXPECT_EQ(PortTextureFromHandle(body_handle).binding.texture, body_image);
    EXPECT_EQ(PortTextureFromHandle(water_handle).binding.texture, water_image);
    EXPECT_EQ(gfx::FindNamedRenderTarget("water"), water_image);
    // The reloaded block takes the entry it left.
    EXPECT_EQ(TexManager.GetTextureHandle(effect_name, -1), effect_handle);
}
