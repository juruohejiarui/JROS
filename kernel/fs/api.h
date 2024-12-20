#ifndef __FS_API_H__
#define __FS_API_H__

#include "../includes/lib.h"
#include "../hardware/diskdevice.h"

typedef struct FS_API {

} FS_API;

typedef struct FS_Partition {
    DiskDevice *device;
    FS_API api;
    u64 st, ed; // start LBA and end LBA
    List listEle;
} FS_Partition;

extern List FS_partitionList;

void FS_registerPartition(FS_Partition *partition);
void FS_init();
#endif