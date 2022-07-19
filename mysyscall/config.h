//
// Created by tangruize on 22-5-14.
//

#ifndef MYSYSCALL_CONFIG_H
#define MYSYSCALL_CONFIG_H

#ifndef CONFIG_FILE_ENV
#define CONFIG_FILE_ENV "MYSYSCALL_CONFIG"
#endif

#include <arpa/inet.h>

#define ROUTER_FD 1022

//#define ADDR_FMT "%s:%d"
//#define ADDR_TO_STR(addr) rstr1(inet_ntoa(((const struct sockaddr_in*)addr)->sin_addr)), ntohs(((const struct sockaddr_in*)addr)->sin_port)
//#define ADDR_TO_STR2(addr) rstr2(inet_ntoa(((const struct sockaddr_in*)addr)->sin_addr)), ntohs(((const struct sockaddr_in*)addr)->sin_port)
//#define CIDR_FMT "%s/%d"
//#define CIDR_TO_STR(addr) rstr1(inet_ntoa(((const struct sockaddr_in*)addr)->sin_addr)), ((const struct sockaddr_in*)addr)->sin_port
//#define CIDR_TO_STR2(addr) rstr2(inet_ntoa(((const struct sockaddr_in*)addr)->sin_addr)), ((const struct sockaddr_in*)addr)->sin_port

#define ADDR_FMT "%s"
#define CIDR_FMT "%s"
#define ADDR_TO_STR(addr) addr_to_str((const struct sockaddr_in *)addr, ":", 1)
#define ADDR_TO_STR2(addr) addr_to_str((const struct sockaddr_in *)addr, ":", 2)
#define CIDR_TO_STR(addr) addr_to_str((const struct sockaddr_in *)addr, "/", 1)
#define CIDR_TO_STR2(addr) addr_to_str((const struct sockaddr_in *)addr, "/", 2)

#ifdef __cplusplus
extern "C" {
#endif

// read config file (get filename from ENV CONFIG_FILE_ENV), store concerned addr
void init_config_file();

struct sockaddr_in convert_mapped_ipv6_to_ipv4(const struct sockaddr_in6 *addr);

// check addr is concerned, so that we can trace later actions of the fd
int check_addr_is_concerned(const struct sockaddr_in *addr);
int check_addr_is_concerned_with_len(const struct sockaddr_in *addr, socklen_t addrlen);

// add fd to concerned fd list
void add_concerned_fd(int fd, const struct sockaddr_in *addr);

// remove fd (close())
void rm_concerned_fd(int fd);

// check fd is concerned to do some actions (send()/sendto())
int check_fd_is_concerned(int fd);
struct sockaddr_in check_fd_is_concerned_with_addr(int fd);

// convert addr to printable string (both ipv4 and ipv6)
const char *addr_to_str(const struct sockaddr_in *addr, const char *delim, int which);

// convert ADDR (format: xxx.xxx.xxx.xxx:port) and CIDR (format: xxx.xxx.xxx.xxx/netmask)
struct sockaddr_in convert_addr(const char *addr, char delim);

// get tokens from line. (CPP only, ts is (vector<string> *) type)
int get_tokens(const char *line, void *ts);

#ifdef __cplusplus
}
#endif

#endif //MYSYSCALL_CONFIG_H
