#ifndef __MQTTSUB_HELP_H
#define __MQTTSUB_HELP_H

#define HELP_KEYS 30
#define HELP_TEXT 56

struct _help_text {
    char keys[HELP_KEYS];
    char text[HELP_TEXT];
};

typedef struct _help_text help_text;

help_text help[] = {
    { "Esc, 'c'", "Reset shifted or moved payloads" },
    { "Crtl-c, 'q'", "Quit" },
    { "'i', '!'", "Info page" },
    { "'h', '?'", "Help" },
    { "'t'", "Switch timestamp format" },
    { "'s'", "Show / hide matching subscriptions" },
    { "'x'", "Switch heat mode on / off" },
    { "Up / down", "Scroll up / down" },
    { "Mouse Up / down", "Scroll up / down" },
    { "Right / left", "Move payloads right / left" },
    { "F1 / F2 / Home / End", "Jump to the top / end of the list" },
    { "F3 / F4 / Page up / down", "Page up / down" },
    { "F5 / F6 / Shift right / left", "Shift payloads (only if payloads don't fit on the line)" },
    { "F7 / F8", "Switch color schemes" },
    { "F9", "Underline highlighted payloads" },
    { "'/', Crtl-f", "Search mode. Enter topics to be highlighted" },
    { "Crtl-d", "Clear search string" },
    { "'n'", "Jump to the next highlighted topic" },
    { "", "" }
};

#endif
