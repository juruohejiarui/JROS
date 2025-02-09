#ifndef __HAL_AMD64_INIT_H__
#define __HAL_AMD64_INIT_H__

// this function is called after preparation from head.S;
// then it will call hal function and architecture-agnositic function and architecture-specific function for initialization
void hal_init();
#endif