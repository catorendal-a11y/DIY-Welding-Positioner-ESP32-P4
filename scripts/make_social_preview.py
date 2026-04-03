from PIL import Image, ImageDraw, ImageFont
import math

W, H = 1280, 640
img = Image.new("RGB", (W, H), "#0A0A0A")
d = ImageDraw.Draw(img)

d.rectangle([0, 0, 3, H], fill="#FF9500")

try:
    font_title = ImageFont.truetype("arialbd.ttf", 52)
    font_sub = ImageFont.truetype("consola.ttf", 16)
    font_card_title = ImageFont.truetype("consola.ttf", 14)
    font_card_val = ImageFont.truetype("arial.ttf", 17)
    font_card_sub = ImageFont.truetype("consola.ttf", 13)
    font_rpm = ImageFont.truetype("consolab.ttf", 68)
    font_rpm_unit = ImageFont.truetype("consola.ttf", 24)
    font_badge = ImageFont.truetype("consolab.ttf", 16)
except:
    font_title = ImageFont.load_default()
    font_sub = font_title
    font_card_title = font_title
    font_card_val = font_title
    font_card_sub = font_title
    font_rpm = font_title
    font_rpm_unit = font_title
    font_badge = font_title

badge_text = "ESP32-P4  |  RISC-V  |  LVGL UI"
bw = 280
d.rounded_rectangle([60, 90, 60 + bw, 122], radius=4, outline="#FF9500", fill="#1A1600")
d.text((60 + bw // 2, 106), badge_text, fill="#FF9500", font=font_badge, anchor="mm")

d.text((60, 150), "DIY Welding", fill="#E0E0E0", font=font_title)
d.text((60, 210), "Positioner Controller", fill="#FF9500", font=font_title)
d.text(
    (60, 270),
    "Open-source TIG welding rotator with LVGL touch UI",
    fill="#555555",
    font=font_sub,
)

cards = [
    ("MOTOR", "NEMA 23 + TB6600", "0.01 - 1.0 RPM"),
    ("DISPLAY", '4.3" MIPI-DSI', "800x480 + GT911 Touch"),
    ("MODES", "5 Welding Modes", "Cont/Pulse/Step/Jog/Timer"),
]
for i, (t, v, s) in enumerate(cards):
    x = 60 + i * 250
    y = 340
    d.rectangle([x, y, x + 240, y + 110], outline="#2E2E2E", fill="#0D0D0D")
    d.rectangle([x, y, x + 3, y + 110], fill="#FF9500")
    d.text((x + 24, y + 14), t, fill="#FF9500", font=font_card_title)
    d.text((x + 24, y + 42), v, fill="#CCCCCC", font=font_card_val)
    d.text((x + 24, y + 72), s, fill="#555555", font=font_card_sub)

gcx, gcy, gr = 1040, 225, 130


def draw_arc(draw, cx, cy, r, w, color, start_deg, end_deg, step=0.5):
    pts = []
    for a_deg_f in [
        start_deg + i * step for i in range(int((end_deg - start_deg) / step) + 1)
    ]:
        a = math.radians(a_deg_f)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    for i in range(len(pts) - 1):
        draw.line([pts[i], pts[i + 1]], fill=color, width=w)


track_r = gr + 15
draw_arc(d, gcx, gcy, track_r, 14, "#1A1A1A", 135, 405, step=0.5)

active_r = gr + 15
active_end = 135 + int(270 * 0.5)
draw_arc(d, gcx, gcy, active_r, 12, "#FF9500", 135, active_end, step=0.5)

tick_r_inner = gr + 1
tick_r_outer = gr + 6
for a_deg in range(135, 406, 27):
    a = math.radians(a_deg)
    x1 = gcx + tick_r_inner * math.cos(a)
    y1 = gcy + tick_r_inner * math.sin(a)
    x2 = gcx + tick_r_outer * math.cos(a)
    y2 = gcy + tick_r_outer * math.sin(a)
    d.line([(x1, y1), (x2, y2)], fill="#333333", width=2)

dot_a = math.radians(active_end)
dot_r = active_r
nx = gcx + dot_r * math.cos(dot_a)
ny = gcy + dot_r * math.sin(dot_a)
d.ellipse([nx - 8, ny - 8, nx + 8, ny + 8], fill="#FF9500")
d.ellipse([nx - 4, ny - 4, nx + 4, ny + 4], fill="#FFFFFF")

d.text((gcx, gcy - 25), "0.50", fill="#FF9500", font=font_rpm, anchor="mm")
d.text((gcx, gcy + 30), "RPM", fill="#555555", font=font_rpm_unit, anchor="mm")

try:
    font_stat_label = ImageFont.truetype("consola.ttf", 12)
    font_stat_val = ImageFont.truetype("consola.ttf", 16)
except:
    font_stat_label = font_sub
    font_stat_val = font_sub

d.rectangle([0, 490, W, 560], fill="#090909")
d.line([(0, 490), (W, 490)], fill="#1A1A1A")

stats = [
    (60, "GEAR", "199.5:1"),
    (200, "MICROSTEPPING", "1/8 (1600 spr)"),
    (430, "UI", "LVGL 9.x"),
    (560, "LICENSE", "MIT"),
    (680, "SAFETY", "E-STOP + FreeRTOS"),
]
for x, label, val in stats:
    d.text((x, 505), label, fill="#555555", font=font_stat_label)
    d.text((x, 525), val, fill="#AAAAAA", font=font_stat_val)

out = "docs/images/social_preview.png"
img.save(out, "PNG")
print(f"Saved {out} ({W}x{H})")
print(f"Gauge: center=({gcx},{gcy}), r={gr}")
print(
    f"Gauge bounds: x=[{gcx - gr},{gcx + gr}], y=[{gcy - gr},{gcy + int(gr * 0.707)}]"
)
print(f"Cards end: x=820, y=340-450")
print(f"Gap gauge-card horizontal: {gcx - gr - 820}px")
print(f"Gap gauge-card vertical: {340 - (gcy + int(gr * 0.707))}px")
print(f"Right margin: {W - (gcx + gr + 22)}px")
