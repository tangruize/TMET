//
// Created by tangruize on 22-5-14.
//

extern "C" {
#include "common.h"
#include "config.h"
#include "router.h"
}

#include <map>
#include <set>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <unistd.h>

using namespace std;

// comparison func of struct sockaddr_in for map/set
struct cmp_addr {
    bool operator()(const struct sockaddr_in& a, const struct sockaddr_in& b) const {
        if (a.sin_addr.s_addr == b.sin_addr.s_addr)
            return a.sin_port < b.sin_port;
        return a.sin_addr.s_addr < b.sin_addr.s_addr;
    }
};
// fd that addr is concerned
static map<int, struct sockaddr_in> fd_map;
// concerned addrs import from config file
static set<struct sockaddr_in, cmp_addr> concern_addr_set;
static set<unsigned short> subnet_suffix;

extern "C" {

struct sockaddr_in convert_mapped_ipv6_to_ipv4(const struct sockaddr_in6 *addr) {
    union {
        uint16_t u16[8];
        uint32_t u32[4];
        uint64_t u64[2];
        in6_addr ipv6_addr;
    } u;
    u.ipv6_addr = addr->sin6_addr;
    struct sockaddr_in ret{};
    ret.sin_port = addr->sin6_port;
    if (u.u64[0] == 0 && u.u16[4] == 0 && u.u16[5] == 0xffff) {
        ret.sin_addr.s_addr = u.u32[3];
    }
    return ret;
}

// check addr is concerned, so that we can trace later actions of the fd
int check_addr_is_concerned(const struct sockaddr_in *addr) {
    struct sockaddr_in to_search{}, addr_buf;
    if (addr->sin_family == AF_INET) {
        to_search.sin_addr.s_addr = addr->sin_addr.s_addr;
        to_search.sin_port = addr->sin_port;
    } else if (addr->sin_family == AF_INET6) {
        addr_buf = convert_mapped_ipv6_to_ipv4((const struct sockaddr_in6 *)addr);
        to_search = addr_buf;
        addr = &addr_buf;
    }
    if (concern_addr_set.find(to_search) != concern_addr_set.end())
        return 1;
    for (auto i: subnet_suffix) {
        to_search.sin_port = i;
        unsigned mask = unsigned(-1) << (32 - i);
        to_search.sin_addr.s_addr = htonl(ntohl(addr->sin_addr.s_addr) & mask);
        if (concern_addr_set.find(to_search) != concern_addr_set.end())
            return 1;
    }
    return 0;
}

int check_addr_is_concerned_with_len(const struct sockaddr_in *addr, socklen_t addrlen) {
    if (addrlen >= sizeof(struct sockaddr_in))
        return check_addr_is_concerned(addr);
    return 0;
}

// add fd to concerned fd list
void add_concerned_fd(int fd, const struct sockaddr_in *addr) {
    if (addr->sin_family == AF_INET)
        fd_map[fd] = *addr;
    else {
        fd_map[fd] = convert_mapped_ipv6_to_ipv4((const sockaddr_in6 *)addr);
    }
}

// remove fd (close())
void rm_concerned_fd(int fd) {
    fd_map.erase(fd);
}

// check fd is concerned to do some actions (send()/sendto())
struct sockaddr_in check_fd_is_concerned_with_addr(int fd) {
    const auto it = fd_map.find(fd);
    if (it != fd_map.end())
        return it->second;
    struct sockaddr_in empty{};
    return empty;
}

int check_fd_is_concerned(int fd) {
    return fd_map.find(fd) != fd_map.end();
}

int get_tokens(const char *line, void *ts) {
    vector<string> &tokens = *reinterpret_cast<vector<string> *>(ts);
    string token;
    istringstream ss(line);
    while (getline(ss, token, ' ')) {
        if (token.empty())
            continue;
        if (token[0] == '#')
            break;
        tokens.push_back(token);
    }
    if (tokens.empty())
        return false;
    return true;
}

// read config file (get filename from ENV CONFIG_FILE_ENV), store concerned addr
void init_config_file() {
    static bool is_inited = false;  // to avoid initialising twice
    const char *config_file = getenv(CONFIG_FILE_ENV);
    if (is_inited || !config_file || strlen(config_file) == 0)
        return;

    print_info("Read config file \"%s\"\n", config_file);
    is_inited = true;
    ifstream f(config_file);
    if (!f.is_open()) {
        print_info_no_prompt("  - WARN: fail to open file");
        return;
    }

    string line, token;
    int line_no = 0;
    static set<string> ignore_cmd = { "node", "strategy" };
    while (getline(f, line)) {
        line_no++;
        vector<string> tokens;
        if (!get_tokens(line.c_str(), &tokens) || ignore_cmd.count(tokens[0])) {
            continue;
        }

        bool ok = false;
        if (tokens.size() >= 2 && (tokens[0] == "map-addr" || tokens[0] == "map-cidr")) {
            char delim = (tokens[0] == "map-addr") ? ':' : '/';
            struct sockaddr_in origin = convert_addr(tokens[1].c_str(), delim);
            if (origin.sin_port) {
                if (delim == ':')
                    print_info_no_prompt("  - %s " ADDR_FMT "\n", tokens[0].c_str(), ADDR_TO_STR(&origin));
                else {
                    print_info_no_prompt("  - %s " CIDR_FMT "\n", tokens[0].c_str(), CIDR_TO_STR(&origin));
                    subnet_suffix.insert(origin.sin_port);
                }
                concern_addr_set.insert(origin);
                ok = true;
            }
        } else if (tokens.size() >= 2 && tokens[0] == "router") {
            if (connect_router(tokens[1].c_str()))
                ok = true;
        }
        if (!ok)
            print_info_no_prompt("  - WARN: invalid cmd at line %d: %s\n", line_no, line.c_str());
    }
    f.close();
}

// convert ADDR (format: xxx.xxx.xxx.xxx:port) and CIDR (format: xxx.xxx.xxx.xxx/netmask)
struct sockaddr_in convert_addr(const char *addr, char delim) {
    struct sockaddr_in res{};
    stringstream ss(addr);
    string ip, port;
    getline(ss, ip, delim);
    getline(ss, port, delim);
    inet_aton(ip.c_str(), &res.sin_addr);
    if (delim == ':') {
        res.sin_port = htons(stoi(port));
    }
    else {
        res.sin_port = stoi(port);
        unsigned mask = unsigned(-1) << (32 - res.sin_port);
        res.sin_addr.s_addr = htonl(ntohl(res.sin_addr.s_addr) & mask);
    }
    return res;
}

const char *addr_to_str(const struct sockaddr_in *addr, const char *delim, int which) {
    char addr_buf[INET6_ADDRSTRLEN+8] = "NULL";
    in_port_t port;
    if (addr->sin_family == AF_INET6) {
        inet_ntop(AF_INET6, &((const struct sockaddr_in6*)addr)->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
        port = ((const struct sockaddr_in6*)addr)->sin6_port;
    } else {
        inet_ntop(AF_INET, &addr->sin_addr, addr_buf, INET6_ADDRSTRLEN);
        port = addr->sin_port;
    }
    if (delim[0] == ':')
        port = ntohs(port);
    strcat(addr_buf, (delim + to_string(port)).c_str());
    return restrict_str_internal(which, addr_buf);
}

} // extern "C"
