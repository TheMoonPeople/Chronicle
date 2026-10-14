from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parent / "fold-inner-50pct2.png"
FONT = Path(r"C:\Windows\Fonts\bahnschrift.ttf")
FONT_BOLD = Path(r"C:\Windows\Fonts\seguibl.ttf")
WHITE = (239, 243, 250)
COLORS = {"triangle": (100, 220, 161), "circle": (248, 116, 119),
          "cross": (114, 171, 255), "square": (222, 143, 235)}

def font(size, bold=False):
    return ImageFont.truetype(str(FONT_BOLD if bold else FONT), size)

def cover(im, size):
    w, h = size
    ratio = max(w / im.width, h / im.height)
    scaled = im.resize((math.ceil(im.width * ratio), math.ceil(im.height * ratio)), Image.Resampling.LANCZOS)
    x = (scaled.width - w) // 2
    y = (scaled.height - h) // 2
    return scaled.crop((x, y, x + w, y + h))

def background(size):
    im = cover(Image.open(SOURCE).convert("RGB"), size).filter(ImageFilter.GaussianBlur(16)).convert("RGBA")
    tint = Image.new("RGBA", size, (8, 14, 27, 128))
    im = Image.alpha_composite(im, tint)
    d = ImageDraw.Draw(im, "RGBA")
    w, h = size
    d.rounded_rectangle((w*.33, h*.32, w*.67, h*.67), radius=25,
                        fill=(8, 15, 28, 92), outline=(198, 214, 235, 38), width=2)
    d.text((w*.5, h*.475), "DARK CLOUD", font=font(round(h*.053), True),
           fill=(230, 237, 248, 205), anchor="mm")
    d.text((w*.5, h*.54), "GAME VIEW", font=font(round(h*.025)),
           fill=(195, 205, 221, 130), anchor="mm")
    return im

def circle(im, x, y, r, label=None, symbol=None, accent=None, alpha=80, ring=125, pressed=False):
    d = ImageDraw.Draw(im, "RGBA")
    d.ellipse((x-r, y-r, x+r, y+r), fill=(8, 15, 27, alpha),
              outline=(232, 240, 251, ring), width=max(2, round(r*.045)))
    if pressed:
        d.ellipse((x-r+5, y-r+5, x+r-5, y+r-5), outline=(255, 255, 255, 85), width=2)
    if symbol:
        c = accent or COLORS[symbol]
        t = max(4, round(r*.105))
        a = r*.38
        if symbol == "triangle":
            pts = [(x,y-a), (x-a,y+a), (x+a,y+a)]
            d.line(pts+[pts[0]], fill=(*c,245), width=t, joint="curve")
        elif symbol == "square":
            d.rounded_rectangle((x-a,y-a,x+a,y+a), radius=t, outline=(*c,245), width=t)
        elif symbol == "circle":
            d.ellipse((x-a,y-a,x+a,y+a), outline=(*c,245), width=t)
        elif symbol == "cross":
            d.line((x-a,y-a,x+a,y+a), fill=(*c,245), width=t)
            d.line((x+a,y-a,x-a,y+a), fill=(*c,245), width=t)
    elif label:
        d.text((x,y+1), label, font=font(max(14,round(r*.42)), True),
               fill=(*WHITE,225), anchor="mm")

def pill(im, x, y, w, h, label, alpha=86):
    d = ImageDraw.Draw(im, "RGBA")
    d.rounded_rectangle((x-w/2,y-h/2,x+w/2,y+h/2), radius=h/2,
                        fill=(7,13,25,alpha), outline=(230,238,250,125), width=2)
    d.text((x,y+1),label,font=font(max(13,round(h*.38)),True),fill=(*WHITE,215),anchor="mm")

def stick(im, x, y, r, label, style="disc"):
    d = ImageDraw.Draw(im, "RGBA")
    if style == "disc":
        d.ellipse((x-r,y-r,x+r,y+r),fill=(5,11,24,87),outline=(225,237,250,100),width=3)
        d.ellipse((x-r*.44,y-r*.44,x+r*.44,y+r*.44),fill=(28,41,62,165),
                  outline=(241,247,255,165),width=3)
    else:
        d.ellipse((x-r,y-r,x+r,y+r),outline=(213,229,248,85),width=3)
        d.ellipse((x-r*.3,y-r*.3,x+r*.3,y+r*.3),fill=(215,231,251,78))
    d.text((x,y+1),label,font=font(max(13,round(r*.31)),True),fill=(*WHITE,220),anchor="mm")

