#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <stdbool.h>

bool is_valid_url(const char *url);
bool parse_timestamp(const char *input, long *seconds);

void extractor_stage1(
    const char *url,
    const char *start,
    const char *end
);

#endif
