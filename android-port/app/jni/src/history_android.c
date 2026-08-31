/*
    Sayri History — Android adaptation
    -----------------------------------
    On Android, the history directory is under the
    app's internal storage. SDL_AndroidGetInternalStoragePath()
    returns the correct path at runtime.
*/

#include "history.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <SDL2/SDL.h>

/*
    Build the full path to the history directory.
    On Android: <internal_storage>/history/
    On desktop: ./history/
*/
static const char *history_dir(void)
{
    static char dir[512] = "";

    if (dir[0])
        return dir;

#ifdef __ANDROID__
    const char *base = SDL_AndroidGetInternalStoragePath();
    if (base && *base)
        snprintf(dir, sizeof(dir), "%s/history", base);
    else
        snprintf(dir, sizeof(dir), "history");
#else
    snprintf(dir, sizeof(dir), "history");
#endif

    /* Ensure the directory exists */
    mkdir(dir, 0755);

    return dir;
}

/* ============================================================
   Encoding
   ============================================================ */

static void encode(const char *in, char *out, size_t out_size)
{
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in;
         *p && o + 4 < out_size; p++) {
        switch (*p) {
        case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
        case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
        case '\t': out[o++] = '\\'; out[o++] = 't';  break;
        default: out[o++] = (char)*p; break;
        }
    }
    out[o] = '\0';
}

static void decode(const char *in, char *out, size_t out_size)
{
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in;
         *p && o + 2 < out_size; p++) {
        if (*p == '\\' && p[1]) {
            switch (p[1]) {
            case 'n': out[o++] = '\n'; break;
            case 't': out[o++] = '\t'; break;
            default:  out[o++] = p[1]; break;
            }
            if (p[1]) p++;
        } else {
            out[o++] = (char)*p;
        }
    }
    out[o] = '\0';
}

/* ============================================================
   API
   ============================================================ */

bool history_save(const char *filename_in, char *filename_out,
                  size_t out_size, const char **roles,
                  const char **texts, int count)
{
    if (count <= 0) return false;

    const char *dir = history_dir();

    char name[64];
    if (filename_in && *filename_in)
        snprintf(name, sizeof(name), "%s", filename_in);
    else
        snprintf(name, sizeof(name), "%010ld.chat", (long)time(NULL));

    if (filename_out && out_size > 0)
        snprintf(filename_out, out_size, "%s", name);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    FILE *f = fopen(path, "w");
    if (!f) return false;

    char enc[HISTORY_TEXT_MAX];
    for (int i = 0; i < count; i++) {
        encode(texts[i], enc, sizeof(enc));
        fprintf(f, "%c\t%s\n",
                roles[i][0] == 'u' ? 'U' : 'A', enc);
    }
    fclose(f);
    return true;
}

int history_list(HistoryList *list)
{
    list->count = 0;
    const char *dir = history_dir();
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) && list->count < HISTORY_MAX_FILES) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 6 || len > 63 || strcmp(name + len - 5, ".chat"))
            continue;
        snprintf(list->names[list->count], sizeof(list->names[0]), "%s", name);
        list->count++;
    }
    closedir(d);

    /* Newest first */
    for (int i = 0; i < list->count - 1; i++) {
        for (int j = i + 1; j < list->count; j++) {
            if (strcmp(list->names[i], list->names[j]) < 0) {
                char tmp[64];
                memcpy(tmp, list->names[i], sizeof(tmp));
                memcpy(list->names[i], list->names[j], sizeof(tmp));
                memcpy(list->names[j], tmp, sizeof(tmp));
            }
        }
    }
    return list->count;
}

int history_load(const char *filename, char texts[][HISTORY_TEXT_MAX],
                 unsigned char *is_user, int max)
{
    const char *dir = history_dir();
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[HISTORY_TEXT_MAX + 8];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len < 2 || line[1] != '\t') continue;
        decode(line + 2, texts[n], HISTORY_TEXT_MAX);
        is_user[n] = (line[0] == 'U') ? 1 : 0;
        n++;
    }
    fclose(f);
    return n;
}
