/*
 * orb.js
 * ------
 * A faithful JavaScript port of the software-rendered orb
 * from the Sayri app (orb.c / orb.h). It writes RGBA bytes
 * into a caller-supplied Uint8Array; the caller displays them.
 *
 * The algorithm is a direct translation of orb.c:
 * fbm-style noise ribbons on a sphere, animated color ramp,
 * diffuse lighting, fresnel rim, soft coverage ramp and glow.
 */

const RES = 256; // match the original app's ORB_RES for crisp rendering

const clampf = (v, lo, hi) => Math.min(Math.max(v, lo), hi);

function renderOrbLine(y, rgba, width, height, time, audio) {
    const radius = 0.285 + audio * 0.018;
    const inv_radius = 1.0 / radius;

    const ld_len = Math.sqrt(0.35 * 0.35 + 0.45 * 0.45 + 1.0);
    const ldx = -0.35 / ld_len;
    const ldy = -0.45 / ld_len;
    const ldz = 1.0 / ld_len;

    const v = y / height;
    const py = v - 0.5;
    const max_reach = radius + 0.06;
    const py_abs = Math.abs(py);
    if (py_abs > max_reach)
        return;

    const py2 = py * py;

    for (let x = 0; x < width; x++) {
        const u = x / width;
        const px = u - 0.5;

        const pr2 = px * px + py2;
        if (pr2 > max_reach * max_reach)
            continue;

        const r = Math.sqrt(pr2);

        const inside = 1.0 - clampf((r - radius) * 60.0, 0.0, 1.0);
        if (inside < 0.001)
            continue;

        const t = time;
        const qx = px * inv_radius;
        const qy = py * inv_radius;

        const qlen2 = qx * qx + qy * qy;
        const omq = 1.0 - qlen2;
        const sphereZ = Math.sqrt(Math.max(omq, 0.0));

        const flow_x = qx + Math.sin(qy * 4.0 + t * 0.75) * 0.10;
        const flow_y = qy + Math.cos(qx * 5.0 - t * 0.62) * 0.08;

        const n1 =
            Math.sin(flow_x * 3.7 + flow_y * 2.3 + t * 0.4) * 0.5 +
            Math.sin(flow_x * 1.9 - flow_y * 4.1 - t * 0.3) * 0.25 +
            0.375;

        const n2 =
            Math.cos(flow_x * 5.1 + flow_y * 1.7 - t * 0.5) * 0.5 +
            Math.cos(flow_x * 2.3 - flow_y * 3.8 + t * 0.2) * 0.25 +
            0.375;

        const wave1 = Math.sin(qx * 4.0 + qy * 2.0 + t * 1.25 + n1 * 2.2);
        const ribbon1 = Math.exp(-Math.abs(wave1) * 2.8);

        const wave2 = Math.sin(qx * 7.0 - qy * 3.0 - t * 1.6 + n2 * 3.0);
        const ribbon2 = Math.exp(-Math.abs(wave2) * 4.0);

        const colorPos = qx * 0.38 + qy * 0.25 + n1 * 0.55 + t * 0.035;

        const ramp = (pos) => {
            const t2 = pos - Math.floor(pos);
            const mr = 1.00, mg = 0.02, mb = 0.42;
            const pr = 0.55, pg = 0.05, pb = 1.00;
            const br = 0.04, bg = 0.20, bb = 1.00;
            const cr = 0.00, cg = 0.85, cb = 1.00;
            const pir = 1.00, pig = 0.30, pib = 0.75;

            if (t2 < 0.20) {
                const s = t2 * 5.0;
                return [mr + (pr - mr) * s, mg + (pg - mg) * s, mb + (pb - mb) * s];
            } else if (t2 < 0.45) {
                const s = (t2 - 0.20) * 4.0;
                return [pr + (br - pr) * s, pg + (bg - pg) * s, pb + (bb - pb) * s];
            } else if (t2 < 0.70) {
                const s = (t2 - 0.45) * 4.0;
                return [br + (cr - br) * s, bg + (cg - bg) * s, bb + (cb - bb) * s];
            } else {
                const s = (t2 - 0.70) * 3.333;
                return [cr + (pir - cr) * s, cg + (pig - cg) * s, cb + (pib - cb) * s];
            }
        };

        const c1 = ramp(colorPos);
        const c2 = ramp(colorPos + 0.37);

        const rb2 = ribbon2 * 0.50;
        let color_r = c1[0] + (c2[0] - c1[0]) * rb2;
        let color_g = c1[1] + (c2[1] - c1[1]) * rb2;
        let color_b = c1[2] + (c2[2] - c1[2]) * rb2;

        let intensity = (ribbon1 * 0.95 + ribbon2 * 0.45) * (0.75 + audio * 0.90);

        const inv_len = 1.0 / Math.sqrt(qlen2 + sphereZ * sphereZ + 0.0001);
        const nz = sphereZ * inv_len;

        let diffuse = qx * inv_len * ldx + qy * inv_len * ldy + nz * ldz;
        if (diffuse < 0.0)
            diffuse = 0.0;

        const lighting = 0.72 + diffuse * 0.55;
        color_r *= lighting;
        color_g *= lighting;
        color_b *= lighting;

        const nz_c = Math.max(nz, 0.0);
        const f_t = 1.0 - nz_c;
        const fresnel = f_t * f_t * f_t * Math.sqrt(f_t);
        color_r += 0.188 * fresnel;
        color_g += 0.488 * fresnel;
        color_b += 0.750 * fresnel;

        color_r *= intensity;
        color_g *= intensity;
        color_b *= intensity;

        let final_r = color_r * inside;
        let final_g = color_g * inside;
        let final_b = color_b * inside;

        const edge = clampf((r - (radius - 0.035)) * 28.571, 0.0, 1.0);
        const ei = edge * 0.55 * inside;
        final_r += 0.25 * ei;
        final_g += 0.55 * ei;
        final_b += 1.00 * ei;

        const r2 = r * r;
        const glow = Math.exp(-r2 * 12.0) * inside;
        const centerGlow = Math.exp(-r2 * 23.0) * inside;
        final_r += glow * 0.29 + centerGlow * 1.8;
        final_g += glow * 0.49 + centerGlow * 0.99;
        final_b += glow * 0.65 + centerGlow * 1.56;

        final_r *= intensity;
        final_g *= intensity;
        final_b *= intensity;

        final_r = clampf(final_r, 0.0, 1.0);
        final_g = clampf(final_g, 0.0, 1.0);
        final_b = clampf(final_b, 0.0, 1.0);

        const a = clampf(inside * 255.0, 0.0, 255.0);

        const o = (y * width + x) * 4;
        rgba[o + 0] = final_r * 255.0; // R
        rgba[o + 1] = final_g * 255.0; // G
        rgba[o + 2] = final_b * 255.0; // B
        rgba[o + 3] = a;               // A
    }
}

/*
 * Render a full frame into `rgba` (Uint8Array, RES*RES*4 bytes).
 * `time` advances the animation; `audio` drives radius / intensity,
 * normally derived from time (see makeAudioDrift).
 */
export function renderOrb(rgba, time, audio) {
    for (let y = 0; y < RES; y++)
        renderOrbLine(y, rgba, RES, RES, time, audio);
}

/*
 * The app synthesizes audio from time exactly like orb.update:
 *   audio = 0.50 + 0.25*sin(t*2.1) + 0.15*sin(t*5.7) + 0.08*sin(t*11)
 */
export function makeAudioDrift(time) {
    const a = 0.50 +
        0.25 * Math.sin(time * 2.1) +
        0.15 * Math.sin(time * 5.7) +
        0.08 * Math.sin(time * 11.0);
    return clampf(a, 0.0, 1.0);
}

export function makeBuffer() {
    return new Uint8Array(RES * RES * 4);
}

export { RES };