//
// Created by tangruize on 22-5-17.
//

#include "TcpNetwork.h"

#include <unistd.h>
#include <thread>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <netinet/tcp.h>

void TcpNetwork::run_epoll() {
    struct epoll_event events[max_events];
    int nfds, listen_fd = tcp->socket();
    pair<int, int> conn_fds;
    while (true) {
        nfds = epoll_wait(epoll_fd, events, max_events, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                cerr_detail << "epoll_wait is interrupted by signal" << endl;
                continue;
            }
            else
                throw_syserror("epoll_wait");
        }
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_fd) {
                conn_fds = do_accept_connect();
                if (conn_fds.first == -1 || conn_fds.second == -1)
                    continue;
                add_monitor_fd(conn_fds.first);
                add_monitor_fd(conn_fds.second);
            } else {
                auto it = fd_to_client.find(events[n].data.fd);
                if (it != fd_to_client.end()) {
                    // router <-> client connection
                    if (!check_connection_is_active(it->first)) {
                        // close
                        cerr_verbose << "Close client fd: " << it->first << " "
                                     << configFile.get_node_name_with_addr(it->second) << " <-> "
                                     << configFile.get_node_name_with_addr(configFile.router_addr) << endl;
                        close(it->first);
                        client_to_fd.erase(it->second);
                        fd_to_client.erase(it->first);
                    }
                } else {
                    // client <-> client connection
                    do_receive(events[n].data.fd);
                }
            }
        }
    }
}

TcpNetwork::TcpNetwork(TcpSocket *tcp_socket) : tcp(tcp_socket) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        throw_syserror("epoll_create1");
    add_monitor_fd(tcp_socket->socket());
    is_direct = configFile.get_strategy() == STRATEGY_DIRECT;
}

void TcpNetwork::set_recv_timeout(int fd) {
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) == -1)
        warn_syserror("set_recv_timeout setsockopt fd: " + to_string(fd));
}

void TcpNetwork::set_conn_retries(int fd) {
    if (setsockopt(fd, IPPROTO_TCP, TCP_SYNCNT, &syn_retries, sizeof(syn_retries)) == -1)
        warn_syserror("set_conn_retries setsockopt fd: " + to_string(fd));
}

int TcpNetwork::connect_peer(const AcceptData &acc) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        warn_syserror("connect_peer socket");
        return -1;
    }
    const int opt = 1;
    // to bind to any addr
    if (setsockopt(fd, SOL_IP, IP_TRANSPARENT, &opt, sizeof(opt)) == -1) {
        warn_syserror("connect_peer setsockopt");
        return -1;
    }
    // to bind to origin addr with a random port
    struct sockaddr_in origin = acc.origin_addr;
    origin.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&origin, sizeof(origin)) == -1) {
        warn_syserror("connect_peer setsockopt");
        return -1;
    }
    set_conn_retries(fd);
    if (connect(fd, (struct sockaddr *)&acc.masque_addr, sizeof(acc.masque_addr)) == -1) {
        warn_syserror("connect_peer connect");
        close(fd);
        return -1;
    }
    cerr_verbose << "Connected peer fd: " << fd << " " << channel_to_string({acc.masque_addr, acc.client_addr}) << endl;
    return fd;
}

void TcpNetwork::add_fd_to_channel(int src_fd, int dst_fd, struct sockaddr_in src, struct sockaddr_in dst) {
    src.sin_port = 0;  // set port  to 0, the channel may have several connections
    dst.sin_port = 0;
    shared_ptr<int> src_fd_ptr(new int{src_fd}), dst_fd_ptr(new int{dst_fd});
    fd_to_channel_status[src_fd] = { .channel = {src, dst}, .self_fd = src_fd_ptr, .fd = dst_fd_ptr, .disconnected = false};
    fd_to_channel_status[dst_fd] = { .channel = {dst, src}, .self_fd = dst_fd_ptr, .fd = src_fd_ptr, .disconnected = false};
}

