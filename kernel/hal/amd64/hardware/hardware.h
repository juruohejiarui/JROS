#ifndef __HAL_AMD64_HARDWARE_H__
#define __HAL_AMD64_HARDWARE_H__

// initialize the hardware system, for amd64, get the UEFI information
int hal_HW_init();

int hal_HW_initMemMap();

int hal_HW_initAdvance();

int hal_HW_initPCIe();

int hal_HW_initTimer();

int hal_HW_initKeyboard();

#endif