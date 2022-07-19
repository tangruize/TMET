//
// Created by fedora on 5/27/22.
//

#include "common.h"
#include "myclock_gettime.h"
#include "timing.h"

#include <bits/time.h>

//MAKE_COUNTER_TEMPLATE(SYS, int, clock_gettime, clockid_t clockid, struct timespec *tp) {
//    CALL(clock_gettime, clockid, tp);
//}

MAKE_SYS_TEMPLATE(int, clock_gettime, clockid_t clockid, struct timespec *tp) {
    if (!check_count_intercepted(CUR_SYSCALL))
        return real_clock_gettime(clockid, tp);
    struct timespec increment;
    increase_time(&increment);
    switch (clockid) {
        case CLOCK_REALTIME:
        case CLOCK_REALTIME_COARSE:
            tp->tv_sec = startup_realtime.tv_sec + curr_monotonic.tv_sec;
            tp->tv_nsec = startup_realtime.tv_nsec + curr_monotonic.tv_nsec;
            break;
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_BOOTTIME:
            tp->tv_sec = curr_monotonic.tv_sec;
            tp->tv_nsec = curr_monotonic.tv_nsec;
            break;
        default:
            print_info("WARN: clock_gettime not implemented clockid: %d\n", clockid);
            assert(0);
    }
//    count_concerned(CUR_SYSCALL);
    LOG_INTERCEPTED(CUR_SYSCALL, "clock_gettime(clockid: %ld, timespec: {tv_sec: %ld, tv_nsec: %ld})", clockid, tp->tv_sec, tp->tv_nsec);
    return 0;
}