"""Generate editable SVG proposals, a contact sheet and a local review gallery.

Standard library only. No firmware changes. Each screen uses native 800x480
coordinates; overview reuses exactly the same artwork. Run from any directory.
"""
from pathlib import Path
from html import escape
import json
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/images/ui_mockup_v3'
OUT.mkdir(parents=True, exist_ok=True)
C = dict(bg='#101316', panel='#1B2025', raised='#252C32', line='#39434B',
         text='#F4F2EC', muted='#AEB8BF', orange='#FFA640', green='#8CE0B0',
         red='#FF7272', redbg='#3B2025', dark='#13171A')
FONT = 'Bahnschrift, Segoe UI, Arial, sans-serif'
screens = []
parts = []


def rect(x, y, w, h, fill=None, stroke=None, r=10):
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" '
                 f'fill="{fill or C["panel"]}"' + (f' stroke="{stroke}"' if stroke else '') + '/>')


def text(x, y, value, size=18, color=None, weight=400, anchor='start'):
    parts.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color or C["text"]}" '
                 f'font-weight="{weight}" text-anchor="{anchor}">{escape(str(value))}</text>')


def line(x1, y1, x2, y2, color=None, width=1):
    parts.append(f'<path d="M{x1},{y1} L{x2},{y2}" fill="none" stroke="{color or C["line"]}" stroke-width="{width}"/>')


def circle(x, y, radius, color, stroke=None, width=1):
    parts.append(f'<circle cx="{x}" cy="{y}" r="{radius}" fill="{color}"' +
                 (f' stroke="{stroke}" stroke-width="{width}"' if stroke else '') + '/>')


def label(x, y, value):
    text(x, y, value, 14, C['muted'], 600)


def button(x, y, w, value, kind='normal', h=56):
    styles = {'normal': (C['raised'], C['text'], C['line']),
              'primary': (C['orange'], C['dark'], C['orange']),
              'stop': (C['redbg'], C['red'], C['red']),
              'disabled': (C['panel'], '#7F8A92', C['line']),
              'selected': ('#393024', C['orange'], C['orange'])}
    fill, fg, border = styles[kind]
    rect(x, y, w, h, fill, border, 8)
    text(x+w/2, y+h/2+7, value, 19, fg, 600, 'middle')


def pill(x, y, value, state='ready', width=130):
    color = C['red'] if state == 'fault' else C['orange'] if state == 'active' else C['green']
    rect(x, y, width, 30, C['panel'], C['line'], 15)
    circle(x+15, y+15, 4, color)
    text(x+28, y+21, value, 14, color, 600)


def base(title, section, status='READY', state='ready'):
    parts.clear()
    rect(0, 0, 800, 480, C['bg'], r=0)
    rect(0, 0, 5, 66, C['orange'], r=0)
    text(24, 27, section.upper(), 12, C['muted'], 600)
    text(24, 54, title, 25, weight=600)
    pill(638, 21, status, state, 138)
    line(24, 70, 776, 70)


def footer(left='BACK', right='SAVE', kind='primary', mid=None):
    line(24, 392, 776, 392)
    button(24, 408, 152, '<  '+left)
    if mid == 'STOP':
        button(192, 408, 272, right, kind)
        button(480, 408, 296, 'STOP', 'stop')
    elif mid:
        button(192, 408, 272, mid)
        button(480, 408, 296, right, kind)
    elif right:
        button(496, 408, 280, right, kind)


def field(x, y, w, title, value, unit='', selected=False):
    rect(x, y, w, 82, C['panel'], C['orange'] if selected else C['line'])
    label(x+16, y+24, title)
    text(x+16, y+62, value, 30, C['orange'] if selected else C['text'], 600)
    if unit:
        text(x+w-16, y+61, unit, 16, C['muted'], anchor='end')


def row(y, title, value, action='>', x=24, w=752, h=56):
    rect(x, y, w, h, C['panel'], C['line'], 8)
    text(x+18, y+h/2+6, title, 18, weight=500)
    text(x+w-52, y+h/2+6, value, 18, C['muted'], anchor='end')
    text(x+w-22, y+h/2+6, action, 20, C['orange'], anchor='middle')


def bar(x, y, w, progress, color=None):
    rect(x, y, w, 8, C['raised'], r=4)
    rect(x, y, max(8, w*progress), 8, color or C['orange'], r=4)


