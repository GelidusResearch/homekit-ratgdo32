#include "json.h"
#include <cassert>
#include <cstring>

int main()
{
    char buffer[128] = {};
    char *end = buffer + sizeof(buffer);
    char *cursor = start_json(buffer, end);
    cursor = add_float(cursor, end, "temperature", -12.5f);
    end_json(cursor, end);
    assert(std::strstr(buffer, "\"temperature\": -12.500000"));
    assert(std::strstr(buffer, "\n}"));

    struct { char buffer[12]; char sentinel[4]; } small = {};
    std::memcpy(small.sentinel, "safe", 4);
    end = small.buffer + sizeof(small.buffer);
    cursor = start_json(small.buffer, end);
    char *before = cursor;
    cursor = add_float(cursor, end, "temperature", 123.5f);
    assert(cursor == before); // Omit an entire field that will not fit.
    end_json(cursor, end);
    assert(std::memcmp(small.sentinel, "safe", 4) == 0);
    assert(std::strcmp(small.buffer, "{\n}") == 0);
}
