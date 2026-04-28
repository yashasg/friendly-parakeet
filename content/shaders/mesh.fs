#version 330

in vec3 fragNormal;

uniform vec4 colDiffuse;

out vec4 finalColor;

void main() {
    float shade = 0.35 + 0.65 * clamp(fragNormal.y, 0.0, 1.0);
    finalColor = vec4(colDiffuse.rgb * shade, colDiffuse.a);
}
