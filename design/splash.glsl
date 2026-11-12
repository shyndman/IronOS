precision mediump float;

/** @resolution */
uniform vec2 u_resolution;

/** @time */
uniform float u_time;

/** @label Sprite Sheet */
uniform sampler2D u_sprite;

/**
 * @label FPS
 * @range 1, 30
 * @default 6
 */
uniform float u_fps;

void main() {
  vec2 uv = gl_FragCoord.xy / u_resolution;

  float frame = mod(floor(u_time * u_fps), 10.0);
  float col = mod(frame, 5.0);
  float row = 1.0 - floor(frame / 5.0);

  vec2 spriteUV = vec2((col + uv.x) / 5.0, (row + uv.y) / 2.0);
  gl_FragColor = texture2D(u_sprite, spriteUV);
}
