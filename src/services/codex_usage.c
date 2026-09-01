#include "services/codex_usage.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static pthread_mutex_t snapshot_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t worker_once = PTHREAD_ONCE_INIT;
static codex_usage_snapshot_t snapshot = {
    .status = CODEX_USAGE_CONNECTING,
    .used_percent = 0,
    .window_minutes = 0,
    .resets_at = 0,
};

static const char request[] =
    "{\"method\":\"initialize\",\"id\":0,\"params\":{\"clientInfo\":{"
    "\"name\":\"tiny_guy\",\"title\":\"Tiny Guy\",\"version\":\"0.1.0\"}}}\n"
    "{\"method\":\"initialized\",\"params\":{}}\n"
    "{\"method\":\"account/rateLimits/read\",\"id\":1}\n";

static const char *after_key(const char *json, const char *key)
{
    const char *position = strstr(json, key);
    if (position == NULL) return NULL;
    position = strchr(position + strlen(key), ':');
    if (position == NULL) return NULL;
    do { position++; } while (*position == ' ' || *position == '\t');
    return position;
}

static bool parse_window(const char *json, codex_usage_snapshot_t *result)
{
    const char *primary = after_key(json, "\"primary\"");
    if (primary == NULL || *primary != '{') return false;

    const char *primary_used = after_key(primary, "\"usedPercent\"");
    const char *primary_reset = after_key(primary, "\"resetsAt\"");
    if (primary_used == NULL || primary_reset == NULL) return false;

    char *end = NULL;
    double primary_used_value = strtod(primary_used, &end);
    if (end == primary_used || primary_used_value < 0.0 || primary_used_value > 100.0)
        return false;
    long long primary_reset_value = strtoll(primary_reset, &end, 10);
    if (end == primary_reset || primary_reset_value <= 0) return false;

    result->five_hour_used_percent = (int)(primary_used_value + 0.5);
    result->five_hour_resets_at = (int64_t)primary_reset_value;

    /* The secondary window is the weekly limit shown by Codex plans. Fall
     * back to primary for accounts that expose only one quota window. */
    const char *window = after_key(json, "\"secondary\"");
    if (window == NULL || strncmp(window, "null", 4) == 0)
        window = primary;
    if (*window != '{') return false;

    const char *used = after_key(window, "\"usedPercent\"");
    const char *minutes = after_key(window, "\"windowDurationMins\"");
    const char *reset = after_key(window, "\"resetsAt\"");
    if (used == NULL || minutes == NULL || reset == NULL) return false;

    double used_value = strtod(used, &end);
    if (end == used || used_value < 0.0 || used_value > 100.0) return false;
    long minutes_value = strtol(minutes, &end, 10);
    if (end == minutes || minutes_value <= 0) return false;
    long long reset_value = strtoll(reset, &end, 10);
    if (end == reset || reset_value <= 0) return false;

    result->status = CODEX_USAGE_READY;
    result->used_percent = (int)(used_value + 0.5);
    result->window_minutes = (int)minutes_value;
    result->resets_at = (int64_t)reset_value;
    return true;
}

static bool fetch_usage(codex_usage_snapshot_t *result)
{
    int input_pipe[2];
    int output_pipe[2];
    if (pipe(input_pipe) != 0) return false;
    if (pipe(output_pipe) != 0) {
        close(input_pipe[0]); close(input_pipe[1]);
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, input_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, input_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, output_pipe[0]);

    pid_t child = 0;
    char *argv[] = { "codex", "app-server", NULL };
    int spawn_error = posix_spawnp(&child, "codex", &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(input_pipe[0]);
    close(output_pipe[1]);
    if (spawn_error != 0) {
        close(input_pipe[1]); close(output_pipe[0]);
        return false;
    }

    size_t sent = 0;
    while (sent < sizeof(request) - 1) {
        ssize_t count = write(input_pipe[1], request + sent,
                              sizeof(request) - 1 - sent);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        sent += (size_t)count;
    }
    char response[32768];
    size_t length = 0;
    bool parsed = false;
    const time_t deadline = time(NULL) + 10;
    while (length < sizeof(response) - 1 && time(NULL) < deadline) {
        struct pollfd descriptor = { .fd = output_pipe[0], .events = POLLIN };
        int ready = poll(&descriptor, 1, 500);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        if (ready == 0) continue;
        ssize_t count = read(output_pipe[0], response + length,
                             sizeof(response) - 1 - length);
        if (count <= 0) break;
        length += (size_t)count;
        response[length] = '\0';
        if (strstr(response, "\"id\":1") != NULL && parse_window(response, result)) {
            parsed = true;
            break;
        }
    }
    close(input_pipe[1]);
    close(output_pipe[0]);
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);
    return parsed;
}

static void *worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        codex_usage_snapshot_t next = { .status = CODEX_USAGE_UNAVAILABLE };
        fetch_usage(&next);
        pthread_mutex_lock(&snapshot_lock);
        snapshot = next;
        pthread_mutex_unlock(&snapshot_lock);
        sleep(next.status == CODEX_USAGE_READY ? 60 : 15);
    }
    return NULL;
}

static void launch_worker(void)
{
    pthread_t worker;
    if (pthread_create(&worker, NULL, worker_main, NULL) == 0)
        pthread_detach(worker);
    else
        snapshot.status = CODEX_USAGE_UNAVAILABLE;
}

void codex_usage_start(void)
{
    pthread_once(&worker_once, launch_worker);
}

codex_usage_snapshot_t codex_usage_get(void)
{
    pthread_mutex_lock(&snapshot_lock);
    codex_usage_snapshot_t result = snapshot;
    pthread_mutex_unlock(&snapshot_lock);
    return result;
}
