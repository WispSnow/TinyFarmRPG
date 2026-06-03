#include "web_shell_ui.h"

#include <emscripten/emscripten.h>

namespace {

EM_JS(void, tf_web_shell_install_ui, (), {
    const rootId = "tinyfarm-web-shell";
    if (document.getElementById(rootId)) {
        return;
    }

    const style = document.createElement("style");
    style.textContent = `
        .tf-web-shell {
            position: fixed;
            inset: 0;
            pointer-events: none;
            font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            color: #f7f3e8;
            z-index: 20;
        }
        .tf-web-shell.is-hidden .tf-shell-panel {
            display: none;
        }
        .tf-web-shell .tf-shell-panel {
            pointer-events: auto;
            position: absolute;
            left: 18px;
            top: 18px;
            width: min(340px, calc(100vw - 36px));
            background: rgba(17, 24, 22, 0.82);
            border: 1px solid rgba(247, 243, 232, 0.22);
            border-radius: 8px;
            box-shadow: 0 16px 42px rgba(0, 0, 0, 0.28);
            backdrop-filter: blur(10px);
        }
        .tf-web-shell .tf-shell-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
            padding: 12px 14px 9px;
            border-bottom: 1px solid rgba(247, 243, 232, 0.14);
        }
        .tf-web-shell .tf-shell-title {
            font-size: 14px;
            font-weight: 700;
            letter-spacing: 0;
        }
        .tf-web-shell .tf-shell-status {
            font-size: 12px;
            color: #c9dacb;
            line-height: 1.35;
        }
        .tf-web-shell .tf-shell-body {
            display: grid;
            gap: 10px;
            padding: 12px 14px 14px;
        }
        .tf-web-shell .tf-shell-stats {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 8px;
        }
        .tf-web-shell .tf-shell-stat {
            min-width: 0;
            padding: 8px;
            background: rgba(247, 243, 232, 0.08);
            border-radius: 6px;
        }
        .tf-web-shell .tf-shell-label {
            display: block;
            font-size: 10px;
            color: rgba(247, 243, 232, 0.64);
            line-height: 1.1;
        }
        .tf-web-shell .tf-shell-value {
            display: block;
            margin-top: 3px;
            font-size: 13px;
            color: #ffffff;
            line-height: 1.15;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .tf-web-shell .tf-shell-actions {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
        }
        .tf-web-shell button {
            min-height: 32px;
            padding: 0 11px;
            border: 1px solid rgba(247, 243, 232, 0.26);
            border-radius: 6px;
            background: rgba(247, 243, 232, 0.12);
            color: #fff7e3;
            font: inherit;
            font-size: 12px;
            font-weight: 650;
            cursor: pointer;
        }
        .tf-web-shell button:hover,
        .tf-web-shell button:focus-visible {
            background: rgba(99, 154, 112, 0.44);
            outline: none;
        }
        .tf-web-shell .tf-shell-drawer {
            display: none;
            padding: 0 14px 13px;
            color: #d7e5d8;
            font-size: 12px;
            line-height: 1.45;
        }
        .tf-web-shell.show-diagnostics .tf-shell-drawer {
            display: block;
        }
        .tf-web-shell .tf-shell-tab {
            pointer-events: auto;
            position: absolute;
            left: 18px;
            top: 18px;
            display: none;
        }
        .tf-web-shell.is-hidden .tf-shell-tab {
            display: inline-flex;
        }
        @media (max-width: 520px) {
            .tf-web-shell .tf-shell-panel,
            .tf-web-shell .tf-shell-tab {
                left: 10px;
                top: 10px;
            }
            .tf-web-shell .tf-shell-panel {
                width: calc(100vw - 20px);
            }
        }
    `;
    document.head.appendChild(style);

    const root = document.createElement("div");
    root.id = rootId;
    root.className = "tf-web-shell";
    root.innerHTML = `
        <section class="tf-shell-panel" aria-label="TinyFarmRPG web status">
            <div class="tf-shell-header">
                <div>
                    <div class="tf-shell-title">TinyFarmRPG Web</div>
                    <div class="tf-shell-status" data-tf-status>Starting</div>
                </div>
                <button type="button" data-tf-hide>Hide</button>
            </div>
            <div class="tf-shell-body">
                <div class="tf-shell-stats">
                    <div class="tf-shell-stat"><span class="tf-shell-label">Map</span><span class="tf-shell-value" data-tf-map>Waiting</span></div>
                    <div class="tf-shell-stat"><span class="tf-shell-label">Draw</span><span class="tf-shell-value" data-tf-draw>Waiting</span></div>
                    <div class="tf-shell-stat"><span class="tf-shell-label">Audio</span><span class="tf-shell-value" data-tf-audio>Locked</span></div>
                    <div class="tf-shell-stat"><span class="tf-shell-label">WebGL</span><span class="tf-shell-value" data-tf-webgl>Pending</span></div>
                </div>
                <div class="tf-shell-actions">
                    <button type="button" data-tf-audio-button>Start Audio</button>
                    <button type="button" data-tf-tone disabled>Ping</button>
                    <button type="button" data-tf-diagnostics>Details</button>
                </div>
            </div>
            <div class="tf-shell-drawer" data-tf-drawer>RmlUi, Effekseer, and Bloom remain deferred in this walking skeleton.</div>
        </section>
        <button type="button" class="tf-shell-tab" data-tf-show>UI</button>
    `;
    document.body.appendChild(root);

    const state = {
        audioContext: null,
        audioUnlocked: false,
    };
    window.__tinyFarmWebShell = state;

    const audioValue = root.querySelector("[data-tf-audio]");
    const audioButton = root.querySelector("[data-tf-audio-button]");
    const toneButton = root.querySelector("[data-tf-tone]");
    const canvas = Module["canvas"] || document.querySelector("canvas");

    const playTone = () => {
        const ctx = state.audioContext;
        if (!ctx || ctx.state !== "running") {
            return;
        }
        const oscillator = ctx.createOscillator();
        const gain = ctx.createGain();
        oscillator.type = "triangle";
        oscillator.frequency.value = 523.25;
        gain.gain.setValueAtTime(0.0001, ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.12, ctx.currentTime + 0.015);
        gain.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + 0.16);
        oscillator.connect(gain);
        gain.connect(ctx.destination);
        oscillator.start();
        oscillator.stop(ctx.currentTime + 0.18);
    };

    const unlockAudio = async () => {
        const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
        if (!AudioContextCtor) {
            audioValue.textContent = "Unavailable";
            console.warn("web audio smoke: AudioContext unavailable");
            return;
        }
        if (!state.audioContext) {
            state.audioContext = new AudioContextCtor();
        }
        try {
            await state.audioContext.resume();
        } catch (error) {
            audioValue.textContent = "Failed";
            console.warn("web audio smoke: resume failed", error);
            return;
        }

        state.audioUnlocked = state.audioContext.state === "running";
        audioValue.textContent = state.audioUnlocked ? "Ready" : state.audioContext.state;
        toneButton.disabled = !state.audioUnlocked;
        if (state.audioUnlocked) {
            playTone();
            console.log("web audio smoke: unlocked by user gesture");
        }
        canvas?.focus();
    };

    audioButton.addEventListener("click", unlockAudio);
    toneButton.addEventListener("click", () => {
        playTone();
        canvas?.focus();
    });
    root.querySelector("[data-tf-hide]").addEventListener("click", () => {
        root.classList.add("is-hidden");
        canvas?.focus();
    });
    root.querySelector("[data-tf-show]").addEventListener("click", () => {
        root.classList.remove("is-hidden");
    });
    root.querySelector("[data-tf-diagnostics]").addEventListener("click", () => {
        root.classList.toggle("show-diagnostics");
        canvas?.focus();
    });
});

