#ifndef __HW_DEVICE_H__
#define __HW_DEVICE_H__

#include "../includes/lib.h"

#define DiskDevice_Task_State_Succ		0x0
#define DiskDevice_Task_State_Fail		0x1
#define DiskDevice_Task_State_Busy			(0 << 1)
#define DiskDevice_Task_State_Refuse		(1 << 1)
#define DiskDevice_Task_State_OutOfRange	(1 << 2)
#define DiskDevice_Task_State_Unknown		(1 << 3)

typedef struct DiskDevice {
	int (*read)(struct DiskDevice *device, void *buf, u64 pos, u64 size);
	int (*write)(struct DiskDevice *device, void *buf, u64 pos, u64 size);

	List listEle;
} DiskDevice;

void HW_DiskDevice_Register(DiskDevice *device);

void HW_DiskDevice_init();

#endif