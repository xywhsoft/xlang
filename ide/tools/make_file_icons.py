"""Build the IDE's association icons from small, editable SVG assets.

Development dependency: Pillow. No icon files are copied into the runtime;
windres embeds the generated ICOs under stable resource identifiers.
"""
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1] / "data" / "file-icons"
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def render(source):
    scale = 8
    canvas = Image.new("RGBA", (64 * scale, 64 * scale))
    draw = ImageDraw.Draw(canvas)
    for node in ET.parse(source).getroot():
        tag = node.tag.rsplit("}", 1)[-1]
        a = node.attrib
        fill = a.get("fill", "#253246")
        if tag == "polygon":
            points = [tuple(float(v) * scale for v in p.split(","))
                      for p in a["points"].split()]
            draw.polygon(points, fill=fill)
        elif tag == "rect":
            x, y, w, h = (float(a.get(k, 0)) * scale for k in ("x", "y", "width", "height"))
            draw.rounded_rectangle((x, y, x + w, y + h), radius=float(a.get("rx", 0)) * scale, fill=fill)
        elif tag == "text":
            font = ImageFont.truetype("C:/Windows/Fonts/consolab.ttf", round(float(a["font-size"]) * scale))
            draw.text((float(a["x"]) * scale, float(a["y"]) * scale), node.text,
                      font=font, fill=fill, anchor="mm")
        else:
            raise ValueError(f"Unsupported SVG element: {tag}")
    return canvas


if __name__ == "__main__":
    for source in sorted(ROOT.glob("*.svg")):
        image = render(source)
        image.save(source.with_suffix(".ico"), format="ICO", sizes=[(s, s) for s in SIZES])
        print(source.with_suffix(".ico"))
