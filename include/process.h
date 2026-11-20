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
 
#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>

bool executable_exists(const char *program);
int run_process(char *const argv[]);

#endif