pair<int, int> TcpNetwork::do_accept_connect() {
    AcceptData acc = tcp->accept();
    if (configFile.is_router_addr(acc.origin_addr)) {
        cerr_verbose << "Client " << configFile.get_node_name_with_addr(acc.client_addr)
                     << " cmd fd: " << acc.socket_fd << endl;
        acc.client_addr.sin_port = 0;
        client_to_fd[acc.client_addr] = acc.socket_fd;
        fd_to_client[acc.socket_fd] = acc.client_addr;
        add_monitor_fd(acc.socket_fd);
        return {-1, -1};
    }
    int peer_fd = connect_peer(acc);
    if (peer_fd == -1) {
        close(acc.socket_fd);
        return {-1, -1};
    }
    set_recv_timeout(acc.socket_fd);
    set_recv_timeout(peer_fd);
    add_fd_to_channel(acc.socket_fd, peer_fd, acc.client_addr, acc.masque_addr);
    return {acc.socket_fd, peer_fd};
}

void TcpNetwork::add_monitor_fd(int fd) const {
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;  // edge triggered
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
        throw_syserror("epoll_ctl");
}

void TcpNetwork::do_receive(int fd) {
    MsgHeader header{};
    ssize_t size;
    auto cf = fd_to_channel_status.find(fd);
    if (cf == fd_to_channel_status.end()) {
        cerr_warning << "do_receive cannot find channel of fd: " << fd << endl;
        close_connection(fd);
        return;
    }
    bool is_unintercepted = unintercepted_fd.count(fd);
    if (!unintercepted_fd.count(fd)) {
        while ((size = recv(fd, &header, sizeof(header), MSG_DONTWAIT | MSG_PEEK)) == sizeof(header)) {
            if (!packet_validation(header, size)) {
                // validation failed, tag as not intercepted
                cerr_warning << "do_receive recv validation failed of fd: " << fd << " size: " << size << endl;
                set_unintercepted(fd, cf->second.channel);
                is_unintercepted = true;
                break;
            }
            Msg msg(cf->second.fd, header);
            // header is still in recv queue because we use MSG_PEEK flag
            size = recv(fd, msg.buffer(), sizeof(header) + msg.size, MSG_WAITALL);
            if (size == -1) {
                if (errno != EAGAIN) {
                    warn_syserror("do_receive recv content fd: " + to_string(fd));
                } else {  // connection is closed
                    // Question? close connection immediately?
                    close_connection(fd);
                }
                return;
            } else if ((size_t) size != sizeof(header) + msg.size) {
                cerr_warning << "do_receive recv incomplete content of fd: " << fd
                             << " size: " << size - sizeof(header) << " expected size: " << msg.size << endl;
                // Question? In what condition partial read can happen except EOF?
                // Question? close connection if partial read?
                close_connection(fd);
                return;
            }
            // ok recv 1 msg
            enqueue_msg(cf->second, msg);
        }
        if (size == -1) {
            if (errno != EAGAIN) {  // EAGAIN: no more data to recv
                warn_syserror("do_receive recv header fd: " + to_string(fd));
                close_connection(fd);
            }
        } else if (size == 0) {
            // EOF
            auto it = fd_to_channel_status.find(fd);
            if (it != fd_to_channel_status.end()) {
                cerr_verbose << "Connection read end closed by peer " << it->second.channel << endl;
            } else {
                cerr_verbose << "Connection read end closed by peer fd: " << fd << endl;
            }
            close_connection(fd);
        } else if (!is_unintercepted) {
            cerr_warning << "do_receive recv incomplete header of fd: " << fd << " size: " << size << endl;
            if (!packet_validation(header, size)) {
                set_unintercepted(fd, cf->second.channel);
                is_unintercepted = true;
            } else {
                cerr_warning << "do_receive validation OK?" << endl;
            }
        }
    }
    if (is_unintercepted) {
        transfer_unintercepted(*cf->second.fd, fd);
    }
}

