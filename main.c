
#include "main.h"

int app_state = APP_INIT;

TSVresult config_tsv = TSVRESULT_INITIALIZER;
char *db_data_path;

int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
Mutex progl_mutex = MUTEX_INITIALIZER;

I1List i1l = LIST_INITIALIZER;

PeerList peer_list = LIST_INITIALIZER;
SensorFTSList sensor_list = LIST_INITIALIZER;
ProgList prog_list = LLIST_INITIALIZER;

#include "util.c"
#include "db.c"

int readSettings(TSVresult* r, const char *data_path, int *port, char **db_data_path) {
    if (!TSVinit(r, data_path)) {
        return 0;
    }
    int _port = TSVgetis(r, 0, "port");
    char *_db_data_path = TSVgetvalues(r, 0, "db_data_path");
    if (TSVnullreturned(r)) {
        return 0;
    }
    *port = _port;
    *db_data_path = _db_data_path;
    return 1;
}

void initApp() {
    if (!readSettings(&config_tsv, CONFIG_FILE, &sock_port, &db_data_path)) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize prog mutex\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
}

int initData() {
    if (!config_getPeerList(&peer_list, NULL, db_data_path)) {
        return 0;
    }
    if (!config_getSensorFTSList(&sensor_list, &peer_list, db_data_path)) {
        freePeerList(&peer_list);
        return 0;
    }
    if (!loadActiveProg(&prog_list, &sensor_list, db_data_path)) {
        freeProgList(&prog_list);
        FREE_LIST(&sensor_list);
        freePeerList(&peer_list);
        return 0;
    }
    return 1;
}
#define PARSE_I1LIST acp_requestDataToI1List(&request, &i1l);if (i1l.length <= 0) {return;}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, &sensor_list, NULL, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, &sensor_list, NULL, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_ENABLE)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->state = INIT;
                    if (item->save)db_saveTableFieldInt("prog", "enable", item->id, 1, NULL, db_data_path);
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_DISABLE)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->state = DISABLE;
                    if (item->save)db_saveTableFieldInt("prog", "enable", item->id, 0, NULL, db_data_path);
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgRuntime(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgInit(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_ENABLED)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgEnabled(item, &response)) {
                    return;
                }
            }
        }

    } else if (ACP_CMD_IS(ACP_CMD_GET_FTS)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgFTS(item, &response)) {
                    return;
                }
            }
        }

    }

    acp_responseSend(&response, &peer_client);
}

void progControl(Prog * item) {
#ifdef MODE_DEBUG
    printf("progId: %d ", item->id);
#endif
    switch (item->state) {
        case INIT:
            item->state = RUN;
            break;
        case RUN:
            if (!acp_readSensorFTS(&item->sensor)) {
#ifdef MODE_DEBUG
                puts("reading from sensor failed");
#endif
                return;
            }
#ifdef MODE_DEBUG
            printf("%.3f %ld %ld %d\n", item->sensor.value.value, item->sensor.value.tm.tv_sec, item->sensor.value.tm.tv_nsec, item->sensor.value.state);
#endif
            if (lockMutex(&item->value_mutex)) {
                item->value = item->sensor.value;
                unlockMutex(&item->value_mutex);
            }
            break;
        case OFF:
            break;
        case DISABLE:
            item->state = OFF;
            break;
        default:
            item->state = OFF;
            break;
    }

}

void cleanup_handler(void *arg) {
    Prog *item = arg;
    printf("cleaning up thread %d\n", item->id);
}

void *threadFunction(void *arg) {
    Prog *item = arg;
    printdo("thread for program with id=%d has been started\n", item->id);

#ifdef MODE_DEBUG
    pthread_cleanup_push(cleanup_handler, item);
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if (threadCancelDisable(&old_state)) {
            if (lockMutex(&item->mutex)) {
                progControl(item);
                unlockMutex(&item->mutex);
            }
            threadSetCancelState(old_state);
        }
        sleepRest(item->cycle_duration, t1);
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop(1);
#endif
}

void freeData() {
    stopAllProgThreads(&prog_list);
    freeProgList(&prog_list);
    FREE_LIST(&sensor_list);
    freePeerList(&peer_list);
    putsdo("freeData: done\n");
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
    freeMutex(&progl_mutex);
    TSVclear(&config_tsv);
}

void exit_nicely() {
    freeApp();
#ifdef MODE_DEBUG
    puts("\nBye...");
#endif
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
#ifdef MODE_DEBUG
    fprintf(stderr, "%s", s);
#endif
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perrorl("mlockall()");
    }
    int data_initialized = 0;
    while (1) {
#ifdef MODE_DEBUG
        printf("%s(): %s %d\n", F, getAppState(app_state), data_initialized);
#endif
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}