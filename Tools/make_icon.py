# Draws the GuitarCoach icon: a cream guitar pick with an eighth note, on a racing-green tile with a gold rim.
import math, sys
from PIL import Image, ImageDraw, ImageFilter

out = sys.argv[1] if len(sys.argv) > 1 else "Resources/icon.png"   # python Tools/make_icon.py (needs Pillow)
S = 2048                      # drawn at twice the size, then scaled down for smooth edges
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

def hexc(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)

GREEN_TOP, GREEN_BOTTOM = hexc("1f4a35"), hexc("0f2219")
GOLD, CREAM, INK = hexc("d8b25a"), hexc("f3ead0"), hexc("1d1f23")

# tile with a vertical gradient
m = int(S * 0.04)
radius = int(S * 0.22)
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([m, m, S - m, S - m], radius=radius, fill=255)
grad = Image.new("RGBA", (S, S))
gd = ImageDraw.Draw(grad)
for y in range(S):
    t = y / (S - 1)
    gd.line([(0, y), (S, y)], fill=tuple(int(GREEN_TOP[k] + (GREEN_BOTTOM[k] - GREEN_TOP[k]) * t) for k in range(3)) + (255,))
img.paste(grad, (0, 0), mask)
d = ImageDraw.Draw(img)
inset = int(S * 0.075)
d.rounded_rectangle([inset, inset, S - inset, S - inset], radius=int(radius * 0.78), outline=GOLD, width=int(S * 0.012))

# guitar pick: convex hull of three circles (two wide shoulders, a rounded point)
def circle_pts(cx, cy, r, n=180):
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]

def hull(points):
    pts = sorted(set(points))
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0: lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0: upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]

cx = S / 2
def ellipse_pts(cx, cy, rx, ry, n=240):
    return [(cx + rx * math.cos(2 * math.pi * i / n), cy + ry * math.sin(2 * math.pi * i / n)) for i in range(n)]

# the classic pick: a broad rounded top narrowing to a rounded tip
pick = hull(ellipse_pts(cx, S * 0.40, S * 0.30, S * 0.20) + circle_pts(cx, S * 0.785, S * 0.07))

# soft shadow under the pick
shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(shadow).polygon([(x + S * 0.012, y + S * 0.02) for x, y in pick], fill=(0, 0, 0, 120))
shadow = shadow.filter(ImageFilter.GaussianBlur(S * 0.015))
img = Image.alpha_composite(img, shadow)
d = ImageDraw.Draw(img)
d.polygon(pick, fill=CREAM)

# eighth note: tilted oval head, stem, flag
hx, hy = cx - S * 0.055, S * 0.555
head = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(head).ellipse([hx - S * 0.085, hy - S * 0.058, hx + S * 0.085, hy + S * 0.058], fill=INK)
head = head.rotate(22, center=(hx, hy), resample=Image.BICUBIC)
img = Image.alpha_composite(img, head)
d = ImageDraw.Draw(img)
stem_x = hx + S * 0.072
top = S * 0.27
d.rectangle([stem_x - S * 0.016, top, stem_x + S * 0.012, hy - S * 0.01], fill=INK)
flag = [(stem_x - S * 0.004, top)]
for i in range(41):                                   # outer edge of the flag
    t = i / 40
    flag.append((stem_x + S * (0.13 * math.sin(t * 2.3)), top + S * 0.24 * t))
for i in range(40, -1, -1):                           # inner edge back up
    t = i / 40
    flag.append((stem_x + S * (0.075 * math.sin(t * 2.1)), top + S * 0.05 + S * 0.16 * t))
d.polygon(flag, fill=INK)

img = img.resize((1024, 1024), Image.LANCZOS)
img.save(out)
print("wrote", out, img.size)
