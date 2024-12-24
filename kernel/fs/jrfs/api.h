#ifndef __FS_JRFS_API_H__
#define __FS_JRFS_API_H__

#include "desc.h"
#include "../api.h"

int FS_JRFS_mkfs(FS_Partition *partition);
int FS_JRFS_loadfs(FS_Partition *FS_Partition);

#endif