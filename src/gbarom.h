#ifndef __GBAROM_H__
#define __GBAROM_H__

#include <stdint.h>
#include "common/util.h"
#include "gbamem.h"

void load_gbarom(char* filename, gbamem_t* mem);

#endif
