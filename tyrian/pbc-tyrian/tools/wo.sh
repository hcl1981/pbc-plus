#!/bin/sh
# wo.sh -- zeigt, wo eine Adresse aus der Absturzmeldung im Code liegt.
#
#   ./tools/wo.sh 100123AE
#
# Die Adresse steht auf dem Geraet in der Zeile "PC ........".
[ -z "$1" ] && { echo "Aufruf: $0 <adresse>   (z.B. 100123AE)"; exit 1; }
ELF="$(dirname "$0")/../build/tyrian.elf"
[ -f "$ELF" ] || { echo "fehlt: $ELF -- erst bauen"; exit 1; }
arm-none-eabi-addr2line -f -p -i -e "$ELF" "0x${1#0x}"
