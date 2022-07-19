//
// Created by fedora on 6/5/22.
//


#include <string>
#include <thread>
#include <vector>
#include <map>

using namespace std;

extern "C" {
#include "common.h"
#include "config.h"
#include "router.h"
#include "mysendto.h"
#include "timing.h"
#include <unistd.h>

static int router_sockfd = -1;

static string tokens_to_string(const vector<string> &tokens) {
    string s = tokens[0];
    for (unsigned i = 1; i < tokens.size(); i++)
        s += " " + tokens[i];
    return s;
}

static bool check_warn_args(unsigned required, const vector<string> &tokens) {
    if (tokens.size() < required) {
        print_info_no_prompt("  - WARN: command requires %d args, but given %d\n", required, tokens.size());
        return false;
    }
    return true;
}

static void inc_time_ns(const vector<string> &tokens) {
    if (!check_warn_args(2, tokens))
        return;
    long ns = stoi(tokens[1]);
    set_time_increment(ns);
}

static void do_router_cmd() {
    string cmd_buffer;
    cmd_buffer.resize(1024);
    cmd_buffer[cmd_buffer.size() - 1] = 0;
    ssize_t size;
    map<string, void (*)(const vector<string> &)> cmd_map = {
            {"inc_time_ns", inc_time_ns},
    };
    struct hacked_sendto_header *header = (hacked_sendto_header *) (&cmd_buffer.front());
    while ((size = syscall(SYS_recvfrom, router_sockfd, &cmd_buffer.front(), sizeof(hacked_sendto_header), MSG_WAITALL, 0, 0)) > 0) {
        assert(ntohl(header->validation) == VALIDATE_STR);
        size = ntohl(header->size);
        if (size >= (ssize_t)cmd_buffer.size())
            cmd_buffer.resize(size + 1);
        size = syscall(SYS_recvfrom, router_sockfd, &cmd_buffer.front(), size, MSG_WAITALL, 0, 0);
        if (size <= 0) {
            break;
        }
        cmd_buffer[size] = 0;
        vector<string> tokens;
        if (!get_tokens(cmd_buffer.c_str(), &tokens)) {
            continue;
        }
        print_info("Router cmd: %s\n", tokens_to_string(tokens).c_str());
        auto it = cmd_map.find(tokens[0]);
        if (it == cmd_map.end()) {
            print_info_no_prompt("  - WARN: no such command\n");
        } else {
            it->second(tokens);
        }
    }
    if (size == 0) {
        syscall(SYS_close, router_sockfd);
        print_info("Router socket is closed\n");
    } else {
        syscall(SYS_close, router_sockfd);
        print_info("Router socket recv failed: %s\n", strerror(errno));
    }
}

int connect_router(const char *addr) {
    static bool is_connected = false;
    if (is_connected) {
        print_info_no_prompt("    - WARN: router already connected\n");
        return false;
    }
    struct sockaddr_in router = convert_addr(addr, ':');
    if (!router.sin_port) {
        return false;
    }
    router.sin_family = AF_INET;
    print_info_no_prompt("  - router "
    ADDR_FMT
    "\n", ADDR_TO_STR(&router));
    router_sockfd = (int)syscall(SYS_socket, AF_INET, SOCK_STREAM, 0);
    if (router_sockfd == -1) {
        print_info_no_prompt("    - WARN: socket: %s\n", strerror(errno));
        return false;
    }
    if (syscall(SYS_connect, router_sockfd, (struct sockaddr *) &router, sizeof(router)) != 0) {
        print_info_no_prompt("    - WARN: connect: %s\n", strerror(errno));
        return false;
    }
    if (syscall(SYS_dup2, router_sockfd, ROUTER_FD) != -1) {
        syscall(SYS_close, router_sockfd);
        router_sockfd = ROUTER_FD;
    }
    std::thread(do_router_cmd).detach();
    is_connected = true;
    return true;
}
} // extern "C"