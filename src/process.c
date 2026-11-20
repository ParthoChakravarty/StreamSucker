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

int run_process_capture(
    char *const argv[],
    char *output,
    size_t output_size
)
{
    if (argv == NULL || argv[0] == NULL ||
        output == NULL || output_size == 0)
        return -1;

    output[0] = '\0';

    int pipefd[2];

    if (pipe(pipefd) < 0)
    {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
        {
            perror("dup2");
            _exit(127);
        }

        close(pipefd[1]);

        execvp(argv[0], argv);

        perror("execvp");
        _exit(127);
    }

    close(pipefd[1]);

    size_t total = 0;
    int read_error = 0;

    while (1)
    {
        char buffer[512];

        ssize_t bytes = read(
            pipefd[0],
            buffer,
            sizeof(buffer)
        );

        if (bytes > 0)
        {
            size_t available = output_size - total - 1;
            size_t copy_size = (size_t)bytes;

            if (copy_size > available)
            {
                copy_size = available;
                read_error = 1;
            }

            if (copy_size > 0)
            {
                memcpy(output + total, buffer, copy_size);
                total += copy_size;
                output[total] = '\0';
            }

            continue;
        }

        if (bytes == 0)
            break;

        if (errno == EINTR)
            continue;

        perror("read");
        read_error = 1;
        break;
    }

    close(pipefd[0]);

    int status;

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return -1;
    }

    if (read_error)
        return -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return -1;
}

