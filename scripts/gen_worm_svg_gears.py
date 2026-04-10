# Spur pair (stage 2): same module on both gears — m=1.5, z1=40 drive / z2=72 driven, PA 20 deg.
# Stage 1: NMRV030 worm gearbox 60:1 (inside housing, not drawn to scale).
# Center distance: a = m*(z1+z2)/2 = 1.5*112/2 = 84 mm. Diagram: A_SVG px = a_mm * k.

import math
import pathlib

M = 1.5  # standard module; both spur gears must match
Z1 = 40  # pinion on worm-wheel shaft
Z2 = 72  # output spur gear
PA_DEG = 20.0  # Fusion default here; use 14.5 and re-run if your set is 14.5 deg PA
A_SVG = 275.0  # horizontal center distance (px) between gear centers
CX1, CY = 250, 260
CX2 = int(CX1 + A_SVG)

A_MM = M * (Z1 + Z2) / 2.0
K = A_SVG / A_MM  # px per mm
M_PX = M * K

RP1 = M * Z1 / 2.0 * K
RP2 = M * Z2 / 2.0 * K
RO1 = RP1 + M_PX
RR1 = RP1 - 1.25 * M_PX
RO2 = RP2 + M_PX
RR2 = RP2 - 1.25 * M_PX

PA = math.radians(PA_DEG)


def xy(r, deg_from_top_ccw):
    rad = math.radians(deg_from_top_ccw)
    return (r * math.sin(rad), -r * math.cos(rad))


def tooth_params(Rp, Ro, Rr, z):
    pitch_deg = 360.0 / z
    # Tooth ~48% of circular pitch at pitch circle; tip/root from similar triangles + PA
    gp = 0.48 * pitch_deg / 2.0
    shrink_tip = math.cos(PA) * 0.88
    go = gp * Rp / Ro * shrink_tip
    gr = gp * Rp / Rr * 1.06
    return go, gp, gr


def tooth_path_d(Ro, Rp, Rr, go, gp, gr):
    pts = [
        xy(Ro, go),
        xy(Rp, gp),
        xy(Rr, gr),
        xy(Rr, -gr),
        xy(Rp, -gp),
        xy(Ro, -go),
    ]
    return "M " + " L ".join(f"{x:.3f},{y:.3f}" for x, y in pts) + " Z"


def gear_block(cx, cy, z, Ro, Rp, Rr, rotate_id, body_fill, tooth_fill, pitch_stroke, title):
    go, gp, gr = tooth_params(Rp, Ro, Rr, z)
    lines = [
        "  <!-- ========================================== -->",
        f"  <!-- {title} -->",
        f"  <!-- m={M}, z={z}, PA={PA_DEG} deg, a={A_SVG}px == {A_MM:.3f} mm * k -->",
        "  <!-- ========================================== -->",
        f'  <g transform="translate({cx}, {cy})">',
        f'    <g id="{rotate_id}">',
        f'      <circle r="{Rr:.3f}" fill="{body_fill}" stroke="#000" stroke-width="2.5"/>',
        f'      <circle r="{Rp:.3f}" fill="none" stroke="{pitch_stroke}" stroke-width="0.9" stroke-dasharray="5 4"/>',
    ]
    for k in range(z):
        ang = k * 360.0 / z
        d = tooth_path_d(Ro, Rp, Rr, go, gp, gr)
        lines.append(
            f'      <g transform="rotate({ang:.6f})">'
            f'<path d="{d}" fill="{tooth_fill}" stroke="#0d0d0d" stroke-width="0.85" stroke-linejoin="round"/>'
            f"</g>"
        )
    hub_o = max(Rp * 0.36, 26.0)
    hub_i = hub_o * 0.48
    lines.append(
        f'      <circle r="{hub_o:.3f}" fill="#999" stroke="#000" stroke-width="2.5"/>'
    )
    lines.append(
        f'      <circle r="{hub_i:.3f}" fill="#f5f5f5" stroke="#000" stroke-width="2"/>'
    )
    lines.append("    </g>")
    lines.append("  </g>")
    return "\n".join(lines) + "\n"


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    svg_path = root / "docs" / "images" / "motor.worm.svg"
    text = svg_path.read_text(encoding="utf-8")
    if "<!-- MOTOR_WORM_GEARS_BEGIN -->" in text:
        start = text.index("<!-- MOTOR_WORM_GEARS_BEGIN -->")
        end = text.index("<!-- MOTOR_WORM_GEARS_END -->") + len("<!-- MOTOR_WORM_GEARS_END -->")
    else:
        start = text.index("  <!-- ========================================== -->\n  <!-- 3. PINION")
        end = text.index("  <!-- ========================================== -->\n  <!-- ETIKETTER")

    b1 = gear_block(
        CX1,
        CY,
        Z1,
        RO1,
        RP1,
        RR1,
        "rotate-small",
        "#3d3d3d",
        "#4f4f4f",
        "#7a7a7a",
        "3. Pinion on worm-wheel shaft / m1.5, z40, PA 20 deg",
    )
    b2 = gear_block(
        CX2,
        CY,
        Z2,
        RO2,
        RP2,
        RR2,
        "rotate-large",
        "#4a4a4a",
        "#5c5c5c",
        "#888888",
        "4. Output spur gear / m1.5, z72, PA 20 deg",
    )

    new_mid = (
        "  <!-- MOTOR_WORM_GEARS_BEGIN -->\n"
        + b1
        + "\n"
        + b2
        + "\n  <!-- MOTOR_WORM_GEARS_END -->\n"
    )
    svg_path.write_text(text[:start] + new_mid + text[end:], encoding="utf-8")
    print("Wrote", svg_path)
    print(f"Rp40={RP1:.2f} Rp72={RP2:.2f} Ro72={RO2:.2f} m_px={M_PX:.2f}px")


if __name__ == "__main__":
    main()
