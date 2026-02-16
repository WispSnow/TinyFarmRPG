#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
out vec2 vUV;
void main(){
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}


