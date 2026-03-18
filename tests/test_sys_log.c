#include "sys_toolkit/sys_log.h"

#include <stdio.h>

int test_sys_log(void)
{
    if (sys_log_init("sys_toolkit_test.log", 4096U) != 0) {
        (void)printf("sys_log_init failed\n");
        return 1;
    }

    sys_log_set_level(SYS_LOG_LEVEL_DEBUG);

    if (sys_log(SYS_LOG_LEVEL_INFO, "hello from sys_log %d", 1) != 0) {
        (void)printf("sys_log(INFO) failed\n");
        sys_log_shutdown();
        return 1;
    }

    if (sys_log(SYS_LOG_LEVEL_DEBUG, "debug message %s", "ok") != 0) {
        (void)printf("sys_log(DEBUG) failed\n");
        sys_log_shutdown();
        return 1;
    }

    sys_log_flush();
    sys_log_shutdown();
    return 0;
}

