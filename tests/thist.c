#include "history.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    HistoryList list;

    const char *roles[3] = {
        "user", "assistant", "user"
    };

    const char *texts[3] = {
        "line one\nline two with \\ slash\tand tab",
        "reply \"quoted\" and unicode \xC3\xA9\xE2\x82\xAC",
        "third"
    };

    char out_name[64];

    if (!history_save(NULL, out_name,
                      sizeof(out_name),
                      roles, texts, 3)) {
        printf("FAIL save\n");
        return 1;
    }

    printf("saved as %s\n", out_name);

    if (history_list(&list) < 1) {
        printf("FAIL list\n");
        return 1;
    }

    if (strcmp(list.names[0], out_name)) {
        printf("FAIL newest-first (%s vs %s)\n",
               list.names[0], out_name);
        return 1;
    }

    static char texts2[8][HISTORY_TEXT_MAX];
    unsigned char flags[8];

    int n = history_load(out_name, texts2,
                         flags, 8);

    if (n != 3) {
        printf("FAIL load count %d\n", n);
        return 1;
    }

    for (int i = 0; i < n; i++) {

        if (strcmp(texts[i], texts2[i])) {
            printf("FAIL round-trip msg %d:\n"
                   "  in : [%s]\n"
                   "  out: [%s]\n",
                   i, texts[i], texts2[i]);
            return 1;
        }

        int want = (roles[i][0] == 'u');

        if (flags[i] != want) {
            printf("FAIL role %d\n", i);
            return 1;
        }
    }

    printf("PASS all\n");
    return 0;
}
