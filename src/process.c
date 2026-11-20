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
 
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

bool executable_exists(const char *program)
{
    if (program == NULL || program[0] == '\0')
        return false;

    if (strchr(program, '/') != NULL)
        return access(program, X_OK) == 0;

    const char *path_env = getenv("PATH");

    if (path_env == NULL)
        return false;

    char *path = strdup(path_env);

    if (path == NULL)
        return false;

    char *saveptr = NULL;
    char *directory = strtok_r(path, ":", &saveptr);

    while (directory != NULL)
    {
        char full_path[4096];

        int length = snprintf(
            full_path,
            sizeof(full_path),
            "%s/%s",
            directory,
            program
        );

        if (length > 0 &&
            (size_t)length < sizeof(full_path) &&
            access(full_path, X_OK) == 0)
        {
            free(path);
            return true;
        }

        directory = strtok_r(NULL, ":", &saveptr);
    }

    free(path);
    return false;
}

int run_process(char *const argv[])
{
    if (argv == NULL || argv[0] == NULL)
        return -1;

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        execvp(argv[0], argv);

        perror("execvp");
        _exit(127);
    }

    int status;

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return -1;
}
