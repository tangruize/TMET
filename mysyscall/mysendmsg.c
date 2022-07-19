//
// Created by tangruize on 22-5-16.
//

#include "common.h"
#include "mysendmsg.h"

MAKE_COUNTER_TEMPLATE(SYS, ssize_t, sendmsg, int sockfd, const struct msghdr *msg, int flags)
{ CALL(sendmsg, sockfd, msg, flags); }
