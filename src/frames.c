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

#include "frames.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define PATH_SIZE 4096

typedef enum
{
    FRAME_AT_TIMESTAMP = 1,
    FRAME_AT_INTERVAL,
    FRAME_ALL
} FrameMode;

/*
 * Read a line of input safely.
 */
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
    else if (!feof(stdin))
    {
        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF)
            ;

        fprintf(stderr, "Input is too long.\n");
        return 0;
    }

    return 1;
}

/*
 * Read and validate a numeric menu choice.
 */
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
 * Validate HH:MM:SS timestamp.
 */
static int validate_timestamp(const char *input)
{
    if (input == NULL)
        return 0;

    int hours;
    int minutes;
    int seconds;
    char extra;

    if (sscanf(
            input,
            "%d:%d:%d%c",
            &hours,
            &minutes,
            &seconds,
            &extra) != 3)
    {
        return 0;
    }

    if (hours < 0 ||
        minutes < 0 || minutes > 59 ||
        seconds < 0 || seconds > 59)
    {
        return 0;
    }

    return 1;
}

/*
 * Validate the frame interval in seconds.
 * Supports values such as 5, 2.5 and 0.5.
 */
static int validate_interval(
    const char *input,
    double *interval
)
{
    if (input == NULL || interval == NULL)
        return 0;

    errno = 0;

    char *endptr;
    double value = strtod(input, &endptr);

    if (errno != 0 ||
        endptr == input ||
        *endptr != '\0' ||
        !(value > 0.0) ||
        value > 86400.0)
    {
        return 0;
    }

    *interval = value;

    return 1;
}

/*
 * Create a directory and any missing parent directories.
 */
static int create_directories(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    char buffer[PATH_SIZE];

    int length = snprintf(buffer, sizeof(buffer), "%s", path);

    if (length < 0 || (size_t)length >= sizeof(buffer))
        return 0;

    length = (int)strlen(buffer);

    while (length > 1 && buffer[length - 1] == '/')
        buffer[--length] = '\0';

    char *p = buffer;

    if (buffer[0] == '/')
        p++;

    for (;; p++)
    {
        if (*p == '/' || *p == '\0')
        {
            char saved = *p;
            *p = '\0';

            if (buffer[0] != '\0')
            {
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
            }

            *p = saved;

            if (saved == '\0')
                break;
        }
    }

    return 1;
}

/*
 * Resolve the default directory.
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
        "%s/Videos/StreamSucker/Frames",
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
 * Expand ~/ in a custom output directory.
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

/*
 * Create a unique directory for each extraction.
 *
 * Example:
 * Frames/extraction-Ab12Cd/
 */
static int create_extraction_directory(
    const char *base_directory,
    char *output,
    size_t size
)
{
    int length = snprintf(
        output,
        size,
        "%s/extraction-XXXXXX",
        base_directory
    );

    if (length < 0 || (size_t)length >= size)
    {
        fprintf(stderr, "Extraction directory path is too long.\n");
        return 0;
    }

    if (mkdtemp(output) == NULL)
    {
        perror("mkdtemp");
        return 0;
    }

    return 1;
}

/*
 * Remove files from a temporary download directory.
 */
static void cleanup_temporary_directory(const char *path)
{
    DIR *directory = opendir(path);

    if (directory == NULL)
        return;

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char file_path[PATH_SIZE];

        int length = snprintf(
            file_path,
            sizeof(file_path),
            "%s/%s",
            path,
            entry->d_name
        );

        if (length < 0 || (size_t)length >= sizeof(file_path))
            continue;

        struct stat info;

        if (lstat(file_path, &info) == 0)
        {
            if (S_ISDIR(info.st_mode))
                rmdir(file_path);
            else
                unlink(file_path);
        }
    }

    closedir(directory);
    rmdir(path);
}

/*
 * Download the source video temporarily using yt-dlp.
 *
 * The video is saved as MKV for broad container compatibility.
 * The function captures the final file path printed by yt-dlp.
 */
