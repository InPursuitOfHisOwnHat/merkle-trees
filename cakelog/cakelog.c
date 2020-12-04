#include "cakelog.h"

#define TIMESTAMP_STR_LEN 27

#ifndef CAKELOG_OUTPUT_STR_MAX_BUF_SIZE
    #define CAKELOG_OUTPUT_STR_MAX_BUF_SIZE 256
#endif

static int _cakelog_initialised = 0;
static int _cakelog_fd;
static bool _force_flush;

char * get_timestamp(void) {

    struct timeval tv;
    gettimeofday(&tv, NULL);

    /* to nearest ms */
    int ms = lrint(tv.tv_usec / 1000.0); 

    /* to nearest second */
    if (ms >= 1000) {
        ms -=1000;
        tv.tv_sec++;
    }

    time_t _time = time(NULL);
    struct tm *_tm = localtime(&_time);
    char *timestamp_str = malloc(TIMESTAMP_STR_LEN);

    if (timestamp_str == NULL) {
        perror("initialise_cakelog(): error in get_time_string() when attempting to allocate memory for timestamp.");
        exit(EXIT_FAILURE);
    }

    snprintf(timestamp_str, TIMESTAMP_STR_LEN, "[%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%03d]\t",
                                            _tm->tm_year+1900,
                                            _tm->tm_mon+1,
                                            _tm->tm_mday, 
                                            _tm->tm_hour, 
                                            _tm->tm_min, 
                                            _tm->tm_sec,
                                            ms);
    return timestamp_str;

}

ssize_t cakelog(const char* msg_str, ...) {

    if ( _cakelog_initialised == 0) {
        return 0;
    }

    char formatted_msg_str[CAKELOG_OUTPUT_STR_MAX_BUF_SIZE];
    va_list args;
    va_start(args, msg_str);
    vsnprintf(formatted_msg_str, CAKELOG_OUTPUT_STR_MAX_BUF_SIZE, msg_str, args);
    va_end(args);

    char *timestamp_str = get_timestamp();
    long str_len = strlen(formatted_msg_str) + strlen(timestamp_str)+2;
    char *full_str = malloc(str_len);
    
    strcpy(full_str, timestamp_str);
    strcat(full_str, formatted_msg_str);
    strcat(full_str, "\n");

    ssize_t bytes_written = write(_cakelog_fd, full_str, strlen(full_str));
    if (bytes_written == -1) {
        perror("initialise_cakelog(): error writing message to log file.");
        exit(EXIT_FAILURE);
    }
    
    // Flush
    if (_force_flush == true) {
        fsync(_cakelog_fd);
    }

}

int cakelog_initialise(const char *executable_name, bool force_flush) {

    if ( _cakelog_initialised == 1) {
        printf("initialise_cakelog(): Attempt to initialise logging when it has already been initialised.\n");
        return -1;
    }

    _force_flush = force_flush;

    // Create filename ([Executable]_[Date]_[Time].log);
    time_t _time = time(NULL);
    struct tm *_tm = localtime(&_time); 

    char *log_file_name;
    size_t log_file_name_len = strlen(executable_name) + 21;

    // Stick on stack - it's small and not returned or passed anywhere
    log_file_name = alloca(log_file_name_len);

    if (log_file_name == NULL) {
        perror("initialise_cakelog(): error when attempting to allocate log_file_name string.");
        return -1;
    }

    snprintf(log_file_name, log_file_name_len, "%s_%.4d%.2d%.2d_%.2d%.2d%.2d.log",executable_name,_tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);

    // Now open file and assign descriptor to global variable.
    _cakelog_fd = open(log_file_name, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

    if (_cakelog_fd == -1){
        perror("initialise_cakelog(): error when attempting to open the log file fr input");
        return -1;
    }

    _cakelog_initialised = 1;

    cakelog("===================================================");
    cakelog("Succesfully Initialised Log with File Descriptor %d", _cakelog_fd);
    cakelog("===================================================");

    return 0;

}

int cakelog_stop() {

    if (_cakelog_initialised == 0 ) {
        /* Nothing to do */
        return 0;
    }

    cakelog("============");
    cakelog("Stopping log");
    cakelog("============");

    if (close(_cakelog_fd) == -1) {
        perror("stop_cakelog() : error when trying to close log file");
        exit(EXIT_FAILURE);
    }

    _cakelog_initialised = 0;
    return 0;

}