def finish(slug, title, screen_id, note, group):
    body = '<g font-family="'+FONT+'">'+''.join(parts)+'</g>'
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="800" height="480" viewBox="0 0 800 480" '
           f'role="img" aria-labelledby="title desc"><title id="title">{escape(title)}</title>'
           f'<desc id="desc">{escape(note)} UI design proposal, not live machine data.</desc>{body}</svg>')
    ET.fromstring(svg)
    (OUT / f'{slug}.svg').write_text(svg, encoding='utf-8')
    screens.append(dict(slug=slug, title=title, screen_id=screen_id, note=note, group=group, body=body))


# 01–06: operation
base('TIG / ROTATOR', 'Power-on check', 'CHECKING', 'active')
text(24, 130, 'Ready for', 38, weight=600)
text(24, 176, 'the next weld.', 38, weight=600)
text(24, 219, 'Motor output stays disabled', 19, C['muted'])
text(24, 246, 'until startup checks complete.', 19, C['muted'])
label(24, 318, 'CONTROLLER v2.0.9')
bar(24, 344, 330, .71)
text(24, 376, '5 / 7 checks complete', 16, C['muted'])
for i,(a,b) in enumerate([('Display & touch','OK'),('Saved settings','OK'),('Motor interface','OK'),('Safety inputs','CHECK'),('Pedal released','WAIT')]):
    y=96+i*56
    text(410,y+25,a,18)
    text(754,y+25,b,15,C['orange'] if b!='OK' else C['green'],600,'end')
    line(410,y+39,776,y+39)
label(24, 449, 'MOTOR DISABLED')
text(776,449,'Do not press the pedal during startup',16,C['muted'],anchor='end')
finish('01_boot','Startup checks','SCREEN_BOOT','Proposed real readiness checks, not a timed progress animation.','Operate')


def main(running=False, fault=False):
    base('Continuous rotation','Operation / continuous', 'INPUT FAULT' if fault else 'ROTATING' if running else 'READY', 'fault' if fault else 'active' if running else 'ready')
    label(24,107,'CALCULATED SPEED' if running else 'TARGET SPEED')
    text(20,218,'0.80' if not fault else '0.00',104,weight=600)
    text(291,215,'RPM',25,C['muted'])
    bar(24,243,368,.8/3)
    text(24,280,'0.001',15,C['muted'])
    text(392,280,'3.000 RPM',15,C['muted'],anchor='end')
    rect(24,301,368,73)
    label(40,325,'SURFACE SPEED / Ø 300 mm')
    text(40,359,'0' if fault else '754',30,weight=600)
    text(118,359,'mm/min',18,C['muted'])
    rect(424,94,352,280)
    label(444,121,'CONTROL SOURCE')
    text(444,153,'Panel dial' if not fault else 'Pedal unavailable',24,C['red'] if fault else C['orange'],600)
    line(444,174,756,174)
    label(444,204,'DIRECTION')
    text(756,205,'CW  ↻',24,anchor='end')
    label(444,248,'PROGRAM')
    text(756,249,'Manual',20,anchor='end')
    label(444,292,'NEXT STEP' if fault else 'ELAPSED' if running else 'START METHOD')
    text(756,293,'Check pedal' if fault else '00:42' if running else 'Touch START',20,anchor='end')
    text(444,349,'Release pedal to re-arm' if fault else 'Stop before changing setup',16,C['muted'])
    footer('MENU','STOP' if running else 'START BLOCKED' if fault else 'START ROTATION','stop' if running else 'disabled' if fault else 'primary', 'PEDAL FAULT' if fault else '0.80 RPM SET' if running else 'PEDAL OFF')


main()
finish('02_main_ready','Main / ready','SCREEN_MAIN','Large target RPM, explicit source and surface speed; no suggestion of measured feedback.','Operate')
main(True)
finish('03_main_running','Main / rotating','SCREEN_MAIN','Running variant: calculated speed and elapsed time, fixed STOP position.','Operate')