static int download_source_video(
    const char *url,
    char *temporary_directory,
    size_t temporary_directory_size,
    char *video_path,
    size_t video_path_size
)
{
    char directory_template[] = "/tmp/stream-sucker-XXXXXX";

    char *created_directory = mkdtemp(directory_template);

    if (created_directory == NULL)
    {
        perror("mkdtemp");
        return 0;
    }

    int length = snprintf(
        temporary_directory,
        temporary_directory_size,
        "%s",
        created_directory
    );

    if (length < 0 ||
        (size_t)length >= temporary_directory_size)
    {
        cleanup_temporary_directory(created_directory);
        return 0;
    }

    char output_template[PATH_SIZE];

    length = snprintf(
        output_template,
        sizeof(output_template),
        "%s/source.%%(ext)s",
        temporary_directory
    );

    if (length < 0 || (size_t)length >= sizeof(output_template))
    {
        cleanup_temporary_directory(temporary_directory);
        return 0;
    }

    char captured_path[PATH_SIZE] = {0};

    char *const args[] =
    {
        "yt-dlp",

        "--quiet",
        "--no-warnings",
        "--no-progress",
        "--no-playlist",

        "--print",
        "after_move:filepath",

        "-f",
        "bv*+ba/b",

        "--merge-output-format",
        "mkv",

        "--remux-video",
        "mkv",

        "-o",
        output_template,

        "--",

        (char *)url,

        NULL
    };

    printf("\nDownloading source video temporarily...\n\n");

    int result = run_process_capture(
        args,
        captured_path,
        sizeof(captured_path)
    );

    if (result != 0)
    {
        fprintf(stderr, "\nUnable to download source video.\n");
        cleanup_temporary_directory(temporary_directory);
        return 0;
    }

    char *value = captured_path;

    while (isspace((unsigned char)*value))
        value++;

    char *newline = strpbrk(value, "\r\n");

    if (newline != NULL)
        *newline = '\0';

    if (value[0] == '\0')
    {
        fprintf(stderr, "yt-dlp did not return the downloaded file path.\n");
        cleanup_temporary_directory(temporary_directory);
        return 0;
    }

    length = snprintf(
        video_path,
        video_path_size,
        "%s",
        value
    );

    if (length < 0 || (size_t)length >= video_path_size)
    {
        fprintf(stderr, "Downloaded file path is too long.\n");
        cleanup_temporary_directory(temporary_directory);
        return 0;
    }

    struct stat info;

    if (stat(video_path, &info) != 0 ||
        !S_ISREG(info.st_mode))
    {
        fprintf(stderr, "Downloaded video file could not be found.\n");
        cleanup_temporary_directory(temporary_directory);
        return 0;
    }

    printf("\nSource video downloaded successfully.\n");

    return 1;
}

/*
 * Execute FFmpeg for frame extraction.
 */
static int extract_frames(
    FrameMode mode,
    const char *video_path,
    const char *timestamp,
    const char *interval_filter,
    const char *output_directory
)
{
    char output_pattern[PATH_SIZE];

    int length = snprintf(
        output_pattern,
        sizeof(output_pattern),
        "%s/frame_%%06d.jpg",
        output_directory
    );

    if (length < 0 || (size_t)length >= sizeof(output_pattern))
    {
        fprintf(stderr, "Output filename is too long.\n");
        return -1;
    }

    char *const timestamp_args[] =
    {
        "ffmpeg",

        "-hide_banner",
        "-nostdin",
        "-y",

        "-ss",
        (char *)timestamp,

        "-i",
        (char *)video_path,

        "-map",
        "0:v:0",

        "-frames:v",
        "1",

        "-q:v",
        "2",

        output_pattern,

        NULL
    };

    char *const interval_args[] =
    {
        "ffmpeg",

        "-hide_banner",
        "-nostdin",
        "-y",

        "-i",
        (char *)video_path,

        "-map",
        "0:v:0",

        "-vf",
        (char *)interval_filter,

        "-q:v",
        "2",

        output_pattern,

        NULL
    };

    char *const all_frames_args[] =
    {
        "ffmpeg",

        "-hide_banner",
        "-nostdin",
        "-y",

        "-i",
        (char *)video_path,

        "-map",
        "0:v:0",

        "-fps_mode",
        "passthrough",

        "-q:v",
        "2",

        output_pattern,

        NULL
    };

    printf("\nExtracting frames...\n\n");

    switch (mode)
    {
        case FRAME_AT_TIMESTAMP:
            return run_process(timestamp_args);

        case FRAME_AT_INTERVAL:
            return run_process(interval_args);

        case FRAME_ALL:
            return run_process(all_frames_args);

        default:
            return -1;
    }
}

/*
 * Main frame extraction interface.
 *
 * Kept as frames_stage1() to remain compatible with main.c.
 */
