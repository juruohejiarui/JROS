#ifndef __FS_JRFS_API_H__
#define __FS_JRFS_API_H__

#include "desc.h"
#include "../api.h"

int FS_JRFS_mkfs(FS_Part *partition);
int FS_JRFS_loadfs(FS_Part *FS_Partition);

#endif