from pathlib import Path
from html import escape
import json

OUT = Path(__file__).resolve().parents[1] / 'docs/images/ui_concept_v4'
OUT.mkdir(parents=True, exist_ok=True)
C = dict(bg='#E9E8E1', paper='#F9F8F2', ink='#162C32', muted='#526268',
         line='#C7CEC9', teal='#086C65', red='#AB302D', lime='#D7E88B')
parts, screens = [], []

def rect(x,y,w,h,c,r=8):
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{C.get(c,c)}"/>')

def text(x,y,s,size=18,c='ink',weight=400):
    parts.append(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{C.get(c,c)}">{escape(str(s))}</text>')

def button(x,y,w,s,c='ink',fg='paper'):
    rect(x,y,w,56,c)
    text(x+18,y+35,s,18,fg,600)

def base(title,state='READY',moving=False):
    parts.clear()
    rect(0,0,800,480,'bg',0); rect(0,0,800,68,'ink',0)
    text(24,28,'ARC / 04',22,'paper',700)
    text(24,49,'WELDING POSITIONER',11,'line',600)
    text(240,41,title,22,'paper',500)
    rect(648,16,128,36,'teal' if moving else 'lime')
    text(664,40,state,15,'paper' if moving else 'ink',700)

def footer(left='SETTINGS',middle='PROGRAMS',right='START',moving=False):
    rect(0,400,800,80,'paper',0)
    button(24,412,174,left,'bg','ink')
    button(210,412,246,middle,'bg','ink')
    button(472,412,304,right,'red' if moving else 'teal')

def save(slug,title,note):
    svg='<svg xmlns="http://www.w3.org/2000/svg" width="800" height="480" viewBox="0 0 800 480"><title>'+escape(title)+'</title><g font-family="Bahnschrift, Segoe UI, sans-serif">'+''.join(parts)+'</g></svg>'
    (OUT/(slug+'.svg')).write_text(svg,encoding='utf-8')
    screens.append(dict(slug=slug,title=title,note=note))

for moving in (False,True):
    base('Continuous rotation','RUNNING' if moving else 'READY',moving)
    rect(24,88,456,288,'paper')
    text(44,120,'CALCULATED SPEED' if moving else 'TARGET SPEED',14,'muted',600)
    text(39,223,'0.500',92,'ink',500); text(370,220,'RPM',22,'muted',600)
    rect(44,247,416,1,'line',0)
    text(44,282,'471',30,'ink',600); text(112,281,'mm/min',17,'muted')
    text(44,306,'At workpiece diameter 300 mm',15,'muted')
    text(44,349,'01:24  /  ELAPSED' if moving else 'Adjust with the panel dial',16,'teal',600)
    text(508,112,'CONTROL',13,'muted',700)
    for y,label,value in ((153,'Speed source','Panel dial'),(229,'Direction','CLOCKWISE'),(305,'Program','Manual rotation')):
        text(508,y,label,15,'muted'); text(508,y+29,value,23,'ink',600)
    text(508,374,'Stop before changing settings' if moving else 'Press START for a new run',14,'muted')
    footer('LOCKED' if moving else 'SETTINGS','MANUAL ROTATION' if moving else 'PROGRAMS','■  STOP' if moving else '▶  START ROTATION',moving)
    save('02_running' if moving else '01_ready','Running' if moving else 'Ready to start','Large value, explicit source and fixed START/STOP. Speed is calculated, without encoder feedback.')

base('Pulse rotation')
text(24,106,'CYCLE',14,'muted',700)
for x,title,value,unit in ((24,'ROTATION','1.2','s'),(280,'PAUSE','0.8','s'),(536,'SPEED','0.50','RPM')):
    rect(x,124,240,178,'paper'); text(x+18,151,title,13,'muted',600)
    text(x+18,208,value,47,'ink',600); text(x+170,207,unit,17,'muted')
    button(x+14,232,98,'−','bg','ink'); button(x+126,232,100,'+','bg','ink')
parts.append('<path d="M24 367 H44 V326 H172 V367 H257 V326 H385 V367 H470 V326 H598 V367 H684 V326 H776" fill="none" stroke="#086C65" stroke-width="3"/>')
text(24,391,'PAUSE STARTS AFTER DECELERATION',11,'muted',600)
footer('BACK','REPEAT: UNLIMITED','▶  START PULSE')
save('03_pulse','Pulse settings','Large adjustment controls and a clear timeline. Pause begins after deceleration.')

base('Program library','3 / 16')
text(24,103,'SELECT A PROGRAM TO REVIEW',13,'muted',700)
for y,n,name,detail in ((122,'01','Root pass','Continuous / 0.30 RPM / Ø 300 mm'),(211,'02','Fill pass','Pulse / 1.2 s on · 0.8 s pause'),(300,'03','Indexing','Step / 90° × 4 / pause 2.0 s')):
    rect(24,y,752,76,'paper'); text(42,y+46,n,25,'teal',600)
    text(99,y+31,name,23,'ink',600); text(99,y+58,detail,15,'muted'); text(735,y+47,'›',34,'teal')
footer('BACK','EDIT PROGRAM','+  NEW PROGRAM')
save('04_programs','Programs','Readable rows with key values. Program selection opens a review before start.')

base('Settings')
for x,y,n,title,detail in ((24,92,'01','Motor','Microstep · ramp · max RPM'),(408,92,'02','Calibration','Angle and workpiece diameter'),(24,192,'03','Foot pedal','Enable and input status'),(408,192,'04','Display','Brightness and dimming'),(24,292,'05','Diagnostics','Inputs and fault history'),(408,292,'06','System','Version and device status')):
    rect(x,y,368,88,'paper'); text(x+16,y+26,n,12,'teal',700)
    text(x+16,y+53,title,24,'ink',600); text(x+16,y+75,detail,14,'muted'); text(x+334,y+48,'›',27,'teal')
rect(0,400,800,80,'paper',0); button(24,412,230,'←  BACK TO OPERATION')
text(286,447,'Change settings while the motor is stopped',17,'muted')
save('05_settings','Settings','Six clear areas and a fixed return to operation.')

base('Motion blocked','STOPED')
rect(24,92,752,92,'red'); text(44,133,'EMERGENCY STOP',30,'paper',700)
text(44,163,'Controller motor output is disabled.',18,'paper')
for y,n,label in ((228,'1','Check the machine and clear the cause.'),(284,'2','Release the physical E-STOP button.'),(340,'3','Reset when inputs are clear.')):
    text(24,y,n,25,'teal',700); text(64,y,label,22,'ink',500)
rect(0,400,800,80,'paper',0)
text(24,433,'WAITING FOR E-STOP INPUT',14,'red',700)
text(24,456,'Reset never starts the motor.',15,'muted')
button(472,412,304,'RESET BLOCKED','line','muted')
save('06_fault','Fault state','Text and steps explain the fault. Reset is blocked while the input is active.')

(OUT/'manifest.json').write_text(json.dumps(screens,ensure_ascii=False,indent=2),encoding='utf-8')
items=''.join(f'<article><header><h2>{s["title"]}</h2><a href="{s["slug"]}.svg">Open SVG ↗</a></header><img src="{s["slug"]}.svg"><p>{s["note"]}</p></article>' for s in screens)
html='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>ARC / 04 · Design proposal</title><style>*{box-sizing:border-box}body{margin:0;background:#dcded6;color:#162c32;font-family:Segoe UI,sans-serif}main{max-width:1720px;margin:auto;padding:48px 32px}h1{font-size:56px;letter-spacing:-2px;margin:8px 0 16px}h2{font-size:21px;margin:0}small{letter-spacing:3px}section{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:36px;margin-top:40px}header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}a{color:#086c65}img{width:100%;display:block;border-radius:8px}p{line-height:1.65;max-width:900px}article p{font-size:14px;margin-top:12px}@media(max-width:1000px){section{grid-template-columns:1fr}h1{font-size:40px}}</style><main><small>DESIGN STUDY 04 / 800 × 480 / NOT INSTALLED</small><h1>Workshop, in daylight.</h1><p>A light instrument panel with a dark header, teal actions and large values. Red marks stop and fault states. Labels are in English.</p><p>All values are illustrative. STOP has a fixed position during operation; locked controls are unavailable. These are six historical concept screens, separate from the V5 firmware.</p><section>'''
(OUT/'index.html').write_text(html+items+'</section></main></html>',encoding='utf-8')
print(f'Created {len(screens)} SVG screens in {OUT}')
