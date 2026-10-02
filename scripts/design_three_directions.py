"""Three independent 800x480 design studies; no firmware changes."""
from pathlib import Path
from html import escape

OUT = Path(__file__).resolve().parents[1] / 'docs/images/three_directions'
OUT.mkdir(parents=True, exist_ok=True)
P = []
def rect(x,y,w,h,fill,r=0,stroke=None):
    P.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}"'+(f' stroke="{stroke}"' if stroke else '')+'/>')
def text(x,y,s,size=18,fill='#F4F4EF',weight=400,spacing=0,font='Bahnschrift, Segoe UI, sans-serif'):
    P.append(f'<text x="{x}" y="{y}" font-family="{font}" font-size="{size}" font-weight="{weight}" letter-spacing="{spacing}" fill="{fill}">{escape(s)}</text>')
def line(x,y,x2,y2,fill='#343838',width=1):
    P.append(f'<path d="M{x} {y}L{x2} {y2}" stroke="{fill}" stroke-width="{width}"/>')
def path(d,fill,stroke=None):
    P.append(f'<path d="{d}" fill="{fill}"'+(f' stroke="{stroke}"' if stroke else '')+'/>')
def circle(x,y,r,fill):
    P.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}"/>')
def save(name,title):
    (OUT/(name+'.svg')).write_text('<svg xmlns="http://www.w3.org/2000/svg" width="800" height="480" viewBox="0 0 800 480"><title>'+title+'</title>'+''.join(P)+'</svg>',encoding='utf-8')
    P.clear()

# 01 — a machine fascia: bolted corners, compressed hierarchy, orange selector.
rect(0,0,800,480,'#151615');rect(8,8,784,464,'#20221F',8,'#434640')
for x,y in [(20,20),(780,20),(20,460),(780,460)]:
    circle(x,y,4,'#4D5049');line(x-2,y+2,x+2,y-2,'#171A16')
rect(32,28,7,43,'#FF882E');text(52,48,'ROTATOR',27,weight=700,spacing=1)
text(53,68,'MOTION CONTROL  /  P4',11,'#A9AEA2',500,1)
rect(592,29,176,42,'#30382B',2);circle(610,50,4,'#BEF67A');text(623,56,'READY TO START',14,'#BEF67A',600)
line(32,86,768,86,'#474A40')
rect(32,103,453,245,'#10120F',3,'#565A4B')
text(50,129,'TARGET SPEED',14,'#B5BBAA',600,1)
text(50,229,'0.500',104,'#ECF5D8',600,-3)
text(384,225,'RPM',23,'#B5BBAA',600)
line(51,248,466,248,'#393E33')
text(51,278,'PANEL DIAL',18,'#ECF5D8',600);text(335,278,'0.00–1.00',16,'#B5BBAA')
for i in range(38):rect(52+i*10.8,300,7,23,'#FF882E' if i<19 else '#2D3227')
text(506,122,'RUN MODE',12,'#A9AEA2',600,1)
text(505,151,'CONTINUOUS',25,weight=700)
rect(505,168,263,57,'#32352E',2,'#555A4A');text(522,203,'↻  CLOCKWISE',22,weight=600)
text(506,249,'CALCULATED SURFACE SPEED',11,'#A9AEA2',600)
text(505,287,'471',37,weight=600);text(580,285,'mm/min',17,'#A9AEA2')
text(506,323,'WORKPIECE  Ø 300 mm',15,'#CDD1C5')
rect(32,363,736,30,'#2C2F28',2)
text(46,383,'SOURCE: PANEL',12,'#C3C9B6',600);text(283,383,'DIRECTION: CW',12,'#C3C9B6',600);text(527,383,'AUTO-STOP: OFF',12,'#C3C9B6',600)
for x,w,label in [(32,126,'SETUP'),(168,156,'PROGRAMS'),(334,126,'MODES')]:
    rect(x,410,w,46,'#393D33',2,'#5C6151');text(x+17,439,label,16,weight=600)
rect(480,410,288,46,'#FF882E',2);path('M501 422L518 433L501 444Z','#141A10');text(536,440,'START ROTATION',19,'#141A10',700)
save('01_industrial','01 — Industrial / Forge')

