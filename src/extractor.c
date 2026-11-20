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

#include "extractor.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PATH_SIZE 4096
#define INPUT_SIZE 4096
#define METADATA_SIZE 256

typedef enum
{
    SECTION_BEST,
    SECTION_2160,
    SECTION_1080,
    SECTION_720,
    SECTION_480
} SectionQuality;

typedef enum
{
    SECTION_MP4,
    SECTION_WEBM
} SectionFormat;

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

bool is_valid_url(const char *url)
{
    if (url == NULL)
        return false;

    size_t length = strlen(url);

    if (length < 10 || length > 8192)
        return false;

    if (strchr(url, '\n') || strchr(url, '\r'))
        return false;

    const char *host;

    if (strncmp(url, "https://", 8) == 0)
        host = url + 8;
    else if (strncmp(url, "http://", 7) == 0)
        host = url + 7;
    else
        return false;

    if (*host == '\0' || *host == '/' ||
        *host == '?' || *host == '#')
        return false;

    const char *end = host;

    while (*end && *end != '/' &&
           *end != '?' && *end != '#')
        end++;

    if (end == host || host[0] == '.')
        return false;

    return true;
}

bool parse_timestamp(const char *input, long *seconds)
{
    if (input == NULL || seconds == NULL)
        return false;

    int hours;
    int minutes;
    int secs;
    char extra;

    if (sscanf(input, "%d:%d:%d%c",
               &hours, &minutes, &secs, &extra) != 3)
        return false;

    if (hours < 0 || minutes < 0 || minutes > 59 ||
        secs < 0 || secs > 59)
        return false;

    *seconds = (long)hours * 3600L +
               (long)minutes * 60L +
               (long)secs;

    return true;
}

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

static const char *quality_selector(SectionQuality quality)
{
    switch (quality)
    {
        case SECTION_2160:
            return "bv*[height<=2160]+ba/b[height<=2160]";

        case SECTION_1080:
            return "bv*[height<=1080]+ba/b[height<=1080]";

        case SECTION_720:
            return "bv*[height<=720]+ba/b[height<=720]";

        case SECTION_480:
            return "bv*[height<=480]+ba/b[height<=480]";

        case SECTION_BEST:
        default:
            return "bv*+ba/b";
    }
}

static int select_quality(SectionQuality *quality)
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

    *quality = (SectionQuality)(choice - 1);

    return 1;
}

static int select_format(SectionFormat *format)
{
    printf("\n========== OUTPUT FORMAT ==========\n");
    printf("1. MP4\n");
    printf("2. WebM\n");
    printf("===================================\n");

    int choice = read_choice("Select format: ", 1, 2);

    if (choice == -1)
        return 0;

    *format = choice == 1 ? SECTION_MP4 : SECTION_WEBM;

    return 1;
}

/*
 * Retrieve video duration in seconds.
 *
 * Returns:
 *  1 = duration available
 *  0 = duration unavailable
 * -1 = command failed
 */
static int get_video_duration(
    const char *url,
    double *duration
)
{
    char output[METADATA_SIZE];

    char *const args[] =
    {
        "yt-dlp",
        "--no-warnings",
        "--no-playlist",
        "--simulate",
        "--print",
        "%(duration)s",
        "--",
        (char *)url,
        NULL
    };

    int result = run_process_capture(
        args,
        output,
        sizeof(output)
    );

    if (result != 0)
        return -1;

    char *value = output;

    while (isspace((unsigned char)*value))
        value++;

    char *newline = strpbrk(value, "\r\n");

    if (newline != NULL)
        *newline = '\0';

    if (value[0] == '\0' || strcmp(value, "NA") == 0)
        return 0;

    errno = 0;

    char *endptr;
    double parsed = strtod(value, &endptr);

    if (errno != 0 || endptr == value || *endptr != '\0' ||
        parsed <= 0)
        return 0;

    *duration = parsed;

    return 1;
}

static void print_duration(double duration)
{
    long total = (long)duration;

    long hours = total / 3600;
    long minutes = (total % 3600) / 60;
    long seconds = total % 60;

    printf("%02ld:%02ld:%02ld", hours, minutes, seconds);
}

