//
// Created by fedora on 5/30/22.
//

#include "common.h"
#include "mygettimeofday.h"
#include "timing.h"

//MAKE_COUNTER_TEMPLATE(SYS, int, gettimeofday, struct timeval *__restrict tv, void *__restrict tz) { CALL(gettimeofday, tv, tz); }

MAKE_SYS_TEMPLATE(int, gettimeofday, struct timeval *tv, void *tz) {
    if (!check_count_intercepted(CUR_SYSCALL))
        return real_gettimeofday(tv, tz);
    if (tz) {
        real_gettimeofday(tv, tz);
    }
    increase_time(NULL);
    tv->tv_sec = startup_realtime.tv_sec + curr_monotonic.tv_sec;
    tv->tv_usec = (startup_realtime.tv_nsec + curr_monotonic.tv_nsec) / US_TO_NS;
//    count_concerned(CUR_SYSCALL);
    LOG_INTERCEPTED(CUR_SYSCALL, "gettimeofday(timeval: {tv_sec: %ld, tv_usec: %ld}, tz: %p)", tv->tv_sec, tv->tv_usec, tz);
    return 0;
}