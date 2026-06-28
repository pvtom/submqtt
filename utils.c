#include "utils.h"

#define HEADER_TIMESTAMP "Timestamp"
#define HEADER_SUB       "#"
#define HEADER_TOPIC     "Topic"
#define HEADER_PAYLOAD   "Payload"

cset_t colorset[] = {
    { COLOR_BLACK, COLOR_WHITE, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_BLACK, COLOR_CYAN, COLOR_CYAN, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_BLACK, COLOR_GREEN, COLOR_GREEN, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_WHITE, COLOR_BLUE, COLOR_BLUE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_BLACK, COLOR_YELLOW, COLOR_MAGENTA, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_BLACK, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK },
    { COLOR_BLACK, COLOR_WHITE, COLOR_CYAN, COLOR_BLUE, COLOR_WHITE, COLOR_BLUE },
    { COLOR_BLACK, COLOR_WHITE, -1, -1, -1, -1 },
    { -1, COLOR_CYAN, COLOR_CYAN, -1, -1, -1 },
    { -1, COLOR_GREEN, COLOR_GREEN, -1, -1, -1 },
    { -1, COLOR_BLUE, COLOR_BLUE, -1, -1, -1 },
    { -1, COLOR_RED, COLOR_GREEN, -1, -1, -1 },
    { -1, COLOR_YELLOW, COLOR_MAGENTA, -1, -1, -1 },
    { -1, COLOR_MAGENTA, COLOR_MAGENTA, -1, -1, -1 },
    { -1, -1, -1, -1, -1, -1 }
};

int colorset_size = sizeof(colorset) / sizeof(colorset[0]);

WINDOW *init_window() {
    initscr();            // Initialize ncurses
    start_color();        // Enable color functionality
    use_default_colors();

    if (can_change_color()) {
        init_color(COLOR_BLACK, 0, 0, 0);
        init_color(COLOR_YELLOW, 700, 700, 0);
    }

    noecho();             // Do not print input characters
    curs_set(FALSE);      // Hide the cursor
    cbreak();             // Disable line buffering
    timeout(100);         // Non-blocking input with 100ms timeout

    WINDOW *win = newwin(LINES, COLS, 0, 0); // Create a new window
    werase(win);
    return(win);
}

int init_colors(WINDOW *win, int c) {
    static int color = 0;
    if (c >= 0) color = c;
    if (c == COLOR_UP) {
        color++;
    } else if (c == COLOR_DOWN) {
        color--;
    }
    if (color >= colorset_size) color = 0;
    if (color < 0) color = colorset_size - 1;
    init_pair(1, colorset[color].font_color, colorset[color].font_bg_color);
    init_pair(2, colorset[color].highlighted_color, colorset[color].highlighted_bg_color);
    init_pair(3, colorset[color].foreground, colorset[color].background);
    wbkgd(win, COLOR_PAIR(3));
    return(color);
}

time_t now(char *ts) {
    struct tm *timeinfo;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timeinfo = localtime(&tv.tv_sec);
    sprintf(ts, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return(tv.tv_sec); 
}

int regex_match(char *string, char *pattern) {
    regex_t preg;
    size_t nmatch = 1;
    regmatch_t pmatch[nmatch];
    if (regcomp(&preg, pattern, REG_EXTENDED|REG_NEWLINE)) {
        return(0);
    }
    if (regexec(&preg, string, nmatch, pmatch, 0) == REG_NOMATCH) {
        regfree(&preg);
        return(0);
    } else {
        regfree(&preg);
        return(1);
    }
}

mqtt_data *mqtt_data_create(char *sub, char *topic, char *payload, int payloadlen, char *timestamp, time_t t, mqtt_data *next) {
    mqtt_data *p;

    if ((p = malloc(sizeof(mqtt_data))) == NULL) return(NULL);
    p->sub = strdup(sub);
    p->topic = strdup(topic);
    p->topiclen = strlen(p->topic);
    p->payload = strndup(payload, payloadlen);
    p->payloadlen = payloadlen;
    p->payloadpos = 0;
    p->changed = true;
    p->outdated = false;
    strcpy(p->timestamp, timestamp);
    p->t = t;
    p->next = next;
    return(p);
}

mqtt_data *mqtt_data_merge(mqtt_data *d, char *sub, char *topic, char *payload, int payloadlen, char *timestamp, time_t t, int unsorted) {
    mqtt_data *p = d;
    mqtt_data *n = NULL;
    bool newlen;

    while (p != NULL) {
        if (!strcmp(p->topic, topic)) {
            newlen = (strlen(p->payload) != payloadlen);
            if (newlen || strncmp(p->payload, payload, payloadlen)) {
                if (newlen) {
                    p->payload = realloc(p->payload, payloadlen + 1);
                    p->payloadlen = payloadlen;
                    bzero(p->payload, payloadlen + 1);
                }
                strncpy(p->payload, payload, payloadlen);
                if (payloadlen < p->payloadpos) p->payloadpos = payloadlen;
                p->changed = true;
                p->outdated = false;
                strcpy(p->timestamp, timestamp);
                p->t = t;
            }
            return(d);
        }
        if (!unsorted) {
            if (!n && (strcmp(p->topic, topic) > 0)) {
               n = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, p);
               return(n);
            }
            if (n && (strcmp(p->topic, topic) > 0)) {
               n->next = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, p);
               return(d);
            }
        }
        n = p;
        p = p->next;
    }
    n->next = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, NULL);
    return(d);
}

