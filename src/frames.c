#include "frames.h"
#include "extractor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_number(const char *prompt, long *value)
{
    char buffer[128];
    char extra;

    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        return 0;

    if (sscanf(buffer, "%ld %c", value, &extra) != 1)
        return 0;

    return 1;
}

void frames_stage1(const char *url)
{
    (void)url;

    int choice;

    printf("\n========== FRAME EXTRACTION ==========\n");
    printf("1. Extract frame at specific timestamp\n");
    printf("2. Extract frames at regular intervals\n");
    printf("3. Extract all frames\n");
    printf("0. Back\n");
    printf("======================================\n");

    printf("Choice: ");

    if (scanf("%d", &choice) != 1)
    {
        printf("Invalid input.\n");

        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;

        return;
    }

    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;

    if (choice == 0)
        return;

    if (choice == 1)
    {
        char timestamp[64];
        long seconds;

        printf("Timestamp (HH:MM:SS): ");

        if (fgets(timestamp, sizeof(timestamp), stdin) == NULL)
            return;

        timestamp[strcspn(timestamp, "\n")] = '\0';

        if (!parse_timestamp(timestamp, &seconds))
        {
            printf("Invalid timestamp.\n");
            return;
        }

        printf("\nTimestamp validated: %s\n", timestamp);
    }
    else if (choice == 2)
    {
        long interval;

        if (!read_number("Interval in seconds: ", &interval) ||
            interval <= 0)
        {
            printf("Invalid interval.\n");
            return;
        }

        printf("\nInterval validated: %ld seconds\n", interval);
    }
    else if (choice == 3)
    {
        printf("\nAll-frame extraction selected.\n");
        printf("Warning: this can generate a very large number of images.\n");
    }
    else
    {
        printf("Invalid choice.\n");
        return;
    }

    printf("\nFrame extraction will be implemented in Stage 4.\n");
}
