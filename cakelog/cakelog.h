#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <alloca.h>
#include <stdbool.h>
#include <math.h>

char * get_timestamp(void);
ssize_t cakelog(const char* msg_str, ...);
int cakelog_initialise(const char *executable_name, bool force_flush);
int cakelog_stop();