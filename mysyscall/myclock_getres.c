//
// Created by fedora on 6/6/22.
//

#include "common.h"
#include "myclock_getres.h"
#include "timing.h"

MAKE_SYS_TEMPLATE(int, clock_getres, clockid_t clockid, struct timespec *res) {
    if (!check_count_intercepted(CUR_SYSCALL))
        return real_clock_getres(clockid, res);
//    count_concerned(CUR_SYSCALL);
    LOG_INTERCEPTED(CUR_SYSCALL, "clock_getres(clockid_t: %d, timespec: %p)", clockid, res);
    *res = resolution;
    return 0;
}