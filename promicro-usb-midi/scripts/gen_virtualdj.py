#!/usr/bin/env python3
"""Gera os arquivos do VirtualDJ (Devices + Mappers) para as placas Deck 1 / Deck 2.
Notas/CCs seguem docs/CDJ1000MK3_MIDI_Mapping.csv e midimap.h. Canais do VirtualDJ comecam em 0."""
import os
OUT = os.path.join(os.path.dirname(__file__), "..", "virtualdj")

# nome, nota, acao VDJscript, tipo ('button'|'toggle'), led(bool)
BTN = [
 ("PLAY",0x0B,"play_pause","button",True), ("CUE",0x0C,"cue_button","button",True),
 ("LOOP_IN",0x10,"loop_in","button",True), ("LOOP_OUT",0x11,"loop_out","button",True),
 ("RELOOP_EXIT",0x12,"reloop_exit","button",True),
 ("HOTCUE_A",0x1B,"hot_cue 1","button",True), ("HOTCUE_B",0x1C,"hot_cue 2","button",True),
 ("HOTCUE_C",0x1D,"hot_cue 3","button",True),
 ("TRACK_BACK",0x15,"browser_scroll -1","button",False), ("TRACK_FWD",0x16,"browser_scroll +1","button",False),
 ("SEARCH_BACK",0x17,"seek -2","button",False), ("SEARCH_FWD",0x18,"seek +2","button",False),
 ("FOLDER_BACK",0x19,"browser_window 'folders'","button",False), ("FOLDER_FWD",0x1A,"browser_window 'songs'","button",False),
 ("MASTERTEMPO",0x1E,"key_lock","button",True), ("TEMPO_RANGE",0x1F,"pitch_range +1","button",False),
 ("REVERSE",0x20,"reverse","toggle",True),
 ("JOG_TOUCH",0x36,"touchwheel_touch","button",False),
 ("JOG_MODE",0x3F,"vinyl_mode","button",True),
 ("TEMPO_RESET",0x40,"pitch_zero","button",True),
 ("REC",0x41,None,"button",False),
 ("CALL_FWD",0x42,"goto_cue +1","button",False), ("CALL_REV",0x43,"goto_cue -1","button",False),
 ("MEMORY",0x44,"set_cue","button",False), ("DELETE",0x45,"delete_cue","button",False),
 ("EJECT",0x46,"unload","button",False),
 ("TIME_MODE",0x47,None,"button",False), ("TEXT_MODE",0x48,None,"button",False),
]

def gen(deck):
    ch = deck - 1                       # canal MIDI VirtualDJ (0-based) = canal do firmware - 1
    name = f"CDJ1000MK3_D{deck}"
    desc = f"CDJ-1000 MK3 {deck}"
    pid = {1: "0x8040", 2: "0x8041"}[deck]
    d = ['<?xml version="1.0" encoding="UTF-8"?>',
         f'<device name="{name}" author="cdj1000mk3-spi-bridge" description="{desc}" version="800" type="MIDI" '
         f'vid="0x2341" pid="{pid}" drivername="{desc}" macdrivername="{desc}" decks="{deck}">']
    d.append('  <!-- Botoes: notas do docs/CDJ1000MK3_MIDI_Mapping.csv (+ extras 0x3F..0x48 do firmware) -->')
    for n, note, _, kind, led in BTN:
        d.append(f'  <{kind} note="0x{note:02X}" name="{n}" deck="{deck}" channel="{ch}" />')
    d.append('  <!-- Jog: CC relativo 64+delta, 900 passos por volta (3600 pulsos / 4) -->')
    d.append(f'  <jog cc="0x21" zero="0x40" full="900" name="JOG_SCRATCH" deck="{deck}" channel="{ch}" />')
    d.append(f'  <jog cc="0x22" zero="0x40" full="900" name="JOG_BEND" deck="{deck}" channel="{ch}" />')
    d.append('  <!-- Pitch 14 bits: MSB = CC 0x00, LSB = CC 0x20. Se o sentido ficar invertido, adicione inverted="true" -->')
    d.append(f'  <slider ccmsb="0x00" cc="0x20" name="PITCH" deck="{deck}" channel="{ch}" />')
    d.append('  <!-- Knobs de freio / release do painel (7 bits) -->')
    d.append(f'  <slider cc="0x08" name="BRAKE" deck="{deck}" channel="{ch}" />')
    d.append(f'  <slider cc="0x09" name="RELEASE" deck="{deck}" channel="{ch}" />')
    d.append('  <!-- LEDs do painel (o firmware acende ao receber a nota; hot cue: velocity 127 = verde) -->')
    for n, note, _, kind, led in BTN:
        if led:
            d.append(f'  <led note="0x{note:02X}" name="LED_{n}" default="{n}" deck="{deck}" channel="{ch}" />')
    d.append('</device>')
    open(os.path.join(OUT, "Devices", f"{name}.xml"), "w").write("\n".join(d) + "\n")

    m = ['<?xml version="1.0" encoding="UTF-8"?>',
         f'<mapper device="{name}" author="cdj1000mk3-spi-bridge" version="800">']
    for n, _, act, _, _ in BTN:
        if act:
            m.append(f'  <map value="{n}" action="{act}" />')
    m.append('  <map value="JOG_SCRATCH" action="touchwheel" />')
    m.append('  <map value="JOG_BEND" action="jogwheel" />')
    m.append('  <map value="PITCH" action="pitch" />')
    m.append('</mapper>')
    open(os.path.join(OUT, "Mappers", f"{name}.xml"), "w").write("\n".join(m) + "\n")

if __name__ == "__main__":
    os.makedirs(os.path.join(OUT, "Devices"), exist_ok=True)
    os.makedirs(os.path.join(OUT, "Mappers"), exist_ok=True)
    gen(1); gen(2)
