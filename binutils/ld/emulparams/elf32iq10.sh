MACHINE=
SCRIPT_NAME=iq2000
TEMPLATE_NAME=generic
EXTRA_EM_FILE=genelf
OUTPUT_FORMAT="elf32-iq2000"
DATA_ADDR=0x1000
TEXT_START_ADDR=0x80000000
ARCH=iq2000
ENTRY=_start
EMBEDDED=yes
ELFSIZE=32
MAXPAGESIZE=256
OTHER_RELOCATING_SECTIONS='PROVIDE (__stack = 0x1800);'
