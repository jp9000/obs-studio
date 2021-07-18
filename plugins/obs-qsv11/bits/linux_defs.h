#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MSDK_FOPEN(FH, FN, M)           { FH=fopen(FN,M); }
#define MSDK_SLEEP(X)                   { usleep(1000*(X)); }

typedef timespec mfxTime;
