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

#include "downloader.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

#define PATH_SIZE 4096
#define INPUT_SIZE 4096

typedef enum
{
    QUALITY_BEST,
    QUALITY_2160,
    QUALITY_1080,
    QUALITY_720,
    QUALITY_480
} VideoQuality;

typedef enum
{
    FORMAT_MP4,
    FORMAT_WEBM
} VideoFormat;

static int read_input(
    const char *prompt,
    char *buffer,
    size_t size
)
{
    printf("%s", prompt);
    fflush(stdout);

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

static int read_choice(
    const char *prompt,
    int minimum,
    int maximum
)
{
    char input[64];
    char extra;
    int choice;

    if (!read_input(prompt, input, sizeof(input)))
        return -1;

    if (sscanf(input, "%d %c", &choice, &extra) != 1)
        return -1;

    if (choice < minimum || choice > maximum)
        return -1;

    return choice;
}

/*
 * Create a directory and any missing parent directories.
 */
static int create_directories(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    char buffer[PATH_SIZE];

    if (strlen(path) >= sizeof(buffer))
        return 0;

    strcpy(buffer, path);

    size_t length = strlen(buffer);

    while (length > 1 && buffer[length - 1] == '/')
        buffer[--length] = '\0';

    for (char *p = buffer + 1; ; p++)
    {
        if (*p == '/' || *p == '\0')
        {
            char saved = *p;
            *p = '\0';

            if (mkdir(buffer, 0755) != 0 && errno != EEXIST)
            {
                perror("mkdir");
                return 0;
            }

            struct stat info;

            if (stat(buffer, &info) != 0 ||
                !S_ISDIR(info.st_mode))
            {
                fprintf(stderr, "Not a directory: %s\n", buffer);
                return 0;
            }

            *p = saved;

            if (saved == '\0')
                break;
        }
    }

    return 1;
}

/*
 * Resolve the default output directory: ~/Videos/StreamSucker
 */
static int get_default_directory(
    char *buffer,
    size_t size
)
{
    const char *home = getenv("HOME");

    if (home == NULL || home[0] == '\0')
    {
        fprintf(stderr, "Cannot determine home directory.\n");
        return 0;
    }

    int length = snprintf(
        buffer,
        size,
        "%s/Videos/StreamSucker",
        home
    );

    if (length < 0 || (size_t)length >= size)
    {
        fprintf(stderr, "Output directory path is too long.\n");
        return 0;
    }

    return 1;
}

/*
 * Expand a leading ~ in a custom output directory.
 */
static int resolve_directory(
    const char *input,
    char *output,
    size_t size
)
{
    const char *home = getenv("HOME");

    if (input[0] == '~' &&
        (input[1] == '/' || input[1] == '\0'))
    {
        if (home == NULL || home[0] == '\0')
            return 0;

        int length = snprintf(
            output,
            size,
            "%s%s",
            home,
            input + 1
        );

        return length >= 0 && (size_t)length < size;
    }

    int length = snprintf(output, size, "%s", input);

    return length >= 0 && (size_t)length < size;
}

static const char *get_quality_selector(VideoQuality quality)
{
    switch (quality)
    {
        case QUALITY_2160:
            return "bv*[height<=2160]+ba/b[height<=2160]";

        case QUALITY_1080:
            return "bv*[height<=1080]+ba/b[height<=1080]";

        case QUALITY_720:
            return "bv*[height<=720]+ba/b[height<=720]";

        case QUALITY_480:
            return "bv*[height<=480]+ba/b[height<=480]";

        case QUALITY_BEST:
        default:
            return "bv*+ba/b";
    }
}

static const char *get_format_name(VideoFormat format)
{
    return format == FORMAT_WEBM ? "webm" : "mp4";
}

static int select_quality(VideoQuality *quality)
{
    printf("\n========== VIDEO QUALITY ==========\n");
    printf("1. Best available quality\n");
    printf("2. Up to 2160p (4K)\n");
    printf("3. Up to 1080p (Full HD)\n");
    printf("4. Up to 720p (HD)\n");
    printf("5. Up to 480p (SD)\n");
    printf("===================================\n");

    int choice = read_choice("Select quality: ", 1, 5);

    if (choice == -1)
        return 0;

    *quality = (VideoQuality)(choice - 1);

    return 1;
}

static int select_format(VideoFormat *format)
{
    printf("\n========== OUTPUT FORMAT ==========\n");
    printf("1. MP4\n");
    printf("2. WebM\n");
    printf("===================================\n");

    int choice = read_choice("Select format: ", 1, 2);

    if (choice == -1)
        return 0;

    *format = choice == 1 ? FORMAT_MP4 : FORMAT_WEBM;

    return 1;
}

void downloader_stage2(const char *url)
{
    if (!executable_exists("yt-dlp"))
    {
        fprintf(stderr, "\nError: yt-dlp is not installed.\n");
        fprintf(stderr, "Install using: sudo dnf install yt-dlp\n");
        return;
    }

    if (!executable_exists("ffmpeg"))
    {
        fprintf(stderr, "\nError: FFmpeg is not installed.\n");
        fprintf(stderr, "Install using: sudo dnf install ffmpeg\n");
        return;
    }

    VideoQuality quality;
    VideoFormat format;

    if (!select_quality(&quality))
    {
        printf("\nInvalid quality selection.\n");
        return;
    }

    if (!select_format(&format))
    {
        printf("\nInvalid format selection.\n");
        return;
    }

    char default_directory[PATH_SIZE];

    if (!get_default_directory(default_directory, sizeof(default_directory)))
        return;

    printf("\nDefault output directory: %s\n", default_directory);

    char custom_directory[PATH_SIZE];

    if (!read_input(
            "Output directory (press Enter for default): ",
            custom_directory,
            sizeof(custom_directory)))
    {
        return;
    }

    char output_directory[PATH_SIZE];

    if (custom_directory[0] == '\0')
    {
        strcpy(output_directory, default_directory);
    }
    else if (!resolve_directory(
                 custom_directory,
                 output_directory,
                 sizeof(output_directory)))
    {
        fprintf(stderr, "\nInvalid output directory.\n");
        return;
    }

    if (!create_directories(output_directory))
    {
        fprintf(stderr, "\nUnable to create output directory.\n");
        return;
    }

    const char *selector = get_quality_selector(quality);
    const char *format_name = get_format_name(format);

    printf("\n====================================\n");
    printf("        DOWNLOAD CONFIGURATION\n");
    printf("====================================\n");

    printf("Quality : %s\n",
        quality == QUALITY_BEST ? "Best available" :
        quality == QUALITY_2160 ? "Up to 2160p" :
        quality == QUALITY_1080 ? "Up to 1080p" :
        quality == QUALITY_720 ? "Up to 720p" :
        "Up to 480p");

    printf("Format  : %s\n", format_name);
    printf("Folder  : %s\n", output_directory);

    printf("====================================\n");

    printf("\nStarting download...\n\n");

    char *const args[] =
    {
        "yt-dlp",

        "--no-playlist",
        "--progress",
        "--newline",
        "--no-colors",

        "-f",
        (char *)selector,

        "--merge-output-format",
        (char *)format_name,

        "--remux-video",
        (char *)format_name,

        "-P",
        output_directory,

        "-o",
        "%(title)s.%(ext)s",

        "--",

        (char *)url,

        NULL
    };

    int result = run_process(args);

    printf("\n====================================\n");

    if (result == 0)
    {
        printf("Download completed successfully!\n");
        printf("Output directory: %s\n", output_directory);
    }
    else
    {
        printf("Download failed.\n");
        printf("yt-dlp exit code: %d\n", result);

        if (result == 130)
            printf("Download interrupted by user.\n");
    }

    printf("====================================\n");
}

