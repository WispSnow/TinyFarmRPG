#version 330 core
in vec2 vUV;         // 输入的纹理坐标
out vec4 FragColor;  // 输出的片元颜色

// --- Uniforms: 由C++代码传入的参数 ---
uniform sampler2D uTex;     // 需要被模糊的输入纹理
uniform vec2 uTexelSize;    // 单个纹理像素的大小 (1.0/纹理宽度, 1.0/纹理高度)
uniform vec2 uDirection;    // 模糊方向：(1,0)表示水平, (0,1)表示垂直
uniform float uSigma;       // 高斯函数的Sigma值，控制模糊的强度/半径

// --- 高斯权重函数 ---
// 根据距离(x)和Sigma值(s)计算高斯权重
float weight(float x, float s){ 
    return exp(-0.5 * (x*x)/(s*s));
}

void main(){
    vec3 sum = vec3(0.0);   // 用于累加加权后的像素颜色
    float wsum = 0.0;       // 用于累加权重值

    // --- 核心模糊循环 ---
    // 这是一个13次采样的模糊核（-6到+6） 
    for (int i = -6; i <= 6; ++i) {
        float fi = float(i); // 将循环变量转为浮点数

        // 1. 计算当前采样点的权重
        float wi = weight(fi, max(uSigma, 0.001));

        // 2. 计算当前采样点的纹理坐标(UV)
        // 这是最关键的一步：从中心点(vUV)出发，沿着指定方向(uDirection)
        // 移动 i 个像素(uTexelSize * fi)的距离
        vec2 uv = vUV + uDirection * uTexelSize * fi;

        // 3. 采样并累加
        // 获取采样点的颜色，乘以其权重，然后加到sum中 
        sum += texture(uTex, uv).rgb * wi;
        wsum += wi; // 
    }

    // --- 归一化并输出 ---
    // 将加权颜色总和除以权重总和，得到加权平均值
    // 这样做可以保证模糊后的图像整体亮度不变
    FragColor = vec4(sum / max(wsum, 1e-6), 1.0);
}