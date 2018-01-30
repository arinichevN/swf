
#ifndef SWF_MAIN_H
#define SWF_MAIN_H


#include "lib/dbl.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/gpio.h"
#include "lib/app.h"
#include "lib/configl.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/prog.h"

#define APP_NAME swf
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif
#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define PROG_FIELDS "id, sensor_id, cycle_duration_sec, cycle_duration_nsec, enable, load"

#define WAIT_RESP_TIMEOUT 3

#define MODE_SIZE 3

#define FLOAT_NUM "%.2f"

#define PROG_LIST_LOOP_ST {Prog *item = prog_list.top; while (item != NULL) {
#define PROG_LIST_LOOP_SP item = item->next; } item = prog_list.top;}

struct prog_st {
    int id;
    SensorFTS sensor;
    FTS value;
    Mutex value_mutex;
    int state;
    
    int save;
    int sock_fd;
    struct timespec cycle_duration;
    pthread_t thread;
    Mutex mutex;
    struct prog_st *next;
};

typedef struct prog_st Prog;

DEC_LLIST(Prog)

typedef struct {
    sqlite3 *db_data;
    SensorFTSList *sensor_list;
    Prog *prog;
    ProgList *prog_list;
} ProgData;

enum {
    INIT,
    RUN,
    DISABLE,
    OFF
};

extern int readSettings() ;

extern void initApp() ;

extern int initData() ;

extern void serverRun(int *state, int init_state) ;

extern void progControl(Prog *item) ;

extern void *threadFunction(void *arg) ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;

#endif 

