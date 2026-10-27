"""Rendered acceptance checks for the round scuba dashboard and input screens."""
from pathlib import Path
import sys
from PIL import Image
root=Path(sys.argv[1])
def pixels(name):
    image=Image.open(root/(name+'.bmp')).convert('RGB')
    return image.resize((620,420)).crop((22,32,342,332))
def count(image,test,box=(0,0,320,300)):
    cropped=image.crop(box)
    return sum(test(*cropped.getpixel((x,y))) for y in range(cropped.height) for x in range(cropped.width))
white=lambda r,g,b:min(r,g,b)>190
teal=lambda r,g,b:r<80 and g>100 and b>100
green=lambda r,g,b:g>120 and g>r*1.5 and g>b*1.25
red=lambda r,g,b:r>150 and r>g*1.5 and r>b*1.5
yellow=lambda r,g,b:r>150 and g>140 and b<110
orange=lambda r,g,b:r>150 and 70<g<200 and b<100
for name in ['scuba-01-ready','scuba-02-settings','scuba-03-depth-alarm','scuba-04-main','scuba-05-options-gate','scuba-06-ring-options','scuba-07-brightness','scuba-08-surface','scuba-09-leave-quarter','scuba-10-safety-stop','scuba-11-stop-complete','scuba-12-surface-exit','scuba-13-deco','scuba-14-ceiling-alert','scuba-15-alert-acknowledged','scuba-16-low-battery','scuba-17-summary']:
    assert count(pixels(name),white)>80,f'{name}: missing screen text'
main=pixels('scuba-04-main')
assert count(pixels('scuba-02-settings'),teal)>200,'Missing fitted settings capsule'
assert count(pixels('scuba-22-bottom-options'),teal)>200,'Missing bottom field selector capsule'
assert count(main,green)>1000,'Missing green NDL perimeter'
assert count(main,white,(80,52,240,105))>150,'Depth is not centred at the top'
assert count(main,white,(172,149,270,202))>100,'Dive time is not on the right'
assert count(main,white,(48,149,147,204))>100,'NDL is not on the left'
assert count(main,white,(65,217,255,236))>15,'Missing lower switchable field caption'
assert count(main,white,(75,239,245,280))>50,'Missing lower switchable value'
assert count(pixels('scuba-05-options-gate'),white,(75,95,245,200))>60,'Missing Options entry page'
assert count(pixels('scuba-06-ring-options'),teal)>200,'Missing ring selector capsule'
assert count(pixels('scuba-07-brightness'),teal,(60,130,260,158))>40,'Missing brightness bar'
assert count(pixels('scuba-10-safety-stop'),yellow,(48,145,147,202))>100,'Missing stop countdown at left'
assert count(pixels('scuba-11-stop-complete'),green)<50,'Completed countdown ring is not empty'
assert count(pixels('scuba-13-deco'),orange,(55,125,145,146))>10,'Missing decompression indicator'
assert count(pixels('scuba-14-ceiling-alert'),red,(40,25,280,52))>20,'Missing ceiling alert'
assert count(pixels('scuba-15-alert-acknowledged'),red,(90,34,230,46))<5,'Acknowledged alert message remains visible'
if (root/'scuba-19-fast-ascent.bmp').exists():
    assert count(pixels('scuba-19-fast-ascent'),red,(150,140,170,204))>20,'Ascent chevrons are not red'
    assert count(pixels('scuba-20-sustained-ascent'),red,(148,140,172,205))>250,'Sustained ascent is not highlighted'
    assert count(pixels('scuba-21-low-ndl'),yellow,(52,127,146,204))>2000,'Low NDL is not highlighted yellow'
    for name in ['max-depth','clock','battery','safety','tts','ppo2','heading','temperature']:
        image=pixels('scuba-field-'+name)
        assert count(image,white,(65,217,255,236))>10,f'{name}: field caption missing'
        # These frames are captured during the screen fade after applying a choice.
        assert count(image,lambda r,g,b:min(r,g,b)>100,(65,239,255,280))>35,f'{name}: value missing'
print('SCUBA RENDER PASS: top depth, central ascent, left NDL/deco/stops, right dive time, all eight footer fields, alerts and display selectors')

# The capsule remains at the centre for the first and later items; text moves.
for name in ['scuba-06-ring-options','scuba-22-bottom-options']:
    im=pixels(name)
    assert count(im,teal,(50,125,270,175))>200,'Selection is not centred'
    assert count(im,teal,(50,80,270,120))<30,'Selection moved to the top'
moving=pixels('scuba-field-moving')
settled=pixels('scuba-22-bottom-options')
difference=sum(moving.getpixel((x,y))!=settled.getpixel((x,y))
               for y in range(90,245) for x in range(50,270))
assert difference>100,'Selector text jumped instead of animating'
print('SCUBA OPTIONS PASS: entry page, fixed selection centre and animated text')

for name in ['scuba-options-first','scuba-options-settled']:
    im=pixels(name)
    assert count(im,teal,(50,125,270,175))>200,'Options selection is not centred'
    assert count(im,teal,(50,80,270,120))<30,'Options selection moved upwards'
moving=pixels('scuba-options-moving')
settled=pixels('scuba-options-settled')
assert sum(moving.getpixel((x,y))!=settled.getpixel((x,y))
           for y in range(90,228) for x in range(50,270))>100,'Options text is not animated'
print('FLAT OPTIONS PASS: direct Ring / Bottom field / Backlight and centred animated list')