void TcpNetwork::enqueue_msg(const channel_status_t &cf, Msg &m) {
    if (cf.disconnected) {
        cerr_verbose << "Drop disconnected msg " << cf.channel << ": size: " << m.size << endl;
        return;
    }
    if (is_direct) {
        // not to enqueue, deliver direct
        deliver_msg(cf.channel, &m);
        return;
    }
    cerr_detail << "Enqueue " << cf.channel << " size: " << m.size << endl;
    ChannelMsgs &cm = network[cf.channel];
    cm.msgs.enqueue(std::move(m));
}

void TcpNetwork::deliver_msg(const channel_t &channel, Msg *m) {
    // get msg to deliver
    Msg msg(m);
    if (!m) {
        auto it = network.find(channel);
//        assert(it != network.end());
        if (it == network.end()) {
            cerr_warning << "deliver_msg cannot find channel " << channel << endl;
            return;
        }
        int drop_count = -1;
        do {
            bool ok = it->second.msgs.wait_dequeue_timed(msg, deliver_timout);
            drop_count++;
            if (!ok) {
                cerr_warning << "deliver_msg wait_dequeue_timed timeout ";
                if (drop_count) {
                    cerr_warning_cont << "(dropped " << drop_count << " closed msgs) ";
                }
                cerr_warning_cont << channel << endl;
                return;
            }
        } while (*msg.fd < 0);
        if (drop_count) {
            cerr_verbose << "Drop " << drop_count << " closed connection msgs: " << channel << endl;
        }
    }
    // check channel is connected or not
    auto fd_it = fd_to_channel_status.find(*msg.fd);
    if (fd_it == fd_to_channel_status.end()) {
        cerr_warning << "deliver_msg cannot find channel for fd (msg dropped): " << msg.fd << endl;
        return;
    } else if (fd_it->second.disconnected) {
        cerr_warning << "deliver_msg channel is disconnected (msg dropped): " << channel << endl;
        return;
    }
    // do deliver
    ssize_t left_size = ssize_t(msg.size), size;
    while (left_size > 0 && (size = write(*msg.fd, msg.body(), left_size)) != -1)
        left_size -= size;
    if (size == -1) {
        warn_syserror("deliver_msg write " + channel_to_string(channel));
        close_connection(*msg.fd, !is_direct);  // close connection on write error
        return;
    }
    cerr_detail << "Deliver msg " << channel << " size: " << msg.size << endl;
}

string TcpNetwork::channel_to_string(const channel_t &c) {
    string src = configFile.get_node_name_with_addr(c.first);
    string dst = configFile.get_node_name_with_addr(c.second);
    return src + " -> " + dst;
}

void TcpNetwork::close_connection(int fd, bool tag, bool close_peer) {
    string verbose_str = tag ? "Tag disconnected " : "Close connection ";
    auto it = fd_to_channel_status.find(fd), it2 = it;
    if (it != fd_to_channel_status.end()) {
        cerr_verbose << verbose_str << "fd: " << fd << " " << it->second.channel << endl;
        it->second.disconnected = true;
//        clear_msgs(it->second.channel);
        it2 = fd_to_channel_status.find(*it->second.fd);
        if (it2 != fd_to_channel_status.end()) {
            if (!close_peer)
                verbose_str = "Tag disconnected ";
            cerr_verbose << verbose_str << "(reverse) " << "fd: " << it2->first << " " << it2->second.channel << endl;
            it2->second.disconnected = true;
//            clear_msgs(it2->second.channel);
        }
        if (!tag) {
            do_close(it);
            if (close_peer)
                do_close(it2);
        }
    } else {
        if (!tag) {
            cerr_verbose << "Close connection fd: " << fd << endl;
            close(fd);
        }
    }
}

void TcpNetwork::do_close(map<int, channel_status_t>::iterator it) {
    if (it == fd_to_channel_status.end()) {
        return;
    }
    auto ui_it = unintercepted_fd.find((it->first));
    if (ui_it != unintercepted_fd.end())
        unintercepted_fd.erase(ui_it);
    close(it->first);
    *it->second.self_fd = -1;
    fd_to_channel_status.erase(it);
}

