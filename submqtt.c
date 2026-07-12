#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <termios.h>
#if !defined(__MACH__)
#include <malloc.h>
#endif
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <mosquitto.h>
#include "utils.h"
#include "hfunc.h"
#include "help.h"

#define RELEASE          "v1.1"
#define OUTDATE_BUF      64
#define REFRESH_SEC      0.1
#define SEARCH_VISIBLE   300

typedef struct _mqttattr {
    int qos;
    int mode;
    int tlen;
    char **topics;
    int flen;
    char **filters;
    regex_t *filters_re;
    int plen;
    char **payloads;
    regex_t *payloads_re;
    int tslen;
    char **ts_filters;
    regex_t *ts_filters_re;
    int trlen;
    char **topic_regex;
    regex_t *topic_regex_re;
    int tplen;
    char **topic_replace;
    bool replace;
    bool unsorted;
    bool payload_cleanup;
    int pos;
} mqttattr;

void init_mqttattr(mqttattr *m) {
    m->qos = 0;
    m->tlen = 0;
    m->topics = NULL;
    m->flen = 0;
    m->filters = NULL;
    m->filters_re = NULL;
    m->plen = 0;
    m->payloads = NULL;
    m->payloads_re = NULL;
    m->tslen = 0;
    m->ts_filters = NULL;
    m->ts_filters_re = NULL;
    m->trlen = 0;
    m->topic_regex = NULL;
    m->topic_regex_re = NULL;
    m->tplen = 0;
    m->topic_replace = NULL;
    m->replace = false;
    m->unsorted = false;
    m->payload_cleanup = false;
    m->pos = 0;
    return;
}

WINDOW *win = NULL;

int color = 0;

bool underline = false;

struct mosquitto *mosq;
mqtt_data *mqttd = NULL;
mqttattr mqtta;
scene_set scene;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char *mqtt_tls_password = NULL;
int outdate_duration = 0;
bool go = true;
bool lock_q_key = false;

static void catch_signal(int sig) {
    go = false;
}

