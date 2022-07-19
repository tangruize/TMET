//
// Created by tangruize on 22-5-10.
//

#ifndef TPROXY_TCPSOCKET_H
#define TPROXY_TCPSOCKET_H

#include "common.h"

struct AcceptData {
    int socket_fd;
    struct sockaddr_in client_addr;
    struct sockaddr_in origin_addr;
    struct sockaddr_in masque_addr;
};

class TcpSocket {
private:
    int socket_fd = -1;
    int bind_port = 1234;
public:
    TcpSocket(int port=-1);
    int socket() const {
        return socket_fd;
    }
    AcceptData accept(int client_fd = -1) const;
};

#endif //TPROXY_TCPSOCKET_H
