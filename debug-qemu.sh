# ovmfPath="/usr/share/OVMF/OVMF_CODE_4M.fd"
ovmfPath=/usr/share/edk2/ovmf/OVMF_CODE.fd
usbVendor=0x21c4
usbProduct=0x0cd1
# usbVendor=0x17ef
# usbProduct=0x3899

qemu-system-x86_64 \
	-drive file="${ovmfPath}",if=pflash,format=raw,unit=0,readonly=on \
	-enable-kvm \
	-monitor stdio \
	-machine type=q35 \
	-cpu qemu64,+avx,+xsave \
	-device qemu-xhci \
	-net none \
	-device usb-host,vendorid=${usbVendor},productid=${usbProduct},id=hostdev0 \
	-device usb-kbd \
	-device usb-mouse \
	-m 512M \
	-smp 4	\
	# -device usb-host,vendorid=0x24ae,productid=0x4056,id=hostdev1 \
