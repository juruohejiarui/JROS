#ifndef __HAL_HARDWARE_H__
#define __HAL_HARDWARE_H__

#include "arch.h"

#if defined(HAL_ARCH_AMD64)
	#include "../amd64/init/init.h"
#elif defined(HAL_ARCH_ARM64)
#endif

#endif