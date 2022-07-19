//
// Created by tangruize on 22-5-10.
//

#include "ConfigFile.h"
#include "common.h"
#include <set>

void ConfigFile::load(const string &file) {
    if (file.empty())
        return;
    cerr_verbose << "Read config file \"" << file << "\"" << endl;
    ifstream f;
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);  // throw exception if failed to open file for read
    f.open(file);
    f.exceptions(std::ifstream::badbit);  // disable failbit throwing exceptions (such as EOF)
    string line, token;
    int line_no = 0;
    while (getline(f, line)) {
        line_no++;
        vector<string> tokens;
        istringstream ss(line);
        while (getline(ss, token, ' ')) {
            if (token.empty())
                continue;
            if (token[0] == '#')
                break;
            tokens.push_back(token);
        }
        if (tokens.empty())
            continue;

        bool ok = false;
        if (tokens.size() == 3 && (tokens[0] == "map-addr" || tokens[0] == "map-cidr")) {
            char delim = ((tokens[0] == "map-addr") ? ':' : '/');
            struct sockaddr_in origin = convert_addr(tokens[1], delim);
            struct sockaddr_in masquerade = convert_addr(tokens[2], delim);
            if (origin.sin_port && masquerade.sin_port) {
                ok = true;
                auto &which = ((tokens[0] == "map-addr") ? addr_map : cidr_map);
                which[origin] = masquerade;
                cerr_verbose_cont << "  - " << tokens[0] << ' ' << addr_to_string_delim(origin, delim)
                                  << ' ' << addr_to_string_delim(masquerade, delim) << endl;
            }
        } else if (tokens.size() == 3 && tokens[0] == "node") {
            struct sockaddr_in addr = convert_addr(tokens[2], ' ');
            if (addr.sin_addr.s_addr) {
                ok = true;
                string node = tokens[1];
                addr_to_node[addr] = node;
                node_to_addr[node] = addr;
                cerr_verbose_cont << "  - " << tokens[0] << ' ' << node << ' ' << inet_ntoa((addr).sin_addr) << endl;
            }
        } else if (tokens.size() >= 2 && tokens[0] == "strategy") {
            if (strategy != STRATEGY_NOT_SET) {
                cerr_verbose_cont << "  - WARN: strategy should not appear twice at line " << line_no << ": " << line << endl;
                continue;
            }
            auto it = allowed_strategies.find(tokens[1]);
            if (it != allowed_strategies.end()) {
                if (tokens.size() > 2 && it->second != STRATEGY_FILE)
                    ok = false;
                else {
                    ok = true;
                    strategy = it->second;
                    if (strategy == STRATEGY_FILE) {
                        cmd_file = std::move(tokens[2]);
                    }
                    cerr_verbose_cont << "  - " << tokens[0] << ' ' << tokens[1] << (cmd_file.empty() ? "" : " " + cmd_file) << endl;
                }
            }
        } else if (tokens.size() >= 2 && tokens[0] == "router") {
            struct sockaddr_in router = convert_addr(tokens[1], ':');
            if (router.sin_port) {
                router_addr = router;
                ok = true;
                router.sin_port = 0;
                addr_to_node[router] = "router";
                node_to_addr["router"] = router;
                cerr_verbose_cont << "  - " << tokens[0] << ' ' << addr_to_string(router) << endl;
            }
        }
        if (!ok)
            cerr_verbose_cont << "  - WARN: invalid cmd at line " << line_no << ": " << line << endl;
    }
    f.close();
    if (strategy == STRATEGY_NOT_SET) {
        strategy = default_strategy;
        cerr_verbose << "Strategy not set, use simple redirect" << endl;
    }
}

struct sockaddr_in ConfigFile::convert_addr(const string &addr, char delim) {
    struct sockaddr_in res{};
    stringstream ss(addr);
    string ip, port;
    getline(ss, ip, delim);
    getline(ss, port, delim);
    res.sin_family = AF_INET;
    inet_aton(ip.c_str(), &res.sin_addr);
    if (delim == ':')
        res.sin_port = htons(stoi(port));
    else if (delim == '/')
        res.sin_port = stoi(port);
    return res;
}

static inline in_addr_t addr_mask(const sockaddr_in &addr, unsigned mask) {
    return ntohl(addr.sin_addr.s_addr) & mask;
}

struct sockaddr_in ConfigFile::get_masquerade_addr(const sockaddr_in &origin) const {
    auto it = addr_map.find(origin);
    if (it != addr_map.end())
        return it->second;
    for (auto &i: cidr_map) {
        unsigned mask = unsigned(-1) << (32 - i.first.sin_port);
        if (addr_mask(i.first, mask) == addr_mask(origin, mask)) {
            mask = unsigned(-1) << (32 - i.second.sin_port);
            sockaddr_in ret = origin;
            ret.sin_addr.s_addr = htonl(addr_mask(i.second, mask) | addr_mask(origin, ~mask));
            return ret;
        }
    }
    return origin;
}

string ConfigFile::get_node_name(const struct sockaddr_in &addr, bool to_string_if_null) const {
    struct sockaddr_in a = addr;
    a.sin_port = 0;
    auto it = addr_to_node.find(a);
    if (it != addr_to_node.end())
        return it->second;
    if (to_string_if_null)
        return addr_to_string(addr);
    return ""; // failed to look up
}

struct sockaddr_in ConfigFile::get_node_addr(const string &name) const {
    auto it = node_to_addr.find(name);
    if (it != node_to_addr.end())
        return it->second;
    struct sockaddr_in ret{};
    if (inet_aton(name.c_str(), &ret.sin_addr))
        return ret;
    // failed to look up
    ret.sin_port = (in_port_t)-1;
    return ret;
}

string ConfigFile::get_node_name_with_addr(const sockaddr_in &addr) const {
    string node_name = get_node_name(addr);
    if (node_name.empty())
        node_name = addr_to_string(addr);
    else
        node_name += " (" + addr_to_string(addr) + ")";
    return node_name;
}

bool ConfigFile::is_router_addr(const struct sockaddr_in &addr) {
    cmp_addr_equal cmp;
    return cmp(addr, router_addr);
}
