#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include "src/common.h"

/* Job status. */
#define S_RUNNING 1
#define S_WAITING 2
#define S_COMPLETE 3

/* Admin operations. */
#define ADMIN 1
#define OP_LIST 1
#define OP_REMOVE 2
#define OP_RESULT 3

/* User operations. */
#define USER 2
#define OP_CHECKSUM 1
#define OP_ENCRYPT 2
#define OP_COMPRESS 3

#define UDBG printf("UDBG %s - %d\n", __func__, __LINE__)

