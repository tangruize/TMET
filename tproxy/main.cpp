//
// Created by tangruize on 22-5-10.
//

#include "TcpSocket.h"
#include "ConfigFile.h"
#include "SimpleRedirect.h"
#include "TcpNetwork.h"
#include "Repl.h"

#include <unistd.h>
#include <csignal>

#include <gflags/gflags.h>
using namespace gflags;

DECLARE_bool(help);
DEFINE_bool(verbose, false, "Show verbose information");
DEFINE_bool(detail, false, "Show detail information");
DEFINE_string(config, "", "Config file");
DEFINE_int32(bind, -1, "Port to bind");

ConfigFile configFile;
Command command;

string info_str = "[INFO] ";
string warn_str = "[WARN] ";
string detail_str = "[DETL] ";

int main(int argc, char **argv) {
    // Parse arguments and show help
    SetUsageMessage("Intercept network TCP/UDP connections using TPROXY");
    ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_help || argc != 1) {
        ShowUsageWithFlagsRestrict(argv[0], "main");
        exit(1);
    }
    else if (FLAGS_detail) {
        FLAGS_verbose = true;
    }
    if (isatty(STDERR_FILENO)) {
        info_str = "\033[1;32m" + info_str + "\033[0m";  // bold green
        warn_str = "\033[1;31m" + warn_str + "\033[0m";  // bold red
        detail_str = "\033[1;34m" + detail_str + "\033[0m";  // bold blue
    }

    TcpSocket tcpSocket(FLAGS_bind);
    configFile.load(FLAGS_config);
    signal(SIGPIPE, SIG_IGN);

    if (configFile.get_strategy() == STRATEGY_NOT_SET) {
        while (true) {
            AcceptData client = tcpSocket.accept();
            SimpleRedirect redirect(client);
            close(client.socket_fd);
        }
    }
    TcpNetwork tcpNetwork(&tcpSocket);
    vector<thread> threads;
    switch (configFile.get_strategy()) {
        case STRATEGY_DIRECT:
            tcpNetwork.run_epoll();
            break;
        case STRATEGY_FILE:
            tcpNetwork.run_epoll_background().detach();
            command.read_file(configFile.get_cmd_file());
            tcpNetwork.run_read_cmd();
            break;
        case STRATEGY_CMD: {
            tcpNetwork.run_epoll_background().detach();
            threads.push_back(tcpNetwork.run_read_cmd_background());
            Repl repl;
            repl.readline();
            break;
        }
        default:
            assert(0);
    }
    for (auto &i: threads) {
        i.join();
    }
}
