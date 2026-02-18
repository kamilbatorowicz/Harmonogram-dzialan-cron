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

int main(int argc, char* argv[]) {
    struct mq_attr attr = {0, 10, sizeof(CronMessage), 0};
    mqd_t check = mq_open(SERVER_QUEUE_NAME, O_CREAT | O_EXCL | O_RDONLY, 0644, &attr);

    if (check != (mqd_t)-1) {
        mq_close(check);
        mq_unlink(SERVER_QUEUE_NAME);

        if (argc > 1) {
            printf("Serwer nie jest uruchomiony. Uruchom program bez argumentow, aby wlaczyc serwer.\n");
            return 1;
        }

        run_server();
        return 0;
    }
    else {
        if (errno != EEXIST) {
            perror("Blad dostepu do kolejki serwera (krytyczny)");
            return 1;
        }
    }

    if (argc < 2) {
        printf("Serwer juz dziala w tle.\n");
        print_usage(argv[0]);
        return 0;
    }

    CronMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.client_pid = getpid();

    if (strcmp(argv[1], "-l") == 0) {
        msg.op = CMD_LIST;
        send_request(&msg);
    }
    else if (strcmp(argv[1], "-q") == 0) {
        msg.op = CMD_STOP_SRV;
        send_request(&msg);
    }
    else if (strcmp(argv[1], "-r") == 0 && argc >= 3) {
        msg.op = CMD_CANCEL;
        msg.task_id = atoi(argv[2]);
        send_request(&msg);
    }
    else if ((strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "-t") == 0) && argc >= 4) {
        if (strcmp(argv[1], "-a") == 0){
            msg.op = CMD_ADD_REL;
        }
        else if (strcmp(argv[1], "-c") == 0) {
            msg.op = CMD_ADD_CYC;
        }
        else {
            msg.op = CMD_ADD_ABS;
        }

        msg.time_val = atoi(argv[2]);

        msg.cmd[0] = '\0';
        for(int i = 3; i < argc; i++) {
            strcat(msg.cmd, argv[i]);
            if (i < argc - 1){
                strcat(msg.cmd, " ");
            }
        }
        send_request(&msg);
        printf("Wyslano polecenie do serwera.\n");
    }
    else {
        print_usage(argv[0]);
    }

    return 0;
}
