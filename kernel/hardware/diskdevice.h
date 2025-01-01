#ifndef __HW_DEVICE_H__
#define __HW_DEVICE_H__

#include "../includes/lib.h"

#define DiskDevice_LbaShift 9
#define DiskDevice_LbaSize (1 << DiskDevice_LbaShift)

#define DiskDevice_Task_State_Succ		0x0
#define DiskDevice_Task_State_Fail		0x1
#define DiskDevice_Task_State_Busy			(0 << 1)
#define DiskDevice_Task_State_Refuse		(1 << 1)
#define DiskDevice_Task_State_OutOfRange	(1 << 2)
#define DiskDevice_Task_State_InvalidArg	(1 << 3)
#define DiskDevice_Task_State_Unknown		(1 << 4)

typedef struct DiskDevice {
	int (*read)(struct DiskDevice *device, void *buf, u64 pos, u64 size);
	int (*write)(struct DiskDevice *device, void *buf, u64 pos, u64 size);

	int (*init)(struct DiskDevice *device);

	List listEle;
} DiskDevice;

extern List HW_DiskDevice_devList;


void HW_DiskDevice_Register(DiskDevice *device);
void HW_DiskDevice_Unregister(DiskDevice *device);

void HW_DiskDevice_init();

#endif