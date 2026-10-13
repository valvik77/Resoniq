"""Rebuild the approved Pulso continuo identity (requires Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import struct

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets' / 'brand'
OUT.mkdir(parents=True, exist_ok=True)
# Three folded ribbon faces; shared geometry for vector and raster assets.
FACES = [
 ('#00d9eb', [('M',10,62),('L',18,48),('C',21,43,26,43,32,43),('L',43,43),('L',32,62),('Z',)]),
 ('#087fff', [('M',25,56),('C',29,68,38,72,44,58),('L',60,24),('C',64,16,69,16,74,19),('C',68,13,57,14,53,21),('L',39,50),('C',35,58,30,61,25,56),('Z',)]),
 ('#00d9eb', [('M',60,24),('C',65,13,71,14,76,25),('L',86,54),('C',89,62,92,66,98,61),('C',91,74,80,71,75,58),('L',65,30),('C',63,25,62,23,60,24),('Z',)]),
 ('#087fff', [('M',86,54),('L',93,42),('C',96,37,99,36,104,36),('L',115,36),('L',100,62),('C',95,70,90,65,86,54),('Z',)])]

def points(commands):
    result=[]; current=(0,0)
    for c in commands:
        if c[0] in ('M','L'):
            current=c[1:];result.append(current)
        elif c[0]=='C':
            start=current;a=c[1:3];b=c[3:5];end=c[5:7]
            for i in range(1,33):
                t=i/32;u=1-t
                result.append(tuple(u**3*start[j]+3*u*u*t*a[j]+3*u*t*t*b[j]+t**3*end[j] for j in (0,1)))
            current=end
    return result

def render(size, tile=False, colour=None):
    scale=8;im=Image.new('RGBA',(size*scale,size*scale))
    d=ImageDraw.Draw(im)
    if tile:d.rounded_rectangle((0,0,size*scale-1,size*scale-1),radius=size*scale*.23,fill='#0b1b31')
    # The horizontal ribbon remains broad enough for the 16px tray image.
    for fill,commands in FACES:
        coords=[(x/128*size*scale,(y+20)/128*size*scale) for x,y in points(commands)]
        d.polygon(coords,fill=colour or fill)
    return im.resize((size,size),Image.Resampling.LANCZOS)

variants={'app':(True,None),'mark':(False,None),'white':(False,'#ffffff'),
          'black':(False,'#0b1b31'),'tray-active':(True,None),
          'tray-bypass':(True,'#b4c1d2'),'tray-disconnected':(True,'#f5b75d')}
sizes=(16,20,24,32,40,48,64,128,256)
for name,(tile,colour) in variants.items():
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">']
    if tile:svg.append('<rect width="128" height="128" rx="29" fill="#0b1b31"/>')
    for fill,commands in FACES:
        path=' '.join(c[0]+' '.join(str(v) for v in c[1:]) for c in commands)
        svg.append(f'<path transform="translate(0 20)" fill="{colour or fill}" d="{path}"/>')
    svg.append('</svg>')
    (OUT/f'resoniq-{name}.svg').write_text('\n'.join(svg),encoding='utf-8')
    for size in (*sizes,512,1024):render(size,tile,colour).save(OUT/f'resoniq-{name}-{size}.png')
    render(256,tile,colour).save(OUT/f'resoniq-{name}.ico',sizes=[(s,s) for s in sizes])
    with (OUT/f'resoniq-{name}.ico').open('rb') as f:
        assert struct.unpack('<HHH',f.read(6))==(0,1,len(sizes))

board=Image.new('RGB',(1200,750),'#f2f5fa');d=ImageDraw.Draw(board)
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',24)
title=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',36)
d.text((42,24),'RESONIQ · Pulso continuo',font=title,fill='#0b1b31')
for i,(name,label) in enumerate([('app','Aplicación'),('tray-active','DSP activo'),('tray-bypass','DSP desactivado'),('tray-disconnected','Sin conexión')]):
    x=40+i*290;tile,colour=variants[name]
    board.paste(render(224,tile,colour),(x,96),render(224,tile,colour))
    d.text((x,334),label,font=font,fill='#0b1b31')
    cursor=x
    for size in (16,20,24,32,48,64):
        icon=render(size,tile,colour);board.paste(icon,(cursor,389),icon);cursor+=size+9
d.text((42,487),'Símbolo · claro / oscuro · versiones monocromáticas',font=font,fill='#0b1b31')
for i,name in enumerate(('mark','white','black')):
    x=42+i*350;bg='#0b1b31' if name=='white' else '#ffffff'
    d.rounded_rectangle((x,535,x+310,710),radius=18,fill=bg)
    tile,colour=variants[name];icon=render(170,tile,colour);board.paste(icon,(x+70,538),icon)
board.save(OUT/'resoniq-variants.png')
banner=Image.new('RGB',(493,58),'#f2f5fa')
icon=render(44,True);banner.paste(icon,(435,7),icon)
banner.save(OUT/'installer-banner.bmp')
dialog=Image.new('RGB',(493,312),'#ffffff');draw=ImageDraw.Draw(dialog)
draw.rectangle((0,0,163,312),fill='#0b1b31')
icon=render(116,False);dialog.paste(icon,(24,66),icon)
small=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',19)
draw.text((29,201),'RESONIQ',font=small,fill='#ffffff')
dialog.save(OUT/'installer-dialog.bmp')
print('Generated vector masters, PNGs, multiresolution ICOs and preview:',OUT)
