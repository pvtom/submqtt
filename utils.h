#ifndef __SUBMQTT_UTILS_H
#define __SUBMQTT_UTILS_H

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <regex.h>
#include <sys/select.h>
#include <ncurses.h>

#define TOPIC_SIZE          128
#define PAYLOAD_SIZE        128

#define UPDATE_DURATION     2

#define COLOR_SET_WHITE     0
#define COLOR_SET_CYAN      1
#define COLOR_SET_GREEN     2
#define COLOR_SET_BLUE      3
#define COLOR_SET_RED       4
#define COLOR_SET_YELLOW    5
#define COLOR_SET_MAGENTA   6
#define COLOR_SET_BLUE_CYAN 7
#define COLOR_SET_T_WHITE   8
#define COLOR_SET_T_CYAN    9
#define COLOR_SET_T_GREEN   10
#define COLOR_SET_T_BLUE    11
#define COLOR_SET_T_RED     12
#define COLOR_SET_T_YELLOW  13
#define COLOR_SET_T_MAGENTA 14
#define COLOR_SET_TERMINAL  15

#define COLOR_UP            -1
#define COLOR_DOWN          -2

#define GITHUB              "https://github.com/pvtom/submqtt"
#define COPYRIGHT           "(c) 2026 Thomas Heiny, Wolfsburg, Germany"

struct _cset_t {
    int font_color;
    int font_bg_color;
    int highlighted_color;
    int highlighted_bg_color;
    int foreground;
    int background;
};

typedef struct _cset_t cset_t;

#define ctrl(x)             ((x) & 0x1f)

#define ENV_STRING_DUP(NAME, VAR)                     \
    do {                                              \
        char *env = getenv(NAME);                     \
        if (env) {                                    \
            VAR = env;                                \
        }                                             \
    } while (0)

#define ENV_STRING(NAME, VAR)                         \
    do {                                              \
        const char *env = getenv(NAME);               \
        if (env) {                                    \
            strncpy(VAR, env, sizeof(VAR) - 1);       \
            VAR[sizeof(VAR) - 1] = '\0';              \
        }                                             \
    } while (0)

#define ENV_STRING_N(NAME, VAR, N)                    \
    do {                                              \
        const char *env = getenv(NAME);               \
        if (env) {                                    \
            strncpy(VAR, env, N);                     \
            VAR[N] = '\0';                            \
        }                                             \
    } while (0)

#define ENV_INT(NAME, VAR)                            \
    do {                                              \
        const char *env = getenv(NAME);               \
        if (env) {                                    \
            char *endptr;                             \
            long val = strtol(env, &endptr, 10);      \
            if (*endptr == '\0')                      \
                VAR = (int)val;                       \
        }                                             \
    } while (0)

#define ENV_INT_RANGE(NAME, VAR, FROM, TO)            \
    do {                                              \
        char *env = getenv(NAME);                     \
        if (env) {                                    \
            char *endptr;                             \
            long val = strtol(env, &endptr, 10);      \
            if ((*endptr == '\0') && (val >= FROM) && (val <= TO)) \
                VAR = (int)val;                       \
        }                                             \
    } while (0)

#define ENV_BOOL(NAME, VAR)                                            \
    do {                                                               \
        char *env = getenv(NAME);                                      \
        if (env) {                                                     \
            if (strcasecmp(env, "true") == 0 || strcmp(env, "1") == 0) \
                VAR = true;                                            \
            else                                                       \
                VAR = false;                                           \
        }                                                              \
    } while (0)

struct _mqtt_data {
        char *sub;
        char *topic;
        int topiclen;
        char *payload;
        int payloadlen;
        int payloadpos;
        char timestamp[24];
        time_t t;
        bool changed;
        bool outdated;
        struct _mqtt_data *next;
};

typedef struct _mqtt_data mqtt_data;

struct _scene_set {
        bool sub;
        bool heat;
        bool search_mode;
        int search_visible;
        int search_occurence;
        char search[80];
        bool help_text;
        bool info;
        int nr;
        int from;
        int to;
        bool show_ts;
        int show_topic_column;
        int show_payload_column;
        bool show_payload_pos_reset;
};

typedef struct _scene_set scene_set;

time_t now(char *ts);
WINDOW *init_window();
int init_colors(WINDOW *win, int c);
int regex_match(char *string, char *pattern);
int read_escape_sequence(unsigned char *buf, int maxlen);
mqtt_data *mqtt_data_create(char *sub, char *topic, char *payload, int payloadlen, char *timestamp, time_t t, mqtt_data *next);
mqtt_data *mqtt_data_merge(mqtt_data *d, char *sub, char *topic, char *payload, int payloadlen, char *timestamp, time_t t, int unsorted);
mqtt_data *mqtt_data_store(mqtt_data *d, char *sub, char *topic, char *payload, int payloadlen, int unsorted);
void mqtt_data_free(mqtt_data *d);
mqtt_data *mqtt_data_position(mqtt_data *d, int position);
int mqtt_data_search(mqtt_data *d, scene_set *scene);
void mqtt_data_print_table(WINDOW *win, mqtt_data *root, mqtt_data *d, scene_set *scene, bool underline, int lines, int cols);
mqtt_data *mqtt_data_clean(mqtt_data *d);
int mqtt_data_set_unchanged(mqtt_data *d, int duration);
int mqtt_data_set_outdated(mqtt_data *d, int duration);
int mqtt_data_count(mqtt_data *d, int all);
int max_move_p(char *s, int len, int add, int width);

#endif