void extractor_stage3(const char *url)
{
    if (!executable_exists("yt-dlp"))
    {
        fprintf(stderr, "\nError: yt-dlp is not installed.\n");
        return;
    }

    if (!executable_exists("ffmpeg"))
    {
        fprintf(stderr, "\nError: FFmpeg is not installed.\n");
        return;
    }

    char start[64];
    char end[64];

    long start_seconds;
    long end_seconds;

    printf("\n========== SECTION DOWNLOAD ==========\n");

    if (!read_input(
            "Start time (HH:MM:SS): ",
            start,
            sizeof(start)))
        return;

    if (!read_input(
            "End time   (HH:MM:SS): ",
            end,
            sizeof(end)))
        return;

    if (!parse_timestamp(start, &start_seconds))
    {
        printf("\nInvalid start timestamp.\n");
        return;
    }

    if (!parse_timestamp(end, &end_seconds))
    {
        printf("\nInvalid end timestamp.\n");
        return;
    }

    if (end_seconds <= start_seconds)
    {
        printf("\nEnd time must be greater than start time.\n");
        return;
    }

    printf("\nFetching video duration...\n");

    double duration = 0;
    int duration_result = get_video_duration(url, &duration);

    if (duration_result == 1)
    {
        printf("Video duration: ");
        print_duration(duration);
        printf("\n");

        if ((double)end_seconds > duration)
        {
            printf("\nError: End time exceeds video duration.\n");
            return;
        }
    }
    else if (duration_result == 0)
    {
        printf("Warning: Video duration is unavailable.\n");
        printf("The section will be attempted without duration validation.\n");
    }
    else
    {
        printf("Warning: Unable to retrieve video metadata.\n");
        printf("The download will be attempted anyway.\n");
    }

    SectionQuality quality;
    SectionFormat format;

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

    char output_directory[PATH_SIZE];

    printf("\nDefault output directory: Videos/StreamSucker\n");

    if (!read_input(
            "Output directory (press Enter for default): ",
            output_directory,
            sizeof(output_directory)))
        return;

    if (output_directory[0] == '\0')
    {
        strcpy(output_directory, "Videos/StreamSucker");
    }

    if (!create_directories(output_directory))
    {
        printf("\nUnable to create output directory.\n");
        return;
    }

    char section[160];

    int length = snprintf(
        section,
        sizeof(section),
        "*%s-%s",
        start,
        end
    );

    if (length < 0 || (size_t)length >= sizeof(section))
    {
        printf("Invalid section configuration.\n");
        return;
    }

    const char *format_name =
        format == SECTION_MP4 ? "mp4" : "webm";

    const char *selector = quality_selector(quality);

    printf("\n====================================\n");
    printf("       SECTION DOWNLOAD CONFIG\n");
    printf("====================================\n");

    printf("Start   : %s\n", start);
    printf("End     : %s\n", end);

    printf("Duration: %ld seconds\n",
           end_seconds - start_seconds);

    printf("Quality : %s\n",
           quality == SECTION_BEST ? "Best available" :
           quality == SECTION_2160 ? "Up to 2160p" :
           quality == SECTION_1080 ? "Up to 1080p" :
           quality == SECTION_720 ? "Up to 720p" :
           "Up to 480p");

    printf("Format  : %s\n", format_name);
    printf("Folder  : %s\n", output_directory);

    printf("====================================\n");

    printf("\nStarting section download...\n\n");

    char *const args[] =
    {
        "yt-dlp",

        "--no-playlist",
        "--progress",
        "--newline",
        "--no-colors",

        "--download-sections",
        section,

        "--force-keyframes-at-cuts",

        "-f",
        (char *)selector,

        "--merge-output-format",
        (char *)format_name,

        "--remux-video",
        (char *)format_name,

        "-P",
        output_directory,

        "-o",
        "%(title)s_section.%(ext)s",

        "--",

        (char *)url,

        NULL
    };

    int result = run_process(args);

    printf("\n====================================\n");

    if (result == 0)
    {
        printf("Section downloaded successfully!\n");
        printf("Output directory: %s\n", output_directory);
    }
    else
    {
        printf("Section download failed.\n");
        printf("yt-dlp exit code: %d\n", result);

        if (result == 130)
            printf("Download interrupted by user.\n");
    }

    printf("====================================\n");
}

