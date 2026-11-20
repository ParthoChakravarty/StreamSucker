#include "extractor.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

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
    {
        end++;
    }

    if (end == host)
        return false;

    if (host[0] == '.')
        return false;

    return true;
}

bool parse_timestamp(const char *input, long *seconds)
{
    if (input == NULL || seconds == NULL)
        return false;

    int hours, minutes, secs;
    char extra;

    if (sscanf(input, "%d:%d:%d%c",
               &hours, &minutes, &secs, &extra) != 3)
        return false;

    if (hours < 0 || minutes < 0 || minutes > 59 ||
        secs < 0 || secs > 59)
        return false;

    if (hours > (2147483647L - 3599) / 3600)
        return false;

    *seconds = (long)hours * 3600 +
               (long)minutes * 60 +
               secs;

    return true;
}

void extractor_stage1(
    const char *url,
    const char *start,
    const char *end
)
{
    (void)url;

    long start_seconds;
    long end_seconds;

    if (!parse_timestamp(start, &start_seconds) ||
        !parse_timestamp(end, &end_seconds))
    {
        printf("\nInvalid timestamp format.\n");
        return;
    }

    if (end_seconds <= start_seconds)
    {
        printf("\nEnd time must be greater than start time.\n");
        return;
    }

    printf("\nSection configuration validated.\n");
    printf("Start: %s\n", start);
    printf("End:   %s\n", end);

    printf("\nActual downloading will be implemented in Stage 3.\n");
}
