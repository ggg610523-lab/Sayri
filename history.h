#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#define HISTORY_TEXT_MAX 2048
#define HISTORY_MAX_FILES 32

typedef struct {
    /*
        File names (without directory prefix),
        newest first.
    */
    char names[HISTORY_MAX_FILES][64];
    int count;
} HistoryList;

/*
    Save a session.

    filename_in: existing name to overwrite, or
                 NULL to generate a timestamped
                 one (written to filename_out).

    Returns false on I/O failure.
*/
bool history_save(
    const char *filename_in,
    char *filename_out,
    size_t out_size,
    const char **roles,
    const char **texts,
    int count
);

/*
    List saved sessions, newest first.
*/
int history_list(
    HistoryList *list
);

/*
    Load a session file.

    texts[]  : message contents
    is_user[]: role per message

    Returns message count (<= max).
*/
int history_load(
    const char *filename,
    char texts[][HISTORY_TEXT_MAX],
    unsigned char *is_user,
    int max
);

#endif /* HISTORY_H */
