
#include "main.h"

int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int load = 0, enable = 0;

    int c = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            item->id = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("sensor_id")) {
            SensorFTS *sensor = getSensorFTSById(atoi(argv[i]), data->sensor_list);
            if (sensor == NULL) {
                return EXIT_FAILURE;
            }
            item->sensor = *sensor;
            c++;

        } else if (DB_COLUMN_IS("cycle_duration_sec")) {
            item->cycle_duration.tv_sec = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("cycle_duration_nsec")) {
            item->cycle_duration.tv_nsec = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("enable")) {
            enable = atoi(argv[i]);
            c++;
        } else if (DB_COLUMN_IS("load")) {
            load = atoi(argv[i]);
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column:%s\n", __func__,DB_COLUMN_STR);
#endif
            c++;
        }
    }
#define N 6
    if (c != N) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): required %d columns but %d found\n", __func__, N, c);
#endif
        return EXIT_FAILURE;
    }
#undef N
    if (enable) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    if (!load) {
        db_saveTableFieldInt("prog", "load", item->id, 1, data->db_data, NULL);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Prog *item, SensorFTSList *sensor_list, sqlite3 *dbl, const char *db_path) {
    if (dbl != NULL && db_path != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): dbl xor db_path expected\n", __func__);
#endif
        return 0;
    }
    sqlite3 *db;
    int close = 0;
    if (db_path != NULL) {
        if (!db_open(db_path, &db)) {
            return 0;
        }
        close = 1;
    } else {
        db = dbl;
    }
    char q[LINE_SIZE];
    ProgData data = {.sensor_list = sensor_list, .prog = item, .db_data = db};
    snprintf(q, sizeof q, "select " PROG_FIELDS " from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): query failed\n", __func__);
#endif
        if (close)sqlite3_close(db);
        return 0;
    }
    if (close)sqlite3_close(db);
    return 1;
}

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): can not load prog with id=%d - list length exceeded\n", __func__, item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockProgList();
        list->top = item;
        unlockProgList();
    } else {
        lockMutex(&list->last->mutex);
        list->last->next = item;
        unlockMutex(&list->last->mutex);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("%s(): prog with id=%d loaded\n", __func__, item->id);
#endif
    return 1;
}

int addProgById(int prog_id, ProgList *list, SensorFTSList *sensor_list, sqlite3 *db_data, const char *db_data_path) {
    extern struct timespec cycle_duration;
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): program with id = %d is being controlled by program\n", __func__, rprog->id);
#endif
        return 0;
    }

    Prog *item = malloc(sizeof *(item));
    if (item == NULL) {
        fprintf(stderr, "%s(): failed to allocate memory\n", __func__);
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;
    item->cycle_duration = cycle_duration;
    if (!initMutex(&item->mutex)) {
        free(item);
        return 0;
    }
    if (!initMutex(&item->value_mutex)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!initClient(&item->sock_fd, WAIT_RESP_TIMEOUT)) {
        freeMutex(&item->value_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!getProgByIdFDB(item->id, item, sensor_list, db_data, db_data_path)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->value_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    item->sensor.peer.fd = &item->sock_fd;
    item->save = 1;
    if (!checkProg(item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->value_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!addProg(item, list)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->value_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!createMThread(&item->thread, &threadFunction, item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->value_mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    return 1;
}

int deleteProgById(int id, ProgList *list, const char* db_path) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            if (prev != NULL) {
                lockMutex(&prev->mutex);
                prev->next = curr->next;
                unlockMutex(&prev->mutex);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            stopProgThread(curr);
            db_saveTableFieldInt("prog", "load", curr->id, 0, NULL, db_data_path);
            freeProg(curr);
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int loadActiveProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData *data = d;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            int id = atoi(argv[i]);
            printf("%s: %d\n", __func__, id);
            addProgById(id, data->prog_list, data->sensor_list, data->db_data, NULL);
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column\n", __func__);
#endif
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveProg(ProgList *list, SensorFTSList *sensor_list, char *db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data = {.prog_list = list, .sensor_list = sensor_list, .db_data = db};
    char *q = "select id from prog where load=1";
    if (!db_exec(db, q, loadActiveProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): query failed\n", __func__);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}