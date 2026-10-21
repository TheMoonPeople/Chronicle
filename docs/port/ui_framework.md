I have updated your **`chronicle-port-dev`** skill (both `SKILL.md` and a new `docs/UI_FRAMEWORK.md` reference inside the skill) with the modular Option B architecture, the 3-slot TrueType font pipeline (`SourceSansProSemibold.ttf`), the recessed/engraved button text rendering, and the raised-bevel/gradient Weapon Status typography.

  

Below is the complete markdown documentation file (**`docs/UI_FRAMEWORK.md`**) that you can also save directly into your repository's `docs/` folder for your team and other agents:

  

Markdown

````
# Modular UI Framework (`UI::` Namespace)

The native UI framework replaces hard-coded PS2 2D sprite and bitmap draws with a declarative, resolution-independent widget toolkit in the `UI::` namespace.

---

## 1. Directory & File Architecture

The UI framework uses a modular Option B layout so each header and implementation unit remains small (~120–340 lines), isolated by responsibility, and fast to compile:

```text
port/
├── include/
│   ├── ui_framework.hpp               # Public umbrella header (#includes all sub-headers below)
│   └── ui_framework/
│       ├── types.hpp                  # Palette constants, Metrics, Alignment, FontWeight, Anchor, Rect, ZLayer, LayerQueue
│       ├── text.hpp                   # TextStyle, Label, WrapText, DrawTextBox, DrawCounter, DrawStatus*
│       ├── surfaces.hpp               # WindowPreset, TrackerWindow, PaperWindow, HelpBox, Window, TabPlate, Divider, SubWindow
│       ├── buttons.hpp                # ButtonPreset, ButtonStyle (Thick recessed / Thin painted), BevelButton, TextureButton
│       ├── widgets.hpp                # ItemIcon, Cursor, Brackets, ScrollBar, ProgressBar
│       └── dialogs.hpp                # DialogOption, DialogStyle, DialogBox, ConfirmModal, ListViewConfig, ListView
└── src/
    └── ui_framework/
        ├── internal.hpp               # Private shared helpers (kBaseCapHeight, ScopedShadow, texture lookups, DrawRoundedRect)
        ├── surfaces.cpp               # Procedural BlueStone & Paper noise/SDF generator, VRAM cache, windows, banners, dividers
        ├── text.cpp                   # Label, WrapText, DrawScaledText (recessed, raised bevel, vertical gradient), DrawStatus*
        ├── buttons.cpp                # BevelButton (Terracotta, AmberGold, StoneGreen + recessed text) & TextureButton
        ├── widgets.cpp                # ItemIcon, Cursor, Brackets, ScrollBar, ProgressBar
        └── dialogs.cpp                # LayerQueue, DialogBox, ConfirmModal, ListView
````

### Build & Header Rules

- **Header Placement**: Keep `ui_framework.hpp` in `port/include/` (never in `port/include/stubs/`, which is strictly reserved for PS2 SDK / Metrowerks shadow headers). Existing translation units can `#include "ui_framework.hpp"` for the full toolkit or include individual sub-headers (e.g. `#include "ui_framework/buttons.hpp"`).
    
      
    
- **CMake Globbing**: `port/CMakeLists.txt` globs `port/src/**/*.cpp` via `file(GLOB_RECURSE PORT_SOURCES CONFIGURE_DEPENDS ${PORT_DIR}/*.cpp)`. Any `.cpp` file added under `port/src/ui_framework/` is compiled and linked automatically. Private headers inside `port/src/ui_framework/` must use `.hpp` (`internal.hpp`).
    
      
    

## 2. Three-Slot TrueType Font Pipeline (`capfont` & `caption`)

`port/src/captionfont.{hpp,cpp}` and `port/src/caption_text.{hpp,cpp}` manage three TrueType font slots (`g_fonts[3]`), switched via RAII guards calling `capfont::UseFontSlot(int slot)`:

  

|**Slot**|**Font Asset (tools/font/ -> lang/)**|**UI::FontWeight**|**Primary Usage**|
|---|---|---|---|
|**Slot 0**|`ChronicleSharp-Bold.ttf` (`lang/font.ttf`)|`FontWeight::Normal`, `FontWeight::Bold`|Standard game messages, window titles and `YES`/`NO` confirmation buttons.|
|**Slot 1**|`ChronicleSharp-Regular.ttf` (`lang/font_thin.ttf`)|`FontWeight::Thin`|Weapon Menu action buttons (`EQUIP`, `CUSTOMIZE`, etc.), the area cards and the floor select's boss names.|
|**Slot 2**|`SourceSansProSemibold.ttf` (`lang/SourceSansProSemibold.ttf`)|`FontWeight::Status`|Weapon Status UI (`At`, `Ed`, `Sp`, `Mp`, element tags `Fl`/`Ch`/`Th`/`Wi`/`Ho`, and status/bar numbers `12/28`, `67/98`, `22`, `34`, `72`, `13`).|

