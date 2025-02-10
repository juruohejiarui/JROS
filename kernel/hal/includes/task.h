#ifndef __HAL_TASK_H__
#define __HAL_TASK_H__

#include "arch.h"

#if defined(HAL_ARCH_AMD64)
	#include "../amd64/task/task.h"
#elif defined(HAL_ARCH_ARM64)
#endif

#endif