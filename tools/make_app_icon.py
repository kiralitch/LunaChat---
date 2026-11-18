# -*- coding: utf-8 -*-
"""生成 LunaChat 软件图标（月牙 + 星点，深紫渐变圆角底）。

输出：
  Client/resources/icons/logo.png   512×512，界面/窗口图标
  Client/resources/app.ico          16~256 多尺寸，exe 文件图标
用法：python tools/make_app_icon.py
"""
import os

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_DIR = os.path.join(ROOT, "Client", "resources", "icons")
RES_DIR = os.path.join(ROOT, "Client", "resources")

S = 1024
SS = 2  # 超采样倍数，最后缩回 S


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def rounded_mask(size, radius):
    m = Image.new("L", size, 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=255)
    return m


def vertical_gradient(size, top, bottom):
    img = Image.new("RGB", size, top)
    d = ImageDraw.Draw(img)
    for y in range(size[1]):
        d.line([(0, y), (size[0], y)], fill=lerp(top, bottom, y / max(1, size[1] - 1)))
    return img


def radial_glow(size, center, radius, color, max_alpha=110):
    """用若干同心圆模拟径向光晕，避免引入 numpy。"""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    steps = 48
    for i in range(steps, 0, -1):
        r = radius * i / steps
        a = int(max_alpha * (1 - i / steps) ** 1.6)
        if a <= 0:
            continue
        d.ellipse((center[0] - r, center[1] - r, center[0] + r, center[1] + r),
                  fill=color + (a,))
    return layer


def draw_star(d, cx, cy, r, color):
    d.polygon([(cx, cy - r), (cx + r * 0.28, cy - r * 0.28), (cx + r, cy),
               (cx + r * 0.28, cy + r * 0.28), (cx, cy + r),
               (cx - r * 0.28, cy + r * 0.28), (cx - r, cy),
               (cx - r * 0.28, cy - r * 0.28)], fill=color)


