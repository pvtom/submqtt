#include "utils.h"
#include "hfunc.h"

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
int total_count = 0;
mqtt_data *cursor = NULL;

WINDOW *init_window() {
    initscr();            // Initialize ncurses
    start_color();        // Enable color functionality
    use_default_colors();

    if (can_change_color()) {
        init_color(COLOR_WHITE, 1000, 1000, 1000);
        init_color(COLOR_BLACK, 0, 0, 0);
        init_color(COLOR_YELLOW, 700, 700, 0);
    }

    noecho();             // Do not print input characters
    curs_set(FALSE);      // Hide the cursor
    cbreak();             // Disable line buffering
    keypad(stdscr, TRUE);
    set_escdelay(25);   
    halfdelay(1);

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
    strftime(ts, 24, "%Y-%m-%d %H:%M:%S", timeinfo);
    return(tv.tv_sec); 
}

int regex_match(regex_t *preg, char *string) {
    size_t nmatch = 1;
    regmatch_t pmatch[nmatch];
    if (regexec(preg, string, nmatch, pmatch, 0) == REG_NOMATCH) {
        return(0);
    } else {
        return(1);
    }
}

void payload_cleanup(char *payload, int len) {
    int c;
    for (c = 0; c < len; c++) {
        if (!isprint(payload[c])) {
            payload[c] = '?';
        }
    }
    return;
}

mqtt_data *mqtt_data_find(mqtt_data *r, mqtt_data *cur, char *topic) {
    mqtt_data *p = cur;

    if (!cur) return(r);

    while (p != NULL) {
        if (strcmp(topic, p->topic) >= 0) return(p);
        p = p->prev;
    }
    return(r);
}

mqtt_data *mqtt_data_create(char *sub, char *topic, char *payload, int payloadlen, char *timestamp, time_t t, mqtt_data *prev, mqtt_data *next, int cleanup) {
    mqtt_data *p;

    if ((p = malloc(sizeof(mqtt_data))) == NULL) return(NULL);
    p->sub = strdup(sub);
    p->sublen = strlen(p->sub);
    p->topic = strdup(topic);
    p->topiclen = strlen(p->topic);
    p->payload = strndup(payload, payloadlen);
    p->payloadlen = payloadlen;
    p->payloadpos = 0;
    p->changed = true;
    p->outdated = false;
    strcpy(p->timestamp, timestamp);
    p->t = t;
    p->prev = prev;
    p->next = next;
    total_count++;
    return(p);
}

mqtt_data *mqtt_data_merge(mqtt_data *r, mqtt_data *d, char *sub, char *topic, char *payload, int payloadlen, char *timestamp, time_t t, int unsorted, int cleanup, bool trigger_only_payload_update) {
    mqtt_data *p = d;
    mqtt_data *n = NULL;

    while (p != NULL) {
        if (!strcmp(p->topic, topic)) {
            if (!trigger_only_payload_update || (p->payloadlen != payloadlen) || strncmp(p->payload, payload, payloadlen)) {
                if (p->payloadlen != payloadlen) {
                    p->payload = realloc(p->payload, payloadlen + 1);
                    p->payloadlen = payloadlen;
                    bzero(p->payload, payloadlen + 1);
                }
                strncpy(p->payload, payload, payloadlen);
                if (payloadlen < p->payloadpos) p->payloadpos = payloadlen;
                p->changed = true;
                if (p->outdated == true) total_count++;
                p->outdated = false;
                strcpy(p->timestamp, timestamp);
                p->t = t;
            }
            cursor = p;
            return(r);
        }
        if (!unsorted) {
            if (!n && (strcmp(p->topic, topic) > 0)) {
               cursor = n = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, NULL, p, cleanup);
               p->prev = n;
               return(n);
            }
            if (n && (strcmp(p->topic, topic) > 0)) {
               cursor = n->next = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, n, p, cleanup);
               p->prev = n->next;
               return(r);
            }
        }
        n = p;
        p = p->next;
    }
    cursor = n->next = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, n, NULL, cleanup);
    return(r);
}

