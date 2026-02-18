#ifndef CRON_COMMON_H
#define CRON_COMMON_H

#include <sys/types.h>

#define SERVER_QUEUE_NAME "/sp_cron_server_q"
#define MAX_CMD_LEN 256
#define MAX_MSG_SIZE 512

// typy operacji
typedef enum {
    CMD_ADD_REL,    // zadanie względne (za X sekund)
    CMD_ADD_ABS,    // zadanie o godzinie HH:MM:SS
    CMD_ADD_CYC,    // zadanie cykliczne (co X sekund)
    CMD_LIST,       // lista zadań
    CMD_CANCEL,     // anuluj zadanie po ID
    CMD_STOP_SRV    // zatrzymaj serwer
} OpCode;

// struktura komunikatu
typedef struct {
    OpCode op;
    pid_t client_pid;
    int time_val;
    int task_id;
    char cmd[MAX_CMD_LEN];
} CronMessage;

// struktura odpowiedzi
typedef struct {
    char text[1024];
} CronResponse;

// server
void run_server();

// client
void send_request(CronMessage* msg);
void print_usage(const char* name);

#endif