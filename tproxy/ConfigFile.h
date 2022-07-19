//
// Created by tangruize on 22-5-10.
//

#ifndef TPROXY_CONFIGFILE_H
#define TPROXY_CONFIGFILE_H

#include <map>
#include <unordered_map>
#include <string>
#include <arpa/inet.h>

using namespace std;

// a < b
struct cmp_addr_less {
    bool operator()(const struct sockaddr_in& a, const struct sockaddr_in& b) const {
        if (a.sin_addr.s_addr == b.sin_addr.s_addr)
            return a.sin_port < b.sin_port;
        return a.sin_addr.s_addr < b.sin_addr.s_addr;
    }
};

// a == b
struct cmp_addr_equal {
    bool operator()(const struct sockaddr_in& a, const struct sockaddr_in& b) const {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }
};

// a.ip + a.port
struct hash_addr {
    // (very unlikely) WARNING: may have collision if sizeof(size_t) == 4
    size_t operator()(const struct sockaddr_in& addr) const {
        return size_t(addr.sin_addr.s_addr) + addr.sin_port;
    }
};

enum strategy_t {STRATEGY_NOT_SET = 0, STRATEGY_DIRECT, STRATEGY_CMD, STRATEGY_FILE};

class ConfigFile {
private:
    map<struct sockaddr_in, struct sockaddr_in, cmp_addr_less> addr_map;  // use map because need to preserve orders
    map<struct sockaddr_in, struct sockaddr_in, cmp_addr_less> cidr_map;  // use map because need to iterate over
    unordered_map<struct sockaddr_in, string, hash_addr, cmp_addr_equal> addr_to_node;
    unordered_map<string, struct sockaddr_in> node_to_addr;
    strategy_t strategy = STRATEGY_NOT_SET;
    string cmd_file;
    const unordered_map<string, strategy_t> allowed_strategies = {
            {"direct", STRATEGY_DIRECT},
            {"cmd",    STRATEGY_CMD},
            {"file",   STRATEGY_FILE}
    };
//    const strategy_t default_strategy = STRATEGY_DIRECT;
    const strategy_t default_strategy = STRATEGY_NOT_SET;
    static struct sockaddr_in convert_addr(const string &addr, char delim=':');
public:
    void load(const string &file);
    ConfigFile(const string &file = "") {
        load(file);
    }
    struct sockaddr_in get_masquerade_addr(const struct sockaddr_in &origin) const;
    strategy_t get_strategy() const {
        return strategy;
    }
    const string &get_cmd_file() const {
        return cmd_file;
    }
    string get_node_name(const struct sockaddr_in &addr, bool to_string_if_null=false) const;
    string get_node_name_with_addr(const struct sockaddr_in &addr) const;
    struct sockaddr_in get_node_addr(const string &name) const;
    struct sockaddr_in router_addr{};
    bool is_router_addr(const struct sockaddr_in &addr);
};

#endif //TPROXY_CONFIGFILE_H
