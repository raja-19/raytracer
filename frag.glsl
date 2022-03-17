#version 430 core

in vec2 vtexcoord;
out vec4 fcolor;

uniform sampler2D tex;

void main() {
    fcolor = texture(tex, vtexcoord);
}
