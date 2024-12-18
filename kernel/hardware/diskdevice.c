#include "diskdevice.h"

List HW_DiskDevice_devList;

void HW_DiskDevice_Register(DiskDevice *device) {
    List_insBefore(&device->listEle, &HW_DiskDevice_devList);
    if (device->init) device->init(device);
}

void HW_DiskDevice_Unregister(DiskDevice *device) {
    List_del(&device->listEle);
}

void HW_DiskDevice_init() {
    List_init(&HW_DiskDevice_devList);
}
