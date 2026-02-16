#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTex;
uniform int uUseTexture;
uniform vec3 uColor;
void main(){
    vec4 sample = (uUseTexture == 1) ? texture(uTex, vUV) : vec4(1.0);
    vec3 base = sample.rgb;
    vec3 emissive = base * vColor.rgb * uColor; // 顶点颜色携带强度/颜色，全局 uColor 可作为调色
    float alpha = clamp(sample.a * vColor.a, 0.0, 1.0);
    FragColor = vec4(emissive, alpha);
}
