#pragma once
#include <string>
#include <deque>

enum ValueType {
    VAL_STRING,
    VAL_LIST
};

struct Entry {
    ValueType type = VAL_STRING;
    std::string string_val;
    std::deque<std::string> list_val; 
    long long expiry_at = 0;
};