#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uDebugTex;
void main(){
    FragColor = texture(uDebugTex, vUV);
}


