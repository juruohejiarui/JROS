#ifndef __HW_DEVICE_H__
#define __HW_DEVICE_H__

#include "../includes/lib.h"

#define DiskDevice_Task_State_Succ		0x0
#define DiskDevice_Task_State_Fail		0x1
#define DiskDevice_Task_State_Busy		(0 << 1)
#define DiskDevice_Task_State_Refuse	(1 << 1)
#define DiskDevice_Task_State_Unknown	(1 << 2)

typedef struct DiskDevice {
	int (*read)(void *buf, u64 pos, u64 size);
	int (*write)(void *buf, u64 pos, u64 size);
	int (*writeZero)(u64 pos, u64 size);

	List listEle;
} DiskDevice;

#endif