void TcpNetwork::set_nonblocking(int fd) const {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        warn_syserror("set_nonblocking fcntl F_GETFL");
        return;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
        warn_syserror("set_nonblocking fcntl F_SETFL");
}

bool TcpNetwork::packet_validation(const MsgHeader &header, unsigned size) const {
    unsigned mask = -1;
    if (size < 4)
        mask >>= (4 - size) * 8;
    return (ntohl(header.validation) & mask) == (VALIDATION_DATA & mask);
}

void TcpNetwork::set_unintercepted(int fd, const channel_t &channel) {
    if (!unintercepted_fd.count(fd)) {
        cerr_verbose << "Unintercept fd: " << fd << " " << channel << endl;
        unintercepted_fd.insert(fd);
//        set_nonblocking(fd);
    }
}

void TcpNetwork::transfer_unintercepted(int out_fd, int in_fd) {
    uint8_t buffer[1460];  // MSS: 1500(MTU)-20(IP)-20(TCP)
    ssize_t size, totol = 0;
    while ((size = recv(in_fd, buffer, sizeof(buffer), MSG_DONTWAIT)) > 0) {
        if (send(out_fd, buffer, size, MSG_DONTWAIT) == -1) {
            warn_syserror("transfer_unintercepted send");
//            if (errno == EAGAIN)
//                break;
            close_connection(in_fd);
            break;
        }
        totol += size;
    }
    if (FLAGS_detail && totol) {
        auto it = fd_to_channel_status.find(in_fd);
        if (it != fd_to_channel_status.end()) {
            cerr_detail << "Transfer unintercepted " << it->second.channel << " size: " << totol << endl;
        }
        else {
            cerr_detail << "Transfer unintercepted in_fd: " << in_fd << " -> out_fd: " << out_fd
                        << " size: " << totol << endl;
        }
    }
    if (size == -1 && errno != EAGAIN) {
        warn_syserror("transfer_unintercepted recv");
        close_connection(in_fd);
    } else if (size == 0) {  // EOF
        auto it = fd_to_channel_status.find(in_fd);
        if (it != fd_to_channel_status.end()) {
            cerr_verbose << "Unintercepted connection read end closed by peer " << it->second.channel << endl;
        } else {
            cerr_verbose << "Unintercepted connection read end closed by peer in_fd: "
                         << in_fd << " -> out_fd: " << out_fd << endl;
        }
        close_connection(in_fd);
    }
}

void TcpNetwork::run_read_cmd() {
    cmd_t c;
#define CHECK_CONTINUE_MORE(argc, m) if (!c.check_prompt_invalid(argc, "", m)) continue; else cerr_detail << "Read cmd: " << c << endl
#define CHECK_CONTINUE(argc) CHECK_CONTINUE_MORE(argc, false)
    while (true) {
        if (!command.dequeue(c))
            break;
        if (c.get_cmd() == "deliver") {
            CHECK_CONTINUE(3);
            channel_t channel{};
            channel.first = configFile.get_node_addr(c.get_arg(1));
            channel.second = configFile.get_node_addr(c.get_arg(2));
            if (!channel.ok()) {
                c.prompt_invalid("cannot find a valid channel");
                continue;
            }
            deliver_msg(channel);
        } else if (c.get_cmd() == "status") {
            CHECK_CONTINUE(1);
            print_status();
        } else if (c.get_cmd() == "partition") {
            CHECK_CONTINUE(2);
            partition(c.get_arg(1));
        } else if (c.get_cmd() == "recover") {
            CHECK_CONTINUE(2);
            recover(c.get_arg(1));
        } else if (c.get_cmd() == "cmd") {
            CHECK_CONTINUE_MORE(3, true);
            send_cmd(c.get_arg(1), c.get_args_from(2));
        }
        else {
            cerr_warning << "Unknown cmd: " << c << endl;
        }
    }
//    for (auto it: fd_to_channel_status)
//        close(it.first);
}

