**English** | [简体中文](README-ZN.md)

# TinyFarmRPG
**TinyFarmRPG** is a cross-platform C++ farm-sim JRPG demo developed using EnTT, SDL3, OpenGL, RmlUi, Lua (Sol2), Effekseer, MiniAudio, FreeType, glm, ImGui, nlohmann-json, and Tiled.

> This project is an educational demonstration project and is the 6th part of the "[C++ Game Development Journey](https://cppgamedev.top/)" tutorial series.

Building on TinyFarm (the previous episode), this project extends a 2D farming demo into a small JRPG: it keeps farming, inventory & hotbar, map traveling, day-night cycle and save slots, while adding a Lua-driven content layer, layered character appearance, RmlUi production UI, Effekseer VFX, and the JRPG core loop — quests, shops, party & equipment, recruitment, and turn-based battles. The game ships with both English and Simplified Chinese text (switch via the `language` field in `config/user_settings.json`).

## Controls
- Move: `W/A/S/D` or Arrow Keys `↑/←/↓/→`
- Use Tool/Item: `Left Mouse Button`
- Cancel Tool/Item Selection: `Right Mouse Button`
- Interact (talk / investigate): `F`
- Toggle Character Light (night only): `L`
- Pause: `P` or `Esc`
- Menu (Inventory): `I`; jump to tabs: `C` Equipment / `J` Quests / `M` Map / `O` Options
- Hotbar: `Tab`
- Hotbar Slot Selection: `1` ~ `0` (corresponding to slots 1~10)
- Menu Confirm/Cancel: `Enter`/`Space`, `Esc`
- Toggle Control Prompt Bar: `F1`
- Camera Zoom/Reset: `Mouse Wheel/Middle Button`
- Engine Layer Debug UI: `F5`
- Game Layer Debug UI: `F6`
- Gamepad is supported: Left Stick/D-Pad to move, `A` Confirm/Use, `B` Cancel, `X` Interact, `Y` Hotbar, `LB/RB` Switch Hotbar Slot, `Start` Pause, `Back` Menu

## Game Screenshots
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr1.webp" style='width: 640px;'/>
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr2.webp" style='width: 640px;'/>
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr3.webp" style='width: 640px;'/>
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr4.webp" style='width: 640px;'/>

## Third-Party Libraries
* [EnTT](https://github.com/skypjack/entt)
* [SDL3](https://github.com/libsdl-org/SDL)
* [RmlUi](https://github.com/mikke89/RmlUi)
* [Lua](https://www.lua.org/)
* [Sol2](https://github.com/ThePhD/sol2)
* [Effekseer](https://github.com/effekseer/Effekseer)
* [MiniAudio](https://miniaud.io/)
* [FreeType](https://github.com/freetype/freetype)
* [HarfBuzz](https://github.com/harfbuzz/harfbuzz)
* [glm](https://github.com/g-truc/glm)
* [ImGui](https://github.com/ocornut/imgui)
* [stb](https://github.com/nothings/stb)
* [nlohmann-json](https://github.com/nlohmann/json)
* [spdlog](https://github.com/gabime/spdlog)
* [GoogleTest](https://github.com/google/googletest)

## Build Guide
The project is built with CMake presets (CMake 3.21+ and a C++20 compiler required; [Ninja](https://ninja-build.org/) is recommended). Missing dependencies will be automatically downloaded via CMake FetchContent during the first configure (internet access required):
```bash
git clone https://github.com/WispSnow/TinyFarmRPG.git
cd TinyFarmRPG
cmake --preset debug
cmake --build --preset debug
```

Then run the executable from the build directory:
```bash
./build/debug/TinyFarmRPG-Darwin     # macOS
./build/debug/TinyFarmRPG-Linux      # Linux
build\debug\TinyFarmRPG-Windows.exe  # Windows
```

> If Ninja is not installed, use the `debug-fallback` / `release-fallback` presets instead, which fall back to the system default generator (Visual Studio on Windows, Unix Makefiles on Linux/macOS).

> For Linux system packages, sanitizer presets, tests and debug tools, see [docs/build_and_run.md](docs/build_and_run.md). The full documentation entry point is [docs/README.md](docs/README.md).

> Note: This project uses paid assets from [farm-rpg](https://emanuelledev.itch.io/farm-rpg). Due to copyright reasons, the repository does not include the original asset files, only image placeholders to ensure the program runs properly (sufficient for game development learning purposes).

> If you want to achieve the effects shown in the demo project, you can purchase the asset pack and copy all contents to the `assets/farm-rpg` folder (overwriting the original files).

If you encounter problems downloading this project (especially in mainland China network environment), you can download the complete source code (including third-party libraries) from [Baidu Netdisk](https://pan.baidu.com/s/xxxxxx) for compilation, or pre-place the dependency sources into the `external/` folder (see the `LOCAL_PATH` of each dependency in `cmake/Dependencies.cmake` for directory naming).

# Acknowledgements
- sprite & UI
    - https://emanuelledev.itch.io/farm-rpg
- battle assets (battle backgrounds / battle SE / part of the Effekseer effects)
    - RPG Maker MZ default materials (© Gotcha Gotcha Games Inc. / YOJI OJIMA)
    - Effekseer official sample effects: https://effekseer.github.io/
- font
    - https://timothyqiu.itch.io/vonwaon-bitmap
    - https://github.com/lxgw/LxgwBright
- sound
    - https://ateliermagicae.itch.io/fantasy-ui-sound-effects
    - https://freesound.org/people/Benboncan/sounds/69422
    - https://freesound.org/people/michaelperfect/sounds/710298/
    - https://freesound.org/people/soundscalpel.com/sounds/110393/
    - https://freesound.org/people/Valenspire/sounds/699492/
    - https://freesound.org/people/wobesound/sounds/488393
    - https://kasse.itch.io/ui-buttons-sound-effects-pack
    - https://mixkit.co/free-sound-effects/garden/
    - https://mmvpm.itch.io/platformer-sound-fx-pack
    - https://pixabay.com/sound-effects/pick-axe-striking-rocks-2-63070/
    - https://tommusic.itch.io/free-fantasy-sfx-and-music-bundle
- music
    - https://nakatomo.itch.io/spring-music
    - https://tommusic.itch.io/free-fantasy-sfx-and-music-bundle
- Sponsors: `sino`, `李同学`, `swrainbow`, `爱发电用户_b7469`, `玉笔难图`, `jl`, `JKR`, `ElectGC`, `RitcheryZ`

## Contact

For support or feedback, please use the Issues section of this repository. Your feedback is essential for improving this tutorial series!

## Buy Me a Coffee
<a href="https://ko-fi.com/ziyugamedev"><img src="https://storage.ko-fi.com/cdn/kofi2.png?v=3" alt="Buy Me A Coffee" width="200" /></a>
<a href="https://afdian.com/a/ziyugamedev"><img src="https://pic1.afdiancdn.com/static/img/welcome/button-sponsorme.png" alt="Support me on Afdian" width="200" /></a>

## QQ Group and My WeChat QR Code

<div style="display: flex; gap: 10px;">
  <img src="https://theorhythm.top/personal/qq_group.webp" width="200" />
  <img src="https://theorhythm.top/personal/wechat_qr.webp" width="200" />
</div>