static void *keyboard_handler(void *v) {
    mqtt_data *p = NULL;
    int c;
    int pos;
#if !defined(__MACH__)
    pthread_setname_np(pthread_self(), "submqtt/keyboard");
#endif
    keypad(stdscr, TRUE);
    set_escdelay(100);
    noecho();
    while (go) {
      c = getch ();
      if (scene.search_mode && (scene.search_visible > 0) && (c == 10)) {
          pthread_mutex_lock(&mutex);
          scene.search_mode = false;
          scene.search_visible = 0;
          scene.search_occurence++;
          pos = mqtt_data_search(mqttd, &scene);
          if (pos > -1) {
              mqtta.pos = pos;
          } else {
              scene.search_occurence = 0;
          }
          pthread_mutex_unlock(&mutex);
      } else if (scene.search_mode && (scene.search_visible > 0) && isgraph(c) && (strlen(scene.search) < 78)) {
          scene.search[strlen(scene.search)] = c;
          scene.search[strlen(scene.search) + 1] = 0;
          regcomp(scene.search_re, scene.search, REG_EXTENDED|REG_NEWLINE);
          if (strlen(scene.search)) scene.search_active = true; else scene.search_active = false;
          scene.search_visible = SEARCH_VISIBLE;
          scene.search_occurence = 0;
      } else {
          pthread_mutex_lock(&mutex);
          switch (c) {
          case KEY_F(7):  color = init_colors(win, COLOR_UP);
                          break;
          case KEY_F(8):  color = init_colors(win, COLOR_DOWN);
                          break;
          case KEY_F(9):  underline = underline?false:true;
                          break;
          case 27:/*Esc*/ scene.show_topic_column = 0;
                          scene.show_payload_pos_reset = true;
                          if (scene.search_mode) {
                              scene.search_mode = false;
                              scene.search_visible = 0;
                          }
                          scene.search_occurence = 0;
          case ' ':       scene.help_text = false;
                          scene.info = false;
                          break;
          case 'q':       if (!lock_q_key) go = false;
                          break;
          case 'c':       scene.show_topic_column = 0;
                          scene.show_payload_pos_reset = true;
                          scene.help_text = false;
                          scene.info = false;
                          break;
          case 't':       if (scene.show_ts) scene.show_ts = false; else scene.show_ts = true;
                          break;
          case 's':       if (scene.sub) scene.sub = false; else scene.sub = true;
                          break;
          case 'h':
          case '?':       scene.help_text = !scene.help_text;
                          scene.info = false;
                          break;
          case 'i':
          case '!':       scene.info = !scene.info;
                          scene.help_text = false;
                          break;
          case 'x':       scene.heat = !scene.heat;
                          break;
          case KEY_DOWN:  if (mqtta.pos + 1  < mqtt_data_count(mqttd, 0)) mqtta.pos++;
                          break;
          case KEY_UP:    if (mqtta.pos > 0) mqtta.pos--;
                          break;
          case KEY_LEFT:  scene.show_topic_column++;
                          break;
          case KEY_RIGHT: if (scene.show_topic_column > 0) scene.show_topic_column--;
                          break;
          case KEY_F(5):
          case KEY_SLEFT: scene.show_payload_column++;
                          break;
          case KEY_F(6):
          case KEY_SRIGHT: scene.show_payload_column--;
                          break;
          case 'n':       scene.search_occurence++;
                          pos = mqtt_data_search(mqttd, &scene);
                          if (pos > -1) {
                              mqtta.pos = pos;
                          } else {
                              scene.search_occurence = 0;
                          }
                          break;
          case KEY_F(3):
          case KEY_PPAGE: mqtta.pos = mqtta.pos - LINES;
                          if (mqtta.pos < 0) mqtta.pos = 0;
                          break;
          case KEY_F(4):
          case KEY_NPAGE: if (mqtta.pos + LINES < mqtt_data_count(mqttd, 0)) mqtta.pos = mqtta.pos + LINES;
                          break;
          case KEY_F(1):
          case KEY_HOME:  mqtta.pos = 0;
                          scene.search_occurence = 0;
                          break;
          case KEY_F(2):
          case KEY_END:   mqtta.pos = mqtt_data_count(mqttd, 0) - 1;
                          scene.search_occurence = 0;
                          break;
          case '/':
          case ctrl('f'): if (scene.search_mode == false) {
                              scene.search_mode = true;
                              scene.search_visible = SEARCH_VISIBLE;
                          } else {
                              scene.search_mode = false;
                              scene.search_visible = 0;
                          }
                          break;
          case ctrl('d'): if (scene.search_mode) {
                              memset(scene.search, 0, sizeof(scene.search));
                              regcomp(scene.search_re, scene.search, REG_EXTENDED|REG_NEWLINE);
                              scene.search_active = false;
                              scene.search_occurence = 0;
                          }
                          break;
          case KEY_BACKSPACE:
                          if (scene.search_mode && strlen(scene.search)) {
                              scene.search[strlen(scene.search) - 1] = 0;
                              regcomp(scene.search_re, scene.search, REG_EXTENDED|REG_NEWLINE);
                              scene.search_visible = SEARCH_VISIBLE;
                              scene.search_occurence = 0;
                              if (strlen(scene.search)) scene.search_active = true; else scene.search_active = false;
                          }
                          break;
          }
          pthread_mutex_unlock(&mutex);
      }
    }
    return(NULL);
}

int add_topic(mqttattr *mqtta, char *topic) {
    if (mosquitto_sub_topic_check(topic) == MOSQ_ERR_SUCCESS) {
        char **p;
        p = (char**) realloc(mqtta->topics, (mqtta->tlen + 1) * sizeof(char*));
        if (p != NULL) {
            mqtta->topics = p;
            mqtta->topics[mqtta->tlen] = strdup(topic);
            mqtta->tlen++;
            return(mqtta->tlen);
        }
    } else {
        fprintf(stderr, "Warning: '%s' is not a valid topic\n", topic);
    }
    return(0);
}

// Github Copilot
char **add_filter(char ***entry, int *len, char *filter) { 
    char **p = realloc(*entry, (*len + 1) * sizeof(char*));
    if (!p) return *entry; // keep old pointer on failure
    p[*len] = strdup(filter);
    *len = *len + 1;
    *entry = p;
    return *entry;
}

