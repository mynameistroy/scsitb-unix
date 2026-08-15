#ifndef __SCSITB_H__
#define __SCSITB_H__

#define NO_ERROR 0
#define INVALID_ARGS -1;
#define ALLOC_ERROR -2;
#define CMD_FAILED -3;
#define NOT_FOUND -4;

#define LOG(level, str) scsitb_log(__LINE__, __FILE__, level, str);
#define LOGF(level, str, ...)                                                  \
    scsitb_log(__LINE__, __FILE__, level, str, __VA_ARGS__)

#define NORMAL 0
#define VERBOSE 1

extern int log_level;
void scsitb_log(int line, char *file, int LEVEL, char *fmt, ...);

#endif