EM_JS(void, tf_web_shell_set_status, (const char* status), {
    const root = document.getElementById("tinyfarm-web-shell");
    const target = root?.querySelector("[data-tf-status]");
    if (target) {
        target.textContent = UTF8ToString(status);
    }
});

EM_JS(void, tf_web_shell_set_map_stats, (int layer_count, int tileset_count, int batch_count, int map_width, int map_height), {
    const root = document.getElementById("tinyfarm-web-shell");
    if (!root) {
        return;
    }
    const mapTarget = root.querySelector("[data-tf-map]");
    const drawTarget = root.querySelector("[data-tf-draw]");
    if (mapTarget) {
        mapTarget.textContent = String(map_width) + "x" + String(map_height) + " px";
    }
    if (drawTarget) {
        drawTarget.textContent =
            String(layer_count) + " layers, " +
            String(tileset_count) + " atlases, " +
            String(batch_count) + " batches";
    }
});

EM_JS(void, tf_web_shell_report_webgl_features, (), {
    const root = document.getElementById("tinyfarm-web-shell");
    const target = root?.querySelector("[data-tf-webgl]");
    const drawer = root?.querySelector("[data-tf-drawer]");
    const canvas = Module["canvas"] || document.querySelector("canvas");
    const gl = canvas?.getContext("webgl2");
    if (!gl) {
        if (target) {
            target.textContent = "Unavailable";
        }
        console.warn("webgl feature probe: webgl2 unavailable");
        return;
    }

    const features = {
        colorBufferFloat: !!gl.getExtension("EXT_color_buffer_float"),
        textureFloatLinear: !!gl.getExtension("OES_texture_float_linear"),
        anisotropy: !!gl.getExtension("EXT_texture_filter_anisotropic"),
    };
    if (target) {
        target.textContent = features.colorBufferFloat ? "Float RT ready" : "Float RT off";
    }
    if (drawer) {
        drawer.textContent =
            "RmlUi deferred. Effekseer off. Bloom off. Float RT: " +
            (features.colorBufferFloat ? "yes" : "no") +
            ". Float linear: " +
            (features.textureFloatLinear ? "yes" : "no") +
            ".";
    }
    console.log(
        "webgl feature probe: color_buffer_float=" +
        (features.colorBufferFloat ? "yes" : "no") +
        " texture_float_linear=" +
        (features.textureFloatLinear ? "yes" : "no") +
        " anisotropy=" +
        (features.anisotropy ? "yes" : "no"));
});

} // namespace

namespace tinyfarm::web {

void installShellUi() {
    tf_web_shell_install_ui();
}

void setShellStatus(const char* status) {
    tf_web_shell_set_status(status != nullptr ? status : "");
}

void setShellMapStats(int layer_count, int tileset_count, int batch_count, int map_width, int map_height) {
    tf_web_shell_set_map_stats(layer_count, tileset_count, batch_count, map_width, map_height);
}

void reportShellWebGlFeatures() {
    tf_web_shell_report_webgl_features();
}

} // namespace tinyfarm::web
