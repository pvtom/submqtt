#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdint.h>
#include <regex.h>
#include "hfunc.h"

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

int hstrcpy(char **str, int *reserved, const char *fmt, ...) {
    if (str == NULL) return(0);

    va_list ap;

    va_start(ap, fmt);
    int space = 1 + vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if ((*str == NULL) || (*reserved < space)) {
        char *n = realloc(*str, space);
        if (n == NULL) {
            if (*str) free(*str);
            *str = NULL;
            *reserved = 0;
            return(0);
        }
        *str = n;
        *reserved = space;
    }

    va_start(ap, fmt);
    int i = vsnprintf(*str, *reserved, fmt, ap);
    va_end(ap);

    if (i < 0) {
        if (*str) free(*str);
        *str = NULL;
        *reserved = 0;
        return(0);
    }

    return(i);
}

size_t hreplace_regex(char **str, int *reserved, regex_t *preg, const char *replace) {
    regmatch_t pmatch[1];
    regoff_t off, len;
    int i;
    char *p, *t;
    char *s = *str;
    char *seg = *str;

    if ((*str == NULL) || (replace == NULL)) return(0);
    size_t size = strlen(*str);
    size_t rlen = strlen(replace);

    for (i = 0; ; i++) {
        if (regexec(preg, s, ARRAY_SIZE(pmatch), pmatch, 0) == REG_NOMATCH) break;
        off = pmatch[0].rm_so + (s - *str);
        len = pmatch[0].rm_eo - pmatch[0].rm_so;
        if (rlen != len) {
            size = size + (rlen - len);
            if (*reserved < size + 1) {
                t = realloc(seg, size + 1);
                if (t == NULL) {
                    if (*str) free(*str);
                    *str = NULL;
                    return(0);
                }
                *str = seg = s = t;
                *reserved = size + 1;
            }
        }
        p = s + off;
        memmove(p + rlen, p + len, strlen(p + len) + 1);
        memcpy(p, replace, rlen);
        s += pmatch[0].rm_eo;
    }
    return(size);
}
