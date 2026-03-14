#ifndef COMMON_H
#define COMMON_H
#define _GNU_SOURCE // for getline

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "jrmalloc.h"

#define MAX_BUFF_LEN 4194304 // 2^22
#define UINT8_COUNT (UINT8_MAX + 1)

#endif // COMMON_H
