#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSceneTex;
uniform sampler2D uLightTex;
uniform vec3 uAmbient;
uniform sampler2D uEmissiveTex;
uniform sampler2D uBloomTex;
uniform float uBloomStrength;
void main(){
    vec3 scene = texture(uSceneTex, vUV).rgb;
    vec3 light = texture(uLightTex, vUV).rgb + uAmbient;
    light = clamp(light, 0.0, 1.0);
    vec3 emissive = texture(uEmissiveTex, vUV).rgb;
    vec3 bloom = texture(uBloomTex, vUV).rgb * uBloomStrength;
    FragColor = vec4(scene * light + emissive + bloom, 1.0);
}


