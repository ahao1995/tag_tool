#pragma once
#include <stdint.h>

struct tag_context
{
    virtual ~tag_context() {}
    virtual void to_string(char *out, int *out_len, int max_len) = 0;
    virtual void destroy() = 0;
};

struct tag
{
    uint16_t tag_id;
    uint64_t guid;
    uint64_t nano_sec;
    tag_context *context{nullptr};
};