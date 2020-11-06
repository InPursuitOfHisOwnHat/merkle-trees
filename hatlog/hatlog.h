#include <unistd.h>

char * get_timestamp(void);
ssize_t hatlog(const char* msg_str, ...);
int hatlog_initialise(const char *executable_name, bool force_flush);
int hatlog_stop();