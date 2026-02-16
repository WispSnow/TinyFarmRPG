#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uTex;
uniform int uUseTexture;

void main(){
    vec4 texColor = vec4(1.0);
    if (uUseTexture == 1) {
        texColor = texture(uTex, vUV);
    }
    FragColor = texColor * vColor;
}


