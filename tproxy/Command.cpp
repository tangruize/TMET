//
// Created by tangruize on 22-5-18.
//

#include "common.h"
#include "Command.h"

void Command::read_file(const string &file) {
    cerr_verbose << "Read command file \"" << file << "\"" << endl;
    ifstream f(file);
    if (!f.is_open()) {
        cerr_warning << "read_file cannot open file: " << file << endl;
        return;
    }
    string line;
    int line_count = 0;
    while (getline(f, line)) {
        if (enqueue(line))
            line_count++;
    }
    if (is_file)
        enqueue("exit");
    cerr_detail << "Read " << line_count << "commands in file \"" << file << "\"" << endl;
}

bool Command::enqueue(const string &line) {
    cmd_t cmd(line);
    if (cmd.empty())
        return false;
    cmds.enqueue(std::move(cmd));
    return true;
}

bool Command::dequeue(cmd_t &c) {
    if (is_file && !cmds.peek())
        return false;
    cmds.wait_dequeue(c);
    if (c.get_cmd() == "exit")
        return false;
    return true;
}

Command::Command() {
    is_file = configFile.get_strategy() == STRATEGY_FILE;
}

bool Command::enqueue(cmd_t &&c) {
    if (c.empty())
        return false;
    cmds.enqueue(std::move(c));
    return true;
}

bool cmd_t::check_prompt_invalid(unsigned argc, const string &info, bool at_least) const {
    assert(!tokens.empty());
    bool ok = at_least ? (argc <= tokens.size()) : (argc == tokens.size());
    if (ok) {
        return true;
    }
    string required_prompt = argc ? string(" (requires ") + (at_least ? ">=" : "") + to_string(argc) + " fields)" : "";
    cerr_warning << "Invalid cmd" << required_prompt;
    if (!info.empty()) {
        cerr_warning_cont << ": " << info;
    }
    cerr_warning_cont << ": \"";
    cerr_warning_cont << get_cmd();
    for (unsigned i = 1; i < tokens.size(); i++) {
        cerr_warning_cont << " " << tokens[i];
    }
    cerr_warning_cont << "\"" << endl;
    return false;
}

std::ostream &operator<<(ostream &os, const cmd_t &c) {
    os << c.get_cmd();
    for (unsigned i = 1; i < c.tokens.size(); i++) {
        cerr_warning_cont << " " << c.tokens[i];
    }
    return os;
}

cmd_t::cmd_t(const string &line) {
    istringstream ss(line);
    string token;
    while (getline(ss, token, ' ')) {
        if (token.empty())
            continue;
        if (token[0] == '#')
            break;
        tokens.push_back(token);
    }
}

string cmd_t::get_args_from(unsigned start) const {
    assert(start < tokens.size());
    string res = tokens[start];
    for (unsigned i = start + 1; i < tokens.size(); ++i) {
        res += " " + tokens[i];
    }
    return res;
}
