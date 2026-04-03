import os
import re

SCREENS_DIR = r"c:\Users\Rendalsniken\Documents\New folder (3) - Copy\src\ui\screens"
SCALE_X = 2.2222
SCALE_Y = 1.92

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # lv_obj_set_size
    def s_cb(m):
        try:
            w, h = int(m.group(2)), int(m.group(3))
            # skip already scaled huge things
            if w >= 800 or h >= 480: return m.group(0)
            if w > 0 and w == h:
                n = int(round(w * SCALE_X))
                return f"lv_obj_set_size({m.group(1)}, {n}, {n})"
            return f"lv_obj_set_size({m.group(1)}, {int(round(w * SCALE_X))}, {int(round(h * SCALE_Y))})"
        except ValueError: return m.group(0)

    content = re.sub(r"lv_obj_set_size\(([^,]+),\s*(-?\d+),\s*(-?\d+)\)", s_cb, content)

    # lv_obj_set_pos
    def p_cb(m):
        try:
            x, y = int(m.group(2)), int(m.group(3))
            return f"lv_obj_set_pos({m.group(1)}, {int(round(x * SCALE_X))}, {int(round(y * SCALE_Y))})"
        except ValueError: return m.group(0)

    content = re.sub(r"lv_obj_set_pos\(([^,]+),\s*(-?\d+),\s*(-?\d+)\)", p_cb, content)

    # lv_obj_align
    def a_cb(m):
        try:
            x, y = int(m.group(3)), int(m.group(4))
            return f"lv_obj_align({m.group(1)}, {m.group(2)}, {int(round(x * SCALE_X))}, {int(round(y * SCALE_Y))})"
        except ValueError: return m.group(0)

    content = re.sub(r"lv_obj_align\(([^,]+),\s*([^,]+),\s*(-?\d+),\s*(-?\d+)\)", a_cb, content)

    # create_svg_btn
    def btn_cb(m):
        try:
            x, y, w, h = int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5))
            if w >= 400: return m.group(0) # likely already scaled
            nx, ny = int(round(x * SCALE_X)), int(round(y * SCALE_Y))
            nw, nh = int(round(w * SCALE_X)), int(round(h * SCALE_Y))
            return f"create_svg_btn({m.group(1)}, {nx}, {ny}, {nw}, {nh}, {m.group(6)})"
        except ValueError: return m.group(0)

    content = re.sub(r"create_svg_btn\(([^,]+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(.*?)\)", btn_cb, content)

    if filepath.endswith("screen_boot.cpp"):
        # explicitly scale loader size
        content = re.sub(r"lv_spinner_create\(([^,]+),\s*1000,\s*(\d+)\)", 
            lambda m: f"lv_spinner_create({m.group(1)}, 1000, {int(int(m.group(2))*SCALE_X)})", content)
        content = re.sub(r"lv_obj_set_size\(spinnerRing,\s*(\d+),\s*(\d+)\)",
            lambda m: f"lv_obj_set_size(spinnerRing, {int(int(m.group(1))*SCALE_X)}, {int(int(m.group(2))*SCALE_X)})", content)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Processed {os.path.basename(filepath)}")

for filename in os.listdir(SCREENS_DIR):
    if filename.endswith(".cpp") and filename != "screen_main.cpp":
        process_file(os.path.join(SCREENS_DIR, filename))

print("Upscaling executed successfully.")
