#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum { OSG_LINE_PENDING, OSG_LINE_READY, OSG_LINE_TOO_LONG } osg_line_result_t;
typedef struct {
    char *buffer;
    size_t capacity, used;
    bool overflow;
} osg_line_t;
void osg_line_init(osg_line_t *line, char *buffer, size_t capacity);
/* The returned ready string is valid until the next input byte. */
osg_line_result_t osg_line_feed(osg_line_t *line, char byte);
