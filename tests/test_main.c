#include "sys_toolkit/sys_arena.h"
#include "sys_toolkit/sys_log.h"
#include "sys_toolkit/sys_threadpool.h"

#include <stdio.h>

int test_sys_arena(void);
int test_sys_threadpool(void);
int test_sys_log(void);

int main(void)
{
    int failed = 0;

    failed += test_sys_arena();
    failed += test_sys_threadpool();
    failed += test_sys_log();

    if (failed != 0) {
        (void)printf("sys-toolkit-c: %d test group(s) failed\n", failed);
        return 1;
    }

    (void)printf("sys-toolkit-c: all tests passed\n");
    return 0;
}

