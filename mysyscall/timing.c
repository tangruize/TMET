//
// Created by fedora on 5/31/22.
//

#include "common.h"
#include "timing.h"

#include <pthread.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int resolution_increment = 0;
struct timespec resolution = {.tv_sec = 0, .tv_nsec = US_TO_NS};  // precision is 1 microseconds
struct timespec startup_realtime = {.tv_sec = 1654321000, .tv_nsec = 0}; // 2022/06/04 13:36:40 GMT+8
#define START_MONOTONIC {.tv_sec = 10, .tv_nsec = 0}  // boot 10 seconds
struct timespec prev_monotonic = START_MONOTONIC;
struct timespec curr_monotonic = START_MONOTONIC;

__attribute__((constructor, unused))
static void init_timing() {
#ifdef TIMING_USE_REAL
    void (*func)() = reg_func_dict[SYS_clock_getres].real_func;
    if (func) {
        func(CLOCK_MONOTONIC, &resolution);
    }
    func = reg_func_dict[SYS_clock_gettime].real_func;
    if (func) {
        func(CLOCK_REALTIME, &startup_realtime);
        startup_realtime.tv_sec++;
        startup_realtime.tv_nsec = 0;
    }
#endif
}

static int is_time_stopped() {
    return (prev_monotonic.tv_sec == curr_monotonic.tv_sec && prev_monotonic.tv_nsec == curr_monotonic.tv_nsec);
}

// if time is not advanced by command, increase with one resolution
static void increase_resolution() {
    resolution_increment++;
    curr_monotonic.tv_nsec += resolution.tv_nsec;
    if (curr_monotonic.tv_nsec >= S_TO_NS) {
        curr_monotonic.tv_sec += 1;
        curr_monotonic.tv_nsec -= S_TO_NS;
    }
    curr_monotonic.tv_sec += resolution.tv_sec;
}

static void get_time_increment(struct timespec *increment) {
    if (!increment)
        return;
    if (curr_monotonic.tv_nsec < prev_monotonic.tv_nsec) {
        assert(curr_monotonic.tv_sec > prev_monotonic.tv_sec);
        increment->tv_nsec = curr_monotonic.tv_nsec + S_TO_NS - prev_monotonic.tv_nsec;
        increment->tv_sec = curr_monotonic.tv_sec - 1 - prev_monotonic.tv_sec;
    } else {
        increment->tv_nsec = curr_monotonic.tv_nsec - prev_monotonic.tv_nsec;
        increment->tv_sec = curr_monotonic.tv_sec - prev_monotonic.tv_sec;
    }
}

// arg increment is set for obtaining the increment, not for setting the increment
void increase_time(struct timespec *increment) {
    pthread_mutex_lock(&mutex);
    if (is_time_stopped()) {
        increase_resolution();
        if (increment) {
            *increment = resolution;
        }
    } else {
        get_time_increment(increment);
    }
    prev_monotonic = curr_monotonic;
    pthread_mutex_unlock(&mutex);
}

void set_time_increment(long tv_nsec) {
    pthread_mutex_lock(&mutex);
    long ns = (resolution.tv_sec * S_TO_NS + resolution.tv_nsec) * resolution_increment;
    if (ns >= tv_nsec) {
        print_info("WARN: Time increment (%ld ns) is less than monotonic increment (%ld ns)\n", tv_nsec, ns);
    } else {
        tv_nsec -= ns;
        curr_monotonic.tv_sec += tv_nsec / S_TO_NS;
        curr_monotonic.tv_nsec += tv_nsec % S_TO_NS;
    }
    resolution_increment = 0;
    pthread_mutex_unlock(&mutex);
}