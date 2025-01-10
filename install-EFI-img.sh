#!/bin/bash
LOOP_ID=20
DISK_PATH=disk.img

# load the first 300M partition
sudo losetup /dev/loop${LOOP_ID} -o 1048576 --sizelimit 314572800 ${DISK_PATH}
./install-EFI.sh /dev/loop${LOOP_ID}
sudo losetup -d /dev/loop${LOOP_ID}
