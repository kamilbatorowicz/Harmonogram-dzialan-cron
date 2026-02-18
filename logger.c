#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>

// -----------------------------------------------------------------------------------------------
// BIBLIOTEKA

static struct {
    int is_initialized;
    volatile int running;
    pthread_t th_dump;
    pthread_t th_toggle;
    pthread_t th_level;
    sem_t sem_dump;
    sem_t sem_toggle;
    sem_t sem_level;
    pthread_mutex_t mutex;
    int logging_enabled;
    LogLevel level;
    char fname[256];
    volatile sig_atomic_t signal_dump_val;
} logger_ctx = { .is_initialized = 0 };

// -----------------------------------------------------------------------------------------------

void get_time_str(char* buffer, size_t size){
    time_t now = time(NULL);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

static void dump_stack(FILE* fp){
    int local_var = 123;
    fprintf(fp, "ZRZUT STOSU\n");
    fprintf(fp, "Adres zmiennej lokalnej: %p\n", (void*)&local_var);
    fprintf(fp, "Wartosc: %d\n", local_var);
    fprintf(fp, "KONIEC ZRZUTU\n");
}

static void dump_heap(FILE* fp){
    void* ptr = malloc(64);
    fprintf(fp, "ZRZUT STERTY\n");
    if(ptr){
        fprintf(fp, "Adres alokacji: %p\n", ptr);
        free(ptr);
    } 
    else{
        fprintf(fp, "Blad malloc!\n");
    }
    fprintf(fp, "KONIEC ZRZUTU\n");
}

// -----------------------------------------------------------------------------------------------
// HANDLERY

void handler_dump(int signo, siginfo_t* info, void* context){
    logger_ctx.signal_dump_val = info->si_value.sival_int;
    sem_post(&logger_ctx.sem_dump);
}

void handler_toggle(int signo, siginfo_t* info, void* context){
    sem_post(&logger_ctx.sem_toggle);
}

void handler_level(int signo, siginfo_t* info, void* context){
    sem_post(&logger_ctx.sem_level);
}

// -----------------------------------------------------------------------------------------------
// WATKI

void* thread_dump_loop(void* arg) {
    while(logger_ctx.running){
        if(sem_wait(&logger_ctx.sem_dump) != 0){
            if(errno == EINTR){
                continue;
            }
            break;
        }
        if(!logger_ctx.running){
            break;
        }

        int val = logger_ctx.signal_dump_val;
        char path[64];
        const char* type_str = (val == DUMP_STACK) ? "STACK" : "HEAP";
        snprintf(path, 64, "dump_%s.txt", type_str);
        
        FILE* fp = fopen(path, "w");
        if(fp){
            if(val == DUMP_STACK){
                dump_stack(fp);
            }
            else{
                dump_heap(fp);
            }
            fclose(fp);
            
            char msg[64];
            snprintf(msg, 64, "Wykonano zrzut: %s", type_str);
            logger_message(LOG_MIN, msg);
        }
    }
    return NULL;
}

void* thread_toggle_loop(void* arg){
    while(logger_ctx.running){
        if(sem_wait(&logger_ctx.sem_toggle) != 0){
            if(errno == EINTR){
                continue;
            }
            break;
        }
        if(!logger_ctx.running){
            break;
        }

        pthread_mutex_lock(&logger_ctx.mutex);
        logger_ctx.logging_enabled = !logger_ctx.logging_enabled;
        
        FILE* fp = fopen(logger_ctx.fname, "a");
        if(fp){
            fprintf(fp, "[SYSTEM] Logowanie: %s\n", logger_ctx.logging_enabled ? "ON" : "OFF");
            fclose(fp);
        }
        pthread_mutex_unlock(&logger_ctx.mutex);
    }
    return NULL;
}

void* thread_level_loop(void* arg){
    while(logger_ctx.running){
        if(sem_wait(&logger_ctx.sem_level) != 0){
            if(errno == EINTR){
                continue;
            }
            break;
        }
        if(!logger_ctx.running){
            break;
        }

        pthread_mutex_lock(&logger_ctx.mutex);
        logger_ctx.level = (logger_ctx.level == LOG_MAX) ? LOG_MIN : logger_ctx.level + 1;
        
        FILE* fp = fopen(logger_ctx.fname, "a");
        if(fp){
            fprintf(fp, "[SYSTEM] Nowy poziom: %d\n", logger_ctx.level);
            fclose(fp);
        }
        pthread_mutex_unlock(&logger_ctx.mutex);
    }
    return NULL;
}

// -----------------------------------------------------------------------------------------------

int logger_init(const char* filename){
    if(logger_ctx.is_initialized){
        fprintf(stderr, "BLAD: Logger juz zainicjalizowany!\n");
        return -1;
    }

    pthread_mutex_init(&logger_ctx.mutex, NULL);

    sem_init(&logger_ctx.sem_dump, 0, 0);
    sem_init(&logger_ctx.sem_toggle, 0, 0);
    sem_init(&logger_ctx.sem_level, 0, 0);
    
    strncpy(logger_ctx.fname, filename, 255);
    logger_ctx.logging_enabled = 1;
    logger_ctx.level = LOG_MIN;
    logger_ctx.running = 1;

    if(pthread_create(&logger_ctx.th_dump, NULL, thread_dump_loop, NULL) != 0){
        return -1;
    }
    if(pthread_create(&logger_ctx.th_toggle, NULL, thread_toggle_loop, NULL) != 0){
        return -1;
    }
    if(pthread_create(&logger_ctx.th_level, NULL, thread_level_loop, NULL) != 0){
        return -1;
    }

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sa.sa_sigaction = handler_dump;
    sigaction(SIG_DUMP, &sa, NULL);

    sa.sa_sigaction = handler_toggle;
    sigaction(SIG_TOGGLE, &sa, NULL);

    sa.sa_sigaction = handler_level;
    sigaction(SIG_LEVEL, &sa, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIG_DUMP);
    sigaddset(&set, SIG_TOGGLE);
    sigaddset(&set, SIG_LEVEL);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    logger_ctx.is_initialized = 1;
    return 0;
}

// -----------------------------------------------------------------------------------------------

void logger_close(){
    if(!logger_ctx.is_initialized){
        return;
    }

    logger_ctx.running = 0;

    sem_post(&logger_ctx.sem_dump);
    sem_post(&logger_ctx.sem_toggle);
    sem_post(&logger_ctx.sem_level);

    pthread_join(logger_ctx.th_dump, NULL);
    pthread_join(logger_ctx.th_toggle, NULL);
    pthread_join(logger_ctx.th_level, NULL);

    sem_destroy(&logger_ctx.sem_dump);
    sem_destroy(&logger_ctx.sem_toggle);
    sem_destroy(&logger_ctx.sem_level);
    pthread_mutex_destroy(&logger_ctx.mutex);

    logger_ctx.is_initialized = 0;
}

// -----------------------------------------------------------------------------------------------

void logger_message(LogLevel lvl, const char* msg){
    if(!logger_ctx.is_initialized){
        return;
    }

    pthread_mutex_lock(&logger_ctx.mutex);
    if(logger_ctx.logging_enabled && lvl >= logger_ctx.level){
        FILE* fp = fopen(logger_ctx.fname, "a");
        if(fp){
            char t[20];
            get_time_str(t, sizeof(t));
            char* ls = (lvl==0) ? "MIN" : (lvl==1) ? "STD" : "MAX";
            fprintf(fp, "[%s] [%s] %s\n", t, ls, msg);
            fclose(fp);
        }
    }
    pthread_mutex_unlock(&logger_ctx.mutex);
}