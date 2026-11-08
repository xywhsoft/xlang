"""Render the editable template SVG family into transparent 4:3 PNG assets."""
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1] / 'data/template-icons'


def render(path, width=384, height=288):
    root = ET.parse(path).getroot()
    vx, vy, vw, vh = map(float, root.attrib['viewBox'].split())
    scale = min(width / vw, height / vh)
    ox, oy = (width - vw * scale) / 2 - vx * scale, (height - vh * scale) / 2 - vy * scale
    canvas = Image.new('RGBA', (width, height))
    draw = ImageDraw.Draw(canvas)
    edge = 1 if vw == 16 else 0
    for node in root:
        tag, a = node.tag.rsplit('}', 1)[-1], node.attrib
        fill = a.get('fill', '#253246')
        if tag == 'rect':
            x, y, w, h = (float(a.get(k, 0)) for k in ('x', 'y', 'width', 'height'))
            draw.rounded_rectangle((ox+x*scale, oy+y*scale, ox+(x+w)*scale-edge, oy+(y+h)*scale-edge),
                                   radius=float(a.get('rx', 0))*scale, fill=fill)
        elif tag == 'polygon':
            points = [tuple(map(float, part.split(','))) for part in a['points'].split()]
            draw.polygon([(ox+x*scale, oy+y*scale) for x, y in points], fill=fill)
        elif tag == 'text':
            font = ImageFont.truetype('C:/Windows/Fonts/consolab.ttf', round(float(a['font-size'])*scale))
            draw.text((ox+float(a['x'])*scale, oy+float(a['y'])*scale), node.text,
                      font=font, fill=fill, anchor='mm')
        else:
            raise ValueError('Unsupported template SVG element: '+tag)
    return canvas


if __name__ == '__main__':
    ROOT.mkdir(exist_ok=True)
    for name in ('script', 'c-source'):
        source = ROOT.parent/'file-icons'/(name+'.svg')
        text = source.read_text(encoding='utf8').replace('width="64" height="64"', 'width="96" height="72"')
        (ROOT/(name+'.svg')).write_text(text, encoding='utf8', newline='\n')
    for svg in sorted(ROOT.glob('*.svg')):
        if svg.stem.startswith('tree-'):
            render(svg,16,16).save(svg.with_suffix('.png'))
            render(svg,32,32).save(svg.with_name(svg.stem+'@2x.png'))
        else:
            render(svg).save(svg.with_suffix('.png'))
        print(svg.with_suffix('.png'))