void frames_stage1(const char *url)
{
    if (url == NULL || url[0] == '\0')
    {
        fprintf(stderr, "Invalid URL.\n");
        return;
    }

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

    printf("\n========== FRAME EXTRACTION ==========\n");
    printf("1. Extract a frame at a timestamp\n");
    printf("2. Extract frames at a time interval\n");
    printf("3. Extract all frames\n");
    printf("======================================\n");

    int choice = read_choice("Select option: ", 1, 3);

    if (choice == -1)
    {
        printf("\nInvalid selection.\n");
        return;
    }

    FrameMode mode = (FrameMode)choice;

    char timestamp[64] = {0};
    char interval_input[64] = {0};
    char interval_filter[128] = {0};

    if (mode == FRAME_AT_TIMESTAMP)
    {
        if (!read_input(
                "\nTimestamp (HH:MM:SS): ",
                timestamp,
                sizeof(timestamp)))
        {
            return;
        }

        if (!validate_timestamp(timestamp))
        {
            printf("\nInvalid timestamp. Use HH:MM:SS.\n");
            return;
        }
    }
    else if (mode == FRAME_AT_INTERVAL)
    {
        printf("\nThe interval is measured in seconds.\n");
        printf("Examples: 5, 2.5, 0.5\n");

        if (!read_input(
                "Frame interval: ",
                interval_input,
                sizeof(interval_input)))
        {
            return;
        }

        double interval;

        if (!validate_interval(interval_input, &interval))
        {
            printf("\nInvalid interval. Enter a positive number up to 86400 seconds.\n");
            return;
        }

        int length = snprintf(
            interval_filter,
            sizeof(interval_filter),
            "fps=1/%.10g",
            interval
        );

        if (length < 0 ||
            (size_t)length >= sizeof(interval_filter))
        {
            printf("\nInvalid interval configuration.\n");
            return;
        }
    }
    else
    {
        char answer[16];

        printf("\nWarning: Extracting all frames can produce");
        printf(" thousands of image files.\n");

        if (!read_input(
                "Continue? (y/N): ",
                answer,
                sizeof(answer)))
        {
            return;
        }

        if (tolower((unsigned char)answer[0]) != 'y')
        {
            printf("\nFrame extraction cancelled.\n");
            return;
        }
    }

    /*
     * Configure output directory.
     */
    char output_base[PATH_SIZE];

    if (!get_default_directory(output_base, sizeof(output_base)))
        return;

    printf("\nDefault output directory: %s\n", output_base);

    char custom_directory[PATH_SIZE];

    if (!read_input(
            "Output directory (press Enter for default): ",
            custom_directory,
            sizeof(custom_directory)))
    {
        return;
    }

    if (custom_directory[0] != '\0')
    {
        if (!resolve_directory(
                custom_directory,
                output_base,
                sizeof(output_base)))
        {
            fprintf(stderr, "Invalid output directory.\n");
            return;
        }
    }

    if (!create_directories(output_base))
    {
        fprintf(stderr, "Unable to create output directory.\n");
        return;
    }

    char extraction_directory[PATH_SIZE];

    if (!create_extraction_directory(
            output_base,
            extraction_directory,
            sizeof(extraction_directory)))
    {
        fprintf(stderr, "Unable to create extraction directory.\n");
        return;
    }

    /*
     * Download the source video to a temporary location.
     */
    char temporary_directory[PATH_SIZE];
    char video_path[PATH_SIZE];

    if (!download_source_video(
            url,
            temporary_directory,
            sizeof(temporary_directory),
            video_path,
            sizeof(video_path)))
    {
        return;
    }

    /*
     * Display extraction configuration.
     */
    printf("\n======================================\n");
    printf("        FRAME EXTRACTION CONFIG\n");
    printf("======================================\n");

    if (mode == FRAME_AT_TIMESTAMP)
        printf("Mode     : Single frame\nTimestamp: %s\n", timestamp);
    else if (mode == FRAME_AT_INTERVAL)
        printf("Mode     : Interval extraction\nInterval : %s seconds\n",
               interval_input);
    else
        printf("Mode     : All frames\n");

    printf("Format   : JPEG\n");
    printf("Output   : %s\n", extraction_directory);

    printf("======================================\n");

    /*
     * Execute FFmpeg.
     */
    int result = extract_frames(
        mode,
        video_path,
        timestamp,
        interval_filter,
        extraction_directory
    );

    /*
     * Remove the temporary source video.
     */
    cleanup_temporary_directory(temporary_directory);

    printf("\n======================================\n");

    if (result == 0)
    {
        printf("Frame extraction completed successfully!\n");
        printf("Output directory: %s\n", extraction_directory);
    }
    else
    {
        printf("Frame extraction failed.\n");
        printf("FFmpeg exit code: %d\n", result);

        if (result == 130)
            printf("Extraction interrupted by user.\n");
    }

    printf("======================================\n");
}