# 02 — editorial instrument: typography, deliberate voids, fine warm rules.
rect(0,0,800,480,'#101112')
text(32,43,'arc.',34,'#F2EEE6',600,-2)
text(118,39,'POSITIONER',11,'#989994',500,3)
circle(682,33,3,'#D0D8C5');text(695,38,'Ready',14,'#D0D8C5')
line(32,64,768,64,'#343531')
text(32,103,'01',14,'#A39A86');text(77,103,'Continuous rotation',20,'#F2EEE6',400)
text(34,150,'TARGET / RPM',11,'#B8AA90',500,2)
text(24,284,'0.500',148,'#F2EEE6',300,-8,'Segoe UI, sans-serif')
line(541,140,541,294,'#343531')
text(580,161,'CONTROL',10,'#9C9C96',500,2)
text(580,187,'Panel dial',23,'#F2EEE6')
text(580,238,'DIRECTION',10,'#9C9C96',500,2)
text(580,264,'Clockwise  ↗',22,'#F2EEE6')
line(32,310,768,310,'#343531')
text(32,338,'SURFACE SPEED',10,'#9C9C96',500,1.5)
text(32,369,'471',28,'#F2EEE6');text(89,368,'mm/min',14,'#B3B2AB')
text(280,338,'WORKPIECE',10,'#9C9C96',500,1.5)
text(280,369,'Ø 300',28,'#F2EEE6');text(366,368,'mm',14,'#B3B2AB')
text(576,340,'Calculated from target speed.',13,'#989994')
text(576,364,'No automatic stop.',13,'#989994')
line(32,398,768,398,'#343531')
text(32,441,'Setup',16,'#CACAC3');text(130,441,'Programs',16,'#CACAC3');text(255,441,'Modes',16,'#CACAC3')
rect(494,414,274,48,'#D9CCB1',24);text(521,444,'Start rotation',18,'#191A17',600);text(726,446,'→',24,'#191A17')
save('02_premium','02 — Premium / Quiet')

# 03 — motorsport telemetry: long segmented scale and sharp asymmetric plates.
rect(0,0,800,480,'#090D13')
path('M0 0H460L429 54H0Z','#1C2634')
text(24,34,'R / 01',27,'#EDFF54',700,1);text(137,32,'ROTATION SYSTEM',12,'#D4DFEA',600,2)
path('M645 14H779V43H632Z','#EDFF54');text(659,34,'READY',17,'#11180F',700,2)
text(24,82,'TARGET RPM',12,'#91A3BB',600,2)
text(631,82,'PANEL CONTROL',12,'#91A3BB',600,1)
for i in range(40):
    x=24+i*18.8
    path(f'M{x} 97H{x+14}L{x+6} 124H{x-8}Z','#EDFF54' if i<20 else '#263142')
text(24,146,'0.00',12,'#7F90A9');text(378,146,'0.50',12,'#EDFF54');text(735,146,'1.00',12,'#7F90A9')
text(18,286,'0.500',142,'#F5F8FD',700,-5)
text(387,284,'RPM',19,'#EDFF54',700,1)
path('M490 164H776V294H466Z','#172130')
rect(490,177,4,27,'#EDFF54')
text(510,197,'CONTINUOUS',24,'#F5F8FD',700)
text(510,223,'MANUAL ROTATION',11,'#94A7C0',600,1.5)
line(504,239,756,239,'#344254')
text(510,270,'CW',28,'#EDFF54',700);text(574,267,'CLOCKWISE  ↻',15,'#D3DFEF',600)
line(24,311,776,311,'#344254')
for x,label,value,unit in [(24,'SURFACE / CALC.','471','mm/min'),(280,'WORKPIECE','300','mm'),(541,'AUTO-STOP','OFF','')]:
    text(x,334,label,11,'#8E9FB8',600,1.5);text(x,374,value,36,'#E7EFFB',600)
    text(x+81,372,unit,14,'#93A4BC')
line(252,324,252,383,'#2B3748');line(513,324,513,383,'#2B3748')
rect(0,399,800,81,'#111923')
for x,w,label in [(24,125,'SETUP'),(157,147,'PROGRAMS'),(312,118,'MODES')]:
    rect(x,417,w,46,'#263244',2);text(x+16,446,label,15,'#D5E0F1',600)
path('M470 414H779V449L763 465H450Z','#EDFF54')
path('M485 427L503 439L485 451Z','#10170A');text(525,446,'START ROTATION',21,'#10170A',700)
save('03_racing','03 — Racing / Apex')

items=''.join(f'<article><h2>{title}</h2><img src="{slug}.svg"><a href="{slug}.svg">SVG</a></article>' for slug,title in [('01_industrial','1 — Industrial / Forge'),('02_premium','2 — Premium / Quiet'),('03_racing','3 — Racing / Apex')])
(OUT/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>Three design directions</title><style>body{background:#181a1e;color:#eef0ed;font:16px Segoe UI,sans-serif;margin:32px}main{max-width:1000px;margin:auto}img{width:100%;display:block}article{margin:40px 0}h2{font-size:20px}a{color:#c9cfd8}</style><main><h1>Three directions. One screen.</h1><p>800 × 480 · Dark mode · English · Static concepts, firmware unchanged.</p>'+items+'</main></html>',encoding='utf-8')
print(OUT)
