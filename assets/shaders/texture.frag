#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uTex;
uniform int uUseTexture;
uniform int uTextureMode;

void main(){
    vec4 texColor = vec4(1.0);
    if (uUseTexture == 1) {
        texColor = texture(uTex, vUV);
        if (uTextureMode == 1) {
            texColor = vec4(1.0, 1.0, 1.0, texColor.r);
        }
    }
    FragColor = texColor * vColor;
}