// Github Copilot
void destroy_mqttattr(mqttattr *mqtta) {
    if (mqtta) {
        if (mqtta->topics) {
            for (int i = 0; i < mqtta->tlen; ++i) free(mqtta->topics[i]);
            free(mqtta->topics);
            mqtta->tlen = 0;
        }
        if (mqtta->filters) {
            for (int i = 0; i < mqtta->flen; ++i) {
                regfree(&mqtta->filters_re[i]);
                free(mqtta->filters[i]);
            }
            free(mqtta->filters_re);
            free(mqtta->filters);
            mqtta->flen = 0;
        }
        if (mqtta->payloads) {
            for (int i = 0; i < mqtta->plen; ++i) {
                regfree(&mqtta->payloads_re[i]);
                free(mqtta->payloads[i]);
            }
            free(mqtta->payloads_re);
            free(mqtta->payloads);
            mqtta->plen = 0;
        }
        if (mqtta->ts_filters) {
            for (int i = 0; i < mqtta->tslen; ++i) {
                regfree(&mqtta->ts_filters_re[i]);
                free(mqtta->ts_filters[i]);
            }
            free(mqtta->ts_filters_re);
            free(mqtta->ts_filters);
            mqtta->tslen = 0;
        }
        if (mqtta->topic_regex) {
            for (int i = 0; i < mqtta->trlen; ++i) {
                regfree(&mqtta->topic_regex_re[i]);
                free(mqtta->topic_regex[i]);
            }
            free(mqtta->topic_regex_re);
            free(mqtta->topic_regex);
            mqtta->trlen = 0;
        }
        if (mqtta->topic_replace) {
            for (int i = 0; i < mqtta->tplen; ++i) {
                free(mqtta->topic_replace[i]);
            }
            free(mqtta->topic_replace);
            mqtta->tplen = 0;
        }
    }
    return;
}

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
    mqttattr *mqtta = obj;
    if (!result) {
        mosquitto_subscribe_multiple(mosq, NULL, mqtta->tlen, (char *const *const)mqtta->topics, mqtta->qos, 0, NULL);
    } else {
        pthread_mutex_lock(&mutex);
        mqttd = mqtt_data_store(mqttd, "submqtt", "mqtt_broker/connect/error", (char *)mosquitto_connack_string(result), strlen(mosquitto_connack_string(result)), mqtta->unsorted, mqtta->payload_cleanup);
        pthread_mutex_unlock(&mutex);
    }
    return;
}

int tls_password_callback(char *buf, int size, int rwflag, void *userdata) {
    int len = strlen(mqtt_tls_password);
    if (len >= size) return 0;
    strcpy(buf, mqtt_tls_password);
    return len;
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    if (!mosq || !obj) return;
    bool match = false;
    mqttattr *mqtta = obj;
    mqtt_data *p = NULL;
    static char *topic = NULL;
    static int topic_size = 0;
    int i, j;

    for (i = 0; i < mqtta->flen; i++) {
        if (regex_match(&mqtta->filters_re[i], message->topic)) return;
    }
    if (mqtta->plen) {
        match = false;
        for (i = 0; i < mqtta->plen; i++) {
            if (regex_match(&mqtta->payloads_re[i], (char*)message->payload)) {
                match = true;
                break;
            }
        }
        if (match == false) return;
    }
    if (mqtta->tslen) {
        char timestamp[24];
        now(timestamp);
        match = false;
        for (i = 0; i < mqtta->tslen; i++) {
            if (regex_match(&mqtta->ts_filters_re[i], timestamp)) {
                match = true;
                break;
            }
        }
        if (match == false) return;
    }
    for (i = 0; i < mqtta->tlen; i++) {
        mosquitto_topic_matches_sub(mqtta->topics[i], message->topic, &match);
        if (match) break;
    }
    pthread_mutex_lock(&mutex);
    if (mqtta->replace) {
        hstrcpy(&topic, &topic_size, "%s", message->topic);
        for (j = 0; j < mqtta->trlen; j++) hreplace_regex(&topic, &topic_size, &mqtta->topic_regex_re[j], mqtta->topic_replace[j]);
        mqttd = mqtt_data_store(mqttd, mqtta->topics[i], topic, (char*)message->payload, message->payloadlen, mqtta->unsorted, mqtta->payload_cleanup);
    } else {
        mqttd = mqtt_data_store(mqttd, mqtta->topics[i], message->topic, (char*)message->payload, message->payloadlen, mqtta->unsorted, mqtta->payload_cleanup);
    }
    scene.nr = mqtt_data_count(mqttd, 0);
    scene.from = mqtta->pos + 1;
    scene.to = mqtta->pos + LINES;
    if (scene.to > scene.nr) scene.to = scene.nr;
    p = mqtt_data_position(mqttd, mqtta->pos);
    mqtt_data_print_table(win, mqttd, p, &scene, underline, LINES, COLS);
    pthread_mutex_unlock(&mutex);
    return;
}