def dpad(im, x, y, r, style="cross"):
    d = ImageDraw.Draw(im, "RGBA")
    if style == "cross":
        arm = r*.46
        pts = [(x-arm,y-r),(x+arm,y-r),(x+arm,y-arm),(x+r,y-arm),
               (x+r,y+arm),(x+arm,y+arm),(x+arm,y+r),(x-arm,y+r),
               (x-arm,y+arm),(x-r,y+arm),(x-r,y-arm),(x-arm,y-arm)]
        d.polygon(pts,fill=(7,14,27,112))
        d.line(pts+[pts[0]],fill=(226,236,250,145),width=3,joint="curve")
        for dx,dy,ang in [(0,-.65,0),(.65,0,90),(0,.65,180),(-.65,0,270)]:
            px=x+dx*r; py=y+dy*r; a=r*.14
            points=[(px,py-a),(px-a,py+a),(px+a,py+a)]
            rad=math.radians(ang)
            points=[(px+(u-px)*math.cos(rad)-(v-py)*math.sin(rad),
                     py+(u-px)*math.sin(rad)+(v-py)*math.cos(rad)) for u,v in points]
            d.polygon(points,fill=(236,244,255,220))
    else:
        for dx,dy,s in [(0,-1,"▲"),(1,0,"▶"),(0,1,"▼"),(-1,0,"◀")]:
            circle(im,x+dx*r*.75,y+dy*r*.75,r*.43,s,alpha=72,ring=125)

def faces(im,x,y,r,spacing=1.05,alpha=82):
    for symbol,dx,dy in [("triangle",0,-1),("circle",1,0),("cross",0,1),("square",-1,0)]:
        circle(im,x+dx*r*spacing,y+dy*r*spacing,r*.53,symbol=symbol,alpha=alpha)

def badge(im, text, x, y):
    d=ImageDraw.Draw(im,"RGBA")
    d.rounded_rectangle((x-33,y-17,x+33,y+17),radius=17,fill=(8,15,29,170),outline=(225,235,250,110),width=1)
    d.text((x,y+1),text,font=font(18,True),fill=(*WHITE,225),anchor="mm")

def footer(im, text):
    w,h=im.size; d=ImageDraw.Draw(im,"RGBA")
    d.rounded_rectangle((w*.34,h*.92,w*.66,h*.985),radius=20,fill=(6,12,23,125))
    d.text((w*.5,h*.952),text,font=font(round(h*.018)),fill=(225,235,249,150),anchor="mm")

def render_standard(variant):
    w,h=1600,720; im=background((w,h))
    if variant==0:
        dpad(im,155,398,62)
        stick(im,171,589,76,"L3")
        faces(im,1435,398,76)
        stick(im,1427,589,70,"R3")
        pill(im,142,93,120,49,"L2");pill(im,142,157,120,49,"L1")
        pill(im,1458,93,120,49,"R2");pill(im,1458,157,120,49,"R1")
        pill(im,747,669,95,38,"SELECT");pill(im,853,669,95,38,"START")
        badge(im,"01",800,38);footer(im,"CLASSIC  /  balanced clusters")
    elif variant==1:
        stick(im,154,555,69,"L3", "ghost")
        dpad(im,165,334,54,"buttons")
        faces(im,1439,545,70,1.03,65)
        stick(im,1405,300,56,"R3","ghost")
        pill(im,128,68,100,40,"L2",65);pill(im,128,121,100,40,"L1",65)
        pill(im,1472,68,100,40,"R2",65);pill(im,1472,121,100,40,"R1",65)
        pill(im,756,659,77,34,"SEL",60);pill(im,844,659,77,34,"START",60)
        badge(im,"02",800,38);footer(im,"FLOATING  /  light touch zones")
    else:
        d=ImageDraw.Draw(im,"RGBA")
        d.rounded_rectangle((24,32,316,688),radius=50,fill=(7,14,27,105),outline=(231,239,251,50),width=2)
        d.rounded_rectangle((1284,32,1576,688),radius=50,fill=(7,14,27,105),outline=(231,239,251,50),width=2)
        pill(im,110,93,115,45,"L2");pill(im,230,93,115,45,"L1")
        pill(im,1370,93,115,45,"R1");pill(im,1490,93,115,45,"R2")
        dpad(im,169,320,70)
        stick(im,171,553,70,"L3")
        faces(im,1432,318,79)
        stick(im,1433,553,70,"R3")
        pill(im,738,654,88,37,"SELECT");pill(im,862,654,88,37,"START")
        badge(im,"03",800,38);footer(im,"GRIP RAILS  /  defined hand zones")
    return im

