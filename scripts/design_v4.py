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

def base(title,state='KLAR',moving=False):
    parts.clear()
    rect(0,0,800,480,'bg',0); rect(0,0,800,68,'ink',0)
    text(24,28,'ARC / 04',22,'paper',700)
    text(24,49,'SVEISEPOSISJONERER',11,'line',600)
    text(240,41,title,22,'paper',500)
    rect(648,16,128,36,'teal' if moving else 'lime')
    text(664,40,state,15,'paper' if moving else 'ink',700)

def footer(left='OPPSETT',middle='PROGRAMMER',right='START',moving=False):
    rect(0,400,800,80,'paper',0)
    button(24,412,174,left,'bg','ink')
    button(210,412,246,middle,'bg','ink')
    button(472,412,304,right,'red' if moving else 'teal')

def save(slug,title,note):
    svg='<svg xmlns="http://www.w3.org/2000/svg" width="800" height="480" viewBox="0 0 800 480"><title>'+escape(title)+'</title><g font-family="Bahnschrift, Segoe UI, sans-serif">'+''.join(parts)+'</g></svg>'
    (OUT/(slug+'.svg')).write_text(svg,encoding='utf-8')
    screens.append(dict(slug=slug,title=title,note=note))

for moving in (False,True):
    base('Kontinuerlig rotasjon','I DRIFT' if moving else 'KLAR',moving)
    rect(24,88,456,288,'paper')
    text(44,120,'BEREGNET HASTIGHET' if moving else 'VALGT HASTIGHET',14,'muted',600)
    text(39,223,'0.500',92,'ink',500); text(370,220,'RPM',22,'muted',600)
    rect(44,247,416,1,'line',0)
    text(44,282,'471',30,'ink',600); text(112,281,'mm/min',17,'muted')
    text(44,306,'Ved emnediameter 300 mm',15,'muted')
    text(44,349,'01:24  /  FORLØPT' if moving else 'Juster med panelhjulet',16,'teal',600)
    text(508,112,'BETJENING',13,'muted',700)
    for y,label,value in ((153,'Hastighetskilde','Panelhjul'),(229,'Rotasjonsretning','MED KLOKKEN'),(305,'Program','Manuell kjøring')):
        text(508,y,label,15,'muted'); text(508,y+29,value,23,'ink',600)
    text(508,374,'Stopp før oppsett endres' if moving else 'Ny start krever et trykk',14,'muted')
    footer('LÅST I DRIFT' if moving else 'OPPSETT','MANUELL KJØRING' if moving else 'PROGRAMMER','■  STOPP' if moving else '▶  START ROTASJON',moving)
    save('02_running' if moving else '01_ready','Drift' if moving else 'Klar til start','Stor verdi, eksplisitt hastighetskilde og fast plass til START/STOPP. Beregnet hastighet er ikke encoderfeedback.')

base('Pulsrotasjon')
text(24,106,'SYKLUS',14,'muted',700)
for x,title,value,unit in ((24,'ROTASJON','1.2','sek'),(280,'PAUSE','0.8','sek'),(536,'HASTIGHET','0.50','RPM')):
    rect(x,124,240,178,'paper'); text(x+18,151,title,13,'muted',600)
    text(x+18,208,value,47,'ink',600); text(x+170,207,unit,17,'muted')
    button(x+14,232,98,'−','bg','ink'); button(x+126,232,100,'+','bg','ink')
parts.append('<path d="M24 367 H44 V326 H172 V367 H257 V326 H385 V367 H470 V326 H598 V367 H684 V326 H776" fill="none" stroke="#086C65" stroke-width="3"/>')
text(24,391,'PAUSE STARTER ETTER NEDBREMSING',11,'muted',600)
footer('TILBAKE','GJENTA: UTEN GRENSE','▶  START PULS')
save('03_pulse','Pulsoppsett','Store pluss/minus-flater og en konkret tidslinje. Pause regnes etter nedbremsing.')

base('Programbibliotek','3 / 16')
text(24,103,'VELG PROGRAM FOR Å SE DETALJER',13,'muted',700)
for y,n,name,detail in ((122,'01','Rotstreng','Kontinuerlig / 0.30 RPM / Ø 300 mm'),(211,'02','Fyllstreng','Puls / 1.2 s på · 0.8 s pause'),(300,'03','Indeksering','Steg / 90° × 4 / pause 2.0 s')):
    rect(24,y,752,76,'paper'); text(42,y+46,n,25,'teal',600)
    text(99,y+31,name,23,'ink',600); text(99,y+58,detail,15,'muted'); text(735,y+47,'›',34,'teal')
