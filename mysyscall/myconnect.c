//
// Created by tangruize on 22-5-14.
//

#include "common.h"
#include "myconnect.h"
#include "config.h"

#include <errno.h>

MAKE_SYS_TEMPLATE(int, connect, int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int ret = real_connect(sockfd, addr, addrlen);
    BEGIN_INTERCEPT;
    if ((ret != -1 || saved_errno == EINPROGRESS) && check_addr_is_concerned_with_len((const struct sockaddr_in*)addr, addrlen)) {
        // the server addr is concerned, add client sockfd
        add_concerned_fd(sockfd, (const struct sockaddr_in*)addr);
        LOG_INTERCEPTED(CUR_SYSCALL, "concern, return %d, connect(sockfd: %d, addr: " ADDR_FMT ", addrlen: %d)",
                        ret, sockfd, ADDR_TO_STR(addr), addrlen);
    }
    END_INTERCEPT;
}
