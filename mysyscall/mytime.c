//
// Created by fedora on 6/6/22.
//

#include "common.h"
#include "mytime.h"
#include "timing.h"

MAKE_SYS_TEMPLATE(time_t, time, time_t *tloc) {
    if (!check_count_intercepted(CUR_SYSCALL))
        return real_time(tloc);
    increase_time(NULL);
    time_t ret = startup_realtime.tv_sec + curr_monotonic.tv_sec;
    if (tloc) {
        if (real_time(tloc) == -1)
            return -1;
        *tloc = ret;
    }
//    count_concerned(CUR_SYSCALL);
    LOG_INTERCEPTED(CUR_SYSCALL, "ret %ld, time(tloc: %p)", ret, tloc);
    return ret;
}