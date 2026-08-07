#version 410 core

in vec3 interpolated_pos;
in vec3 interpolated_normal;

out vec4 frag_colour;

void main (void) {
    frag_colour = vec4 (normalize(interpolated_normal) * 0.5 + 0.5, 1.0);
}