#include "sys_toolkit/sys_log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct sys_log_state {
    FILE *file;
    char *path;
    size_t max_bytes;
    sys_log_level level;
    pthread_mutex_t mutex;
    int initialized;
} sys_log_state;

static sys_log_state g_sys_log = {0};

static void sys_log_rotate_if_needed(void)
{
    if (g_sys_log.file == NULL || g_sys_log.max_bytes == 0U) {
        return;
    }

    long pos = ftell(g_sys_log.file);
    if (pos < 0) {
        return;
    }

    if ((size_t)pos >= g_sys_log.max_bytes) {
        (void)fclose(g_sys_log.file);
        g_sys_log.file = fopen(g_sys_log.path, "w");
    }
}

int sys_log_init(const char *file_path, size_t max_bytes)
{
    if (file_path == NULL || max_bytes == 0U) {
        return -1;
    }

    sys_log_state new_state;
    new_state.file = fopen(file_path, "a");
    if (new_state.file == NULL) {
        return -1;
    }

    new_state.path = (char *)malloc(strlen(file_path) + 1U);
    if (new_state.path == NULL) {
        (void)fclose(new_state.file);
        return -1;
    }

    (void)strcpy(new_state.path, file_path);
    new_state.max_bytes = max_bytes;
    new_state.level = SYS_LOG_LEVEL_INFO;
    new_state.initialized = 1;

    if (pthread_mutex_init(&new_state.mutex, NULL) != 0) {
        free(new_state.path);
        (void)fclose(new_state.file);
        return -1;
    }

    if (g_sys_log.initialized != 0) {
        sys_log_shutdown();
    }

    g_sys_log = new_state;
    return 0;
}

void sys_log_set_level(sys_log_level level)
{
    g_sys_log.level = level;
}

static void sys_log_format_prefix(char *buffer, size_t size, sys_log_level level)
{
    const char *level_str = "ERR";
    if (level == SYS_LOG_LEVEL_INFO) {
        level_str = "INFO";
    } else if (level == SYS_LOG_LEVEL_DEBUG) {
        level_str = "DEBUG";
    }

    time_t now = time(NULL);
    struct tm *tm_ptr = NULL;

#if defined(_POSIX_VERSION)
    {
        static struct tm tm_storage;
        tm_ptr = localtime_r(&now, &tm_storage);
    }
#else
    tm_ptr = localtime(&now);
#endif

    if (tm_ptr == NULL) {
        buffer[0] = '\0';
        return;
    }

    (void)strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_ptr);

    size_t len = strlen(buffer);
    if (len + 4U < size) {
        buffer[len] = ' ';
        buffer[len + 1U] = '[';
        buffer[len + 2U] = level_str[0];
        buffer[len + 3U] = ']';
        buffer[len + 4U] = ' ';
        buffer[len + 5U] = '\0';
    }
}

int sys_log(sys_log_level level, const char *fmt, ...)
{
    if (fmt == NULL) {
        return -1;
    }

    if (!g_sys_log.initialized || level > g_sys_log.level) {
        return 0;
    }

    if (pthread_mutex_lock(&g_sys_log.mutex) != 0) {
        return -1;
    }

    char prefix[64];
    sys_log_format_prefix(prefix, sizeof(prefix), level);

    va_list args;
    va_start(args, fmt);

    (void)fprintf(stdout, "%s", prefix);
    (void)vfprintf(stdout, fmt, args);
    (void)fprintf(stdout, "\n");

    (void)fflush(stdout);

    if (g_sys_log.file != NULL) {
        va_list args_copy;
        va_copy(args_copy, args);

        (void)fprintf(g_sys_log.file, "%s", prefix);
        (void)vfprintf(g_sys_log.file, fmt, args_copy);
        (void)fprintf(g_sys_log.file, "\n");
        (void)fflush(g_sys_log.file);

        va_end(args_copy);
        sys_log_rotate_if_needed();
    }

    va_end(args);

    (void)pthread_mutex_unlock(&g_sys_log.mutex);
    return 0;
}

void sys_log_flush(void)
{
    if (!g_sys_log.initialized) {
        return;
    }

    if (pthread_mutex_lock(&g_sys_log.mutex) != 0) {
        return;
    }

    (void)fflush(stdout);
    if (g_sys_log.file != NULL) {
        (void)fflush(g_sys_log.file);
    }

    (void)pthread_mutex_unlock(&g_sys_log.mutex);
}

void sys_log_shutdown(void)
{
    if (!g_sys_log.initialized) {
        return;
    }

    if (pthread_mutex_lock(&g_sys_log.mutex) != 0) {
        return;
    }

    if (g_sys_log.file != NULL) {
        (void)fclose(g_sys_log.file);
        g_sys_log.file = NULL;
    }

    free(g_sys_log.path);
    g_sys_log.path = NULL;
    g_sys_log.max_bytes = 0U;
    g_sys_log.level = SYS_LOG_LEVEL_INFO;
    g_sys_log.initialized = 0;

    (void)pthread_mutex_unlock(&g_sys_log.mutex);
    (void)pthread_mutex_destroy(&g_sys_log.mutex);
}

