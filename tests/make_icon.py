"""Render the repo-native SVG mark with antialiasing at each Windows icon size."""
import io
import re
import struct
from pathlib import Path
from xml.etree import ElementTree as ET

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SVG = ET.parse(ROOT / 'src/jump.svg').getroot()
NS = '{http://www.w3.org/2000/svg}'
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def paths(data):
    tokens = iter(re.findall(r'[MLC]|-?\d+(?:\.\d+)?', data))
    result, line, current = [], [], (0, 0)
    for command in tokens:
        if command in ('M', 'L'):
            current = (float(next(tokens)), float(next(tokens)))
            if command == 'M' and line:
                result.append(line)
                line = []
            line.append(current)
        elif command == 'C':
            start = current
            a, b, end = [(float(next(tokens)), float(next(tokens))) for _ in range(3)]
            for step in range(1, 33):
                t = step / 32
                line.append(tuple((1-t)**3 * start[i] + 3*(1-t)**2*t*a[i] +
                                  3*(1-t)*t*t*b[i] + t**3*end[i] for i in (0, 1)))
            current = end
    return result + [line]


def render(size):
    scale = size * 8 / 24
    canvas = Image.new('RGBA', (size * 8, size * 8))
    mask = Image.new('L', canvas.size)
    rect = SVG.find(NS + 'rect')
    x, y, width, height, radius = [float(rect.get(k)) * scale for k in ('x', 'y', 'width', 'height', 'rx')]
    ImageDraw.Draw(mask).rounded_rectangle((x, y, x+width, y+height), radius=radius, fill=255)
    gradient = Image.new('RGBA', canvas.size)
    draw = ImageDraw.Draw(gradient)
    top, bottom = (71, 135, 249), (39, 93, 208)
    for row in range(canvas.height):
        t = max(0, min(1, (row-y) / height))
        color = tuple(round(a*(1-t)+b*t) for a, b in zip(top, bottom))
        draw.line((0, row, canvas.width, row), fill=(*color, 255))
    canvas.paste(gradient, (0, 0), mask)
    draw = ImageDraw.Draw(canvas)
    for path in SVG.findall(NS + 'path'):
        stroke = float(path.get('stroke-width')) * scale
        for points in paths(path.get('d')):
            points = [(x*scale, y*scale) for x, y in points]
            draw.line(points, fill='white', width=round(stroke), joint='curve')
            for x, y in points:
                draw.ellipse((x-stroke/2, y-stroke/2, x+stroke/2, y+stroke/2), fill='white')
    return canvas.resize((size, size), Image.Resampling.LANCZOS)


icons = [render(size) for size in SIZES]
payloads = []
for icon in icons:
    data = io.BytesIO()
    icon.save(data, format='PNG')
    payloads.append(data.getvalue())
offset = 6 + 16 * len(SIZES)
entries = []
for size, data in zip(SIZES, payloads):
    entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
    offset += len(data)
(ROOT / 'src/jump.ico').write_bytes(struct.pack('<HHH', 0, 1, len(SIZES)) + b''.join(entries) + b''.join(payloads))
out = ROOT / 'tests/output/icons'
out.mkdir(parents=True, exist_ok=True)
preview = Image.new('RGB', (640, 210), '#171D26')
draw = ImageDraw.Draw(preview)
x = 20
for size, icon in zip(SIZES[:-1], icons[:-1]):
    preview.paste(icon, (x, 30 + (128-size)//2), icon)
    draw.text((x, 178), str(size), fill='#A0ACBC')
    x += size + 20
preview.save(out / 'icon-sizes.png')
print('Updated src/jump.ico: ' + ', '.join(map(str, SIZES)))
