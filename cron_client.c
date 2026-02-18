#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "cron_common.h"

// ---------------------------------------------------------------------------

void send_request(CronMessage* msg) {
    mqd_t q = mq_open(SERVER_QUEUE_NAME, O_WRONLY);
    if (q == (mqd_t)-1) {
        fprintf(stderr, "Nie mozna polaczyc sie z serwerem. Uruchom najpierw ./server\n");
        exit(1);
    }
    
    mqd_t my_q;
    char my_q_name[64];
    sprintf(my_q_name, "/sp_cron_cli_%d", getpid());
    if (msg->op == CMD_LIST) {
        struct mq_attr attr = {0, 5, sizeof(CronResponse), 0};
        my_q = mq_open(my_q_name, O_CREAT | O_RDONLY, 0644, &attr);
    }

    if (mq_send(q, (char*)msg, sizeof(CronMessage), 0) == -1) {
        perror("Blad wysylania");
    }

    if (msg->op == CMD_LIST) {
        CronResponse resp;
        if (mq_receive(my_q, (char*)&resp, sizeof(CronResponse), NULL) >= 0) {
            printf("%s", resp.text);
        }
        mq_close(my_q);
        mq_unlink(my_q_name);
    }

    mq_close(q);
}

// ---------------------------------------------------------------------------

void print_usage(const char* name) {
    printf("Uzycie: %s [opcje]\n", name);
    printf("  -a <sec> <cmd>   : Zadanie za X sekund\n");
    printf("  -c <sec> <cmd>   : Zadanie cykliczne co X sekund\n");
    printf("  -t <HHMMSS> <cmd>: Zadanie o godzinie\n");
    printf("  -l               : Lista zadan\n");
    printf("  -r <id>          : Usun zadanie\n");
    printf("  -q               : Wylacz serwer\n");
}
