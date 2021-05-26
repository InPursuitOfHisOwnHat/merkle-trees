#include "cakelog.h"

// Includes tab character

#define TIMESTAMP_STR_LEN 28

// This can be changed at compile time using the -D switch. A default line
// length of 1K should be sufficient for most projects, though. This isn't a
// memory issue, as such, because once a line is written to the log file, it's
// freed anyway. It's because the string functions used to build the log file
// line need to be aware of buffer sizes.

#ifndef CAKELOG_OUTPUT_STR_MAX_BUF_SIZE
    #define CAKELOG_OUTPUT_STR_MAX_BUF_SIZE 1024
#endif

static int _cakelog_initialised = 0;
static int _cakelog_fd;
static bool _force_flush;

// Get a nicely formatted timestamp in the following format:
//
// [YYYY-MM-DD HH:MM:SS.MS]\t

char * get_timestamp(void) {

    // Use system calls to get the time ('gettimeofday()' and 'localtime()')

    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    time_t t = tv.tv_sec;
    struct tm *_tm = localtime(&t);

    int ms = tv.tv_usec / 1000;

    char *timestamp_str = malloc(TIMESTAMP_STR_LEN);

    if (timestamp_str == NULL) {
        perror("initialise_cakelog(): error in get_time_string() when attempting to allocate memory for timestamp.");
        exit(EXIT_FAILURE);
    }

    // It is known, in advance, how big the timestamp will be

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

// Writes message to the log file including timestamp.

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

    // We allocate space for the log line on the stack, not the heap as we don't
    // need it beyond this function.

    char *full_str = alloca(str_len);
    
    strcpy(full_str, timestamp_str);
    strcat(full_str, formatted_msg_str);
    strcat(full_str, "\n");

    ssize_t bytes_written = write(_cakelog_fd, full_str, strlen(full_str));
    if (bytes_written == -1) {
        perror("initialise_cakelog(): error writing message to log file.");
        exit(EXIT_FAILURE);
    }
    
    free(timestamp_str);

    // User may have requested that the log flushes after each line, in which
    // case use system call 'fsync()' to do so.

    if (_force_flush == true) {
        fsync(_cakelog_fd);
    }

    return bytes_written;

}

// Set-up and create a new log file and write an initialisation message.

int cakelog_initialise(const char *executable_name, bool force_flush) {

    if ( _cakelog_initialised == 1) {
        printf("initialise_cakelog(): Attempt to initialise logging when it has already been initialised.\n");
        return -1;
    }

    _force_flush = force_flush;

    // Create filename with format: [Executable]_[Date]_[Time].log;

    time_t _time = time(NULL);
    struct tm *_tm = localtime(&_time); 

    char *log_file_name;
    size_t log_file_name_len = strlen(executable_name) + 21;

    // We allocate space for the filename on the stack, not the heap as it's not
    // needed beyond this function.

    log_file_name = alloca(log_file_name_len);

    if (log_file_name == NULL) {
        perror("initialise_cakelog(): error when attempting to allocate log_file_name string.");
        return -1;
    }

    snprintf(log_file_name, log_file_name_len, "%s_%.4d%.2d%.2d_%.2d%.2d%.2d.log",executable_name,_tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);

    // Now open file and assign descriptor to global variable ready to use.

    _cakelog_fd = open(log_file_name, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

    if (_cakelog_fd == -1){
        perror("initialise_cakelog(): error when attempting to open the log file fr input");
        return -1;
    }

    _cakelog_initialised = 1;

    cakelog("---------------------------------------------------------");
    cakelog("| Succesfully Initialised CakeLog with File Descriptor %d |", _cakelog_fd);
    cakelog("---------------------------------------------------------");

    return 0;

}

// Close and uninitialise the log file

int cakelog_stop() {

    if (_cakelog_initialised == 0 ) {
        /* Nothing to do */
        return 0;
    }

    cakelog("--------------------");
    cakelog("| Stopping CakeLog |");
    cakelog("--------------------");

    if (close(_cakelog_fd) == -1) {
        perror("stop_cakelog() : error when trying to close log file");
        exit(EXIT_FAILURE);
    }

    _cakelog_initialised = 0;
    return 0;

}