void TcpNetwork::print_status() {
    bool showed = false;
    for (auto &i: client_to_fd) {
        cerr << "FD: " << i.second << ", " << configFile.get_node_name_with_addr(i.first)
             << " <-> " << configFile.get_node_name_with_addr(configFile.router_addr) << endl;
        showed = true;
    }
    for (auto &i: fd_to_channel_status) {
        cerr << "FD: " << i.first << ", " << i.second.channel << ", "
             << (i.second.disconnected ? "disconnected" : "connnected")
             << ", msgs: " << network[i.second.channel].msgs.size_approx() << endl;
        showed = true;
    }
    if (!showed)
        cerr << "(empty network buffer)" << endl;
}

void TcpNetwork::clear_msgs(const channel_t &channel) {
    auto it = network.find(channel);
    if (it != network.end()) {
        int count = 0;
        for (; it->second.msgs.pop(); count++);
        if (count) {
            cerr_detail << "Clear " << count << " msgs " << channel << endl;
        }
    }
}

void TcpNetwork::partition(const string &node, bool clear_msg, bool is_recover) {
    struct sockaddr_in addr = configFile.get_node_addr(node);
    if (addr.sin_port == (in_port_t)-1) {
        if (is_recover)
            cerr_warning << "recover ";
        else
            cerr_warning << "partition ";
        cerr_warning_cont << "cannot find node: \"" << node << "\"" << endl;
        return;
    }
    cmp_addr_equal cmp;
    vector<int> fd_to_close;
    bool matched = false;
    for (auto & it : fd_to_channel_status) {
        if (cmp(it.second.channel.first, addr)) {
            matched = true;
            if (is_recover) {
                it.second.disconnected = false;
                // close fd will delete it from fd_to_channel_status, which corrupt the loop, close it afterwards
                fd_to_close.push_back(it.first);
            }
            else
                close_connection(it.first, true);
            if (clear_msg)
                clear_msgs(it.second.channel);
        }
    }
    for (auto i: fd_to_close) {
        close_connection(i, false);
    }
    if (!matched) {
        cerr_warning << "No matched channel for node: " << configFile.get_node_name_with_addr(addr) << endl;
    }
}

void TcpNetwork::recover(const string &node) {
    partition(node, true, true);
}

void TcpNetwork::send_cmd(const string &node, const string &cmd) {
    struct sockaddr_in addr = configFile.get_node_addr(node);
    if (addr.sin_port == in_port_t(-1)) {
        cerr_warning << "send_cmd cannot find node: \"" << node << "\"" << endl;
        return;
    }
    auto it = client_to_fd.find(addr);
    if (it == client_to_fd.end()) {
        cerr_warning << "send_cmd cannot find client-router connection: \""
                     << configFile.get_node_name_with_addr(addr) << "\"" << endl;
        return;
    }
    string to_send;
    to_send.resize(sizeof(struct MsgHeader));
    *(MsgHeader *)(&to_send.front()) = {.validation = htonl(VALIDATION_DATA), .size = (htonl(cmd.size()))};
    to_send += cmd;
    if (write(it->second, &to_send.front(), to_send.size()) == -1) {
        warn_syserror("send_cmd write");
        close(it->second);
        fd_to_client.erase(it->second);
        client_to_fd.erase(it);
        return;
    }
    cerr_detail << "Send cmd to " << configFile.get_node_name_with_addr(addr) << ": " << cmd << endl;
}

bool TcpNetwork::check_connection_is_active(int fd) {
    char data;
    ssize_t size = recv(fd, &data, 1, MSG_PEEK | MSG_DONTWAIT);
    if (size == 0 || (size == -1 && errno != EAGAIN))
        return false;
    return true;
//    int error_code;
//    socklen_t size = sizeof(error_code);
//    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error_code, &size) == -1) {
//        warn_syserror("check_connection_is_active getsockopt");
//    }
//    return error_code == 0;
}

std::ostream &operator<<(ostream &os, const channel_t &channel) {
    os << TcpNetwork::channel_to_string(channel);
    return os;
}
