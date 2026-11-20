/*
 * This file is part of StreamSucker.
 *
 * Copyright (C) 2026 Partho Chakravarty
 *
 * StreamSucker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * See <https://www.gnu.org/licenses/>.
 */
 

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <stdbool.h>

bool is_valid_url(const char *url);

bool parse_timestamp(
    const char *input,
    long *seconds
);

void extractor_stage3(const char *url);

#endif

