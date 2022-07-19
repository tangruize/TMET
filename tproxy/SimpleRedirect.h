//
// Created by tangruize on 22-5-11.
//

#ifndef TPROXY_SIMPLEREDIRECT_H
#define TPROXY_SIMPLEREDIRECT_H

#include "common.h"
#include "TcpSocket.h"

// Simply connect the peer server, redirect request and redirect response back.
class SimpleRedirect {
private:
    int peer_fd = -1;
    static const int DEFAULT_TIMEOUT = 5;
public:
    SimpleRedirect(const AcceptData &client, int timeout=DEFAULT_TIMEOUT);
    ~SimpleRedirect();
};


#endif //TPROXY_SIMPLEREDIRECT_H
