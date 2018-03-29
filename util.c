
#include "main.h"

FUN_LLIST_GET_BY_ID(Prog)

extern int getProgByIdFDB(int prog_id, Prog *item, SensorFTSList *sensor_list, sqlite3 *dbl, const char *db_path);

void stopProgThread(Prog *item) {
#ifdef MODE_DEBUG
    printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
}

void stopAllProgThreads(ProgList * list) {
    PROG_LIST_LOOP_ST
#ifdef MODE_DEBUG
            printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    PROG_LIST_LOOP_SP

    PROG_LIST_LOOP_ST
            void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
    PROG_LIST_LOOP_SP
}

void freeProg(Prog*item) {
    freeSocketFd(&item->sock_fd);
    freeMutex(&item->value_mutex);
    freeMutex(&item->mutex);
    free(item);
}

void freeProgList(ProgList *list) {
    Prog *item = list->top, *temp;
    while (item != NULL) {
        temp = item;
        item = item->next;
        freeProg(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

int checkProg(const Prog *item) {
    if (item->cycle_duration.tv_sec < 0) {
        fprintf(stderr, "%s(): negative cycle_duration_sec but positive expected where prog id = %d\n", __func__, item->id);
        return 0;
    }
    if (item->cycle_duration.tv_nsec < 0) {
        fprintf(stderr, "%s(): negative cycle_duration_nsec but positive expected where prog id = %d\n", __func__, item->id);
        return 0;
    }
    return 1;
}

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProgList: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

char * stateToStr(char state) {
    switch (state) {
        case INIT:
            return "INIT";
        case RUN:
            return "RUN";
        case DISABLE:
            return "STOP";
        case OFF:
            return "RESET";
    }
    return "";
}

int bufCatProgRuntime(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        char *state = stateToStr(item->state);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_ROW_STR,
                item->id,
                state
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgInit(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
                item->id,
                item->sensor.peer.id,
                item->sensor.remote_id,
                item->cycle_duration.tv_sec,
                item->cycle_duration.tv_nsec
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgFTS(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->value_mutex)) {
        int r = acp_responseFTSCat(item->id, item->value.value, item->value.tm, item->value.state, response);
        unlockMutex(&item->value_mutex);
        return r;
    }
    return 0;
}

int bufCatProgEnabled(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        int enabled = 1;
        if (item->state == DISABLE || item->state == OFF) {
            enabled = 0;
        }
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                item->id,
                enabled
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

void printData(ACPResponse *response) {
    ProgList *list = &prog_list;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)
    snprintf(q, sizeof q, "prog_list length: %d\n", list->length);
    SEND_STR(q)
    SEND_STR("+-----------------------------------------------+\n")
    SEND_STR("|                     Program                   |\n")
    SEND_STR("|                       +-----------------------+\n")
    SEND_STR("|                       |         Sensor        |\n")
    SEND_STR("|-----------+-----------+-----------+-----------+\n")
    SEND_STR("|    id     |   state   |  peer_id  | remote_id |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")

    PROG_LIST_LOOP_ST
            char *state = stateToStr(item->state);
    snprintf(q, sizeof q, "|%11d|%11s|%11s|%11d|\n",
            item->id,
            state,
            item->sensor.peer.id,
            item->sensor.remote_id
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+-----------+-----------+-----------+\n")

    acp_sendPeerListInfo(&peer_list, response, &peer_client);

    SEND_STR("+-----------+------------------------------------------------------------------------------+\n")
    SEND_STR("|    Prog   |                                   Sensor                                     |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+------------------------------------------+\n")
    SEND_STR("|           |           |           |           |                   value                  |\n")
    SEND_STR("|           |           |           |           |-----------+-----------+-----------+------+\n")
    SEND_STR("|    id     |    id     | remote_id |  peer_id  |   value   |    sec    |   nsec    | state|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+------+\n")
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%11d|%11d|%11s|%11f|%11ld|%11ld|%6d|\n",
            item->id,
            item->sensor.id,
            item->sensor.remote_id,
            item->sensor.peer.id,
            item->sensor.value.value,
            item->sensor.value.tm.tv_sec,
            item->sensor.value.tm.tv_nsec,
            item->sensor.value.state
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR_L("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+------+\n")

}

void printHelp(ACPResponse *response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tload prog into RAM and start its execution; program id expected\n", ACP_CMD_PROG_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM; program id expected\n", ACP_CMD_PROG_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM, after that load it; program id expected\n", ACP_CMD_PROG_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tenable running program; program id expected\n", ACP_CMD_PROG_ENABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable running program; program id expected\n", ACP_CMD_PROG_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog state (1-enabled, 0-disabled); program id expected\n", ACP_CMD_PROG_GET_ENABLED);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog sensor value; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog runtime data; program id expected\n", ACP_CMD_PROG_GET_DATA_RUNTIME);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog initial data; program id expected\n", ACP_CMD_PROG_GET_DATA_INIT);
    SEND_STR_L(q)
}
