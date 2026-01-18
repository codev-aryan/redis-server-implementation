#include "parser.hpp"
#include <string>
#include <vector>
#include <sstream>

size_t Parser::parse_resp_array(const std::string& buffer, size_t pos, std::vector<std::string>& args) {
    if (pos >= buffer.size()) return 0;
    if (buffer[pos] != '*') return 0;
    size_t line_end = buffer.find("\r\n", pos);
    if (line_end == std::string::npos) return 0;

    int num_args = 0;
    try {
        num_args = std::stoi(buffer.substr(pos + 1, line_end - (pos + 1)));
    } catch (...) {
        return 0;
    }

    size_t current_pos = line_end + 2;
    args.clear();

    for (int i = 0; i < num_args; ++i) {
        if (current_pos >= buffer.size()) return 0;

        if (buffer[current_pos] != '$') return 0;

        size_t len_end = buffer.find("\r\n", current_pos);
        if (len_end == std::string::npos) return 0;

        int arg_len = 0;
        try {
            arg_len = std::stoi(buffer.substr(current_pos + 1, len_end - (current_pos + 1)));
        } catch (...) {
            return 0;
        }

        size_t arg_start = len_end + 2;
        if (buffer.size() < arg_start + arg_len + 2) return 0;

        args.push_back(buffer.substr(arg_start, arg_len));
        current_pos = arg_start + arg_len + 2;
    }

    return current_pos - pos;
}