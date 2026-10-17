#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { OSG_ACTION_SCHEMA_VERSION=1, OSG_ACTION_MAX_COUNT=16,
       OSG_ACTION_MAX_PARAMETERS=4, OSG_ACTION_MAX_OPTIONS=16,
       OSG_ACTION_ID_CAPACITY=32, OSG_ACTION_LABEL_CAPACITY=64,
       OSG_ACTION_STRING_CAPACITY=64, OSG_ACTION_COMMAND_CAPACITY=384,
       OSG_ACTION_REPLY_CAPACITY=4096, OSG_ACTION_PAGE_SIZE=4,
       OSG_ACTION_LEASE_MIN_MS=250, OSG_ACTION_LEASE_MAX_MS=5000,
       OSG_ACTION_TOKEN_CAPACITY=37 };
typedef enum { OSG_ACTION_BOOLEAN, OSG_ACTION_INTEGER, OSG_ACTION_ENUM,
               OSG_ACTION_RGB, OSG_ACTION_STRING } osg_action_type_t;
typedef enum { OSG_ACTION_SETTER, OSG_ACTION_ONE_SHOT } osg_action_semantics_t;
typedef enum { OSG_ACTION_DISPLAY_UTF8, OSG_ACTION_DISPLAY_ASCII } osg_action_display_encoding_t;
/* One Unicode scalar per logical cell for UTF-8; one byte per cell for ASCII.
 * Hardware character repertoire/font ownership remains with the callback. */
typedef struct {
    uint8_t columns, rows;
    osg_action_display_encoding_t encoding;
} osg_action_display_t;
typedef struct {
    const char *id, *label;
    osg_action_type_t type;
    int32_t minimum, maximum; /* INTEGER only. */
    const char *const *options; uint8_t option_count; /* ENUM only. */
    uint8_t max_bytes; /* STRING only, UTF-8 bytes excluding NUL. */
} osg_action_parameter_t;
typedef struct {
    const char *id, *label, *endpoint;
    osg_action_semantics_t semantics;
    bool readable;
    const osg_action_parameter_t *parameters; uint8_t parameter_count;
    /* Optional, readable one-string setter only. Full row-major surface. */
    const osg_action_display_t *display;
} osg_action_descriptor_t;
typedef struct {
    osg_action_type_t type;
    union { bool boolean; int32_t integer; uint32_t rgb;
            char text[OSG_ACTION_STRING_CAPACITY]; } value;
} osg_action_value_t;
typedef enum { OSG_ACTION_SCOPE_LOCAL, OSG_ACTION_SCOPE_USB, OSG_ACTION_SCOPE_CAN } osg_action_scope_t;
typedef struct {
    char token[OSG_ACTION_TOKEN_CAPACITY];
    osg_action_value_t fallback[OSG_ACTION_MAX_PARAMETERS];
    osg_action_value_t last_owned[OSG_ACTION_MAX_PARAMETERS];
    uint32_t id,expires_at;
    uint8_t action_index,fallback_status;
    osg_action_scope_t scope;
    bool owned,last_owned_valid;
} osg_action_lease_t;
/* Callbacks run only after complete command and catalogue validation. read must
 * supply one correctly typed value per parameter. A false return is a device
 * failure; it is not a request to retry a one-shot action. The registry, tick,
 * scope invalidation and endpoint state callbacks must run in one serialized,
 * non-reentrant owner context. This makes compare-before-restore indivisible
 * with respect to all product writes of the same software endpoint state. */
typedef bool (*osg_action_invoke_fn)(void *context, const char *id,
                                   const osg_action_value_t *values, size_t count);
typedef bool (*osg_action_read_fn)(void *context, const char *id,
                                 osg_action_value_t *values, size_t count);
typedef struct {
    const osg_action_descriptor_t *descriptors; size_t count;
    uint32_t catalogue_revision;
    osg_action_invoke_fn invoke; osg_action_read_fn read; void *context;
    bool valid;
    bool ownership_enabled;
    uint32_t lease_counter,now;
    /* Indexed by the first descriptor for each endpoint; aliases share it. */
    osg_action_lease_t leases[OSG_ACTION_MAX_COUNT];
} osg_actions_t;

/* Catalogue storage belongs to the caller and remains immutable/alive. Invalid
 * IDs, bounds, duplicate IDs, oversized wire representations and zero revision
 * fail closed. No configuration writer claim or persistent storage is used. */
bool osg_actions_init(osg_actions_t *, const osg_action_descriptor_t *, size_t,
                      uint32_t catalogue_revision, osg_action_invoke_fn,
                      osg_action_read_fn, void *context);
/* Recognizes the full singular `action ...` command. Unrelated commands return
 * false without writing. Recognized commands return true and bounded JSON. A
 * reply buffer smaller than OSG_ACTION_REPLY_CAPACITY never executes an action.
 * Transport request IDs/deduplication are owned by the caller, not this API. */
bool osg_actions_command(osg_actions_t *, const char *line, char *reply, size_t capacity);
/* Enable endpoint leases only when owned command representations fit. Product
 * loops tick independently of host traffic and invalidate the affected transport
 * scope on connection/route loss. Catalogue and callback lifetime is unchanged. */
bool osg_actions_enable_ownership(osg_actions_t *);
void osg_actions_tick(osg_actions_t *,uint32_t now);
void osg_actions_invalidate_scope(osg_actions_t *,osg_action_scope_t,uint32_t now);
/* Expired leases compare current state with the last validated owned tuple and
 * restore only while unchanged, independently of reply capacity. A mismatch
 * preserves current state and relinquishes ownership; unreadable/uncertain state
 * never authorizes a fallback write.
 * Explicit commands never invoke callbacks with an undersized reply buffer. */
bool osg_actions_command_at(osg_actions_t *,const char *line,uint32_t now,osg_action_scope_t,
                            char *reply,size_t capacity);