- **Flatpak & Sandbox Resilience**: `port/CMakeLists.txt` copies and installs `tools/font/SourceSansProSemibold.ttf` into `lang/SourceSansProSemibold.ttf`. In addition, `port/src/caption_text.cpp` embeds `SourceSansProSemibold.ttf` at compile time via C++26 `#embed` (`capfont::LoadStatusFontMemory`) as an automatic fallback.
    
      
    

## 3. Typography & Visual Effects (`ui_framework/text.hpp`)

`UI::TextStyle` and `DrawScaledText` in `port/src/ui_framework/text.cpp` support four specialized visual treatments:

  

### A. Recessed / Engraved Button Text (`recessed = true`)

Used on `BevelButton` (`ButtonStyle::Thick`) in place of a traditional drop shadow so text looks stamped/carved into the button surface:

  

1. **Bottom-Right Rim Catch-Light (`+1.0px, +1.0px`)**: Drawn with `recess_light_r/g/b` (`opacity = 0.70f`) where top-left light catches the lower carved edge.
    
      
    
2. **Top-Left Inner Cast Shadow (`-1.0px, -1.0px`)**: Drawn with `recess_dark_r/g/b` (`edge_width = 0.65f`, `opacity = 0.95f`) inside the upper carved lip.
    
      
    
3. **Main Stamped Face (`0, 0`)**: Off-white (`236, 230, 218`) with a `0.55f` material-colored inner rim.
    
      
    

|**ButtonPreset**|**recess_dark_r/g/b (Top-Left Shadow)**|**recess_light_r/g/b (Bottom-Right Catch-Light)**|
|---|---|---|
|`Terracotta`|`82, 36, 18`|`252, 178, 132`|
|`AmberGold`|`88, 58, 20`|`255, 222, 152`|
|`StoneGreen`|`28, 76, 62`|`168, 238, 214`|

### B. Raised 3D Bevel + Outer Stroke (`raised_bevel = true`)

Used by `UI::DrawStatusNumber` and `UI::DrawStatusSlashPair` to replicate the stock PS2 Weapon Status numbers:

  

1. **Pass 1 (`+0.9px, +0.9px`)**: Extruded slate-blue bevel body (`RGB: 108, 134, 192`) + thick dark navy outer stroke (`RGB: 10, 18, 46`, `outline_width = 1.55f`).
    
      
    
2. **Pass 2 (`0, 0`)**: Cool-white top face (`RGB: 242, 248, 255`) + outer stroke, vertically squished (`yscale = 0.78f`, `xscale = 1.04f`) to match the compact stock bitmap digits.
    
      
    

### C. Vertical Color Gradients (`vertical_grad = true` via `caption::FillField`)

Used by `UI::DrawStatusText` for Weapon Status abbreviations (`At`, `Ed`, `Sp`, `Mp`) and element labels (`Fl`, `Ch`, `Th`, `Wi`, `Ho`):

  

- Generates a cached 2x16 `caption::FillField` that transitions from a pale cream-white highlight in the top ~35% of the glyph down into the saturated base color in the lower 65%:
    
      
    - **`At` (Attack)**: Mint-Green (`65, 235, 145`) with dark teal stroke (`8, 38, 36`)
        
          
        
    - **`Ed` (Endurance)**: Warm Gold (`235, 190, 35`) with dark bronze stroke (`38, 32, 10`)
        
          
        
    - **`Sp` (Speed)**: Seafoam Cyan-Blue (`75, 210, 215`) with dark navy-teal stroke (`8, 36, 46`)
        
          
        
    - **`Mp` (Magic)**: Rose-Lavender (`225, 145, 215`) with dark plum stroke (`36, 18, 44`)
        
          
        
- **Note**: `style.weight` must remain `0.0f` whenever `style.field` is active so `caption::DrawLine`'s `textured_letter()` path is used.
    
      
    

### D. Thick vs. Thin Button Presets (`UI::ButtonStyle`)

- **`UI::ButtonStyle::Thick(preset, brightness)`**: Uses `FontWeight::Normal`, `text_scale = 1.08f`, `text_xscale = 1.25f`, `recessed_text = true`. Default for `YES` / `NO` confirmation dialogs (`ConfirmModal`, `DrawWeaponLeftConfirmDialog`).
    
      
    
- **`UI::ButtonStyle::Thin(preset, brightness)`**: Lettered as the painted weapon buttons are (`painted_text`): the English words letter by letter where the painting has them (`caption::DrawButtonWord`), others fitted in the narrower font with a soft brown glow. Used for Weapon Menu action buttons (`EQUIP`, `CUSTOMIZE`, `ATTRIBUTE`, `REPAIR`, `UPGRADE`, `STATUS BREAK`, `BUILD-UP`, `DISCARD`).
    
      
    

## 4. Surfaces, Buttons & PS2 Texture State Discipline

- **Procedural Surfaces (`surfaces.cpp`)**:
    
      
    - `WindowPreset::BlueStone` and `WindowPreset::Paper` generate periodic 512x512 FBM + soft-ridged noise textures with rounded-box SDF bevels (`GetProceduralWindowTex`), cached in VRAM by `(width, height, preset)` and validated via `PortKeyLive(tbp)`.
        
          
        
    - Optional PNG overrides can be placed at `textures/plates/ui_stone.png` or `textures/plates/ui_paper.png`.
        
          
        
    - `TrackerWindow::DrawStoneBanner` renders the 3-slice stone scroll header from `wepstatus` and centers a `UI::ColorGold` title (`scale = 1.14f` in the Weapon Menu).
        
          
        
