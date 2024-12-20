#include "api.h"

List FS_partitionList;

void FS_registerPartition(FS_Partition *partition) {
    List_insBefore(&partition->listEle, &FS_partitionList);
}

void FS_init()
{
    List_init(&FS_partitionList);
}