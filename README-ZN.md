[English](README.md) | **简体中文**

# TinyFarmRPG
**TinyFarmRPG** 是一个使用 EnTT, SDL3, OpenGL, RmlUi, Lua (Sol2), Effekseer, MiniAudio, FreeType, glm, ImGui, nlohmann-json 和 Tiled 开发的跨平台 C++ 农场 JRPG 演示游戏。

> 本项目是一个教学演示项目，是 “[C++ 游戏开发之旅](https://cppgamedev.top/)” 系列教程的第 6 篇。

本项目在上一篇 TinyFarm 的基础上，将 2D 农场经营演示逐步扩展为小型日式 RPG：保留农场种植、背包与快捷栏、地图切换、昼夜循环和存档等玩法，同时新增 Lua 脚本内容层、分层角色外观、RmlUi 生产 UI、Effekseer 特效，以及任务、商店、队伍与装备、招募、回合制战斗等 JRPG 核心闭环。游戏内置中英双语文本（通过 `config/user_settings.json` 中的 `language` 字段切换）。

## 操作说明
- 移动：`W/A/S/D` 或方向键 `↑/←/↓/→`
- 使用工具/物品：`鼠标左键`
- 取消工具/物品选择：`鼠标右键`
- 交互（对话/调查）：`F`
- 角色灯光切换（夜晚有效）：`L`
- 暂停：`P` 或 `Esc`
- 菜单（物品栏）：`I`；直达页签：`C` 装备 / `J` 任务 / `M` 地图 / `O` 设置
- 快捷栏：`Tab`
- 快捷栏选槽：`1` ~ `0`（对应 1~10）
- 菜单确认/取消：`Enter`/`Space`、`Esc`
- 操作提示栏开关：`F1`
- 镜头缩放/重置：`鼠标滚轮/中键`
- 引擎层调试UI：`F5`
- 游戏层调试UI：`F6`
- 支持手柄操作：左摇杆/十字键移动，`A` 确认/使用，`B` 取消，`X` 交互，`Y` 快捷栏，`LB/RB` 切换快捷栏槽位，`Start` 暂停，`Back` 菜单

## 网页版试玩
[TinyFarm](https://wispsnow.github.io/TinyFarmRPG-preview/)

## 游戏截图
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr1.webp" style='width: 640px;'/>
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr2.webp" style='width: 640px;'/>
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr3.webp" style='width: 640px;'/>
<img src="https://theorhythm.top/gamedev/TFR/screen_shot_tfr4.webp" style='width: 640px;'/>

## 第三方库
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

## 构建指南
项目使用 CMake 预设构建（需要 CMake 3.21+ 与支持 C++20 的编译器，推荐安装 [Ninja](https://ninja-build.org/)）。缺失的依赖项将在首次配置时通过 CMake FetchContent 自动下载（需要联网）：
```bash
git clone https://github.com/WispSnow/TinyFarmRPG.git
cd TinyFarmRPG
cmake --preset debug
cmake --build --preset debug
```

然后从构建目录运行可执行文件：
```bash
./build/debug/TinyFarmRPG-Darwin     # macOS
./build/debug/TinyFarmRPG-Linux      # Linux
build\debug\TinyFarmRPG-Windows.exe  # Windows
```

> 如果没有安装 Ninja，可改用 `debug-fallback` / `release-fallback` 预设，它们会回退到系统默认生成器（Windows 上为 Visual Studio，Linux/macOS 上为 Unix Makefiles）。

> Linux 系统依赖包、sanitizer 预设、测试与调试工具等详见 [docs/build_and_run.md](docs/build_and_run.md)；完整项目文档入口见 [docs/README.md](docs/README.md)。

> 注意：本项目使用了付费素材 [farm-rpg](https://emanuelledev.itch.io/farm-rpg)，因版权原因，仓库中不提供原始素材文件，只有图片占位符可确保程序正常运行（用于游戏开发学习已足够）。

> 如果你想要实现演示项目中的效果，可购买该素材后，将所有内容复制到 `assets/farm-rpg` 文件夹中（覆盖原始文件）即可。

如果你在下载此项目时遇到问题（尤其是在中国大陆网络环境下），可从 [百度网盘](https://pan.baidu.com/s/xxxxxx) 中下载全部源码（包含第三方库）进行编译；也可以将依赖库源码预先放入 `external/` 文件夹（目录命名参见 `cmake/Dependencies.cmake` 中各依赖的 `LOCAL_PATH`）。

# 致谢
- sprite & UI
    - https://emanuelledev.itch.io/farm-rpg
- 战斗素材（战斗背景 / 战斗音效 / 部分 Effekseer 特效）
    - RPG Maker MZ 默认素材（© Gotcha Gotcha Games Inc. / YOJI OJIMA）
    - Effekseer 官方示例特效：https://effekseer.github.io/
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

## 联系方式

如需支持或反馈，请通过本仓库的 Issues 版块反馈。您的反馈对于改进这一系列教程至关重要！

## 请我喝咖啡
<a href="https://ko-fi.com/ziyugamedev"><img src="https://storage.ko-fi.com/cdn/kofi2.png?v=3" alt="Buy Me A Coffee" width="200" /></a>
<a href="https://afdian.com/a/ziyugamedev"><img src="https://pic1.afdiancdn.com/static/img/welcome/button-sponsorme.png" alt="Support me on Afdian" width="200" /></a>

## QQ 交流群及我的微信二维码

<div style="display: flex; gap: 10px;">
  <img src="https://theorhythm.top/personal/qq_group.webp" width="200" />
  <img src="https://theorhythm.top/personal/wechat_qr.webp" width="200" />
</div>
