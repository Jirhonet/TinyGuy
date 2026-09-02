#include "services/agent_status.h"

#include <glob.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum {
    ROLLOUT_TAIL_BYTES = 65536,
    ACTIVE_FILE_MAX_AGE_SECONDS = 600,
};

static pthread_mutex_t status_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t worker_once = PTHREAD_ONCE_INIT;
static bool agent_running;

static const char *last_occurrence(const char *text, const char *needle)
{
    const char *last = NULL;
    const char *match = text;
    while ((match = strstr(match, needle)) != NULL) {
        last = match;
        match++;
    }
    return last;
}

static bool lifecycle_file_is_active(const char *path, time_t now,
                                     const char *started_marker,
                                     const char *ended_marker,
                                     const char *aborted_marker)
{
    struct stat info;
    if (stat(path, &info) != 0 || now - info.st_mtime > ACTIVE_FILE_MAX_AGE_SECONDS)
        return false;

    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;

    char *tail = malloc(ROLLOUT_TAIL_BYTES + 1);
    if (tail == NULL) {
        fclose(file);
        return false;
    }

    bool active = false;
    long end = info.st_size;
    while (end > 0) {
        long start = end > ROLLOUT_TAIL_BYTES ? end - ROLLOUT_TAIL_BYTES : 0;
        size_t requested = (size_t)(end - start);
        if (fseek(file, start, SEEK_SET) != 0) break;
        size_t length = fread(tail, 1, requested, file);
        tail[length] = '\0';

        const char *started = last_occurrence(tail, started_marker);
        const char *completed = last_occurrence(tail, ended_marker);
        const char *aborted = aborted_marker == NULL
                                  ? NULL
                                  : last_occurrence(tail, aborted_marker);
        const char *latest = started;
        if (completed != NULL && (latest == NULL || completed > latest))
            latest = completed;
        if (aborted != NULL && (latest == NULL || aborted > latest))
            latest = aborted;
        if (latest != NULL) {
            active = latest == started;
            break;
        }

        /* Re-read a small overlap so a marker split across chunks is found. */
        end = start == 0 ? 0 : start + 32;
    }
    fclose(file);
    free(tail);
    return active;
}

static bool glob_has_active_file(const char *pattern, time_t now,
                                 const char *started_marker,
                                 const char *ended_marker,
                                 const char *aborted_marker)
{
    glob_t files;
    if (glob(pattern, 0, NULL, &files) != 0) return false;

    bool active = false;
    for (size_t i = 0; i < files.gl_pathc && !active; i++) {
        active = lifecycle_file_is_active(files.gl_pathv[i], now,
                                          started_marker, ended_marker,
                                          aborted_marker);
    }
    globfree(&files);
    return active;
}

static bool fetch_agent_running(void)
{
    const char *user_home = getenv("HOME");
    if (user_home == NULL) return false;

    const char *codex_root = getenv("CODEX_HOME");
    char fallback[4096];
    if (codex_root == NULL || *codex_root == '\0') {
        snprintf(fallback, sizeof(fallback), "%s/.codex", user_home);
        codex_root = fallback;
    }

    time_t now = time(NULL);
    char pattern[4352];
    snprintf(pattern, sizeof(pattern), "%s/sessions/*/*/*/*.jsonl", codex_root);
    if (glob_has_active_file(pattern, now,
                             "\"type\":\"task_started\"",
                             "\"type\":\"task_complete\"",
                             "\"type\":\"turn_aborted\""))
        return true;

    snprintf(pattern, sizeof(pattern), "%s/.claude/projects/*/*.jsonl",
             user_home);
    if (glob_has_active_file(pattern, now,
                             "\"type\":\"user\"",
                             "\"stop_reason\":\"end_turn\"", NULL))
        return true;

    snprintf(pattern, sizeof(pattern),
             "%s/.cursor/projects/*/agent-transcripts/*/*.jsonl", user_home);
    if (glob_has_active_file(pattern, now,
                             "\"role\":\"user\"",
                             "\"type\":\"turn_ended\"", NULL))
        return true;

    snprintf(pattern, sizeof(pattern),
             "%s/.cursor/projects/*/agent-transcripts/*/subagents/*.jsonl",
             user_home);
    return glob_has_active_file(pattern, now,
                                "\"role\":\"user\"",
                                "\"type\":\"turn_ended\"", NULL);
}

static void *worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        bool active = fetch_agent_running();
        pthread_mutex_lock(&status_lock);
        agent_running = active;
        pthread_mutex_unlock(&status_lock);
        usleep(500000);
    }
    return NULL;
}

static void launch_worker(void)
{
    pthread_t worker;
    if (pthread_create(&worker, NULL, worker_main, NULL) == 0)
        pthread_detach(worker);
}

void agent_status_start(void)
{
    pthread_once(&worker_once, launch_worker);
}

bool agent_is_running(void)
{
    pthread_mutex_lock(&status_lock);
    bool result = agent_running;
    pthread_mutex_unlock(&status_lock);
    return result;
}
