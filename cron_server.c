#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>

#include "cron_common.h"
#include "logger.h"

// ---------------------------------------------------------------------------

typedef struct Task {
    int id;
    timer_t timer_id;
    int interval;
    char command[MAX_CMD_LEN];
    struct Task* next;
    struct Task* prev;
} Task;

Task* task_list_head = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
int next_task_id = 1;
volatile int server_running = 1;
mqd_t server_q;

// ---------------------------------------------------------------------------

void parse_cmd_string(char* cmd_str, char** argv) {
    int i = 0;
    char* token = strtok(cmd_str, " ");
    while (token != NULL && i < 63) {
        argv[i++] = token;
        token = strtok(NULL, " ");
    }
    argv[i] = NULL;
}

// ---------------------------------------------------------------------------

int seconds_until(int hhmmss) {
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);

    int target_h = hhmmss / 10000;
    int target_m = (hhmmss % 10000) / 100;
    int target_s = hhmmss % 100;

    struct tm tm_target = *tm_now;
    tm_target.tm_hour = target_h;
    tm_target.tm_min = target_m;
    tm_target.tm_sec = target_s;

    time_t target_time = mktime(&tm_target);
    if (target_time <= now) {
        target_time += 24 * 3600;
    }
    return (int)difftime(target_time, now);
}

// ---------------------------------------------------------------------------

void task_thread_handler(union sigval sv) {
    Task* task = (Task*)sv.sival_ptr;

    char log_msg[300];
    snprintf(log_msg, sizeof(log_msg), "Uruchamianie zadania ID %d: %s", task->id, task->command);
    logger_message(LOG_STANDARD, log_msg);

    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGCHLD, SIG_DFL);
        char* argv[64];
        char cmd_copy[MAX_CMD_LEN];
        strncpy(cmd_copy, task->command, MAX_CMD_LEN);
        parse_cmd_string(cmd_copy, argv);
        execvp(argv[0], argv);
        exit(1);
    }

    if (task->interval == 0) {
        pthread_mutex_lock(&list_mutex);
        if (task->prev){
            task->prev->next = task->next;
        }
        if (task->next) {
            task->next->prev = task->prev;
        }
        if (task == task_list_head) {
            task_list_head = task->next;
        }

        timer_delete(task->timer_id);
        snprintf(log_msg, sizeof(log_msg), "Zadanie ID %d zakonczone i usuniete.", task->id);
        logger_message(LOG_MIN, log_msg);
        free(task);
        pthread_mutex_unlock(&list_mutex);
    }
}

// ---------------------------------------------------------------------------

void add_task(const char* cmd, int initial_delay, int interval) {
    Task* new_task = (Task*)malloc(sizeof(Task));
    new_task->id = next_task_id++;
    new_task->interval = interval;
    strncpy(new_task->command, cmd, MAX_CMD_LEN);

    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = task_thread_handler;
    sev.sigev_value.sival_ptr = new_task;
    sev.sigev_notify_attributes = NULL;

    if (timer_create(CLOCK_REALTIME, &sev, &new_task->timer_id) == -1) {
        perror("timer_create");
        free(new_task);
        return;
    }

    struct itimerspec its;
    its.it_value.tv_sec = initial_delay;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = interval;
    its.it_interval.tv_nsec = 0;

    timer_settime(new_task->timer_id, 0, &its, NULL);

    pthread_mutex_lock(&list_mutex);
    new_task->next = task_list_head;
    new_task->prev = NULL;
    if (task_list_head != NULL){
        task_list_head->prev = new_task;
    }
    task_list_head = new_task;
    pthread_mutex_unlock(&list_mutex);

    char msg[128];
    sprintf(msg, "Dodano zadanie ID %d: '%s' (start za %ds)", new_task->id, cmd, initial_delay);
    logger_message(LOG_STANDARD, msg);
}

// ---------------------------------------------------------------------------

