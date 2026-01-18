#pragma once
#include <string>
#include <vector>

class Parser {
public:
    static size_t parse_resp_array(const std::string& buffer, size_t pos, std::vector<std::string>& args);
};