//
// Created by tangruize on 22-5-17.
//

#ifndef TPROXY_TCPNETWORK_H
#define TPROXY_TCPNETWORK_H

#include "common.h"
#include "TcpSocket.h"
#include "Command.h"
#include <ctime>
#include <set>
#include <thread>
#include <memory>

#include <readerwriterqueue.h>
//#include <blockingconcurrentqueue.h>
using namespace moodycamel;

struct __attribute__((__packed__))  // packed to recv network packet
MsgHeader {
    uint32_t validation;     // validation
    uint32_t size;         // msg length
};

#define VALIDATION_DATA 0xdeadbeef

struct Msg {
    shared_ptr<int> fd;  // each msg has a fd because a channel may have several connections
    struct MsgHeader *header = nullptr;  // caution to use header! it is network endian and is not converted
    string content;
    uint32_t size = 0;
    Msg(const Msg&) = delete;  // no copy constructor
    Msg& operator=(const Msg &m) = delete;  // no copy assignment
    explicit Msg(Msg *m = nullptr) {
        if (m)
            *this = std::move(*m);
    }
    Msg& operator=(Msg &&m) noexcept {  // move assign
        fd = std::move(m.fd);
        content = std::move(m.content);
        size = m.size;
        header = (struct MsgHeader *)buffer();
        m.size = 0;
        m.header = nullptr;
        return *this;
    }
    Msg(const shared_ptr<int> &sockfd, const struct MsgHeader &h) {
        fd = sockfd;
        size = ntohl(h.size);
        content.resize(size + sizeof(struct MsgHeader));
        header = (struct MsgHeader *)buffer();
        *header = h;
    }
    Msg(Msg &&m) noexcept {  // move constructor
        *this = std::move(m);
    }
    char *buffer() {  // buffer pointer
        return &content.front();
    }
    char *body() {  // msg body
        return &content[sizeof(struct MsgHeader)];
    }
    const char *buffer() const {
        return &content.front();
    }
    const char *body() const {
        return &content[sizeof(struct MsgHeader)];
    }
};

struct ChannelMsgs {
//    int disconnected;
//    BlockingConcurrentQueue<Msg> msgs;
    BlockingReaderWriterQueue<Msg> msgs;
};

//typedef pair<struct sockaddr_in, struct sockaddr_in> channel_t;
struct channel_t {
    struct sockaddr_in first;
    struct sockaddr_in second;
    friend std::ostream & operator<<(std::ostream &os, const channel_t& channel);
    bool ok() const {
        return first.sin_port != (in_port_t)-1 && second.sin_port != (in_port_t)-1;
    }
};

struct channel_status_t {
    channel_t channel;
    shared_ptr<int> self_fd, fd;  // use shared pointer to invalidate buffered msgs whose connections are closed
    bool disconnected;
};

struct cmp_channel {
    cmp_addr_less less;
    cmp_addr_equal equal;
    bool operator()(const channel_t& a, const channel_t& b) const {
        if (equal(a.first, b.first))
            return less(a.second, b.second);
        return less(a.first, b.first);
    }
};

class TcpNetwork {
private:
    TcpSocket *tcp;
    map<int, channel_status_t> fd_to_channel_status;
    map<channel_t, struct ChannelMsgs, cmp_channel> network;
    map<struct sockaddr_in, int, cmp_addr_less> client_to_fd;
    map<int, struct sockaddr_in> fd_to_client;
    set<int> unintercepted_fd;
    int epoll_fd;
    bool is_direct;
    const int max_events = 10;
    const struct timeval recv_timeout = { .tv_sec = 3, .tv_usec = 0};
    const int64_t deliver_timout = recv_timeout.tv_sec * 1000000 + recv_timeout.tv_usec;  // reuse recv timeout
    const int syn_retries = 1;  // connection syn retry times. 1: ~3s timeout, 2: ~7s timeout, 0: ~127s (unintuitive)
    void set_recv_timeout(int fd);
    void set_conn_retries(int fd);
    int connect_peer(const AcceptData &acc);
    void add_fd_to_channel(int src_fd, int dst_fd, struct sockaddr_in src, struct sockaddr_in dst);
    pair<int, int> do_accept_connect();
    void add_monitor_fd(int fd) const;
    void do_receive(int fd);
    void enqueue_msg(const channel_status_t &cf, Msg &m);
    void deliver_msg(const channel_t &channel, Msg *m = nullptr);
    void do_close(map<int, channel_status_t>::iterator it);
    void close_connection(int fd, bool tag_disconnected=false, bool close_peer=true);
    void set_nonblocking(int fd) const;
    bool packet_validation(const MsgHeader &header, unsigned size) const;
    void set_unintercepted(int fd, const channel_t &channel);
    void transfer_unintercepted(int out_fd, int in_fd);
    void clear_msgs(const channel_t &channel);
    void partition(const string &node, bool clear_msg=false, bool is_recover=false);
    void recover(const string &node);
    void send_cmd(const string &node, const string &cmd);
public:
    explicit TcpNetwork(TcpSocket *tcp_socket);
    void run_epoll();
    void run_read_cmd();
    std::thread run_epoll_background() { return std::thread( [this] { this->run_epoll(); } ); }
    std::thread run_read_cmd_background() { return std::thread( [this] { this->run_read_cmd(); } ); }
    static string channel_to_string(const channel_t &c);
    static bool check_connection_is_active(int fd) ;
    void print_status();
};

#endif //TPROXY_TCPNETWORK_H
