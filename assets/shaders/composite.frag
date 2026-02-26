#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSceneTex;
uniform sampler2D uLightTex;
uniform vec3 uAmbient;
uniform sampler2D uEmissiveTex;
uniform sampler2D uBloomTex;
uniform sampler2D uWorldVfxTex;
uniform float uBloomStrength;
void main(){
    vec3 scene = texture(uSceneTex, vUV).rgb;
    vec3 light = texture(uLightTex, vUV).rgb + uAmbient;
    light = clamp(light, 0.0, 1.0);
    vec3 emissive = texture(uEmissiveTex, vUV).rgb;
    vec3 bloom = texture(uBloomTex, vUV).rgb * uBloomStrength;
    vec4 worldVfx = texture(uWorldVfxTex, vUV);
    vec3 base = scene * light + emissive + bloom;
    // 当前 world 通道先按加色增量合成，避免对 Effekseer 混合语义做错误假设。
    // 该策略更适合 additive 类资源；alpha-blend 类资源应优先走 overlay 通道。
    vec3 composed = clamp(base + worldVfx.rgb, vec3(0.0), vec3(1.0));
    FragColor = vec4(composed, 1.0);
}
