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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "downloader.h"
#include "extractor.h"
#include "frames.h"
#include "process.h"

#define VERSION "0.2.0"
#define INPUT_SIZE 8192

static void print_banner(void)
{
    printf("\n");
    printf("============================================\n");
    printf("          STREAMSUCKER v%s\n", VERSION);
    printf("       Universal Video Downloader\n");
    printf("============================================\n");
}

static void print_dependencies(void)
{
    printf("\nDependency status:\n");

    printf("yt-dlp : %s\n",
        executable_exists("yt-dlp") ? "Available" : "Not found");

    printf("ffmpeg : %s\n",
        executable_exists("ffmpeg") ? "Available" : "Not found");
}

static int read_line(
    const char *prompt,
    char *buffer,
    size_t size
)
{
    printf("%s", prompt);

    if (fgets(buffer, size, stdin) == NULL)
        return 0;

    size_t length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n')
    {
        buffer[length - 1] = '\0';
    }
    else
    {
        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }

    return 1;
}

static int read_choice(void)
{
    char buffer[32];
    char extra;
    int choice;

    if (!read_line("\nEnter choice: ", buffer, sizeof(buffer)))
        return -1;

    if (sscanf(buffer, "%d %c", &choice, &extra) != 1)
        return -1;

    return choice;
}

static void process_url(void)
{
    char url[INPUT_SIZE];

    if (!read_line("\nEnter video URL: ", url, sizeof(url)))
        return;

    if (!is_valid_url(url))
    {
        printf("\nInvalid URL.\n");
        printf("Please enter a valid HTTP or HTTPS URL.\n");
        return;
    }

    printf("\nURL accepted.\n");

    printf("\n========== DOWNLOAD OPTIONS ==========\n");
    printf("1. Download full video\n");
    printf("2. Download particular section\n");
    printf("3. Extract frames\n");
    printf("0. Back\n");
    printf("======================================\n");

    int choice = read_choice();

    switch (choice)
    {
        case 1:
            downloader_stage2(url);
            break;

        case 2:
        {
            char start[64];
            char end[64];

            if (!read_line(
                    "Start time (HH:MM:SS): ",
                    start, sizeof(start)))
                break;

            if (!read_line(
                    "End time   (HH:MM:SS): ",
                    end, sizeof(end)))
                break;

            extractor_stage1(url, start, end);
            break;
        }

        case 3:
            frames_stage1(url);
            break;

        case 0:
            break;

        default:
            printf("\nInvalid option.\n");
    }
}

int main(void)
{
    while (1)
    {
        print_banner();
        print_dependencies();

        printf("\n");
        printf("1. Download video\n");
        printf("2. Exit\n");

        int choice = read_choice();

        switch (choice)
        {
            case 1:
                process_url();
                break;

            case 2:
                printf("\nThank you for using StreamSucker!\n");
                return EXIT_SUCCESS;

            default:
                if (choice == -1)
                    printf("\nInvalid input.\n");
                else
                    printf("\nInvalid option.\n");
        }

        printf("\nPress Enter to continue...");

        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }

    return EXIT_SUCCESS;
}

