#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "defs.h"
#include "types.h"
#include "generic.h"

// os-specific functions
#ifdef __linux__
#include "linux.h"
#else
#include "baremetal.h"
#endif