int main(int argc, char *argv[], char *envp[]) {
    char key[128], value[128];
    char *mqtt_host = NULL;
    int mqtt_port = 1883;
    char *mqtt_user = NULL;
    char *mqtt_password = NULL;
    bool tls = false;
    char *tls_cafile = NULL;
    char *tls_capath = NULL;
    char *tls_certfile = NULL;
    char *tls_keyfile = NULL;
    char cid[128];
    strcpy(cid, "");
    int rc = 0;
    int outd = 0;
    int i = 0;
    mqtt_data *p = NULL;
    useconds_t usec = REFRESH_SEC * 1e6;

    pthread_t thread_k;

    init_mqttattr(&mqtta);

    scene.heat = false;
    scene.search_mode = false;
    scene.search_visible = 0;
    scene.search_occurence = 0;
    scene.help_text = false;
    scene.info = false;
    strcpy(scene.search, "");
    scene.search_active = false;
    scene.sub = false;
    scene.show_ts = true;
    scene.show_topic_column = 0;
    scene.show_payload_column = 0;
    scene.show_payload_pos_reset = false;

    while(i < argc) {
        if ((!strcmp(argv[i], "-h")) && (i+1 < argc)) mqtt_host = argv[++i];
        if ((!strcmp(argv[i], "-p")) && (i+1 < argc)) mqtt_port = atoi(argv[++i]);
        if ((!strcmp(argv[i], "--client_id")) && (i+1 < argc)) strncpy(cid, argv[++i], 127);
        if ((!strcmp(argv[i], "--user")) && (i+1 < argc)) mqtt_user = argv[++i]; 
        if ((!strcmp(argv[i], "--password")) && (i+1 < argc)) mqtt_password = argv[++i];
        if (!strcmp(argv[i], "--tls")) tls = true;
        if ((!strcmp(argv[i], "--cafile")) && (i+1 < argc)) tls_cafile = argv[++i];
        if ((!strcmp(argv[i], "--capath")) && (i+1 < argc)) tls_capath = argv[++i];
        if ((!strcmp(argv[i], "--certfile")) && (i+1 < argc)) tls_certfile = argv[++i];
        if ((!strcmp(argv[i], "--keyfile")) && (i+1 < argc)) tls_keyfile = argv[++i];
        if ((!strcmp(argv[i], "--tls_password")) && (i+1 < argc)) mqtt_tls_password = argv[++i];
        if ((!strcmp(argv[i], "-q")) && (i+1 < argc)) mqtta.qos = atoi(argv[++i]);
        if (!strcmp(argv[i], "--lock_q")) lock_q_key = true;
        if ((!strcmp(argv[i], "-t") || !strcmp(argv[i], "--topic")) && (i+1 < argc)) add_topic(&mqtta, argv[++i]);
        if ((!strcmp(argv[i], "-f") || !strcmp(argv[i], "--filter")) && (i+1 < argc)) mqtta.filters = add_filter(&mqtta.filters, &mqtta.flen, argv[++i]);
        if ((!strcmp(argv[i], "-s") || !strcmp(argv[i], "--search")) && (i + 1 < argc)) mqtta.topic_regex = add_filter(&mqtta.topic_regex, &mqtta.trlen, argv[++i]);
        if ((!strcmp(argv[i], "-r") || !strcmp(argv[i], "--replace")) && (i + 1 < argc)) mqtta.topic_replace = add_filter(&mqtta.topic_replace, &mqtta.tplen, argv[++i]);
        if ((!strcmp(argv[i], "-l") || !strcmp(argv[i], "--payload")) && (i+1 < argc)) mqtta.payloads = add_filter(&mqtta.payloads, &mqtta.plen, argv[++i]);
        if ((!strcmp(argv[i], "-ts") || !strcmp(argv[i], "--timestamp")) && (i+1 < argc)) mqtta.ts_filters = add_filter(&mqtta.ts_filters, &mqtta.tslen, argv[++i]);
        if (!strcmp(argv[i], "--heat")) scene.heat = true;
        if (!strcmp(argv[i], "--highlight") && (i+1 < argc)) strcpy(scene.search, argv[++i]);
        if (!strcmp(argv[i], "--outdate") && (i+1 < argc)) outdate_duration = atoi(argv[++i]);
        if (!strcmp(argv[i], "--sub")) scene.sub = true;
        if (!strcmp(argv[i], "--unsorted")) mqtta.unsorted = true;
        if (!strcmp(argv[i], "--cleanup")) mqtta.payload_cleanup = true;
        else if ((!strcmp(argv[i], "--color")) && (i+1 < argc)) color = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--white")) color = COLOR_SET_WHITE;
        else if (!strcmp(argv[i], "--blue")) color = COLOR_SET_BLUE;
        else if (!strcmp(argv[i], "--green")) color = COLOR_SET_GREEN;
        else if (!strcmp(argv[i], "--cyan")) color = COLOR_SET_CYAN;
        else if (!strcmp(argv[i], "--magenta")) color = COLOR_SET_MAGENTA;
        else if (!strcmp(argv[i], "--red")) color = COLOR_SET_RED;
        else if (!strcmp(argv[i], "--yellow")) color = COLOR_SET_YELLOW;
        else if (!strcmp(argv[i], "--blue-screen")) color = COLOR_SET_BLUE_CYAN;
        else if (!strcmp(argv[i], "--terminal-white")) color = COLOR_SET_T_WHITE;
        else if (!strcmp(argv[i], "--terminal-cyan")) color = COLOR_SET_T_CYAN;
        else if (!strcmp(argv[i], "--terminal-green")) color = COLOR_SET_T_GREEN;
        else if (!strcmp(argv[i], "--terminal-blue")) color = COLOR_SET_T_BLUE;
        else if (!strcmp(argv[i], "--terminal-red")) color = COLOR_SET_T_RED;
        else if (!strcmp(argv[i], "--terminal-yellow")) color = COLOR_SET_T_YELLOW;
        else if (!strcmp(argv[i], "--terminal-magenta")) color = COLOR_SET_T_MAGENTA;
        else if (!strcmp(argv[i], "--terminal")) color = COLOR_SET_TERMINAL;
        else if (!strcmp(argv[i], "--underline")) underline = true;
        if (!strcmp(argv[i], "--help")) {
            printf("MQTT Subscription Tool %s\n\nusage: submqtt\n", RELEASE);
            printf("\n\tMQTT broker:\n");
            printf("\t\t-h <host> (default: localhost)\n");
            printf("\t\t-p <port> (default: 1883)\n");
            printf("\t\t--client_id <client_id>\n");
            printf("\t\t--user <user>\n");
            printf("\t\t--password <password>\n");
            printf("\t\t-q <0..2> QOS (default: 0)\n");
            printf("\n\tTLS mode:\n");
            printf("\t\t--tls use TLS\n");
            printf("\t\t--cafile <ca.crt>\n");
            printf("\t\t--capath <capath>\n");
            printf("\t\t--certfile <client.crt>\n");
            printf("\t\t--keyfile <client.key>\n");
            printf("\t\t--tls_password <password>\n");
            printf("\n\tFilters: (can occur multiple times and in every combination)\n");
            printf("\t\t-t --topic <topic> will be subscribed\n");
            printf("\t\t-f --filter <regex> matching topics will be filtered out\n");
            printf("\t\t-l --payload <regex> matching payloads will be printed\n");
            printf("\t\t-ts --timestamp <regex> matching timestamps will be printed\n");
            printf("\t\t-s --search <regex> term to search in the topic (needs a corresponding -r value, can occur multiple times)\n");
            printf("\t\t-r --replace <string> which replaces the term in the topic (needs a corresponding -s value, can occur multiple times)\n");
            printf("\n\tOutput:\n");
            printf("\t\t--heat shows changes highlighted\n");
            printf("\t\t--highlight highlight topics by regex\n");
            printf("\t\t--underline underline highlighted topics\n");
            printf("\t\t--outdate <number of seconds> clear buffered data\n");
            printf("\t\t--sub add the matching subscribed topic to the output\n");
            printf("\t\t--unsorted output of the buffered table\n");
            printf("\t\t--cleanup replace non-printable characters in payload\n");
            printf("\n\tColor Sets:\n");
            printf("\t\t--white --blue --cyan --red --green --magenta --yellow --blue-screen\n");
            printf("\t\t--terminal-white --terminal-blue --terminal-cyan --terminal-red\n");
            printf("\t\t--terminal-green --terminal-magenta --terminal-yellow --terminal\n");
            printf("\n\tControl:\n");
            printf("\t\t--lock_q lock 'q' key\n");
            printf("\n\tInteractive Controls:\n");
            printf("\t\tPress the 'h' key for an overview of the interactive commands\n");
            printf("\n%s  ---  %s\n", COPYRIGHT, GITHUB);
            printf("\n");
            return(0);
        }
        i++;
    }

    // Environment
    ENV_STRING_DUP("MQTT_HOST", mqtt_host);
    ENV_INT_RANGE("MQTT_PORT", mqtt_port, 0, 65535);
    ENV_STRING_DUP("MQTT_USER", mqtt_user);
    ENV_STRING_DUP("MQTT_PASSWORD", mqtt_password);
    ENV_STRING_N("MQTT_CLIENT_ID", cid, 127);
    ENV_INT_RANGE("MQTT_QOS", mqtta.qos, 0, 2);
    ENV_BOOL("TLS", tls);
    ENV_STRING_DUP("TLS_CAFILE", tls_cafile);
    ENV_STRING_DUP("TLS_CAPATH", tls_capath);
    ENV_STRING_DUP("TLS_CERTFILE", tls_certfile);
    ENV_STRING_DUP("TLS_KEYFILE", tls_keyfile);
    ENV_STRING_DUP("TLS_PASSWORD", mqtt_tls_password);
    ENV_BOOL("SUB", scene.sub);
    ENV_BOOL("UNSORTED", mqtta.unsorted);
    ENV_BOOL("CLEANUP", mqtta.payload_cleanup);
    ENV_BOOL("HEAT", scene.heat);
    ENV_STRING("HIGHLIGHT", scene.search);
    ENV_INT("OUTDATE", outdate_duration);
    ENV_BOOL("UNDERLINE", underline);
    ENV_BOOL("LOCK_Q", lock_q_key);

    for (i = 0; envp[i] != NULL; i++) {
        if (sscanf(envp[i], "%127[^ \t=]=%127[^\n]", key, value) == 2) {
            if (strcasestr(key, "TOPIC")) {
                add_topic(&mqtta, value);
            } else if (strcasestr(key, "FILTER")) {
                mqtta.filters = add_filter(&mqtta.filters, &mqtta.flen, value);
            } else if (strcasestr(key, "PAYLOAD")) {
                mqtta.payloads = add_filter(&mqtta.payloads, &mqtta.plen, value);
            } else if (strcasestr(key, "TIMESTAMP")) {
                 mqtta.ts_filters = add_filter(&mqtta.ts_filters, &mqtta.tslen, value);
            } else if (strcasestr(key, "SEARCH")) {
                 mqtta.topic_regex = add_filter(&mqtta.topic_regex, &mqtta.trlen, value);
            } else if (strcasestr(key, "REPLACE")) {
                 mqtta.topic_replace = add_filter(&mqtta.topic_replace, &mqtta.tplen, value);
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "white")) {
                color = COLOR_SET_WHITE;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "blue")) {
                color = COLOR_SET_BLUE;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "green")) {
                color = COLOR_SET_GREEN;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "cyan")) {
                color = COLOR_SET_CYAN;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "magenta")) {
                color = COLOR_SET_MAGENTA;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "red")) {
                color = COLOR_SET_RED;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "yellow")) {
                color = COLOR_SET_YELLOW;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "blue-screen")) {
                color = COLOR_SET_BLUE_CYAN;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-white")) {
                color = COLOR_SET_T_WHITE;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-cyan")) {
                color = COLOR_SET_T_CYAN;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-green")) {
                color = COLOR_SET_T_GREEN;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-blue")) {
                color = COLOR_SET_T_BLUE;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-red")) {
                color = COLOR_SET_T_RED;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-yellow")) {
                color = COLOR_SET_T_YELLOW;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal-magenta")) {
                color = COLOR_SET_T_MAGENTA;
            } else if (!strcmp(key, "COLOR") && !strcmp(value, "terminal")) {
                color = COLOR_SET_TERMINAL;
            }
        }
    }

    if (!mqtt_host) mqtt_host = "localhost";
    if (!mqtta.tlen) add_topic(&mqtta, "#");
    if ((mqtta.qos < 0) || (mqtta.qos > 2)) mqtta.qos = 0;

    if (!strcmp(cid, "")) {
        char hname[128];
        time_t sec;
        time(&sec);
        if (gethostname(hname, 127) == 0)
            snprintf(cid, 127, "smqtt-%d-%.100s-%lx", getpid(), hname, sec);
        else
            snprintf(cid, 127, "smqtt-%d-%lx", getpid(), sec);
    } else if (cid[strlen(cid)-1] == '%') {
        char hname[128]; 
        char buffer[128];
        time_t sec;
        time(&sec);
        cid[strlen(cid)-1] = 0;
        strcpy(buffer, cid);
        if (gethostname(hname, 127) == 0)
            snprintf(cid, 127, "%s-%d-%.100s-%lx", buffer, getpid(), hname, sec);
        else 
            snprintf(cid, 127, "%s-%d-%lx", buffer, getpid(), sec);
    }

    if (mqtta.trlen && (mqtta.trlen == mqtta.tplen)) mqtta.replace = true;

    mqtta.filters_re = malloc(mqtta.flen * sizeof(regex_t));
    mqtta.ts_filters_re = malloc(mqtta.tslen * sizeof(regex_t));
    mqtta.payloads_re = malloc(mqtta.plen * sizeof(regex_t));
    mqtta.topic_regex_re = malloc(mqtta.trlen * sizeof(regex_t));
    for (int i = 0; i < mqtta.flen; i++) regcomp(&mqtta.filters_re[i], mqtta.filters[i], REG_EXTENDED|REG_NEWLINE);
    for (int i = 0; i < mqtta.tslen; i++) regcomp(&mqtta.ts_filters_re[i], mqtta.ts_filters[i], REG_EXTENDED|REG_NEWLINE);
    for (int i = 0; i < mqtta.plen; i++) regcomp(&mqtta.payloads_re[i], mqtta.payloads[i], REG_EXTENDED|REG_NEWLINE);
    for (int i = 0; i < mqtta.trlen; i++) regcomp(&mqtta.topic_regex_re[i], mqtta.topic_regex[i], REG_EXTENDED|REG_NEWLINE);

    scene.search_re = malloc(1 * sizeof(regex_t));
    regcomp(scene.search_re, scene.search, REG_EXTENDED|REG_NEWLINE);
    if (strlen(scene.search)) scene.search_active = true; else scene.search_active = false;

    if (signal(SIGINT, catch_signal) == SIG_ERR) {
        printf("error: signal SIGINT couldn't be set.\n");
        exit(0);
    }
    if (signal(SIGTERM, catch_signal) == SIG_ERR) {
        printf("error: signal SIGTERM couldn't be set.\n");
        exit(0);
    }

    mosquitto_lib_init();
    mosq = mosquitto_new(cid, true, &mqtta);

    if (mosq) {
        mosquitto_connect_callback_set(mosq, connect_callback);
        mosquitto_message_callback_set(mosq, message_callback);
        if (tls && mqtt_tls_password) {
            if (mosquitto_tls_set(mosq, tls_cafile, tls_capath, tls_certfile, tls_keyfile, tls_password_callback) != MOSQ_ERR_SUCCESS) {
                printf("Error: Unable to set TLS options.\n");
            }
        } else if (tls) {
            if (mosquitto_tls_set(mosq, tls_cafile, tls_capath, tls_certfile, tls_keyfile, NULL) != MOSQ_ERR_SUCCESS) {
                printf("Error: Unable to set TLS options.\n");
            }
        }
        if (mqtt_user) mosquitto_username_pw_set(mosq, mqtt_user, mqtt_password);
        rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
        if (!rc) {
            win = init_window();
            color = init_colors(win, color);
            pthread_create(&thread_k, NULL, &keyboard_handler, NULL);
            mosquitto_loop_start(mosq);
            while(go) {
                usleep(usec);
                if (mqttd == NULL) {
                    wattron(win, COLOR_PAIR(1) | A_BOLD);
                    mvwprintw(win, 0, 0, " %s%-*s", "Waiting for messages...", (int)(COLS - 1 - strlen("Waiting for messages...")), "");
                    wclrtoeol(win);
                    wattroff(win, COLOR_PAIR(1) | A_BOLD);
                    wrefresh(win);
                    continue;
                }
                pthread_mutex_lock(&mutex);
                if (outdate_duration > 0) {
                    outd += mqtt_data_set_outdated(mqttd, outdate_duration);
                    scene.nr = mqtt_data_count(mqttd, 0);
                    if (mqtta.pos > scene.nr) mqtta.pos = scene.nr - 1;
                }
                if (mqtt_data_set_unchanged(mqttd, UPDATE_DURATION)) {
                    scene.from = mqtta.pos + 1; 
                    scene.to = mqtta.pos + LINES;
                    if (scene.to > scene.nr) scene.to = scene.nr;
                    p = mqtt_data_position(mqttd, mqtta.pos);
                    mqtt_data_print_table(win, mqttd, p, &scene, underline, LINES, COLS);
                    if ((scene.search_visible > 0) && (--scene.search_visible == 0)) {
                        scene.search_mode = false;
                    }
                }   
                if (outd > OUTDATE_BUF) { 
                    mqttd = mqtt_data_clean(mqttd);
                    outd = 0;
                }
                if (scene.search_mode) {
                    wattron(win, COLOR_PAIR(1) | A_BOLD);
                    mvwprintw(win, LINES-1, 0, " Topic >>> %s%-*s", scene.search, (int)(COLS - 1 - strlen(scene.search)), "");
                    wattroff(win, COLOR_PAIR(1) | A_BOLD);
                    wclrtoeol(win);
                }
                if (scene.help_text) {
                    int k = 0;
                    bool l = true;
                    wattron(win, COLOR_PAIR(2) | A_BOLD);
                    mvwprintw(win, 1, 0, " %s", "Help - Interactive Commands");
                    wattroff(win, COLOR_PAIR(2) | A_BOLD);
                    wclrtoeol(win);
                    mvwprintw(win, 2, 0, " %-*s %s", HELP_KEYS, "Key", "Command");
                    wclrtoeol(win);
                    do {
                        mvwprintw(win, 3 + k, 0, " %-*s %s", HELP_KEYS, help[k].keys, help[k].text);
                        wclrtoeol(win);
                        l = strcmp(help[++k].keys, "")?true:false;
                    } while (l);
                    mvwprintw(win, 3 + k, 0, " %s", " ");
                    wclrtoeol(win);
                }
                if (scene.info) {
                    char buffer[HELP_KEYS + HELP_TEXT + 1];
                    int i, k = 0;
                    bool l = true;
                    wattron(win, COLOR_PAIR(2) | A_BOLD);
                    mvwprintw(win, 1, 0, " %s", "Info");
                    wattroff(win, COLOR_PAIR(2) | A_BOLD);
                    wclrtoeol(win);
                    sprintf(buffer, "submqtt %s", RELEASE);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Release", buffer);
                    wclrtoeol(win);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Mqtt: client_id", cid);
                    wclrtoeol(win);
                    for (i = 0; i < mqtta.tlen; i++) {
                        sprintf(buffer, "Mqtt: subscription [%d]", i);
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, buffer, mqtta.topics[i]);
                        wclrtoeol(win);
                    }
                    for (i = 0; i < mqtta.flen; i++) {
                        sprintf(buffer, "Mqtt: topic filter [%d]", i);
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, buffer, mqtta.filters[i]);
                        wclrtoeol(win);
                    }
                    for (i = 0; i < mqtta.plen; i++) {
                        sprintf(buffer, "Mqtt: payload filter [%d]", i);
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, buffer, mqtta.payloads[i]);
                        wclrtoeol(win);
                    }
                    for (i = 0; i < mqtta.tslen; i++) {
                        sprintf(buffer, "Mqtt: timestamp filter [%d]", i);
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, buffer, mqtta.ts_filters[i]);
                        wclrtoeol(win);
                    }
                    for (i = 0; i < mqtta.trlen; i++) {
                        sprintf(buffer, "Mqtt: Topic search [%d]", i);
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, buffer, mqtta.topic_regex[i]);
                        wclrtoeol(win);
                    }
                    for (i = 0; i < mqtta.tplen; i++) {
                        sprintf(buffer, "Mqtt: Topic replace [%d]", i);
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, buffer, mqtta.topic_replace[i]);
                        wclrtoeol(win);
                    }
                    if (mqtta.trlen != mqtta.tplen) {
                        mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Warning: Topic search/replace", "Must have the same number of occurence");
                        wclrtoeol(win);
                    }
                    sprintf(buffer, "%d", color);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Color Set", buffer);
                    wclrtoeol(win);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Ncurses: has_colors", has_colors()?"true":"false");
                    wclrtoeol(win);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Ncurses: can_change_color", can_change_color()?"true":"false");
                    wclrtoeol(win);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Heat mode", scene.heat?"true":"false");
                    wclrtoeol(win);
                    mvwprintw(win, 2 + k++, 0, " %-*s %s", HELP_KEYS, "Search text", scene.search);
                    wclrtoeol(win);
                    mvwprintw(win, 2 + k, 0, "%s", " ");
                    wclrtoeol(win);
                }
                pthread_mutex_unlock(&mutex);
                wrefresh(win);
            }
            pthread_join(thread_k, NULL);
        } else {
            printf("Error: Could not connect to '%s:%d'\n", mqtt_host, mqtt_port);
        }
        mosquitto_disconnect(mosq);
        mosquitto_loop_stop(mosq, false);
        mosquitto_destroy(mosq);
    }
    mosquitto_lib_cleanup();
    if (mqttd) mqtt_data_free(mqttd);

    endwin();
    destroy_mqttattr(&mqtta);
    regfree(scene.search_re);
    free(scene.search_re);

    exit(0);
}
