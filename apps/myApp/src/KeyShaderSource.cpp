#include "KeyShaderSource.h"

const std::string &getKeyFragmentShaderSource() {
    static const std::string kFragment = R"(
#version 150
uniform sampler2DRect tex0;
uniform float keyHue;
uniform float keyHueRange;
uniform float keyMinSat;
uniform float keyMinVal;
uniform float levels;
uniform float edgeStrength;
uniform float time;
uniform float bpm;
uniform float pulseAmount;
uniform float pulseColorize;
uniform float pulseHueMode;
uniform float pulseHueShift;
uniform float pulseAttack;
uniform float pulseDecay;
uniform float pulseHueBoost;
uniform float wooferOn;
uniform float wooferStrength;
uniform float wooferFalloff;
uniform float satOn;
uniform float satScale;
uniform float kaleidoOn;
uniform float kaleidoSegments;
uniform float kaleidoSpin;
uniform vec2 texSize;
uniform float halftoneOn;
uniform float halftoneScale;
uniform float halftoneEdge;

in vec2 vTexCoord;
out vec4 outputColor;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)),
                d / (q.x + e),
                q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    float phase = fract(time * (bpm / 60.0));
    float attack = max(0.001, pulseAttack);
    float decay = max(0.001, pulseDecay);
    float ramp = smoothstep(0.0, attack, phase);
    float fall = (phase <= attack) ? 1.0 : exp(-(phase - attack) * decay);
    float kick = ramp * fall;
    float boostedKick = min(1.0, kick * pulseHueBoost);
    float pulse = 1.0 + (pulseAmount * boostedKick);

    vec2 coord = vTexCoord;
    if (kaleidoOn > 0.5) {
        vec2 center = texSize * 0.5;
        vec2 p = coord - center;
        float r = length(p);
        float angle = atan(p.y, p.x) + kaleidoSpin * time;
        float segments = max(1.0, kaleidoSegments);
        float sector = 6.2831853 / segments;
        angle = mod(angle, sector);
        angle = abs(angle - sector * 0.5);
        vec2 newP = vec2(cos(angle), sin(angle)) * r;
        coord = newP + center;
    }

    if (wooferOn > 0.5) {
        vec2 center = texSize * 0.5;
        vec2 p = coord - center;
        float maxR = max(1.0, min(texSize.x, texSize.y) * 0.5);
        float rNorm = length(p) / maxR;
        float falloff = pow(clamp(1.0 - rNorm, 0.0, 1.0), wooferFalloff);
        float bulge = 1.0 + (wooferStrength * boostedKick * falloff);
        coord = center + (p * bulge);
    }

    coord = clamp(coord, vec2(0.0), texSize - 1.0);

    vec3 rgb = texture(tex0, coord).rgb;
    float dotMask = 1.0;
    if (halftoneOn > 0.5) {
        float cell = max(2.0, halftoneScale);
        vec2 cellSize = vec2(cell);
        vec2 cellCenter = (floor(coord / cellSize) + 0.5) * cellSize;
        vec2 offset = coord - cellCenter;
        float lum = luma(rgb);
        float radius = (1.0 - lum) * 0.5 * cell;
        float edge = max(0.001, radius * halftoneEdge);
        float dist = length(offset);
        dotMask = 1.0 - smoothstep(radius - edge, radius + edge, dist);
    }
    vec3 hsv = rgb2hsv(rgb);

    float hueDist = abs(hsv.x - keyHue);
    hueDist = min(hueDist, 1.0 - hueDist);

    float hueOk = 1.0 - smoothstep(keyHueRange, keyHueRange + 0.02, hueDist);
    float satOk = smoothstep(keyMinSat, keyMinSat + 0.05, hsv.y);
    float valOk = smoothstep(keyMinVal, keyMinVal + 0.05, hsv.z);

    float keyMask = hueOk * satOk * valOk;
    float alpha = 1.0 - keyMask;

    float safeLevels = max(levels, 2.0);
    vec3 poster = floor(rgb * safeLevels) / (safeLevels - 1.0);

    float lumC = luma(rgb);
    float lumR = luma(texture(tex0, coord + vec2(1.0, 0.0)).rgb);
    float lumU = luma(texture(tex0, coord + vec2(0.0, 1.0)).rgb);
    float edge = abs(lumC - lumR) + abs(lumC - lumU);

    vec3 color = mix(rgb, poster, 0.85);
    color += edgeStrength * edge;
    color *= pulse;
    color = mix(color, color * vec3(1.1, 0.85, 1.2), pulseColorize * boostedKick);
    if (abs(pulseHueMode) > 0.5) {
        vec3 hsvOut = rgb2hsv(color);
        float shift = (pulseHueShift / 360.0) * boostedKick;
        if (pulseHueMode > 0.0) {
            hsvOut.x = fract(hsvOut.x - shift);
        } else {
            hsvOut.x = fract(hsvOut.x + shift);
        }
        color = hsv2rgb(hsvOut);
    }
    if (satOn > 0.5) {
        vec3 hsvSat = rgb2hsv(color);
        hsvSat.y = clamp(hsvSat.y * satScale, 0.0, 1.0);
        color = hsv2rgb(hsvSat);
    }
    color = clamp(color, 0.0, 1.0);

    color *= dotMask;
    outputColor = vec4(color * alpha, alpha * dotMask);
}
)";
    return kFragment;
}
