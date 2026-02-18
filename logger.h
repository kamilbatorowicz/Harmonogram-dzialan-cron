#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <signal.h>

// poziomy logowania
typedef enum {
    LOG_MIN = 0,
    LOG_STANDARD = 1,
    LOG_MAX = 2
} LogLevel;

// definicje sygnalow
#define SIG_DUMP   (SIGRTMIN + 0)
#define SIG_TOGGLE (SIGRTMIN + 1)
#define SIG_LEVEL  (SIGRTMIN + 2)

// typy zrzutow
#define DUMP_STACK 1
#define DUMP_HEAP  2

// funkcje biblioteki
int logger_init(const char* log_filename);
void logger_close();
void logger_message(LogLevel importance, const char* msg);

#endif // LOGGER_H