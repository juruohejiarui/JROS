#ifndef __FS_GPT_API_H__
#define __FS_GPT_API_H__

#include "desc.h"
#include "../../hardware/diskdevice.h"

int FS_GPT_scan(DiskDevice *device);

#endif