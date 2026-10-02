"""Measure actual browser font bounds and flat SVG text contrast at 800x480."""
from pathlib import Path
import json
from playwright.sync_api import sync_playwright

OUT=Path(__file__).resolve().parents[1]/'docs/images/ui_mockup_v5'
JS=r'''() => {
 const els=[...document.querySelector('svg').querySelectorAll('*')];
 const texts=els.filter(e=>e.tagName==='text').map(e=>{const b=e.getBBox();return {e,t:e.textContent,x:b.x,y:b.y,w:b.width,h:b.height,size:+e.getAttribute('font-size')};});
 const rgb=s=>s.match(/[\d.]+/g).slice(0,3).map(Number);
 const lum=c=>c.map(v=>{v/=255;return v<=.04045?v/12.92:((v+.055)/1.055)**2.4}).reduce((s,v,i)=>s+v*[.2126,.7152,.0722][i],0);
 const contrast=(a,b)=>{a=lum(a);b=lum(b);return (Math.max(a,b)+.05)/(Math.min(a,b)+.05)};
 const result={outside:[],overlap:[],lowContrast:[],container:[],minimumFont:Math.min(...texts.map(t=>t.size)),textCount:texts.length};
 for(let i=0;i<texts.length;i++){
  const a=texts[i];if(a.x<0||a.y<0||a.x+a.w>800||a.y+a.h>480)result.outside.push(a.t);
  for(let j=i+1;j<texts.length;j++){const b=texts[j];if(Math.min(a.x+a.w,b.x+b.w)-Math.max(a.x,b.x)>1&&Math.min(a.y+a.h,b.y+b.h)-Math.max(a.y,b.y)>1)result.overlap.push([a.t,b.t]);}
  const preceding=els.slice(0,els.indexOf(a.e)).reverse();
  const bg=preceding.find(e=>{if(e.tagName!=='rect')return false;const b=e.getBBox();return a.x+a.w/2>=b.x&&a.x+a.w/2<=b.x+b.width&&a.y+a.h/2>=b.y&&a.y+a.h/2<=b.y+b.height;});
  if(bg){const b=bg.getBBox();const c=contrast(rgb(getComputedStyle(a.e).fill),rgb(getComputedStyle(bg).fill));
   if(c<4.5)result.lowContrast.push({text:a.t,ratio:+c.toFixed(2)});
   if(b.width<790&&(a.x<b.x+6||a.x+a.w>b.x+b.width-6||a.y<b.y+3||a.y+a.h>b.y+b.height-3))result.container.push(a.t);
  }
 }
 return result;
}'''
reports=[]
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=p.chromium.executable_path,headless=True)
    page=browser.new_page(viewport={'width':800,'height':480})
    for item in json.loads((OUT/'manifest.json').read_text(encoding='utf-8')):
        page.goto((OUT/(item['slug']+'.svg')).as_uri())
        report=page.evaluate(JS)
        report['screen']=item['slug'];reports.append(report)
    browser.close()
(OUT/'readability_audit.json').write_text(json.dumps(reports,indent=2,ensure_ascii=False),encoding='utf-8')
for r in reports:
    issues={k:r[k] for k in ['outside','overlap','lowContrast','container'] if r[k]}
    if issues:print(r['screen'],json.dumps(issues,ensure_ascii=False))
print('Views:',len(reports),'Text elements:',sum(r['textCount'] for r in reports),'Minimum font:',min(r['minimumFont'] for r in reports))
