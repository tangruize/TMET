//
// Created by tangruize on 22-5-11.
//

#include "SimpleRedirect.h"
#include <unistd.h>
#include <ctime>

SimpleRedirect::SimpleRedirect(const AcceptData &client, int timeout) {
    if (configFile.is_router_addr(client.origin_addr)) {
        cerr_verbose << "Not redirect to router: " << configFile.get_node_name_with_addr(client.client_addr) << endl;
        return;
    }
    cerr_verbose << "Redirect peer " << configFile.get_node_name_with_addr(client.masque_addr)
                 << " -> " << configFile.get_node_name_with_addr(client.client_addr) << endl;

    peer_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_fd == -1) {
        warn_syserror("socket");
        return;
    }

    cerr_verbose_cont << "  - Set receive timeout " << timeout << "s" << endl;
    struct timeval tv{};
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(peer_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
        warn_syserror("setsockopt peer");
        return;
    }
    if (setsockopt(client.socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
        warn_syserror("setsockopt client");
        return;
    }

    cerr_verbose_cont << "  - Connect peer" << endl;
    if (connect(peer_fd, (struct sockaddr *)&client.masque_addr, sizeof(client.masque_addr)) == -1) {
        warn_syserror("connect");
        return;
    }

    cerr_verbose_cont << "  - Redirect client request" << endl;
    char recv_buff[1024];
    ssize_t num_read, num_write;
    num_read = read(client.socket_fd, recv_buff, sizeof(recv_buff)-1);
    if (num_read == -1)
        warn_syserror("read client");
    else if (num_read > 0) {
        recv_buff[num_read] = 0;
        if (write(peer_fd, recv_buff, num_read) == -1)
            warn_syserror("write peer");
        while (num_read > 0 && (recv_buff[num_read-1] == '\n' || recv_buff[num_read-1] == '\r'))
            recv_buff[--num_read] = 0;
        cerr_detail_cont << "  - @@@@ Client Request Content @@@@" << endl
                    << recv_buff << endl
                    << "  - @@@@ End Content @@@@" << endl;
    }

    cerr_verbose_cont << "  - Redirect peer response" << endl;
    cerr_detail_cont << "  - #### Peer Response Content ####" << endl;
    while ((num_read = read(peer_fd, recv_buff, sizeof(recv_buff) - 1)) > 0) {
        recv_buff[num_read] = 0;
        cerr_detail_cont << recv_buff;
        if ((num_write = write(client.socket_fd, recv_buff, num_read)) != num_read)
            break;
    }
    cerr_detail_cont << "  - #### End Content ####" << endl;
    if (num_read == -1)
        warn_syserror("read peer");
    if (num_write == -1)
        warn_syserror("write client");

    cerr_verbose << "Finish redirect" << endl;
}

SimpleRedirect::~SimpleRedirect() {
    if (peer_fd > 0)
        close(peer_fd);
}
