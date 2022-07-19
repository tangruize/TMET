//
// Created by tangruize on 22-5-15.
//

#include "common.h"
#include "config.h"
#include "mywrite.h"
#include "mysendto.h"

MAKE_SYS_TEMPLATE(ssize_t, write, int fd, const void *buf, size_t count) {
    if (!check_intercept(CUR_SYSCALL))
        return real_write(fd, buf, count);

    if (check_fd_is_concerned(fd)) {  // write() equals send(flags:0, dest_addr:NULL, addrlen:0)
        nr_send_syscall = CUR_SYSCALL;
        return sendto(fd, buf, count, 0, NULL, 0);
    }
    else {
        count_intercepted(CUR_SYSCALL);
        return real_write(fd, buf, count);
    }
}