- **Custom Texture Button Dimensions (Photoshop Reference)**:
    
      
    - **1x Native (640x480 Logical)**: `96 x 24` px visible button face (`96 x 32` px full UV cell with `4px` shadow margin; `26` px vertical row stride).
        
          
        
    - **4x 4K Scale (Recommended)**: `384 x 96` px visible button face (`384 x 128` px full UV cell; `104` px vertical row stride).
        
          
        
- **PS2 Texture State Discipline**:
    
      
    - Always call `setbilinear(0)` and `MenuTextureReload(BtlMenuReadBlock)` (or `BtlMenuExtBlock` under `#ifdef PAL` for `wepstatus`) before drawing paletted PS2 menu sprites (`StayTex`, `DrawMenuWaku`, `WepStatus`, `MenuCharaFace`) so bilinear filtering never bleeds magenta color-key texels.
        
          
        
    - Call `MenuTextureReload(0x1A)` before drawing `GameText` / `UI::Label` text.
        



## 5. Battle Menu Ring, In-Game Save/Load & 2D Sprite Clamping

* **Custom Ring Captions & Selection Brackets (`port/src/caption_text.cpp`, `port/src/battlemenu/main_ring.cpp`)**:
  * All Battle Menu ring captions preserve retail's baseline sizing (`kEm = 25.0f`, `kOutlineWidth = 1.6f`, `kLeastShrink = 0.62f`).
  * Because icon `5` (`SAVE`) has a 4-letter painted `label_width` of `56px` in `MENU_ICON_INFO`, `caption::Draw` expands `effective_box_w` to `124.0f` (matching icon `12`, `QUIT GAME`) exclusively when `custom_caption && icon == 5` contains `'/'` (`"SAVE/LOAD"`).
  * `DrawBattleMainOverlay()` in `port/src/battlemenu/main_ring.cpp` widens `DrawMenuWaku` on Town row `5` (`SAVE/LOAD`) using a `1.32f` caption-to-message width ratio and floors it to `GetMenuIconInfo(12)->label_width + 0x4A` so the animated yellow selection brackets frame `"SAVE/LOAD"` cleanly.
* **In-Game Save/Load Modal & Town Reload (`port/src/battlemenu/main_ring.cpp`, `port/src/gameloop.cpp`)**:
  * Selecting `SAVE/LOAD` in the Town Battle Menu opens a two-button `UI::ConfirmModal` (`SAVE` on top, `LOAD` on bottom).
  * Choosing `LOAD` invokes `InitMenuSave(SAVE_MENU_MODE_LOAD, MenuExtendReadBlock, buffer)` and sets `s_ingame_load_active = true`.
  * When `McAccess.LoadFromMc` finishes reading a slot into `SaveData`, `MenuSaveKey()` sets `SaveMenu.loaded = true` and `SaveMenu.key_no = SAVE_KEY_FADE_OUT`, causing `BattleMenuSaveKey()` to slide the icon back and transition directly to `BTLMENU_STATE_MAIN` (skipping `BTLMENU_STATE_SAVE_CLOSE`).
  * `StepInGameLoadMonitor()` checks `SaveMenu.loaded != 0` *before* checking `BattleMenuFlag == BTLMENU_STATE_MAIN`. Once the close transition completes, it calls `ExitBattleMenu(1)` and sets `NextMapNo = 9999` and `BattleMenuFlag = 0`.
  * In `port/src/gameloop.cpp`, `NextMapNo == 9999 && BattleMenuFlag == 0` extracts the loaded save's `map_no` (`SaveData + 0x1C8`), sets `MapNo = loaded_map`, `LocalMapNo = MapNo`, and `OldMapNo = -1`, and invokes `GameApplyLoopResult(GAME_MODE_TITLE, 2)` (`main_select_menu_no = 1`, `main_select_param = "e01"`) so `EditInit(nullptr)` performs a full Title-style town reload.
* **1:1 Atlas Bleed Clamping (`port/src/menu_draw.cpp`)**:
  * Retail PS2 2D menu calls frequently specify `texel.height` 1 or 2 texels taller than `screen.height` on 1:1 sprites (e.g., Georama plates `0x100x0x78` sampled from `0x100x0x79` in `Sozai`, and sockets `0x24x0x24` sampled from `0x24x0x25` in `HoleGray`/`HoleGold`).
  * At scaled PC resolutions, those extra texels sample the top row of the adjacent sprite below in the atlas. `DrawMenu2DSprite` clamps `texel.height = screen.height` whenever `texel.width == screen.width` and `texel.height` exceeds `screen.height` by 1–2 texels, and trims the completed gold Georama plate (`u = 0, v = 0x78, 0x100x0x78` in `Sozai`) to `0x77` (`119px`) so the orange gauge cap at row `239` (`0xEF`) does not bleed beneath the bottom-right border.