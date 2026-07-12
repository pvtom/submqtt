#ifndef _HFUNC_H
#define _HFUNC_H

int hstrcpy(char **str, int *reserved, const char *fmt, ...);
size_t hreplace_regex(char **str, int *reserved, regex_t *preg, const char *replace);

#endif
