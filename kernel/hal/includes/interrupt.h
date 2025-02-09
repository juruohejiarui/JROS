#ifndef __HAL_INTERRUPT_H__
#define __HAL_INTERRUPT_H__

#include "arch.h"

#if defined(HAL_ARCH_AMD64)
	#include "../amd64/interrupt/interrupt.h"
#elif defined(HAL_ARCH_ARM64)
#endif

#endif