mqtt_data *mqtt_data_store(mqtt_data *d, char *sub, char *topic, char *payload, int payloadlen, int unsorted) {
    mqtt_data *n = d;
    char timestamp[24];
    time_t t = now(timestamp);
    if (!d) n = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, NULL);
    else n = mqtt_data_merge(d, sub, topic, payload, payloadlen, timestamp, t, unsorted);
    return(n);
}

mqtt_data *mqtt_data_clean_entry(mqtt_data *d) {
    mqtt_data *n = NULL;

    if (d) {
        if (d->sub) free(d->sub);
        if (d->topic) free(d->topic);
        if (d->payload) free(d->payload);
        n = d->next;
        free(d);
    }
    return(n);
}

mqtt_data *mqtt_data_clean(mqtt_data *d) {
    mqtt_data *p = d;
    mqtt_data *v = d;
    mqtt_data *n = d;
    mqtt_data *t;
    int s = 0;

    while (p != NULL) {
        if (p->outdated) {
            t = mqtt_data_clean_entry(p);
            if (!s) {
                v = t;
                n = t;
            } else {
                v->next = t;
            }
            p = t;
        } else {
            s = 1;
            v = p;
            p = p->next;
        }
    }
    return(n);
}

void mqtt_data_free(mqtt_data *d) {
    mqtt_data *p = d;

    while (p != NULL) {
        p = mqtt_data_clean_entry(p);
    }
    return;
}

int mqtt_data_count(mqtt_data *d, int all) {
    mqtt_data *p = d;
    int c = 0;

    while (p != NULL) {
        if (all || (p->outdated == false)) c++;
        p = p->next;
    }
    return(c);
}

mqtt_data *mqtt_data_position(mqtt_data *d, int position) {
    mqtt_data *p = d;
    mqtt_data *pos = NULL;
    int c = 0;

    while ((p != NULL) && (position + 1 > c)) {
        if (p->outdated == false) {
            pos = p;
            c++;
        }
        p = p->next;
    }
    return(pos);
}

int mqtt_data_set_unchanged(mqtt_data *d, int duration) {
    mqtt_data *p = d;
    int changed = 0;
    time_t rawtime;
    time(&rawtime);

    while (p != NULL) {
        if (rawtime - p->t >= duration) {
            p->changed = false;
            changed++;
        }
        p = p->next;
    }
    return(changed);
}

bool mqtt_data_hot(mqtt_data *p, int duration) {
    time_t rawtime;
    time(&rawtime);

    if ((p != NULL) && (rawtime - p->t >= duration)) return(true);
    return(false);
}

int mqtt_data_set_outdated(mqtt_data *d, int duration) {
    mqtt_data *p = d;
    int outdated = 0;
    time_t rawtime;
    time(&rawtime);

    while (p != NULL) {
        if (rawtime - p->t >= duration) {
            p->outdated = true;
            outdated++;
        }
        p = p->next;
    }
    return(outdated);
}

int mqtt_data_search(mqtt_data *d, scene_set *scene) {
    mqtt_data *p;
    int o, i, c, pos;

    for (i = 0; i < 2; i++) {
        p = d;
        o = scene->search_occurence;
        c = 0;
        pos = 0;
        while (p != NULL) {
            if (strlen(scene->search) && regex_match(p->topic, scene->search) && (p->outdated == false)) {
                o--;
                if (!o) {
                    pos = c;
                    i = 2;
                    break;
                }
            }
            c++;
            p = p->next;
        }
        if (!i) scene->search_occurence = 1;
    }
    if (p == NULL) pos = -1;
    return(pos);
}

int max_move_p(char *s, int len, int add, int width) {
    if (len < width - add) return(0);
    if (len >= width - add) return(len - (width - add));
    return(0);
}

int max_scroll_p(int len, int pos, int add, int width) {
    int scroll = 0;
    if (len < width) return(0);
    scroll = pos + add; 
    if (scroll < 0) scroll = 0;
    else if (scroll >= len - width) scroll = len - width;
    return(scroll);
}

void mqtt_data_payload_scroll(mqtt_data *d, int add, int size, bool reset) {
    mqtt_data *p = d;

    while (p != NULL) {
        if (p->outdated == false) {
            if (reset) p->payloadpos = 0;
            else p->payloadpos = max_scroll_p(p->payloadlen, p->payloadpos, add, size);
        }
        p = p->next;
    }
    return;
}

