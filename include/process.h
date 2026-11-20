#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>

bool executable_exists(const char *program);
int run_process(char *const argv[]);

#endif

