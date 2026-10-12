#include "opensimgear/button.h"
#include "opensimgear/line.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    osg_button_t b;
    osg_button_rearm(&b, true, 0, 10);
    assert(!osg_button_update(&b, true, 100));
    assert(!osg_button_update(&b, false, 101));
    assert(!osg_button_update(&b, true, 105)); /* release bounce is not rearming */
    assert(!osg_button_update(&b, false, 106));
    assert(!osg_button_update(&b, false, 116));
    assert(!osg_button_update(&b, true, 117));
    assert(osg_button_update(&b, true, 127));
    assert(osg_button_update(&b, false, 128));
    assert(!osg_button_update(&b, false, 138));
    osg_button_rearm(&b, true, 139, 10); /* a held reconnect cannot replay */
    assert(!osg_button_update(&b, true, 200));
    osg_button_rearm(&b, false, UINT32_MAX - 5, 10);
    assert(!osg_button_update(&b, false, 4));
    assert(!osg_button_update(&b, true, 5));
    assert(osg_button_update(&b, true, 15));

    char buffer[5]; osg_line_t l;
    osg_line_init(&l, buffer, sizeof buffer);
    for (const char *s = "help\r"; *s; ++s) assert(osg_line_feed(&l, *s) == OSG_LINE_PENDING);
    assert(osg_line_feed(&l, '\n') == OSG_LINE_READY);
    assert(strcmp(buffer, "help") == 0);
    for (const char *s = "toolonghelp"; *s; ++s) osg_line_feed(&l, *s);
    assert(osg_line_feed(&l, '\n') == OSG_LINE_TOO_LONG);
    osg_line_feed(&l, 'o'); osg_line_feed(&l, 'k');
    assert(osg_line_feed(&l, '\n') == OSG_LINE_READY);
    assert(strcmp(buffer, "ok") == 0);
    osg_line_feed(&l, '\0');
    assert(osg_line_feed(&l, '\n') == OSG_LINE_TOO_LONG);
    return 0;
}
