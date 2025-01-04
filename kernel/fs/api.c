#include "api.h"

List FS_partitionList;

void FS_registerPart(FS_Part *part) {
	List_insBefore(&part->listEle, &FS_partitionList);
}
void FS_unregisterPart(FS_Part *part) {
	List_del(&part->listEle);
	if (part->unregister) part->unregister(part);
}

void FS_init() {
	List_init(&FS_partitionList);
}