void cancel_task(int id) {
    pthread_mutex_lock(&list_mutex);
    Task* curr = task_list_head;
    while(curr) {
        if(curr->id == id) {
            timer_delete(curr->timer_id);
            if(curr->prev) {
                curr->prev->next = curr->next;
            }
            if(curr->next) {
                curr->next->prev = curr->prev;
            }
            if(curr == task_list_head) {
                task_list_head = curr->next;
            }
            free(curr);
            char msg[64];
            sprintf(msg, "Anulowano zadanie ID %d", id);
            logger_message(LOG_STANDARD, msg);
            pthread_mutex_unlock(&list_mutex);
            return;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&list_mutex);
}

// ---------------------------------------------------------------------------

void cancel_all_tasks() {
    pthread_mutex_lock(&list_mutex);
    Task* curr = task_list_head;
    while(curr) {
        Task* next = curr->next;
        timer_delete(curr->timer_id);
        free(curr);
        curr = next;
    }
    task_list_head = NULL;
    pthread_mutex_unlock(&list_mutex);
}

// ---------------------------------------------------------------------------

void get_task_list_str(char* buffer, size_t size) {
    pthread_mutex_lock(&list_mutex);
    if (!task_list_head) {
        snprintf(buffer, size, "Brak zaplanowanych zadan.\n");
    }
    else {
        snprintf(buffer, size, "Zaplanowane zadania:\n");
        Task* curr = task_list_head;
        while(curr) {
            char line[512];
            snprintf(line, sizeof(line), "[ID: %d] Cmd: '%s' (Int: %ds)\n", curr->id, curr->command, curr->interval);
            if (strlen(buffer) + strlen(line) < size - 1) {
                strcat(buffer, line);
            }
            curr = curr->next;
        }
    }
    pthread_mutex_unlock(&list_mutex);
}

// ---------------------------------------------------------------------------

void run_server() {
    printf("[SERVER] Uruchamianie...\n");

    if (logger_init("cron_server.log") != 0) {
        fprintf(stderr, "Blad inicjalizacji loggera\n");
        exit(1);
    }
    logger_message(LOG_MIN, "Serwer CRON wystartowal.");

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(CronMessage);
    attr.mq_curmsgs = 0;

    server_q = mq_open(SERVER_QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if (server_q == (mqd_t)-1) {
        perror("Server mq_open błąd");
        logger_close();
        exit(1);
    }

    CronMessage msg;
    while(server_running) {
        ssize_t bytes_read = mq_receive(server_q, (char*)&msg, sizeof(CronMessage), NULL);
        if (bytes_read >= 0) {
            switch(msg.op) {
                case CMD_ADD_REL:
                    add_task(msg.cmd, msg.time_val, 0);
                    break;
                case CMD_ADD_CYC:
                    add_task(msg.cmd, msg.time_val, msg.time_val);
                    break;
                case CMD_ADD_ABS: {
                    int delay = seconds_until(msg.time_val);
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Planowanie absolutne na %06d (za %ds)", msg.time_val, delay);
                    logger_message(LOG_MIN, buf);
                    add_task(msg.cmd, delay, 0);
                    break;
                }
                case CMD_CANCEL:
                    cancel_task(msg.task_id);
                    break;
                case CMD_LIST: {
                    CronResponse resp;
                    get_task_list_str(resp.text, sizeof(resp.text));

                    char client_q_name[64];
                    sprintf(client_q_name, "/sp_cron_cli_%d", msg.client_pid);
                    mqd_t client_q = mq_open(client_q_name, O_WRONLY);

                    if (client_q != (mqd_t)-1) {
                        mq_send(client_q, (char*)&resp, sizeof(CronResponse), 0);
                        mq_close(client_q);
                    }
                    break;
                }
                case CMD_STOP_SRV:
                    logger_message(LOG_MAX, "Otrzymano polecenie wylaczenia serwera.");
                    server_running = 0;
                    break;
            }
        }
    }

    cancel_all_tasks();
    mq_close(server_q);
    mq_unlink(SERVER_QUEUE_NAME);
    logger_message(LOG_MIN, "Serwer konczy prace.");
    logger_close();
    printf("[SERVER] Zamknieto.\n");
}
