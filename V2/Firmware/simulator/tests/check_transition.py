"""Check the rendered slide reaches beyond the obsolete V1 viewport."""
from pathlib import Path
import sys
from PIL import Image

directory = Path(sys.argv[1])
visible_pixels = {}
for path in sorted(directory.glob('*transition-*.bmp')):
    image = Image.open(path).convert('RGB')
    zoom = image.width // 620
    count = 0
    for y in range(20, 280):
        for x in list(range(12, 48)) + list(range(272, 308)):
            # Stay safely inside the true circular aperture, excluding the mask.
            if (x - 160)**2 + (y - 150)**2 > 145**2:
                continue
            pixel = image.getpixel(((22 + x)*zoom, (32 + y)*zoom))
            if max(pixel) > 20:
                count += 1
    print(f'{path.name}: {count} visible pixels beyond the old rectangular crop')
    direction = 'backward' if 'backward' in path.name else 'forward'
    visible_pixels[direction] = visible_pixels.get(direction, 0) + count
assert visible_pixels, 'No transition screenshots were captured'
for direction, count in visible_pixels.items():
    assert count > 10, f'{direction} slide is still clipped by the obsolete 128 x 96 canvas'
print('TRANSITION CHECK PASS: moving UI reaches the circular display edges')
for name, dimensions in [('12-zoom-x2.bmp', (1240, 840)), ('13-zoom-x1.bmp', (620, 420))]:
    path = directory / name
    if path.exists():
        assert Image.open(path).size == dimensions, f'Wrong pixel dimensions for {name}'
        print(f'ZOOM CHECK PASS: {name} is {dimensions}')
