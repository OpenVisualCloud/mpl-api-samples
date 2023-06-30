/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class ParseContext {
  public:
    ParseContext() : working(true), outputInfo(true) {}
    ~ParseContext() {}

    void GetCommand(int argc, char **argv);
    template <class dataType>
    bool get(std::string opt, std::string optinfo, dataType *variable, dataType default_para, bool is_required);
    bool get(std::string opt, std::string optinfo, int &v1, int &v2, std::string default_para, bool is_required);
    void usage(const char *info);
    void check(const char *info);

  private:
    bool extract(std::string input, int &val1, int &val2);
    auto get_opt(std::string opt);

  private:
    typedef std::vector<std::vector<std::string>> Command;
    typedef std::pair<std::string, std::string> Infopair;
    Command params;
    std::vector<Infopair> help;
    bool working;
    bool outputInfo;
};

inline bool ParseContext::extract(std::string input, int &val1, int &val2) {
    bool Flag  = false;
    size_t pos = input.find("X");
    if (pos != input.npos) {
        val1 = atoi(input.substr(0, pos).c_str());
        val2 = atoi(input.substr(pos + 1, input.size()).c_str());
        Flag = true;
    }
    return Flag;
}

inline auto ParseContext::get_opt(std::string opt) {
    Command::iterator it;
    for (it = params.begin(); it != params.end(); it++) {
        if (opt == it->at(0))
            break;
    }
    return it;
}

inline void ParseContext::GetCommand(int argc, char **argv) {
    if (argc >= 2) {
        std::string in(argv[1]);
        if (in == "-h" || in == "-help") {
            outputInfo = false;
            working    = false;
        }
    }
    params.emplace_back();
    Command::iterator it = params.begin();
    for (int i = 1; i < argc; i++) {
        if ('-' == argv[i][0]) {
            params.emplace_back();
            it = params.end() - 1;
            it->emplace_back(argv[i]);
        } else {
            it->emplace_back(argv[i]);
        }
    }
    if (params.begin()->empty())
        params.erase(params.begin());

#if DEBUG
    std::cout << "\tparsed parameter table : \n";
    for (auto it = params.begin(); it != params.end(); it++) {
        for (auto its = it->begin(); its != it->end(); its++)
            std::cout << *its << " ";
        std::cout << "\n";
    }
#endif
}

template <class dataType>
bool ParseContext::get(std::string opt, std::string optinfo, dataType *variable, dataType default_para,
                       bool is_required) {
    bool Flag = false;
    help.emplace_back(Infopair(opt, optinfo));

    auto it = get_opt(opt);
    if (it != params.end()) {
        std::istringstream ss(it->at(1));
        ss >> *variable;
        Flag = true;
        params.erase(it);
    }

    if (!Flag) {
        if (is_required) {
            working = false;
            if (outputInfo)
                std::cout << "missing required parameter : " << opt << "\n";
        } else
            *variable = default_para;
    }
    return Flag;
}

template <>
inline bool ParseContext::get<bool>(std::string opt, std::string optinfo, bool *variable, bool default_para,
                                    bool is_required) {
    bool Flag = false;
    help.emplace_back(Infopair(opt, optinfo));

    auto it = get_opt(opt);
    if (it != params.end()) {
        *variable = !default_para;
        Flag      = true;
        params.erase(it);
    }

    if (!Flag) {
        if (is_required) {
            working = false;
            if (outputInfo)
                std::cout << "missing required parameter : " << opt << "\n";
        } else
            *variable = default_para;
    }
    return Flag;
}

inline bool ParseContext::get(std::string opt, std::string optinfo, int &v1, int &v2, std::string default_para,
                              bool is_required) {
    bool Flag = false;
    help.emplace_back(Infopair(opt, optinfo));

    auto it = get_opt(opt);
    std::string input;
    if (it != params.end()) {
        input    = it->at(1);
        Flag     = true;
        bool ret = extract(input, v1, v2);
        Flag     = Flag && ret;
        if (Flag)
            params.erase(it);
    }

    if (!Flag) {
        if (is_required) {
            working = false;
            if (outputInfo)
                std::cout << "missing required parameter : " << opt << "\n";
        } else
            extract(default_para, v1, v2);
    }
    return Flag;
}

inline void ParseContext::usage(const char *info) {
    std::cout << info << "\n";
    for (auto it = help.begin(); it != help.end(); ++it)
        std::cout << it->first << " " << it->second << "\n";
    exit(1);
}

inline void ParseContext::check(const char *info) {
    if (!params.empty() && outputInfo) {
        working = false;
        std::cout << "failed to parse command :";
        for (auto it = params.begin(); it != params.end(); it++)
            for (auto its = it->begin(); its != it->end(); its++)
                std::cout << *its << " ";
        std::cout << "\n";
    }

    if (!working)
        usage(info);
}
