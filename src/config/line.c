#include "opensimgear/line.h"

void osg_line_init(osg_line_t *l, char *buffer, size_t capacity) {
    *l = (osg_line_t){.buffer = buffer, .capacity = capacity};
    if (capacity) buffer[0] = '\0';
}

osg_line_result_t osg_line_feed(osg_line_t *l, char byte) {
    if (byte == '\r') return OSG_LINE_PENDING;
    if (byte == '\n') {
        osg_line_result_t result = l->overflow ? OSG_LINE_TOO_LONG : OSG_LINE_READY;
        if (l->capacity) l->buffer[l->used] = '\0';
        l->used = 0;
        l->overflow = false;
        return result;
    }
    if (byte == '\0' || l->capacity < 2 || l->used >= l->capacity - 1) {
        l->overflow = true;
    } else if (!l->overflow) {
        l->buffer[l->used++] = byte;
    }
    return OSG_LINE_PENDING;
}
