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
    // Effekseer 输出到 world-vfx FBO 的 alpha 语义会随资源混合模式变化；
    // 先采用纯加色叠加，避免透明区出现黑底/黑框。
    vec3 composed = base + worldVfx.rgb;
    FragColor = vec4(composed, 1.0);
}