def build():
    size = (S, S)

    # 底：深紫 -> 靛蓝渐变 + 左上高光
    bg = vertical_gradient(size, (58, 36, 120), (26, 20, 62)).convert("RGBA")
    bg.alpha_composite(radial_glow(size, (300, 210), 620, (124, 96, 240), 120))
    bg.alpha_composite(radial_glow(size, (820, 860), 520, (70, 120, 220), 70))

    # 细碎星点
    stars = [(250, 640, 3), (700, 250, 2), (800, 640, 3), (620, 760, 2),
             (350, 300, 2), (760, 420, 2), (520, 180, 2), (180, 470, 2)]
    for x, y, r in stars:
        ImageDraw.Draw(bg).ellipse((x - r, y - r, x + r, y + r), fill=(255, 255, 255, 90))

    # 月亮光晕
    glow = radial_glow(size, (470, 470), 430, (255, 231, 170), 90)
    glow = glow.filter(ImageFilter.GaussianBlur(28))
    bg.alpha_composite(glow)

    # 月牙：大圆 - 偏移圆
    moon = Image.new("RGBA", size, (0, 0, 0, 0))
    md = ImageDraw.Draw(moon)
    md.ellipse((470 - 268, 470 - 268, 470 + 268, 470 + 268), fill=(255, 245, 214, 255))
    # 渐变感：下半部分压暗一点
    shade = vertical_gradient(size, (255, 233, 170), (255, 203, 120)).convert("RGBA")
    shade.putalpha(moon.split()[3].point(lambda v: v))
    mask = moon.split()[3]
    cut = Image.new("L", size, 0)
    ImageDraw.Draw(cut).ellipse((590 - 232, 380 - 232, 590 + 232, 380 + 232), fill=255)
    inner = Image.composite(shade, Image.new("RGBA", size, (0, 0, 0, 0)), mask)
    inner.putalpha(Image.eval(mask, lambda v: v))
    # 挖掉偏移圆
    hole = Image.new("L", size, 0)
    ImageDraw.Draw(hole).ellipse((590 - 232, 380 - 232, 590 + 232, 380 + 232), fill=255)
    alpha = Image.composite(Image.new("L", size, 255), Image.new("L", size, 0), hole)
    alpha = Image.eval(alpha, lambda v: 255 - v)
    alpha = Image.composite(mask, Image.new("L", size, 0), alpha)
    inner.putalpha(alpha)
    bg.alpha_composite(inner)

    # 星点装饰（亮色）
    d = ImageDraw.Draw(bg)
    draw_star(d, 735, 315, 26, (255, 246, 214, 235))
    draw_star(d, 790, 470, 14, (255, 246, 214, 190))
    draw_star(d, 330, 700, 12, (255, 255, 255, 150))

    # 描边 + 内发光，增强"应用图标"质感
    d.rounded_rectangle((10, 10, S - 11, S - 11), radius=214,
                        outline=(255, 255, 255, 46), width=6)

    # 圆角裁切
    mask = rounded_mask(size, 220)
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out.paste(bg, (0, 0), mask)

    # 缩回 512（LANCZOS 抗锯齿）
    logo = out.resize((512, 512), Image.LANCZOS)
    os.makedirs(ICON_DIR, exist_ok=True)
    logo_path = os.path.join(ICON_DIR, "logo.png")
    logo.save(logo_path)

    # Windows ico：多尺寸
    ico_path = os.path.join(RES_DIR, "app.ico")
    sizes = [16, 24, 32, 48, 64, 128, 256]
    out.save(ico_path, format="ICO", sizes=[(s, s) for s in sizes])

    print("logo:", logo_path, os.path.getsize(logo_path) // 1024, "KB")
    print("ico :", ico_path, os.path.getsize(ico_path) // 1024, "KB")


def build_ui_icons():
    """登录页输入框用的线性图标：user / lock / eye / eye_off。"""
    base = 48
    ss = 4
    color = (183, 176, 216, 225)
    w = 3.4 * ss
    out = {}

    def new_canvas():
        img = Image.new("RGBA", (base * ss, base * ss), (0, 0, 0, 0))
        return img, ImageDraw.Draw(img)

    # 用户
    img, d = new_canvas()
    d.ellipse((16.5 * ss, 9.5 * ss, 31.5 * ss, 24.5 * ss), outline=color, width=int(w))
    d.arc((11 * ss, 26 * ss, 37 * ss, 48 * ss), 180, 360, fill=color, width=int(w))
    out["user.png"] = img.resize((base, base), Image.LANCZOS)

    # 锁
    img, d = new_canvas()
    d.arc((15.5 * ss, 8.5 * ss, 32.5 * ss, 27 * ss), 180, 360, fill=color, width=int(w))
    d.rounded_rectangle((12 * ss, 22 * ss, 36 * ss, 41 * ss), radius=4 * ss,
                        outline=color, width=int(w))
    d.ellipse((22.6 * ss, 29 * ss, 25.4 * ss, 31.8 * ss), fill=color)
    out["lock.png"] = img.resize((base, base), Image.LANCZOS)

    # 睁眼
    img, d = new_canvas()
    d.arc((6 * ss, 14 * ss, 42 * ss, 34 * ss), 180, 360, fill=color, width=int(w))
    d.arc((6 * ss, 14 * ss, 42 * ss, 34 * ss), 0, 180, fill=color, width=int(w))
    d.ellipse((18 * ss, 18 * ss, 30 * ss, 30 * ss), outline=color, width=int(w))
    out["eye.png"] = img.resize((base, base), Image.LANCZOS)

    # 闭眼
    img, d = new_canvas()
    d.arc((6 * ss, 14 * ss, 42 * ss, 34 * ss), 180, 360, fill=color, width=int(w))
    d.arc((6 * ss, 14 * ss, 42 * ss, 34 * ss), 0, 180, fill=color, width=int(w))
    d.line((10 * ss, 38 * ss, 38 * ss, 10 * ss), fill=color, width=int(w))
    out["eye_off.png"] = img.resize((base, base), Image.LANCZOS)

    for name, img in out.items():
        path = os.path.join(ICON_DIR, name)
        img.save(path)
        print("ui icon:", path)


if __name__ == "__main__":
    build()
    build_ui_icons()