footer('TILBAKE','REDIGER PROGRAM','+  NYTT PROGRAM')
save('04_programs','Programmer','Lesbare rader med nøkkelverdier. Programvalg åpner gjennomgang før start.')

base('Oppsett')
for x,y,n,title,detail in ((24,92,'01','Motor','Mikrosteg · rampe · maks RPM'),(408,92,'02','Kalibrering','Vinkel og emnediameter'),(24,192,'03','Fotpedal','Aktivering og inngangsstatus'),(408,192,'04','Skjerm','Lysstyrke og dimming'),(24,292,'05','Diagnostikk','Innganger og feilhistorikk'),(408,292,'06','System','Versjon og enhetsstatus')):
    rect(x,y,368,88,'paper'); text(x+16,y+26,n,12,'teal',700)
    text(x+16,y+53,title,24,'ink',600); text(x+16,y+75,detail,14,'muted'); text(x+334,y+48,'›',27,'teal')
rect(0,400,800,80,'paper',0); button(24,412,230,'←  TILBAKE TIL DRIFT')
text(286,447,'Innstillinger endres når motoren står stille',17,'muted')
save('05_settings','Oppsett','Seks tydelige områder og fast retur til drift.')

base('Bevegelse sperret','STOPPET')
rect(24,92,752,92,'red'); text(44,133,'NØDSTOPP AKTIV',30,'paper',700)
text(44,163,'Motorutgangen er deaktivert av styringen.',18,'paper')
for y,n,label in ((228,'1','Kontroller maskinen og fjern årsaken.'),(284,'2','Frigi den fysiske nødstoppknappen.'),(340,'3','Kvitter når inngangene er klare.')):
    text(24,y,n,25,'teal',700); text(64,y,label,22,'ink',500)
rect(0,400,800,80,'paper',0)
text(24,433,'VENTER PÅ NØDSTOPPINNGANG',14,'red',700)
text(24,456,'Kvittering starter aldri motoren.',15,'muted')
button(472,412,304,'KVITTERING SPERRET','line','muted')
save('06_fault','Feiltilstand','Tekst og handlingstrinn forklarer feilen. Kvittering er sperret mens inngangen er aktiv.')

(OUT/'manifest.json').write_text(json.dumps(screens,ensure_ascii=False,indent=2),encoding='utf-8')
items=''.join(f'<article><header><h2>{s["title"]}</h2><a href="{s["slug"]}.svg">Åpne SVG ↗</a></header><img src="{s["slug"]}.svg"><p>{s["note"]}</p></article>' for s in screens)
html='''<!doctype html><html lang="no"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>ARC / 04 · Designforslag</title><style>*{box-sizing:border-box}body{margin:0;background:#dcded6;color:#162c32;font-family:Segoe UI,sans-serif}main{max-width:1720px;margin:auto;padding:48px 32px}h1{font-size:56px;letter-spacing:-2px;margin:8px 0 16px}h2{font-size:21px;margin:0}small{letter-spacing:3px}section{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:36px;margin-top:40px}header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}a{color:#086c65}img{width:100%;display:block;border-radius:8px}p{line-height:1.65;max-width:900px}article p{font-size:14px;margin-top:12px}@media(max-width:1000px){section{grid-template-columns:1fr}h1{font-size:40px}}</style><main><small>DESIGNSTUDIE 04 / 800 × 480 / IKKE INSTALLERT</small><h1>Verksted, i dagslys.</h1><p>Et lyst instrumentpanel med mørk topp, petroleumsgrønne handlinger og store tall. Grafikken er rolig under drift. Rødt brukes til stopp og feil. Norske tekster er en del av forslaget.</p><p>Alle data er eksempler. STOPP har fast plass i aktive kjørebilder; låste kontroller skal være utilgjengelige. Dette er seks konseptskjermer. Ingen firmware er endret.</p><section>'''
(OUT/'index.html').write_text(html+items+'</section></main></html>',encoding='utf-8')
print(f'Created {len(screens)} SVG screens in {OUT}')
