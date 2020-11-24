#include <unistd.h>

char * get_timestamp(void);
ssize_t cakelog(const char* msg_str, ...);
int cakelog_initialise(const char *executable_name, bool force_flush);
int cakelog_stop();