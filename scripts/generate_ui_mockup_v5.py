"""Reference-led dark/orange mockups for every registered screen.

Reuse the V3 screen inventory/content, replacing the shared design primitives
and main-screen composition. No firmware files are modified.
"""
from pathlib import Path
import re

source = Path(__file__).with_name('generate_ui_mockup_v3.py').read_text(encoding='utf-8')
source = source.replace("'docs/images/ui_mockup_v3'", "'docs/images/ui_mockup_v5'")
source = source.replace("bg='#101316', panel='#1B2025', raised='#252C32', line='#39434B',", "bg='#192124', panel='#252F33', raised='#303B40', line='#465258',")
source = source.replace("text='#F4F2EC', muted='#AEB8BF', orange='#FFA640', green='#8CE0B0',", "text='#F5F5F0', muted='#B0BBC2', orange='#FF6B38', green='#B9F278',")
source = source.replace("red='#FF7272', redbg='#3B2025', dark='#13171A'", "red='#FF7770', redbg='#44282B', dark='#11191C'")
source = source.replace("FONT = 'Bahnschrift, Segoe UI, Arial, sans-serif'", "FONT = 'Segoe UI, Arial, sans-serif'")
source = source.replace('r=10):', 'r=16):')
start = source.index('def button(')
end = source.index('def footer(', start)
source = source[:start] + '''def button(x, y, w, value, kind='normal', h=56):
    styles = {'normal': (C['raised'], C['text'], None),
              'primary': (C['dark'], C['text'], C['orange']),
              'stop': (C['redbg'], C['red'], C['red']),
              'disabled': (C['panel'], '#A1ADB3', C['line']),
              'selected': (C['orange'], C['dark'], None)}
    fill, fg, border = styles[kind]
    rect(x, y, w, h, fill, border, 12)
    text(x+w/2, y+h/2+6, value, 17, fg, 700, 'middle')


def pill(x, y, value, state='ready', width=130):
    color = C['red'] if state == 'fault' else C['orange'] if state == 'active' else C['green']
    circle(x+6, y+15, 4, color)
    text(x+18, y+20, value, 12, color, 700)


def base(title, section, status='READY', state='ready'):
    parts.clear()
    rect(0, 0, 800, 480, C['bg'], r=0)
    rect(0, 0, 800, 76, C['dark'], r=0)
    text(24, 22, 'TIG / ROTATOR', 12, C['muted'], 700)
    text(24, 59, title, 26, weight=700)
    pill(638, 23, status, state, 138)


''' + source[end:]
start=source.index('def field(');end=source.index('def row(',start)
source=source[:start]+'''def field(x, y, w, title, value, unit='', selected=False):
    rect(x, y, w, 82, C['orange'] if selected else C['panel'], r=16)
    fg = C['dark'] if selected else C['text']
    secondary = '#49291E' if selected else C['muted']
    text(x+16, y+24, title, 12, secondary, 700)
    text(x+16, y+62, value, 30, fg, 700)
    if unit:
        text(x+w-16, y+61, unit, 14, secondary, anchor='end')


'''+source[end:]
start=source.index('def main(');end=source.index('\n\nmain()',start)
source=source[:start]+'''def main(running=False, fault=False):
    base('Continuous rotation', 'Operation', 'INPUT FAULT' if fault else 'ROTATING' if running else 'SYSTEM READY', 'fault' if fault else 'active' if running else 'ready')
    rect(24,96,460,278,C['orange'],r=18)
    text(46,126,'01 / CALCULATED SPEED' if running else '01 / TARGET SPEED',13,'#49291E',700)
    text(42,250,'0.00' if fault else '0.80',112,C['dark'],800)
    text(386,240,'RPM',23,'#49291E',700)
    line(46,273,462,273,'#AF4727')
    for i in range(43):
        x=47+i*9.7
        line(x,288,x,304 if i%5==0 else 297,'#66301E' if i<12 else '#C64E2B',2)
    px=47+(0 if fault else 11.2)*9.7
    parts.append(f'<path d="M{px-7} 312L{px} 304L{px+7} 312Z" fill="{C["dark"]}"/>')
    text(46,349,'0.001',12,'#49291E',700)
    text(452,349,'MAX 3.000 RPM',12,'#49291E',700,'end')
    rect(504,96,272,74,C['raised'],r=16)
    rect(510,102,128,62,C['dark'],C['orange'],12)
    text(574,142,'CW ↻',24,C['text'],700,'middle')
    text(707,142,'CCW',22,C['muted'],700,'middle')
    text(506,205,'SURFACE SPEED / CALC.',11,C['muted'],700)
    text(504,253,'0' if fault else '754',39,C['text'],700)
    text(595,252,'mm/min',15,C['muted'])
    text(774,252,'Ø 300',14,C['muted'],anchor='end')
    line(504,266,776,266)
    text(506,294,'SOURCE',11,C['muted'],700)
    text(774,294,'Pedal fault' if fault else 'Panel dial',16,C['red'] if fault else C['text'],700,'end')
    text(506,332,'ELAPSED' if running else 'PROGRAM',11,C['muted'],700)
    text(774,332,'00:42' if running else 'Manual',16,C['text'],700,'end')
    line(504,350,776,350)
    if fault: text(506,375,'Release pedal; check input',13,C['red'])
    button(24,408,72,'MENU')
    button(108,408,94,'−','disabled' if running or fault else 'normal')
    button(214,408,94,'+','disabled' if running or fault else 'normal')
    rect(328,408,448,56,C['redbg'] if running else C['dark'],C['red'] if running else C['line'],12)
    text(350,443,'STOP ROTATION' if running else 'START BLOCKED' if fault else 'START ROTATION',21,C['red'] if running else C['muted'] if fault else C['text'],700)
    circle(744,436,17,C['red'] if running else C['raised'] if fault else C['orange'])
    if running: rect(738,430,12,12,C['dark'],r=1)
    elif not fault: parts.append(f'<path d="M740 428L750 436L740 444Z" fill="{C["dark"]}"/>')

'''+source[end:]
source=source.replace("'#393024'", "'#3B2821'")
source=source.replace('Dark · Amber','Dark · Orange')
source=source.replace('V3 DESIGN PROPOSAL','V5 DARK / ORANGE PROPOSAL')
source=source.replace('UI proposal V3','UI proposal V5').replace('complete UI proposal V3','complete UI proposal V5')
source=source.replace('DESIGN PROPOSAL 03','REFERENCE-LED DESIGN / 05')
source=source.replace('AMBER = ACTION / ACTIVE','ORANGE = PRIMARY VALUE / SELECTION')
source=source.replace('An operator-first interface.','Bold values. Clear controls.')
source=source.replace('22 SCREENS + 8 STATES','22 SCREEN TYPES / 30 VIEWS')
source=source.replace('22 registered screens + 8 states','22 registered screen types covered · 30 views including overlays and keyboards')
source=source.replace('#ffa640','#ff6b38')
# Correct known design-copy ambiguities while keeping the inventory intact.
source=source.replace('1 min 00 s','60 s + ramps').replace('TOTAL PROGRAM TIME','NOMINAL CYCLE TIME')
source=source.replace('3.0 s per cycle','3.0 s + braking').replace('67% on-time','67% nominal on-time')
source=source.replace('5 / 7 checks complete','Initializing controller')
source=source.replace('until startup checks complete.','until initialization is complete.')
source=source.replace('New safety interlocks, readiness checks and source handling require implementation and validation.','Illustrative data. New layouts and optional workflow changes require implementation and validation.')
source=source.replace("text(24, 130, 'Ready for'", "text(24, 124, 'Ready for'")
source=source.replace("text(24,110,'STARTING IN'", "text(24,94,'STARTING IN'")
source=source.replace("text(426,264,'42°C'", "text(426,272,'42°C'")
source=source.replace("text(x+w-52, y+h/2+6, value", "text(x+w-(78 if action=='EDIT' else 52), y+h/2+6, value")
# Readability floor for the actual 4.3-inch screen, rather than the gallery zoom.
source=source.replace("def text(x, y, value, size=18, color=None, weight=400, anchor='start'):\n", "def text(x, y, value, size=18, color=None, weight=400, anchor='start'):\n    size = max(size, 14)\n")
source=source.replace("'stop': (C['redbg'], C['red'], C['red'])", "'stop': ('#B52C35', '#FFFFFF', '#F88589')")
source=source.replace("C['redbg'] if running else C['dark']", "'#B52C35' if running else C['dark']")
source=source.replace("C['red'] if running else C['muted']", "'#FFFFFF' if running else C['muted']")
start=source.index('def estop(');end=source.index('\n\nestop()',start)
source=source[:start]+'''def estop(reset=False):
    parts.clear()
    rect(0,0,800,480,'#201619',r=0)
    rect(0,0,800,106,'#B52C35',r=0)
    rect(24,23,62,62,'#FFFFFF',r=12)
    text(55,69,'!',40,'#B52C35',800,'middle')
    text(108,39,'TIG / ROTATOR — MOTION LOCKED',14,'#FFFFFF',700)
    text(108,82,'EMERGENCY STOP',36,'#FFFFFF',800)
    text(24,147,'MOTOR OUTPUT DISABLED',26,'#FFFFFF',700)
    text(24,177,'Reset returns to idle. It does not start the motor.',18,'#E3CBCD')
    rect(24,200,752,62,'#342328',r=12)
    text(42,239,'Physical E-STOP',20,'#FFFFFF',600)
    text(750,239,'RELEASED' if reset else 'ACTIVE',22,'#B9F278' if reset else '#FFACAF',700,'end')
    rect(24,274,752,62,'#342328',r=12)
    text(42,313,'Driver alarm',20,'#FFFFFF',600)
    text(750,313,'CLEAR',22,'#B9F278',700,'end')
    text(24,373,'Check the machine, then reset the lock.' if reset else 'Release the physical E-STOP and check the machine.',18,'#F4DCDD',600)
    button(24,408,248,'DIAGNOSTICS')
    if reset:
        rect(288,408,488,56,'#F5F5F0',r=12)
        text(532,443,'RESET TO IDLE',21,'#201619',700,'middle')
    else:
        rect(288,408,488,56,'#49343A',r=12)
        text(532,443,'RESET BLOCKED — INPUT ACTIVE',18,'#F1D5D9',700,'middle')

'''+source[end:]
source=source.replace("text(x+w-22, y+h/2+6, action", "text(x+w-(36 if action=='EDIT' else 22), y+h/2+6, action")
exec(compile(source, str(Path(__file__)), 'exec'))
