// Per-frame data shared by scene and particle shaders (std140; matches FrameUniforms).
layout(set=1,binding=0) uniform Frame {
    mat4 viewProjection;
    vec4 eye;     // xyz: camera position.
    vec4 right;   // xyz: camera right (world space).
    vec4 up;      // xyz: camera up.
    vec4 fog;     // rgb: fog/background color (linear), a: density over squared distance.
    vec4 light;   // x: ambient, y: sun, z: local light.
    mat4 shadowViewProjection;
    vec4 sun;     // xyz: direction toward the sun (normalized), w: shadow strength (0 disables).
    vec4 shadow;  // x: world-space texel size, y: 1 / shadow-map resolution.
} frame;
float fogAmount(float distanceToEye) { return 1-exp(-distanceToEye*distanceToEye*frame.fog.a); }
