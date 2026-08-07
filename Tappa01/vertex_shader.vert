#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 model;
uniform mat4 vp;

void main (void) {
    gl_Position = vp * model * vec4 (position, 1.0);
}