def render_fold(variant):
    w,h=1000,900; im=background((w,h))
    if variant==0:
        dpad(im,172,554,72)
        stick(im,194,756,80,"L3")
        faces(im,824,554,85)
        stick(im,806,756,76,"R3")
        pill(im,132,97,103,49,"L2");pill(im,250,97,103,49,"L1")
        pill(im,750,97,103,49,"R1");pill(im,868,97,103,49,"R2")
        pill(im,435,840,100,39,"SELECT");pill(im,565,840,100,39,"START")
        badge(im,"04",500,38);footer(im,"FOLD CORNERS  /  clear center")
    elif variant==1:
        stick(im,184,731,76,"L3", "ghost")
        dpad(im,192,488,60,"buttons")
        faces(im,806,485,73,1.07,70)
        stick(im,814,733,66,"R3","ghost")
        pill(im,122,91,95,43,"L2",65);pill(im,240,91,95,43,"L1",65)
        pill(im,760,91,95,43,"R1",65);pill(im,878,91,95,43,"R2",65)
        pill(im,442,832,88,36,"SELECT",65);pill(im,558,832,88,36,"START",65)
        badge(im,"05",500,38);footer(im,"FOLD FLOATING  /  minimum visual weight")
    else:
        d=ImageDraw.Draw(im,"RGBA")
        d.rounded_rectangle((25,42,285,858),radius=40,fill=(7,14,27,104),outline=(231,239,251,48),width=2)
        d.rounded_rectangle((715,42,975,858),radius=40,fill=(7,14,27,104),outline=(231,239,251,48),width=2)
        pill(im,96,106,90,42,"L2");pill(im,213,106,90,42,"L1")
        pill(im,787,106,90,42,"R1");pill(im,904,106,90,42,"R2")
        dpad(im,155,407,68)
        stick(im,155,694,73,"L3")
        faces(im,845,407,76)
        stick(im,845,694,73,"R3")
        pill(im,432,838,85,36,"SELECT");pill(im,568,838,85,36,"START")
        badge(im,"06",500,38);footer(im,"FOLD RAILS  /  stable thumb zones")
    return im

def main():
    ROOT.mkdir(exist_ok=True)
    items=[]
    names=["standard-classic", "standard-floating", "standard-grip-rails",
           "fold-corners", "fold-floating", "fold-rails"]
    for i,name in enumerate(names):
        im=render_standard(i) if i<3 else render_fold(i-3)
        path=ROOT / f"{i+1:02d}-{name}.png"
        im.convert("RGB").save(path,quality=94)
        items.append((name,im))
    sheet=Image.new("RGB",(2400,3120),(11,17,31))
    d=ImageDraw.Draw(sheet)
    d.text((90,60),"DARK CLOUD  /  TOUCH OVERLAY CONCEPTS",font=font(56,True),fill=WHITE)
    d.text((90,130),"Three standard landscape layouts  •  Three Z Fold inner landscape layouts",font=font(29),fill=(150,171,200))
    positions=[(70,230),(70,800),(70,1370),(1410,230),(1410,1130),(1410,2030)]
    for i,(name,im) in enumerate(items):
        x,y=positions[i]
        thumb=im.copy()
        thumb.thumbnail((1260 if i<3 else 920,820 if i>=3 else 520),Image.Resampling.LANCZOS)
        d.text((x,y-46),f"{i+1:02d}  {name.upper().replace('-', ' ')}",font=font(28,True),fill=WHITE)
        sheet.paste(thumb.convert("RGB"),(x,y))
    sheet.save(ROOT/"all-six-overlays.png",quality=92)

if __name__=="__main__":
    main()
