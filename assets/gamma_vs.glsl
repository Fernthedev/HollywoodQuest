#version 310 es

layout (location = 0) out vec2 texCoords;

const vec2 pos[4] = vec2[4](
    vec2(-1.0, 1.0),
    vec2(-1.0,-1.0),
    vec2( 1.0, 1.0),
    vec2( 1.0,-1.0)
);

void main() {
    texCoords = 0.5 * pos[gl_VertexID] + vec2(0.5);
    // Flip image upside down. glReadPixels will flip it again, so we get the normal image.
    // texCoords.y = 1.0 - texCoords.y;
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}
