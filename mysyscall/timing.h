//
// Created by fedora on 5/31/22.
//

#ifndef MYSYSCALL_TIMING_H
#define MYSYSCALL_TIMING_H

#include <bits/types/struct_timespec.h>
#include <bits/time.h>

#define S_TO_NS  1000000000
#define US_TO_NS 1000

extern int resolution_increment;
extern struct timespec resolution;
extern struct timespec startup_realtime;
extern struct timespec prev_monotonic;
extern struct timespec curr_monotonic;

void set_time_increment(long tv_nsec);
void increase_time(struct timespec *increment);

#endif //MYSYSCALL_TIMING_H
