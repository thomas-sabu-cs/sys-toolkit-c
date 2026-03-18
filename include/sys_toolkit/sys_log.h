/**
 * @file sys_log.h
 * @brief Thread-safe logging with level control and rolling file output.
 *
 * This module is intentionally self-contained and built purely on top of the
 * C standard library and pthreads. It demonstrates how to construct a logging
 * facility that remains simple enough for systems interviews while still
 * expressing real-world concerns such as:
 * - Log level filtering.
 * - Contended multi-threaded writes.
 * - Bounded on-disk footprint via naive log file rotation.
 */

#ifndef SYS_LOG_H
#define SYS_LOG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log verbosity levels.
 *
 * The ordering is intentional: higher numeric values are more verbose.
 */
typedef enum sys_log_level {
    SYS_LOG_LEVEL_ERR = 0,
    SYS_LOG_LEVEL_INFO = 1,
    SYS_LOG_LEVEL_DEBUG = 2
} sys_log_level;

/**
 * @brief Initialize the logging subsystem.
 *
 * The logger writes to both `stdout` and a rolling file at `file_path`.
 * When the file exceeds `max_bytes`, it is truncated and reused. This
 * approach avoids complex rotation policies while still bounding disk usage.
 *
 * The logger is global and process-wide; repeated calls replace the existing
 * configuration.
 *
 * @param file_path Path to the log file on disk.
 * @param max_bytes Maximum size in bytes before truncation occurs. Values
 *                  smaller than a few kilobytes are accepted but not useful.
 * @return 0 on success, non-zero on failure.
 */
int sys_log_init(const char *file_path, size_t max_bytes);

/**
 * @brief Change the active log level.
 *
 * Messages with a level greater than the current threshold are discarded
 * early to minimize formatting work under contention.
 *
 * @param level New minimum level to emit.
 */
void sys_log_set_level(sys_log_level level);

/**
 * @brief Emit a log message.
 *
 * The format string follows `printf` semantics. The call is safe to use
 * concurrently from multiple threads.
 *
 * @param level Message level.
 * @param fmt `printf`-style format string.
 * @return 0 on success, non-zero on internal error.
 */
int sys_log(sys_log_level level, const char *fmt, ...);

/**
 * @brief Flush both stdout and the backing log file.
 */
void sys_log_flush(void);

/**
 * @brief Tear down the logging subsystem and release resources.
 */
void sys_log_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_LOG_H */

