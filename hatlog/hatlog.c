#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <alloca.h>
#include <stdbool.h>
#include "hatlog.h"

#define TIME_STR_LEN 23

#ifndef HATLOG_OUTPUT_STR_MAX_BUF_SIZE
    #define HATLOG_OUTPUT_STR_MAX_BUF_SIZE 256
#endif

static int _hatlog_initialised = 0;
static int _hatlog_fd;
static bool _force_flush;

char * get_timestamp(void) {


    time_t _time = time(NULL);
    struct tm *_tm = localtime(&_time);
    char *timestamp_str = malloc(TIME_STR_LEN);

    if (timestamp_str == NULL) {
        perror("initialise_hatlog(): error in get_time_string() when attempting to allocate memory for timestamp.");
        exit(EXIT_FAILURE);
    }

    snprintf(timestamp_str, TIME_STR_LEN, "[%.4d-%.2d-%.2d %.2d:%.2d:%.2d]\t",_tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);
    return timestamp_str;

}

ssize_t hatlog(const char* msg_str, ...) {

    if ( _hatlog_initialised == 0) {
        return 0;
    }

    char formatted_msg_str[HATLOG_OUTPUT_STR_MAX_BUF_SIZE];
    va_list args;
    va_start(args, msg_str);
    vsnprintf(formatted_msg_str, HATLOG_OUTPUT_STR_MAX_BUF_SIZE, msg_str, args);
    va_end(args);

    char *timestamp_str = get_timestamp();
    long str_len = strlen(formatted_msg_str) + strlen(timestamp_str)+2;
    char *full_str = malloc(str_len);
    
    strcpy(full_str, timestamp_str);
    strcat(full_str, formatted_msg_str);
    strcat(full_str, "\n");

    ssize_t bytes_written = write(_hatlog_fd, full_str, strlen(full_str));
    if (bytes_written == -1) {
        perror("initialise_hatlog(): error writing message to log file.");
        exit(EXIT_FAILURE);
    }
    
    // Flush
    if (_force_flush == true) {
        fsync(_hatlog_fd);
    }

}

int hatlog_initialise(const char *executable_name, bool force_flush) {

    if ( _hatlog_initialised == 1) {
        printf("initialise_hatlog(): Attempt to initialise logging when it has already been initialised.\n");
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
        perror("initialise_hatlog(): error when attempting to allocate log_file_name string.");
        return -1;
    }

    snprintf(log_file_name, log_file_name_len, "%s_%.4d%.2d%.2d_%.2d%.2d%.2d.log",executable_name,_tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);

    // Now open file and assign descriptor to global variable.
    _hatlog_fd = open(log_file_name, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

    if (_hatlog_fd == -1){
        perror("initialise_hatlog(): error when attempting to open the log file fr input");
        return -1;
    }

    _hatlog_initialised = 1;

    hatlog("===================================================");
    hatlog("Succesfully Initialised Log with File Descriptor %d", _hatlog_fd);
    hatlog("===================================================");

    return 0;

}

int hatlog_stop() {

    if (_hatlog_initialised == 0 ) {
        /* Nothing to do */
        return 0;
    }

    hatlog("============");
    hatlog("Stopping log");
    hatlog("============");

    if (close(_hatlog_fd) == -1) {
        perror("stop_hatlog() : error when trying to close log file");
        exit(EXIT_FAILURE);
    }

    _hatlog_initialised = 0;
    return 0;

}



