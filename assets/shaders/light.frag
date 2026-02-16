#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform int uLightType; // 0: point, 1: spot, 2: directional
uniform vec2 uSpotDir;  // 局部矩形坐标（与 vUV 对齐）的归一化方向
uniform float uSpotInnerCos;
uniform float uSpotOuterCos;
// Directional light params
uniform vec2 uDir2D;       // 方向光方向（与 vUV 对齐），需要已归一化
uniform float uDirOffset;  // 0..1，过渡中心位置（类似地平线位置）
uniform float uDirSoftness;// 0..0.5，过渡柔和宽度
uniform float uMiddayBlend; // 0..1，中午混合强度，1 表示全屏近似均匀
// 计算相对于中心(0.5, 0.5)的平方衰减（0..1）
float computeAttenuation(vec2 uv) {
    vec2 d = uv - vec2(0.5);
    float dist = length(d) * 2.0; // 0..1 近似半径
    float a = clamp(1.0 - dist, 0.0, 1.0);
    return a * a;
}

// 计算聚光锥形遮罩（0..1）
float computeSpotMask(vec2 uv, vec2 spotDir, float innerCos, float outerCos) {
    vec2 d = uv - vec2(0.5);
    vec2 dir = normalize(spotDir);
    vec2 l = (length(d) > 1e-5) ? normalize(d) : dir; // 避免中心处零向量
    float cd = dot(l, dir);
    if (abs(outerCos - innerCos) < 1e-5) {
        return 1.0;
    }
    return smoothstep(outerCos, innerCos, cd);
}

// 方向光的屏幕空间渐变遮罩
float computeDirectionalMask(vec2 uv, vec2 dir2d, float offset, float softness, float middayBlend) {
    vec2 nd = normalize(dir2d);
    float t = dot(nd, uv - vec2(0.5)) + 0.5; // 将中心对齐到 0.5
    float edge0 = clamp(offset - softness, 0.0, 1.0);
    float edge1 = clamp(offset + softness, 0.0, 1.0);
    float ramp = smoothstep(edge0, edge1, t);
    return mix(ramp, 1.0, clamp(middayBlend, 0.0, 1.0));
}

// 三类光源的贡献评估
vec3 evaluatePoint(vec2 uv, vec3 color, float intensity) {
    float attenuation = computeAttenuation(uv);
    return color * (intensity * attenuation);
}

vec3 evaluateSpot(vec2 uv, vec3 color, float intensity, vec2 spotDir, float innerCos, float outerCos) {
    float attenuation = computeAttenuation(uv);
    float spotMask = computeSpotMask(uv, spotDir, innerCos, outerCos);
    return color * (intensity * attenuation * spotMask);
}

vec3 evaluateDirectional(vec3 color, float intensity, float dirMask) {
    return color * (intensity * dirMask);
}

void main(){
    vec3 rgb;
    if (uLightType == 2) {
        float dirMask = computeDirectionalMask(vUV, uDir2D, uDirOffset, uDirSoftness, uMiddayBlend);
        rgb = evaluateDirectional(uLightColor, uLightIntensity, dirMask);
    } else if (uLightType == 1) {
        rgb = evaluateSpot(vUV, uLightColor, uLightIntensity, uSpotDir, uSpotInnerCos, uSpotOuterCos);
    } else {
        rgb = evaluatePoint(vUV, uLightColor, uLightIntensity);
    }
    FragColor = vec4(rgb, 1.0);
}


