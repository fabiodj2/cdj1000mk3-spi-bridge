#!/bin/bash
# Uso: scripts/set_deck.sh 1|2   (macOS) - ajusta nome USB e PID da placa Leonardo no Arduino IDE.
# Deck 1: "CDJ-1000 MK3 1" PID 0x8040 | Deck 2: "CDJ-1000 MK3 2" PID 0x8041. Uso pessoal.
set -e
case "$1" in 1) PID=0x8040;; 2) PID=0x8041;; *) echo "uso: $0 1|2"; exit 1;; esac
for f in "$HOME"/Library/Arduino15/packages/arduino/hardware/avr/*/boards.txt; do
  [ -f "$f.orig" ] || cp "$f" "$f.orig"
  sed -i '' "s/^leonardo.build.usb_product=.*/leonardo.build.usb_product=\"CDJ-1000 MK3 $1\"/" "$f"
  sed -i '' "s/^leonardo.build.pid=.*/leonardo.build.pid=$PID/" "$f"
  grep -n "leonardo.build.pid\|leonardo.build.usb_product" "$f"
done
rm -rf "$HOME/Library/Caches/arduino"
echo "Pronto: feche e reabra o Arduino IDE e grave a pasta deck$1/cdj_deck$1."
echo "Para restaurar o original: cp boards.txt.orig boards.txt (mesma pasta)."
