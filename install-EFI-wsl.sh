BUSID='3-4'
HAREWWAREID='21c4:0cd1'
usbipd.exe attach -w Ubuntu-22.04 --hardware-id ${HAREWWAREID}
if [ $? -ne 0 ];then
	echo -e "${RED_COLOR}==usbipd attach failed!==${RESET}"
else
	. ./install-EFI.sh
	usbipd.exe detach --hardware-id ${HAREWWAREID}  
fi