base('Menu','Navigation')
for i,(title,sub,tag) in enumerate([('Run modes','Pulse · Step · Jog · Countdown','01'),('Programs','Save and repeat your setup','02'),('Settings','Motor · Pedal · Display','03'),('Diagnostics','Inputs, faults and event log','04')]):
    x=24+(i%2)*384; y=94+(i//2)*140
    rect(x,y,368,122,C['panel'],C['line'])
    text(x+18,y+31,tag,14,C['orange'],600)
    text(x+18,y+65,title,25,weight=600)
    text(x+18,y+97,sub,17,C['muted'])
    text(x+340,y+36,'↗',24,C['orange'])
footer('HOME',None)
finish('04_menu','Navigation menu','SCREEN_MENU','Four clear destinations with fixed return to the main operating screen.','Operate')

base('Choose a run mode','Operation / modes')
for i,(title,sub,symbol) in enumerate([('Pulse','Rotate, pause, repeat','▰'),('Step','Move to a set angle','∠'),('Jog','Hold to move','↔'),('Countdown','Delay a continuous start','3')]):
    x=24+(i%2)*384; y=94+(i//2)*140
    rect(x,y,368,122,C['panel'],C['line'])
    text(x+18,y+43,symbol,34,C['orange'],600)
    text(x+78,y+43,title,26,weight=600)
    text(x+18,y+93,sub,19,C['muted'])
footer('MENU',None)
finish('05_run_modes','Run mode selection','SCREEN_RUN_MODES','Countdown is labelled as a delayed start rather than a timed-stop mode.','Operate')

base('Jog','Operation / manual motion')
field(24,94,304,'JOG SPEED','0.20','RPM')
button(344,94,88,'−',h=82); button(448,94,88,'+',h=82)
text(568,128,'RELEASE',16,C['orange'],600)
text(568,155,'TO STOP',21,weight=600)
button(24,196,368,'↶  HOLD CCW',h=126)
button(408,196,368,'HOLD CW  ↷','selected',h=126)
text(24,363,'Motion lasts only while you hold a direction button.',19,C['muted'])
footer('MODES','STOP','stop')
finish('06_jog','Jog / hold to move','SCREEN_JOG','Separated direction pads, slow speed control, prominent release-to-stop instruction.','Operate')

# 07–12: process and safety
base('Pulse rotation','Operation / pulse')
field(24,94,232,'ROTATE FOR','2.0','s',True)
field(272,94,232,'PAUSE FOR','1.0','s')
field(520,94,256,'TARGET SPEED','0.80','RPM')
label(24,211,'CYCLE PREVIEW')
for i in range(4):
    x=24+i*188
    rect(x,236,120,53,'#393024',C['orange'],4)
    text(x+60,268,'ROTATE',15,C['orange'],600,'middle')
    rect(x+124,263,60,26,C['raised'],r=4)
text(24,336,'3.0 s per cycle',21,weight=600)
text(300,336,'67% on-time',19,C['muted'])
text(776,336,'Continuous repeat',19,C['muted'],anchor='end')
footer('MODES','START PULSE',mid='STOP')
finish('07_pulse','Pulse setup','SCREEN_PULSE','Readable timing blocks with an explanatory cycle preview.','Process & safety')

base('Step by angle','Operation / step')
field(24,94,368,'TARGET ON WORKPIECE','90.0','degrees',True)
field(408,94,176,'SPEED','0.80','RPM')
field(600,94,176,'PART Ø','300','mm')
for i,a in enumerate(['45°','90°','180°','360°']):
    button(24+i*192,194,176,a,'selected' if i==1 else 'normal')
rect(24,270,752,104)
label(42,297,'DIRECTION'); text(42,342,'CW  ↻',27,weight=600)
label(242,297,'EST. MOVE'); text(242,342,'18.8 s',27,weight=600)
label(474,297,'TRAVEL'); text(474,342,'0.0° / 90.0°',27,weight=600)
footer('MODES','MOVE 90°',mid='STOP')
finish('08_step','Step setup','SCREEN_STEP','Angle presets and diameter remain visible; duration excludes acceleration.','Process & safety')

base('Countdown start','Operation / delayed start','ARMED','active')
text(24,110,'STARTING IN',15,C['muted'],600)
text(18,260,'03',150,C['orange'],600)
text(230,255,'seconds',27,C['muted'])
text(24,324,'Motor remains disabled until zero.',20)
text(24,357,'STOP cancels the pending start.',18,C['muted'])
rect(424,94,352,280)
label(444,125,'NEXT ACTION'); text(444,160,'Continuous rotation',25,weight=600)
line(444,183,756,183)
label(444,221,'TARGET'); text(756,221,'0.80 RPM',22,anchor='end')
label(444,272,'DIRECTION'); text(756,272,'CW',22,anchor='end')
text(444,345,'No automatic stop is set.',18,C['orange'])
footer('MODES','CANCEL START','stop')
finish('09_timer','Countdown / armed','SCREEN_TIMER','Explicitly states that countdown starts continuous motion, not a timed stop.','Process & safety')


def estop(reset=False):
    base('Emergency stop','Safety / motion locked','LOCKED','fault')
    rect(24,94,752,92,C['redbg'],C['red'])
    rect(42,116,48,48,C['red'],r=8)
    text(66,151,'!',32,C['dark'],700,'middle')
    text(110,132,'MOTOR OUTPUT DISABLED',26,C['red'],600)
    text(110,165,'Reset returns to idle. A new start is required.',18)
    row(206,'Emergency-stop input','Released' if reset else 'ACTIVE','✓' if reset else '!',h=56)
    row(274,'Driver alarm','Clear','✓',h=56)
    text(24,366,'Inputs are clear. You can reset the lock.' if reset else 'Release the physical E-STOP, then check the machine.',18,C['muted'])
    button(24,408,248,'DIAGNOSTICS')
    button(288,408,488,'RESET TO IDLE' if reset else 'RESET BLOCKED','primary' if reset else 'disabled')


estop()
finish('10_estop_active','Emergency stop / active','ESTOP_OVERLAY','Blocking fault view; no start action and reset unavailable while input is active.','Process & safety')
estop(True)
finish('11_estop_clear','Emergency stop / reset allowed','ESTOP_OVERLAY','Clear inputs permit reset to idle only, never automatic restart.','Process & safety')

base('Confirm action','Programs / delete')
rect(80,105,640,270,C['panel'],C['line'],12)
label(112,143,'DELETE PROGRAM 03')
text(112,190,'Delete “Tube Ø300”?',34,weight=600)
text(112,237,'The saved setup will be permanently removed.',20,C['muted'])
text(112,272,'This does not change motor settings.',19,C['muted'])
text(112,336,'You cannot undo this action.',18,C['red'])
button(24,408,368,'KEEP PROGRAM'); button(408,408,368,'DELETE PROGRAM','stop')
finish('12_confirm','Confirmation dialog','SCREEN_CONFIRM','Specific object and consequence; safe cancel action and labelled destructive action.','Process & safety')

# 13–18: presets
base('Welding programs','Programs / 3 of 16 slots')
for i,(name,detail,sel) in enumerate([('Tube Ø300','CONTINUOUS  ·  0.80 RPM  ·  CW',True),('Tack sequence','STEP  ·  90° × 4  ·  2.0 s dwell',False),('Pulse finish','PULSE  ·  2.0 s on / 1.0 s off',False)]):
    y=94+i*94
    rect(24,y,752,80,C['panel'],C['orange'] if sel else C['line'])
    text(42,y+32,f'0{i+1}',19,C['orange'],600)
    text(96,y+32,name,23,weight=600)
    text(96,y+60,detail,16,C['muted'])
    button(652,y+12,108,'EDIT',h=56)
footer('MENU','REVIEW & RUN',mid='+ NEW PROGRAM')
finish('13_programs','Program library','SCREEN_PROGRAMS','Select then review before running; edit is separate from start.','Programs')

base('Tube Ø300','Programs / edit 03','UNSAVED','active')
row(94,'Program name','Tube Ø300','EDIT',h=56)
label(24,182,'AVAILABLE MODES — TAP TO ENABLE')
button(24,198,240,'CONTINUOUS','selected'); button(280,198,240,'PULSE'); button(536,198,240,'STEP')
row(272,'Run configuration','0.80 RPM · CW','>',h=56)
text(24,365,'Continuous is selected as the starting mode.',18,C['muted'])
footer('CANCEL','SAVE PROGRAM',mid='DELETE')
finish('14_program_edit','Program editor','SCREEN_PROGRAM_EDIT','Named program, explicit mode selection and a separate run-configuration editor.','Programs')

base('Continuous settings','Programs / Tube Ø300','EDITING','active')
field(24,94,368,'TARGET SPEED','0.80','RPM',True)
label(424,114,'DIRECTION')
button(424,130,168,'CW','selected');button(608,130,168,'CCW')
row(208,'Soft start','ON','✓',h=64)
row(288,'Automatic stop','OFF','>',h=64)
footer('CANCEL','SAVE SETTINGS')
finish('15_edit_cont','Edit continuous program','SCREEN_EDIT_CONT','Speed, direction, soft start and a proposed explicit automatic-stop setting.','Programs')

base('Pulse settings','Programs / Pulse finish','EDITING','active')
field(24,94,368,'ROTATE FOR','2.0','s',True)
field(408,94,368,'PAUSE FOR','1.0','s')
field(24,194,368,'TARGET SPEED','0.80','RPM')
field(408,194,368,'CYCLES','20','repeats')
rect(24,296,752,78)
label(42,322,'TOTAL PROGRAM TIME')
text(42,355,'1 min 00 s',26,weight=600)
text(752,352,'67% on-time · 3.0 s cycle',19,C['muted'],anchor='end')
footer('CANCEL','SAVE SETTINGS')
finish('16_edit_pulse','Edit pulse program','SCREEN_EDIT_PULSE','All four editable pulse parameters with a derived total-time summary.','Programs')

base('Step settings','Programs / Tack sequence','EDITING','active')
for x,y,w,t,v,u in [(24,94,240,'ANGLE','90','degrees'),(280,94,240,'SPEED','0.80','RPM'),(536,94,240,'PART Ø','300','mm'),(24,194,240,'REPEATS','4','moves'),(280,194,240,'DWELL','2.0','s'),(536,194,240,'DIRECTION','CW','↻')]:
    field(x,y,w,t,v,u)
rect(24,296,752,78)
label(42,323,'PROGRAM SUMMARY')
text(42,355,'360° total travel',25,weight=600)
text(752,352,'3 pauses between 4 moves',19,C['muted'],anchor='end')
footer('CANCEL','SAVE SETTINGS')
finish('17_edit_step','Edit step program','SCREEN_EDIT_STEP','Six parameters at consistent touch sizes; summary explains repeat semantics.','Programs')

base('Welding programs','Programs / 0 of 16 slots')
rect(24,94,752,280,C['panel'],C['line'])
circle(400,160,32,'#393024'); text(400,174,'+',42,C['orange'],400,'middle')
text(400,239,'Your next weld, saved.',32,weight=600,anchor='middle')
text(400,279,'Store speed, direction and cycle settings in one program.',19,C['muted'],anchor='middle')
text(400,323,'Up to 16 programs available.',17,C['muted'],anchor='middle')
footer('MENU','CREATE FIRST PROGRAM')
finish('18_programs_empty','Program library / empty','SCREEN_PROGRAMS','Purposeful empty state with one clear next action.','Programs')

# 19–24: configuration
base('Settings','Setup / motor idle')
for i,(a,b) in enumerate([('Motor','Drive & motion'),('Calibration','Angle accuracy'),('Pedal','Input & arming'),('Display','Screen & mirror'),('Diagnostics','Inputs & events'),('System info','Health & firmware')]):
    x=24+(i%2)*384; y=94+(i//2)*92
    rect(x,y,368,76,C['panel'],C['line'])
    text(x+18,y+30,a,22,weight=600)
    text(x+18,y+57,b,16,C['muted'])
    text(x+341,y+44,'>',24,C['orange'])
footer('MENU','ABOUT','normal')
finish('19_settings','Settings hub','SCREEN_SETTINGS','Six touch-sized destinations plus About; no tiny menu rows.','Setup')

base('Motor configuration','Setup / motion parameters')
field(24,94,240,'DRIVER','DM542T')
field(280,94,240,'MICROSTEP','1 / 16','3200 / rev')
field(536,94,240,'MAX SPEED','3.00','RPM')
field(24,194,368,'ACCELERATION','7500','steps/s²')
button(408,194,176,'INVERT OFF',h=82);button(600,194,176,'DIR SW ON','selected',h=82)
rect(24,296,752,78)
text(42,327,'Match microstepping to the driver DIP switches.',19,C['orange'],500)
text(42,355,'Changes apply only when motion is stopped.',18,C['muted'])
footer('CANCEL','SAVE & APPLY')
finish('20_motor_config','Motor configuration','SCREEN_MOTOR_CONFIG','Explicit physical DIP-switch match and stopped-motion apply semantics.','Setup')

base('Foot pedal','Setup / operator input','DISARMED','active')
row(94,'Pedal control','OFF','>',h=56)
row(162,'Start switch','RELEASED','✓',h=56)
row(230,'Analog speed input','CONNECTED','✓',h=56)
text(24,329,'Release the pedal before enabling control.',21,weight=500)
text(24,360,'Input failure stops pedal-controlled motion.',18,C['muted'])
footer('SETTINGS','ENABLE PEDAL')
finish('21_pedal','Pedal settings','SCREEN_PEDAL_SETTINGS','Proposed release-to-arm and input-failure interlock; requires firmware changes.','Setup')

base('Display settings','Setup / screen & USB')
label(24,113,'BRIGHTNESS');text(776,113,'80%',20,anchor='end')
bar(24,142,752,.8);circle(24+752*.8,146,12,C['orange'])
row(177,'Dim after','1 minute','>',h=56)
row(245,'Appearance','Dark · Amber','>',h=56)
row(313,'USB mirror','VIEW ONLY','>',h=56)
footer('CANCEL','SAVE DISPLAY')
finish('22_display','Display settings','SCREEN_DISPLAY','Quiet appearance controls; USB viewing distinguished from remote control.','Setup')


def calibration(verify=False):
    base('Calibrate rotation','Setup / calibration','VERIFIED' if verify else 'READY')
    for i,(a,b) in enumerate([('1','Prepare'),('2','Measure'),('3','Verify')]):
        x=24+i*256
        rect(x,94,240,44,'#393024' if (i==2 if verify else i==0) else C['panel'],r=6)
        text(x+15,123,a,18,C['orange'],600)
        text(x+44,123,b,18)
    if verify:
        field(24,158,368,'VERIFICATION RESULT','359.8','degrees')
        field(408,158,368,'ANGLE ERROR','−0.2','degrees')
        rect(24,260,752,114)
        text(42,297,'PASS · within ±1.0° tolerance',25,C['green'],600)
        text(42,333,'New factor 1.0056 is ready to save.',19)
        text(42,360,'Calculated from the measured workpiece angle.',16,C['muted'])
        footer('BACK','SAVE CALIBRATION',mid='VERIFY AGAIN')
    else:
        text(24,177,'Mark the workpiece at its starting position.',22,weight=500)
        text(24,211,'Run one full turn, then enter the measured angle.',18,C['muted'])
        field(24,238,232,'PART Ø','300','mm')
        field(272,238,240,'TEST SPEED','0.20','RPM')
        field(528,238,248,'CURRENT FACTOR','1.0000')
        text(24,363,'Saving stays locked until a verification move passes.',18,C['orange'])
        footer('SETTINGS','MOVE 360°',mid='STOP')


calibration()
finish('23_calibration','Calibration / prepare','SCREEN_CALIBRATION','Guided workpiece calibration with diameter and speed visible before movement.','Setup')
calibration(True)
finish('24_calibration_verify','Calibration / verification','SCREEN_CALIBRATION','Measured angle, explicit tolerance and save gate; values are illustrative.','Setup')

# 25–30: service and auxiliary states
base('Diagnostics','Service / live inputs')
label(24,110,'SIGNAL');label(268,110,'STATE');label(408,110,'RECENT EVENTS')
for i,(a,b) in enumerate([('E-STOP','CLEAR'),('Driver alarm','CLEAR'),('Pedal switch','OPEN'),('Direction','CW'),('Motor output','DISABLED')]):
    y=132+i*46
    text(24,y+22,a,18)
    text(352,y+22,b,16,C['green'] if i<2 else C['muted'],600,'end')
    line(24,y+36,368,y+36)
rect(392,128,384,246)
for i,(a,b) in enumerate([('14:32:08','Stop requested'),('14:32:09','Motion stopped'),('14:32:09','Motor disabled'),('14:33:12','Pedal control off')]):
    text(412,159+i*51,a,14,C['orange'])
    text(412,181+i*51,b,18)
footer('SETTINGS',None)
finish('25_diagnostics','Input diagnostics','SCREEN_DIAGNOSTICS','Human-readable signal states beside a timestamped event history.','Service & input')

base('System information','Service / controller health')
field(24,94,368,'FIRMWARE','v2.0.9')
field(408,94,368,'UPTIME','02:14:08')
rect(24,196,368,178)
label(42,224,'MEMORY AVAILABLE')
text(42,259,'Heap',19);text(370,259,'186 KB',21,anchor='end')
bar(42,276,328,.57,C['green'])
text(42,318,'PSRAM',19);text(370,318,'24.8 MB',21,anchor='end')
bar(42,337,328,.77,C['green'])
rect(408,196,368,178)
label(426,224,'PROCESSOR')
text(426,264,'42°C',36,weight=600)
label(426,307,'CORE 0 / CORE 1 LOAD')
text(426,345,'12% / 28%',26,weight=600)
footer('SETTINGS','RESTART DEVICE','normal')
finish('26_sysinfo','System health','SCREEN_SYSINFO','Illustrative runtime metrics; restart opens the confirmation screen.','Service & input')

base('About this controller','Service / project')
text(24,130,'TIG / ROTATOR',42,weight=600)
text(24,166,'DIY welding positioner',23,C['muted'])
rect(24,198,752,176)
for i,(a,b) in enumerate([('FIRMWARE','v2.0.9'),('CONTROLLER','ESP32-P4 · 4.3-inch touch'),('MOTION','NEMA 23 · 108:1 reduction'),('SOFTWARE','LVGL 9 · FastAccelStepper')]):
    label(42,229+i*40,a)
    text(752,229+i*40,b,19,anchor='end')
footer('SETTINGS',None)
finish('27_about','About','SCREEN_ABOUT','Compact identity and useful hardware information, without competing with normal operation.','Service & input')

base('Set target speed','Input / numeric keypad','EDITING','active')
field(24,94,272,'TARGET SPEED','0.80','RPM',True)
text(24,214,'Allowed range',18,C['muted'])
text(24,247,'0.001–3.000 RPM',22,weight=600)
text(24,306,'Changes apply after',18,C['muted'])
text(24,334,'you tap APPLY.',18,C['muted'])
for i,key in enumerate(['7','8','9','4','5','6','1','2','3','.','0','⌫']):
    x=328+(i%3)*152; y=94+(i//3)*72
    button(x,y,144,key,h=64)
footer('CANCEL','APPLY VALUE')
finish('28_numeric_keypad','Numeric input','AUX_NUMERIC_KEYPAD','Full-size keypad, units, valid range and an explicit apply action.','Service & input')

base('Name your program','Input / program name','EDITING','active')
rect(24,87,752,48,C['panel'],C['orange'],6)
text(40,119,'Tube Ø300',23,weight=500)
text(754,118,'9 / 31',15,C['muted'],anchor='end')
for j,letters in enumerate(['QWERTYUIOP','ASDFGHJKL','ZXCVBNM']):
    offset=[24,62,138][j]
    for i,key in enumerate(letters):
        button(offset+i*76,148+j*64,68,key,h=56)
button(24,340,120,'123',h=48);button(160,340,408,'SPACE',h=48)
button(584,340,192,'⌫  DELETE',h=48)
footer('CANCEL','APPLY NAME')
finish('29_text_keyboard','Text input','AUX_TEXT_KEYBOARD','Dedicated landscape keyboard with legible keycaps and a visible length limit.','Service & input')

main(fault=True)
finish('30_input_fault','Main / pedal fault','SCREEN_MAIN','Proposed non-restarting input fault state with explicit re-arm instruction.','Service & input')

# Overview: full vector screens, grouped by workflow; no rasterized UI.
poster = []
parts = poster
W,H=2608,6064
rect(0,0,W,H,'#090C0E',r=0)
text(48,58,'TIG / ROTATOR',34,weight=600)
text(48,119,'An operator-first interface.',52,weight=600)
text(48,161,'V3 DESIGN PROPOSAL   /   22 SCREENS + 8 STATES   /   800 × 480 NATIVE',19,C['muted'])
rect(2028,44,532,108,C['panel'],C['line'])
text(2050,77,'AMBER = ACTION / ACTIVE',17,C['orange'],600)
text(2050,106,'RED = STOP / FAULT',17,C['red'],600)
text(2050,135,'Values are illustrative · Firmware unchanged',16,C['muted'])
for gi,group in enumerate(dict.fromkeys(s['group'] for s in screens)):
    top=211+gi*1150
    text(48,top+27,f'0{gi+1}  /  {group.upper()}',25,weight=600)
    line(48,top+49,W-48,top+49)
    for j,s in enumerate([s for s in screens if s['group']==group]):
        x=48+(j%3)*856; y=top+69+(j//3)*534
        text(x,y+18,s['slug'][:2]+'  '+s['title'],18,C['muted'])
        poster.append(f'<svg x="{x}" y="{y+31}" width="800" height="480" viewBox="0 0 800 480">{s["body"]}</svg>')
text(48,H-36,'PROPOSAL ONLY  /  New safety interlocks, readiness checks and source handling require implementation and validation.',18,C['muted'])
overview=(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
          f'font-family="{FONT}"><title>TIG Rotator — complete UI proposal V3</title>'+''.join(poster)+'</svg>')
ET.fromstring(overview)
(OUT/'all_screens.svg').write_text(overview,encoding='utf-8')

metadata=[{k:v for k,v in s.items() if k!='body'} for s in screens]
(OUT/'manifest.json').write_text(json.dumps(metadata,indent=2,ensure_ascii=False),encoding='utf-8')
registered=set(re.findall(r'^\s+(SCREEN_[A-Z_]+)',(ROOT/'src/ui/screens.h').read_text(encoding='utf-8'),re.M))-{'SCREEN_NONE','SCREEN_COUNT'}
covered={s['screen_id'] for s in screens}
assert not registered-covered, f'Missing screens: {registered-covered}'
assert len(registered)==22 and len(screens)==30

gallery='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TIG Rotator · UI proposal V3</title><style>
*{box-sizing:border-box}body{margin:0;background:#101316;color:#f4f2ec;font:16px 'Segoe UI',sans-serif}
header{padding:32px 40px;border-bottom:1px solid #39434b}h1{font-size:32px;margin:8px 0}p{color:#aeb8bf;line-height:1.6}
a{color:#ffa640}main{display:grid;grid-template-columns:280px minmax(0,1fr);max-width:1500px;margin:auto}
nav{padding:20px;height:calc(100vh - 175px);overflow:auto;position:sticky;top:0}nav a{display:block;text-decoration:none;color:#aeb8bf;padding:10px;border-radius:6px;font-size:14px}nav a:hover,nav a:focus{background:#252c32;color:#ffa640}nav strong{display:block;padding:18px 10px 6px}
section{padding:28px 32px 60px;min-width:0}article{margin-bottom:44px;scroll-margin-top:20px}h2{font-size:23px;margin:0 0 12px}
img{display:block;width:100%;max-width:1000px;height:auto;border:1px solid #39434b;border-radius:10px}article p{max-width:900px}small{color:#aeb8bf}
@media(max-width:850px){main{display:block}nav{height:auto;position:static;display:flex;flex-wrap:wrap;gap:4px}nav strong{width:100%}section{padding:16px}header{padding:20px}}
</style><header><small>TIG / ROTATOR — DESIGN PROPOSAL 03</small><h1>Every screen. One control language.</h1><p>22 registered screens + 8 states · Native 800 × 480 · <a href="all_screens.svg">Open the complete SVG overview</a></p></header><main><nav>'''
last=None
for s in screens:
    if s['group']!=last:
        gallery+=f'<strong>{escape(s["group"])}</strong>';last=s['group']
    gallery+=f'<a href="#{s["slug"]}">{s["slug"][:2]} · {escape(s["title"])}</a>'
gallery+='</nav><section>'
for s in screens:
    gallery+=f'<article id="{s["slug"]}"><h2>{s["slug"][:2]} / {escape(s["title"])}</h2><a href="{s["slug"]}.svg"><img src="{s["slug"]}.svg" width="800" height="480" alt="{escape(s["title"])}" loading="lazy"></a><p>{escape(s["note"])}</p><small>{s["screen_id"]} · <a href="{s["slug"]}.svg" download>Download SVG</a></small></article>'
gallery+='</section></main></html>'
(OUT/'index.html').write_text(gallery,encoding='utf-8')
print(f'Generated {len(screens)} screens; all {len(registered)} registered ScreenIds covered. Output: {OUT}')