mqtt_data *mqtt_data_store(mqtt_data *d, char *sub, char *topic, char *payload, int payloadlen, bool unsorted, int cleanup, bool trigger_every_update) {
    mqtt_data *r = d;
    mqtt_data *n = d;
    char timestamp[24];
    time_t t = now(timestamp);

    if ((!payloadlen) || (payloadlen != strlen(payload))) return(n);

    if (cleanup == 1) { 
        payloadlen = trim_my_string(payload);
    } else if (cleanup == 2) {
        payloadlen = trim_my_string(payload);
        payload_cleanup(payload, payloadlen);
    }

    if (!r) cursor = n = mqtt_data_create(sub, topic, payload, payloadlen, timestamp, t, NULL, NULL, cleanup);
    else {
        if (!unsorted) n = mqtt_data_find(r, cursor, topic);
        n = mqtt_data_merge(r, n, sub, topic, payload, payloadlen, timestamp, t, unsorted, cleanup, trigger_every_update);
    }
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
    mqtt_data *next;
    mqtt_data *prev;
    int s = 0;

    while (p != NULL) {
        prev = p->prev;
        if (p->outdated) {
            next = mqtt_data_clean_entry(p);
            if (next) next->prev = prev;
            if (prev && (cursor == p)) cursor = prev;
            else if (next && (cursor == p)) cursor = next;
            if (!s) {
                v = next;
                n = next;
            } else {
                v->next = next;
            }
            p = next;
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
        if ((p->outdated == false) && (rawtime - p->t >= duration)) {
            total_count--;
            p->outdated = true;
            outdated++;
        }
        p = p->next;
    }
    return(outdated);
}

mqtt_data *mqtt_data_search_up(mqtt_data *d, scene_set *scene) {
    mqtt_data *p = d;
    bool found = false;
    int s = strlen(scene->search);

    if (p) {
        if (scene->search_occurence != 0) p = p->next;
        while (p != NULL) {
            if (s && regex_match(scene->search_re, p->topic) && (p->outdated == false)) {
                scene->search_occurence = 1;
                found = true;
                break;
            }
            p = p->next;
        }
    }
    if (found) return(p); 
    else return(d);
}

mqtt_data *mqtt_data_search_down(mqtt_data *d, scene_set *scene) {
    mqtt_data *p = d;
    bool found = false; 
    int s = strlen(scene->search);
    
    if (p) {
        p = p->prev;
        while (p != NULL) {
            if (s && regex_match(scene->search_re, p->topic) && (p->outdated == false)) {
                scene->search_occurence = 1;
                found = true;
                break;
            }
            p = p->prev;
        }
    }
    if (found) return(p); 
    else return(d);
}

int total_data_count() {
    return(total_count);
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

mqtt_data *mqtt_data_pos_plus(mqtt_data *d, int move) {
    mqtt_data *p = d;
    mqtt_data *o = d;
    int i = move + 1; 

    while ((p != NULL) && i) {
        if (p->outdated == false) {
            i--;
            o = p;
        }
        p = p->next;
    }
    return(o);
}

mqtt_data *mqtt_data_pos_minus(mqtt_data *d, int move) {
    mqtt_data *p = d;
    mqtt_data *o = d;
    int i = move + 1;

    while ((p != NULL) && i) {
        if (p->outdated == false) {
            i--;
            o = p;
        } 
        p = p->prev;
    }
    return(o);
}

void mqtt_data_print_table(WINDOW *win, mqtt_data *root, mqtt_data *d, scene_set *scene, bool underline, int lines, int cols) {
    mqtt_data *p = d;
    static int timestamp_width = strlen(HEADER_TIMESTAMP);
    static int topic_width = strlen(HEADER_TOPIC);
    static int match_width = strlen(HEADER_SUB);
    int payload_width = strlen(HEADER_PAYLOAD) + 1;
    static int payload_size = 0;
    int data_count = 0;
    char *buffer = malloc(sizeof(char) * cols + 32);
    char pinfo[17];
    int hlight = A_BOLD;
    int pinfolen, bufferlen;

    time_t rawtime;
    time(&rawtime);

    payload_size = cols - 6 - (scene->show_ts?22:11) - (scene->sub?match_width:1) - topic_width + scene->show_topic_column;
    if (underline) hlight = A_BOLD | A_UNDERLINE;

    // Header
    snprintf(pinfo, 16, "[%d] ", total_count);
    pinfolen = strlen(pinfo);
    if (scene->search_mode) lines--;
    if (scene->sub) {
        snprintf(buffer, cols, "%-*s %-*s %-*s %-*s", scene->show_ts?timestamp_width + 1:0, HEADER_TIMESTAMP, match_width, HEADER_SUB, topic_width - scene->show_topic_column, HEADER_TOPIC, payload_width, HEADER_PAYLOAD);
    } else {
        snprintf(buffer, cols, "%-*s %-*s %-*s", scene->show_ts?timestamp_width + 1:0, HEADER_TIMESTAMP, topic_width - scene->show_topic_column, HEADER_TOPIC, payload_width, HEADER_PAYLOAD);
    }
    bufferlen = strlen(buffer);

    wattron(win, COLOR_PAIR(1) | A_BOLD);

    if ((bufferlen + pinfolen) < cols) {
        mvwprintw(win, 0, 0, " %s%-*s%s", buffer, (int)(cols - 1 - bufferlen - pinfolen), "", pinfo);
    } else {
        mvwprintw(win, 0, 0, " %s%-*s", buffer, (int)(cols - 1 - bufferlen), "");
    }
    wclrtoeol(win);

    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    if (scene->show_payload_column || scene->show_payload_pos_reset) mqtt_data_payload_scroll(root, scene->show_payload_column, payload_size, scene->show_payload_pos_reset);

    // Content
    int i = 0;
    while ((p != NULL) && (i < lines - 1)) {
        if (p->outdated == false) {
            i++;
            data_count++;
            if (timestamp_width < strlen(p->timestamp)) timestamp_width = strlen(p->timestamp);
            if (topic_width < p->topiclen) topic_width = p->topiclen;
            if (match_width < p->sublen) match_width = p->sublen;
            if ((cols > timestamp_width + topic_width + match_width + p->payloadlen) && (payload_width < p->payloadlen)) payload_width = p->payloadlen;
            if (scene->show_topic_column > topic_width) scene->show_topic_column = topic_width;
            if (scene->sub) {
                if (snprintf(buffer, cols, "[%s] %-*s %-*s %s", p->timestamp + (scene->show_ts?0:11), match_width, p->sub, topic_width - scene->show_topic_column, p->topic + max_move_p(p->topic, p->topiclen, scene->show_topic_column, topic_width), p->payload + p->payloadpos) > cols - 4) strcpy(&buffer[cols - 4], "...");
            } else {
                if (snprintf(buffer, cols, "[%s] %-*s %s", p->timestamp + (scene->show_ts?0:11), topic_width - scene->show_topic_column, p->topic + max_move_p(p->topic, p->topiclen, scene->show_topic_column, topic_width), p->payload + p->payloadpos) > cols - 4) strcpy(&buffer[cols - 4], "...");
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
                    if (scene->search_active && regex_match(scene->search_re, p->topic)) wattron(win, COLOR_PAIR(2) | hlight);
                    else wattron(win, A_BOLD);
                }
                mvwprintw(win, i, 0, "%s", buffer);
                if (p->changed) {
                    if (scene->search_active && regex_match(scene->search_re, p->topic)) wattroff(win, COLOR_PAIR(2) | hlight);
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

    free(buffer);
    return;
}
