# TinyFarmRPG Wasm Smoke

最小 SDL3/WebGL2 冒烟工程，用于 Phase 0 验证 Emscripten 5.0.7、SDL3 port 与浏览器可打开产物。

```bash
source "$HOME/.local/emsdk/emsdk_env.sh"
emcmake cmake -S tools/wasm_smoke -B build/wasm-smoke -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/wasm-smoke
```

成功后会生成：

- `build/wasm-smoke/sdl3_minimal.html`
- `build/wasm-smoke/sdl3_minimal.js`
- `build/wasm-smoke/sdl3_minimal.wasm`
