CFLAGS = -mcmodel=large -fno-builtin -fno-stack-protector -Wno-packed-bitfield-compat -Wno-address-of-packed-member -mno-red-zone -m64 -fmerge-constants
LDFLAGS = -b elf64-x86-64 -b elf64-x86-64 -z muldefs --no-warn-rwx-segments -z noexecstack

ARCH = $(shell uname -m)

ifeq ($(ARCH),aarch64)
CC = /usr/bin/x86_64-linux-gnu-gcc
ASM = /usr/bin/x86_64-linux-gnu-as
LD = /usr/bin/x86_64-linux-gnu-ld
NM = /usr/bin/x86_64-linux-gnu-nm
OBJCOPY = /usr/bin/x86_64-linux-gnu-objcopy
else 
CC = gcc
ASM = as
LD = ld
NM = nm
OBJCOPY = objcopy
endif

ASMFLAG = --64

.DEFAULT_GOAL = all

include ./.depend

all: system_tmp ../Tools/kallsyms.o
	@$(LD) $(LDFLAGS) $(LDFLAGS_ARCH) -o system $(ALLOBJS) ../Tools/kallsyms.o -T Kernel.lds 
	@echo "[LD] system"
	@$(OBJCOPY) -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin
	@echo "[OBJDUMP] kernel.bin"

system_tmp: $(ALLOBJS)
	@$(LD) $(LDFLAGS) $(LDFLAGS_ARCH) -o system_tmp $(ALLOBJS) -T Kernel.lds
	@echo "[LD] system_tmp"

../Tools/kallsyms.o: ../Tools/kallsyms.c system_tmp
	@$(CC) $(CC_HEADER) -o ../Tools/kallsyms ../Tools/kallsyms.c
	@$(NM) -n system_tmp | ../Tools/kallsyms > ../Tools/kallsyms.S
	@$(CC) -c ../Tools/kallsyms.S -o ../Tools/kallsyms.o -Wl,--no-warn-rwx-segments
	@echo "[Multi] kallsyms.o"

clean:
	@python3 clear.py
	rm -R system system_tmp kernel.bin .depend ../Tools/kallsyms.S ../Tools/kallsyms ../Tools/kallsyms.o
	@echo "[SCRIPT] clear.py"