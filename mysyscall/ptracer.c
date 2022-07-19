//
// Created by tangruize on 22-5-17.
//

#include "common.h"
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

static struct reg_func_t *func_table;

static void init() {
    func_table = reg_func_dict;
    int i, count;
    for (i = 0, count = 0; i < NR_REG_FUNC_DICT; i++)
        if (func_table[i].state & STATE_ENABLED) {
            if (func_table[i].state & STATE_LIBRARY) {
                print_ptracer("Unregister not supported library function \"%s\"\n", func_table[i].name);
                unregister_intercepted(i);
                continue;
            }
            func_table[i].state |= STATE_PTRACE;
            count++;
        }
    print_ptracer("Registered %d syscall interceptions\n", count);
}

int main(int args, char **argv) {
    if (args <= 1) {
        print_ptracer("Usage: %s COMMAND [ARGS]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    init();
    pid_t pid = fork();
    switch (pid) {
        case -1: // error
            print_ptracer("Failed to fork: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        case 0:  // child
            ptrace(PTRACE_TRACEME, 0, 0, 0);
            execvp(argv[1], argv + 1);  // never returns if no error
            print_ptracer("Failed to execvp %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        default: // parent
            break;
    }
    print_ptracer("Ptracer is currently not implemented!\n");
    abort();
}