void mqtt_data_print_table(WINDOW *win, mqtt_data *root, mqtt_data *d, scene_set *scene, bool underline, int lines, int cols) {
    mqtt_data *p = d;
    static int timestamp_width = strlen(HEADER_TIMESTAMP);
    static int topic_width = strlen(HEADER_TOPIC);
    static int match_width = strlen(HEADER_SUB);
    int payload_width = strlen(HEADER_PAYLOAD) + 1;
    static int payload_size = 0;
    int data_count = mqtt_data_count(d, 0);
    char *buffer = malloc(sizeof(char) * cols + 32);
    char pinfo[33];
    int hlight = A_BOLD;
    int len;

    time_t rawtime;
    time(&rawtime);

    payload_size = cols - 6 - (scene->show_ts?22:11) - (scene->sub?match_width:1) - topic_width + scene->show_topic_column;
    if (underline) hlight = A_BOLD | A_UNDERLINE;

    // Header
    len = cols;

    snprintf(pinfo, 32, "%d-%d(%d) ", scene->from, scene->to, scene->nr);
    if (scene->search_mode) lines--;
    if (scene->sub) {
        snprintf(buffer, len, "%-*s %-*s %-*s %-*s", scene->show_ts?timestamp_width + 1:0, HEADER_TIMESTAMP, match_width, HEADER_SUB, topic_width - scene->show_topic_column, HEADER_TOPIC, payload_width, HEADER_PAYLOAD);
    } else {
        snprintf(buffer, len, "%-*s %-*s %-*s", scene->show_ts?timestamp_width + 1:0, HEADER_TIMESTAMP, topic_width - scene->show_topic_column, HEADER_TOPIC, payload_width, HEADER_PAYLOAD);
    }

    wattron(win, COLOR_PAIR(1) | A_BOLD);

    if (strlen(buffer) + strlen(pinfo) < cols) {
        mvwprintw(win, 0, 0, " %s%-*s%s", buffer, (int)(cols - 1 - strlen(buffer) - strlen(pinfo)), "", pinfo);
    } else {
        mvwprintw(win, 0, 0, " %s%-*s", buffer, (int)(cols - 1 - strlen(buffer)), "");
    }
    wclrtoeol(win);

    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    if (scene->show_payload_column || scene->show_payload_pos_reset) mqtt_data_payload_scroll(root, scene->show_payload_column, payload_size, scene->show_payload_pos_reset);

    //logdebug("submqtt.log", __FILE__, __LINE__, "header >%s< len >%d pinfo >%s< color %d\n", buffer, len, pinfo, c);

    // Content
    int i = 0;
    while ((p != NULL) && (i < lines - 1)) {
        if (p->outdated == false) {
            i++;
            if (timestamp_width < strlen(p->timestamp)) timestamp_width = strlen(p->timestamp);
            if (topic_width < strlen(p->topic)) topic_width = strlen(p->topic);
            if (match_width < strlen(p->sub)) match_width = strlen(p->sub);
            if ((len > timestamp_width + topic_width + match_width + strlen(p->payload)) && (payload_width < p->payloadlen)) payload_width = p->payloadlen;
            if (scene->show_topic_column > topic_width) scene->show_topic_column = topic_width;
            len = cols;
            if (scene->sub) {
                if (snprintf(buffer, len, "[%s] %-*s %-*s %s", p->timestamp + (scene->show_ts?0:11), match_width, p->sub, topic_width - scene->show_topic_column, p->topic + max_move_p(p->topic, p->topiclen, scene->show_topic_column, topic_width), p->payload + p->payloadpos) > len - 4) strcpy(&buffer[len - 4], "...");
            } else {
                if (snprintf(buffer, len, "[%s] %-*s %s", p->timestamp + (scene->show_ts?0:11), topic_width - scene->show_topic_column, p->topic + max_move_p(p->topic, p->topiclen, scene->show_topic_column, topic_width), p->payload + p->payloadpos) > len - 4) strcpy(&buffer[len - 4], "...");
            }
            if (scene->heat) {
                if (p->changed) {
                    wattron(win, COLOR_PAIR(2) | hlight);
                } else if (rawtime - p->t < UPDATE_DURATION + 1) {
                    wattron(win, hlight);
                }
                mvwprintw(win, i, 0, "%s", buffer);
                if (p->changed) {
                    wattroff(win, COLOR_PAIR(2) | hlight);
                } else if (rawtime - p->t < UPDATE_DURATION + 1) {
                    wattroff(win, hlight);
                }
            } else {
                if (p->changed) {
                    if (strlen(scene->search) && regex_match(p->topic, scene->search)) wattron(win, COLOR_PAIR(2) | hlight);
                    else wattron(win, A_BOLD);
                }
                mvwprintw(win, i, 0, "%s", buffer);
                if (p->changed) {
                    if (strlen(scene->search) && regex_match(p->topic, scene->search)) wattroff(win, COLOR_PAIR(2) | hlight);
                    else wattroff(win, A_BOLD);
                }
            }
            wclrtoeol(win);
        }
        p = p->next;
    }
    scene->show_payload_column = 0;
    if (scene->show_payload_pos_reset) scene->show_payload_pos_reset = false;

    // clean up
    i++;
    if (lines > data_count) {
        while (lines >= data_count++) {
            mvwprintw(win, i++, 0, "%s", "");
            wclrtoeol(win);
        }
    }

    if ((scene->search_visible > 0) && (--scene->search_visible == 0)) {
        scene->search_mode = false;
    }

    free(buffer);
    return;
}
