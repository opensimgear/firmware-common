#include "opensimgear/actions.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { JSON_CAPACITY=OSG_ACTION_REPLY_CAPACITY-4, JSON_RESERVED=256 };
typedef struct { char *data; size_t capacity, used; bool failed; } writer_t;
static void append(writer_t *w, const char *s, size_t n) {
    if (n >= w->capacity || w->used >= w->capacity - n) { w->failed=true; return; }
    if (w->data) { memcpy(w->data+w->used,s,n); w->data[w->used+n]='\0'; }
    w->used+=n;
}
static void literal(writer_t *w, const char *s) { append(w,s,strlen(s)); }
static void number(writer_t *w, int64_t n) {
    char b[24]; int len=snprintf(b,sizeof b,"%lld",(long long)n);
    if (len>0) append(w,b,(size_t)len);
}
static void quoted(writer_t *w, const char *s) {
    static const char hex[]="0123456789ABCDEF";
    literal(w,"\"");
    for (const unsigned char *p=(const unsigned char *)s; *p; ++p) {
        if (*p=='"' || *p=='\\') { char b[2]={'\\',(char)*p}; append(w,b,2); }
        else if (*p<32) { char b[6]={'\\','u','0','0',hex[*p>>4],hex[*p&15]}; append(w,b,6); }
        else append(w,(const char *)p,1);
    }
    literal(w,"\"");
}
static bool utf8(const char *s, size_t max) {
    size_t n=0; while (n<=max && s[n]) ++n;
    if (n>max) return false;
    for (size_t i=0; i<n;) {
        uint32_t c=(unsigned char)s[i++], value=c; unsigned continuation=0;
        if (c<0x20 || c==0x7F) return false;
        if (c<0x80) continue;
        if (c>=0xC2 && c<=0xDF) { continuation=1; value=c&31; }
        else if (c>=0xE0 && c<=0xEF) { continuation=2; value=c&15; }
        else if (c>=0xF0 && c<=0xF4) { continuation=3; value=c&7; }
        else return false;
        if (n-i<continuation) return false;
        for (unsigned j=0;j<continuation;++j) {
            c=(unsigned char)s[i++]; if ((c&0xC0)!=0x80) return false;
            value=(value<<6)|(c&63);
        }
        if ((continuation==2 && value<0x800) || (continuation==3 && value<0x10000) ||
            (value>=0x80 && value<=0x9F) ||
            (value>=0xD800 && value<=0xDFFF) || value>0x10FFFF) return false;
    }
    return true;
}
static bool identifier(const char *s) {
    if (!s || !*s) return false;
    size_t n=0;
    while (*s) {
        char c=*s++;
        if (!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') ||
              c=='_' || c=='.' || c=='-') || ++n>=OSG_ACTION_ID_CAPACITY) return false;
    }
    return true;
}
static const char *type_name(osg_action_type_t type) {
    static const char *const names[]={"boolean","integer","enum","rgb","string"};
    return names[type];
}
static void descriptor(writer_t *w, const osg_action_descriptor_t *a) {
    literal(w,"{\"id\":"); quoted(w,a->id); literal(w,",\"label\":"); quoted(w,a->label);
    literal(w,",\"endpoint\":"); quoted(w,a->endpoint);
    literal(w,",\"semantics\":"); quoted(w,a->semantics==OSG_ACTION_SETTER ? "setter":"oneShot");
    literal(w,",\"readable\":"); literal(w,a->readable ? "true":"false");
    if(a->display) {
        literal(w,",\"display\":{\"columns\":");number(w,a->display->columns);
        literal(w,",\"rows\":");number(w,a->display->rows);
        literal(w,",\"encoding\":");quoted(w,a->display->encoding==OSG_ACTION_DISPLAY_ASCII?"ascii":"utf8");
        literal(w,"}");
    }
    literal(w,",\"parameters\":[");
    for (size_t i=0;i<a->parameter_count;++i) {
        const osg_action_parameter_t *p=&a->parameters[i];
        if (i) literal(w,",");
        literal(w,"{\"id\":"); quoted(w,p->id);
        literal(w,",\"label\":"); quoted(w,p->label); literal(w,",\"type\":"); quoted(w,type_name(p->type));
        if (p->type==OSG_ACTION_INTEGER) {
            literal(w,",\"minimum\":"); number(w,p->minimum); literal(w,",\"maximum\":"); number(w,p->maximum);
        } else if (p->type==OSG_ACTION_ENUM) {
            literal(w,",\"options\":[");
            for (size_t j=0;j<p->option_count;++j) { if(j) literal(w,","); quoted(w,p->options[j]); }
            literal(w,"]");
        } else if (p->type==OSG_ACTION_STRING) { literal(w,",\"maxBytes\":"); number(w,p->max_bytes); }
        literal(w,"}");
    }
    literal(w,"]}");
}
static void base_header(writer_t *w, const osg_actions_t *a) {
    literal(w,"{\"ok\":true,\"schemaVersion\":1,\"catalogueRevision\":"); number(w,a->catalogue_revision);
}
static void header(writer_t *w, const osg_actions_t *a) {
    base_header(w,a);
    if(a->ownership_enabled) literal(w,",\"ownership\":{\"schemaVersion\":1,\"scope\":\"endpoint\",\"maxLeaseMs\":5000}");
}
static void values(writer_t *w, const osg_action_value_t *v, size_t count) {
    literal(w,"[");
    for (size_t i=0;i<count;++i) {
        if(i) literal(w,",");
        switch(v[i].type) {
        case OSG_ACTION_BOOLEAN: literal(w,v[i].value.boolean ? "true":"false"); break;
        case OSG_ACTION_INTEGER: number(w,v[i].value.integer); break;
        case OSG_ACTION_ENUM: case OSG_ACTION_STRING: quoted(w,v[i].value.text); break;
        case OSG_ACTION_RGB: {
            char b[7]; (void)snprintf(b,sizeof b,"%06lX",(unsigned long)(v[i].value.rgb & UINT32_C(0xFFFFFF))); quoted(w,b); break;
        }
        }
    }
    literal(w,"]");
}
static bool valid_value(const osg_action_parameter_t *p, const osg_action_value_t *v) {
    if (v->type!=p->type) return false;
    switch(p->type) {
    case OSG_ACTION_BOOLEAN: return true;
    case OSG_ACTION_INTEGER: return v->value.integer>=p->minimum && v->value.integer<=p->maximum;
    case OSG_ACTION_RGB: return v->value.rgb<=0xFFFFFF;
    case OSG_ACTION_STRING: return utf8(v->value.text,p->max_bytes);
    case OSG_ACTION_ENUM:
        if (!utf8(v->value.text,OSG_ACTION_ID_CAPACITY-1)) return false;
        for(size_t i=0;i<p->option_count;++i) if (!strcmp(v->value.text,p->options[i])) return true;
        return false;
    }
    return false;
}
static bool valid_values(const osg_action_descriptor_t *d, const osg_action_value_t *v) {
    for(size_t i=0;i<d->parameter_count;++i) if(!valid_value(&d->parameters[i],&v[i])) return false;
    if(d->display) {
        unsigned cells=0;
        for(const unsigned char *p=(const unsigned char *)v[0].value.text;*p;p++) {
            if(d->display->encoding==OSG_ACTION_DISPLAY_ASCII && *p>=128) return false;
            if((*p&0xC0)!=0x80) cells++;
        }
        if(cells>(unsigned)d->display->columns*d->display->rows) return false;
    }
    return true;
}
bool osg_actions_init(osg_actions_t *a, const osg_action_descriptor_t *d, size_t count,
                      uint32_t revision, osg_action_invoke_fn invoke,
                      osg_action_read_fn read, void *context) {
    if (!a) return false;
    *a=(osg_actions_t){.descriptors=d,.count=count,.catalogue_revision=revision,
                       .invoke=invoke,.read=read,.context=context};
    if (!revision || count>OSG_ACTION_MAX_COUNT || (count && (!d || !invoke))) return false;
    size_t state_size=JSON_RESERVED;
    for(size_t i=0;i<count;++i) {
        const osg_action_descriptor_t *x=&d[i];
        if (!identifier(x->id) || !identifier(x->endpoint) || !x->label || !*x->label ||
            !utf8(x->label,OSG_ACTION_LABEL_CAPACITY-1) ||
            (x->semantics!=OSG_ACTION_SETTER && x->semantics!=OSG_ACTION_ONE_SHOT) ||
            x->parameter_count>OSG_ACTION_MAX_PARAMETERS || (x->parameter_count && !x->parameters) ||
            (x->readable && !read)) return false;
        for(size_t j=0;j<i;++j) if (!strcmp(x->id,d[j].id)) return false;
        size_t command_size=25+strlen(x->id), action_state_size=48+strlen(x->id);
        for(size_t j=0;j<x->parameter_count;++j) {
            const osg_action_parameter_t *p=&x->parameters[j];
            if (!identifier(p->id) || !p->label || !*p->label || !utf8(p->label,OSG_ACTION_LABEL_CAPACITY-1) ||
                (unsigned)p->type>OSG_ACTION_STRING) return false;
            for(size_t k=0;k<j;++k) if (!strcmp(p->id,x->parameters[k].id)) return false;
            size_t wire=1, json=5;
            if (p->type==OSG_ACTION_INTEGER) {
                if (p->minimum>p->maximum) return false;
                wire=11; json=11;
            } else if(p->type==OSG_ACTION_ENUM) {
                if (!p->option_count || p->option_count>OSG_ACTION_MAX_OPTIONS || !p->options) return false;
                for(size_t k=0;k<p->option_count;++k) {
                    if (!identifier(p->options[k])) return false;
                    for(size_t m=0;m<k;++m) if (!strcmp(p->options[k],p->options[m])) return false;
                    size_t n=strlen(p->options[k]); if(n>wire) wire=n;
                }
                json=wire+2;
            } else if(p->type==OSG_ACTION_RGB) { wire=6; json=8; }
            else if(p->type==OSG_ACTION_STRING) {
                if (!p->max_bytes || p->max_bytes>=OSG_ACTION_STRING_CAPACITY) return false;
                wire=2*p->max_bytes; json=6*p->max_bytes+2;
            }
            command_size+=1+wire; action_state_size+=1+json;
        }
        if(x->display) {
            const osg_action_display_t *display=x->display;
            unsigned cells=(unsigned)display->columns*display->rows;
            if(!display->columns || display->columns>63 || !display->rows || display->rows>4 ||
               cells>63 || (unsigned)display->encoding>OSG_ACTION_DISPLAY_ASCII ||
               x->semantics!=OSG_ACTION_SETTER || !x->readable || x->parameter_count!=1 ||
               x->parameters[0].type!=OSG_ACTION_STRING || x->parameters[0].max_bytes<cells) return false;
        }
        if(command_size>=OSG_ACTION_COMMAND_CAPACITY) return false;
        if(x->readable) state_size+=action_state_size;
        writer_t w={.capacity=JSON_CAPACITY};
        descriptor(&w,x); if(w.failed || w.used+JSON_RESERVED>=JSON_CAPACITY) return false;
    }
    if(state_size>=JSON_CAPACITY) return false;
    a->valid=true; return true;
}
static bool unsigned_number(const char *s, uint32_t *value) {
    if(!*s) return false;
    uint32_t n=0;
    while(*s) {
        if(*s<'0'||*s>'9') return false;
        unsigned d=(unsigned)(*s++-'0');
        if(n>(UINT32_MAX-d)/10) return false;
        n=n*10+d;
    }
    *value=n; return true;
}
static bool signed_number(const char *s, int32_t *value) {
    bool negative=*s=='-'; if(negative) ++s; uint32_t magnitude;
    if(!unsigned_number(s,&magnitude) || magnitude>(negative ? UINT32_C(2147483648):INT32_MAX)) return false;
    *value=negative ? (int32_t)(-(int64_t)magnitude):(int32_t)magnitude; return true;
}
static int hex_digit(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
static bool parse_value(const osg_action_parameter_t *p, const char *s, osg_action_value_t *v) {
    v->type=p->type;
    if(p->type==OSG_ACTION_BOOLEAN) {
        if(strcmp(s,"0") && strcmp(s,"1")) return false;
        v->value.boolean=*s=='1';
    } else if(p->type==OSG_ACTION_INTEGER) { if(!signed_number(s,&v->value.integer)) return false; }
    else if(p->type==OSG_ACTION_RGB) {
        if(strlen(s)!=6) return false;
        v->value.rgb=0;
        for(size_t i=0;i<6;++i) { int d=hex_digit(s[i]); if(d<0) return false; v->value.rgb=(v->value.rgb<<4)|(unsigned)d; }
    } else if(p->type==OSG_ACTION_ENUM) {
        if(strlen(s)>=sizeof v->value.text) return false;
        strcpy(v->value.text,s);
    } else {
        size_t n=!strcmp(s,"-") ? 0:strlen(s);
        if(n%2 || n/2>p->max_bytes) return false;
        for(size_t i=0;i<n;i+=2) {
            int high=hex_digit(s[i]),low=hex_digit(s[i+1]); if(high<0||low<0||!(high||low)) return false;
            v->value.text[i/2]=(char)((high<<4)|low);
        }
        v->value.text[n/2]='\0';
    }
    return valid_value(p,v);
}
static void error(char *reply, size_t capacity, const char *message) {
    if (!reply || !capacity) return;
    int n=snprintf(reply,capacity,"{\"ok\":false,\"error\":\"%s\"}",message);
    if(n<0 || (size_t)n>=capacity) reply[0]='\0';
}
static size_t endpoint_index(const osg_actions_t *a,size_t index) {
    for(size_t i=0;i<index;i++) if(!strcmp(a->descriptors[i].endpoint,a->descriptors[index].endpoint)) return i;
    return index;
}
static bool token(const char *text,char result[OSG_ACTION_TOKEN_CAPACITY]) {
    if(strlen(text)!=36) return false;
    bool nonzero=false;
    for(unsigned i=0;i<36;i++) {
        if(i==8 || i==13 || i==18 || i==23) {if(text[i]!='-') return false;result[i]='-';}
        else {int value=hex_digit(text[i]);if(value<0) return false;result[i]="0123456789abcdef"[value];nonzero|=value!=0;}
    }
    result[36]=0;return nonzero;
}
static bool ttl(const char *text,uint32_t *value) {
    return unsigned_number(text,value) && *value>=OSG_ACTION_LEASE_MIN_MS && *value<=OSG_ACTION_LEASE_MAX_MS;
}
bool osg_actions_enable_ownership(osg_actions_t *a) {
    if(!a || !a->valid) return false;
    for(size_t i=0;i<a->count;i++) {
        const osg_action_descriptor_t *d=&a->descriptors[i];size_t bytes=89+strlen(d->id);
        for(size_t j=0;j<d->parameter_count;j++) {
            const osg_action_parameter_t *p=&d->parameters[j];size_t wire=1;
            if(p->type==OSG_ACTION_INTEGER) wire=11;
            else if(p->type==OSG_ACTION_RGB) wire=6;
            else if(p->type==OSG_ACTION_STRING) wire=2*p->max_bytes;
            else if(p->type==OSG_ACTION_ENUM) for(size_t k=0;k<p->option_count;k++) if(strlen(p->options[k])>wire) wire=strlen(p->options[k]);
            bytes+=1+wire;
        }
        if(bytes>=OSG_ACTION_COMMAND_CAPACITY) return false;
    }
    a->ownership_enabled=true;return true;
}
static bool fallback(osg_actions_t *a,osg_action_lease_t *lease) {
    const osg_action_descriptor_t *d=&a->descriptors[lease->action_index];
    osg_action_value_t v[OSG_ACTION_MAX_PARAMETERS];memcpy(v,lease->fallback,sizeof v);
    lease->owned=false;memset(lease->token,0,sizeof lease->token);
    bool ok=a->invoke(a->context,d->id,v,d->parameter_count) &&
        a->read(a->context,d->id,v,d->parameter_count) && valid_values(d,v);
    for(size_t i=0;ok && i<d->parameter_count;i++) {
        const osg_action_value_t *wanted=&lease->fallback[i],*actual=&v[i];
        switch(wanted->type) {
        case OSG_ACTION_BOOLEAN:ok=wanted->value.boolean==actual->value.boolean;break;
        case OSG_ACTION_INTEGER:ok=wanted->value.integer==actual->value.integer;break;
        case OSG_ACTION_RGB:ok=wanted->value.rgb==actual->value.rgb;break;
        case OSG_ACTION_ENUM:case OSG_ACTION_STRING:ok=!strcmp(wanted->value.text,actual->value.text);break;
        }
    }
    lease->fallback_status=ok?1:2;return ok;
}
void osg_actions_tick(osg_actions_t *a,uint32_t now) {
    if(!a || !a->valid) return;
    a->now=now;
    if(a->ownership_enabled) for(size_t i=0;i<a->count;i++) if(a->leases[i].owned && (int32_t)(now-a->leases[i].expires_at)>=0) (void)fallback(a,&a->leases[i]);
}
void osg_actions_invalidate_scope(osg_actions_t *a,osg_action_scope_t scope,uint32_t now) {
    if(!a || !a->valid) return;
    osg_actions_tick(a,now);
    if(a->ownership_enabled) for(size_t i=0;i<a->count;i++) if(a->leases[i].owned && a->leases[i].scope==scope) (void)fallback(a,&a->leases[i]);
}
static osg_action_lease_t *owned(osg_actions_t *a,const char *secret,uint32_t id,osg_action_scope_t scope,const char **why) {
    for(size_t i=0;i<a->count;i++) if(a->leases[i].id==id && a->leases[i].owned) {
        if(strcmp(a->leases[i].token,secret) || a->leases[i].scope!=scope) {*why="outputNotOwned";return NULL;}
        return &a->leases[i];
    }
    for(size_t i=0;i<a->count;i++) if(a->leases[i].owned && !strcmp(a->leases[i].token,secret)) {*why="staleOutputLease";return NULL;}
    *why="outputNotOwned";return NULL;
}
static void lease_reply(writer_t *w,const osg_actions_t *a,const osg_action_lease_t *lease,bool released) {
    header(w,a);literal(w,",\"leaseId\":");number(w,lease->id);
    literal(w,",\"endpoint\":");quoted(w,a->descriptors[lease->action_index].endpoint);
    if(released) literal(w,",\"released\":true");
    else {literal(w,",\"remainingMs\":");number(w,(uint32_t)(lease->expires_at-a->now));}
    literal(w,"}");
}
static void lease_entry(writer_t *w,const osg_actions_t *a,size_t index) {
    const osg_action_lease_t *lease=&a->leases[index];
    const osg_action_descriptor_t *d=&a->descriptors[lease->id?lease->action_index:index];
    literal(w,"{\"endpoint\":");quoted(w,d->endpoint);literal(w,",\"actionId\":");quoted(w,d->id);
    literal(w,",\"owned\":");literal(w,lease->owned?"true":"false");literal(w,",\"leaseId\":");number(w,lease->id);
    literal(w,",\"remainingMs\":");number(w,lease->owned?(uint32_t)(lease->expires_at-a->now):0);
    literal(w,",\"fallback\":");values(w,lease->fallback,lease->id?d->parameter_count:0);
    literal(w,",\"fallbackStatus\":");quoted(w,lease->fallback_status==1?"applied":lease->fallback_status==2?"failed":"none");literal(w,"}");
}
bool osg_actions_command_at(osg_actions_t *a, const char *line,uint32_t now,osg_action_scope_t scope,char *reply, size_t capacity) {
    if(!line || strncmp(line,"action",6) || (line[6] && line[6]!=' ' && line[6]!='\t')) return false;
    if(!a || !a->valid) { error(reply,capacity,"invalidCatalogue"); return true; }
    osg_actions_tick(a,now);
    if(!reply || capacity<OSG_ACTION_REPLY_CAPACITY) { error(reply,capacity,"replyTooSmall"); return true; }
    if(strlen(line)>=OSG_ACTION_COMMAND_CAPACITY) { error(reply,capacity,"invalidArgument"); return true; }
    char command[OSG_ACTION_COMMAND_CAPACITY]; strcpy(command,line);
    char *tokens[OSG_ACTION_MAX_PARAMETERS+6]; size_t count=0;
    for(char *p=command;*p;) {
        while(*p==' '||*p=='\t') ++p;
        if(!*p) break;
        if(count>=sizeof tokens/sizeof tokens[0]) { error(reply,capacity,"invalidArgument"); return true; }
        tokens[count++]=p; while(*p && *p!=' ' && *p!='\t') ++p; if(*p) *p++='\0';
    }
    if(count<2) { error(reply,capacity,"unknownCommand"); return true; }
    writer_t w={.data=reply,.capacity=JSON_CAPACITY}; reply[0]='\0';
    if(!strcmp(tokens[1],"describe")) {
        uint32_t offset;
        if(count!=3 || !unsigned_number(tokens[2],&offset) || offset>a->count) { error(reply,capacity,"invalidArgument"); return true; }
        /* Measure each entry first: retain room for the bounded header/footer. */
        size_t end=offset, used=JSON_RESERVED;
        while(end<a->count && end-offset<OSG_ACTION_PAGE_SIZE) {
            writer_t measure={.capacity=SIZE_MAX}; descriptor(&measure,&a->descriptors[end]);
            if(used+measure.used+1>=JSON_CAPACITY) break;
            used+=measure.used+1; ++end;
        }
        header(&w,a); literal(&w,",\"total\":"); number(&w,a->count); literal(&w,",\"next\":");
        if(end<a->count) number(&w,end); else literal(&w,"null"); literal(&w,",\"actions\":[");
        for(size_t i=offset;i<end;++i) { if(i>offset) literal(&w,","); descriptor(&w,&a->descriptors[i]); }
        literal(&w,"]}");
    } else if(!strcmp(tokens[1],"state")) {
        if(count!=2) { error(reply,capacity,"invalidArgument"); return true; }
        header(&w,a); literal(&w,",\"states\":["); bool comma=false;
        for(size_t i=0;i<a->count;++i) {
            const osg_action_descriptor_t *d=&a->descriptors[i]; if(!d->readable) continue;
            osg_action_value_t v[OSG_ACTION_MAX_PARAMETERS]={0};
            if(!a->read(a->context,d->id,v,d->parameter_count) || !valid_values(d,v)) { error(reply,capacity,"failed"); return true; }
            if(comma) literal(&w,",");
            comma=true; literal(&w,"{\"id\":"); quoted(&w,d->id);
            literal(&w,",\"values\":"); values(&w,v,d->parameter_count); literal(&w,"}");
        }
        literal(&w,"]}");
    } else if(!strcmp(tokens[1],"ownership")) {
        if(!a->ownership_enabled) {error(reply,capacity,"ownershipUnsupported");return true;}
        size_t indexes[OSG_ACTION_MAX_COUNT],total=0;
        for(size_t i=0;i<a->count;i++) if(endpoint_index(a,i)==i) indexes[total++]=i;
        uint32_t offset;if(count!=3 || !unsigned_number(tokens[2],&offset) || offset>total) {error(reply,capacity,"invalidArgument");return true;}
        size_t end=offset,used=JSON_RESERVED;
        while(end<total && end-offset<OSG_ACTION_PAGE_SIZE) {
            writer_t measure={.capacity=SIZE_MAX};lease_entry(&measure,a,indexes[end]);
            if(used+measure.used+1>=JSON_CAPACITY) break;
            used+=measure.used+1;end++;
        }
        base_header(&w,a);literal(&w,",\"total\":");number(&w,total);literal(&w,",\"next\":");
        if(end<total) number(&w,end);else literal(&w,"null");literal(&w,",\"ownership\":[");
        for(size_t i=offset;i<end;i++) {if(i>offset) literal(&w,",");lease_entry(&w,a,indexes[i]);}
        literal(&w,"]}");
    } else if(!strcmp(tokens[1],"renew") || !strcmp(tokens[1],"release")) {
        bool releasing=!strcmp(tokens[1],"release");uint32_t id,duration=0;char secret[OSG_ACTION_TOKEN_CAPACITY];
        if(!a->ownership_enabled) {error(reply,capacity,"ownershipUnsupported");return true;}
        if((unsigned)scope>OSG_ACTION_SCOPE_CAN || count!=(releasing?4U:5U) || !token(tokens[2],secret) || !unsigned_number(tokens[3],&id) || !id ||
           (!releasing && !ttl(tokens[4],&duration))) {error(reply,capacity,"invalidArgument");return true;}
        const char *why;osg_action_lease_t *lease=owned(a,secret,id,scope,&why);if(!lease) {error(reply,capacity,why);return true;}
        if(releasing) {if(!fallback(a,lease)) {error(reply,capacity,"fallbackFailed");return true;}}
        else lease->expires_at=now+duration;
        lease_reply(&w,a,lease,releasing);
    } else if(!strcmp(tokens[1],"claim")) {
        uint32_t revision,duration;char secret[OSG_ACTION_TOKEN_CAPACITY];
        if(!a->ownership_enabled) {error(reply,capacity,"ownershipUnsupported");return true;}
        if((unsigned)scope>OSG_ACTION_SCOPE_CAN || count<6 || !unsigned_number(tokens[2],&revision) || !token(tokens[4],secret) || !ttl(tokens[5],&duration)) {error(reply,capacity,"invalidArgument");return true;}
        if(revision!=a->catalogue_revision) {error(reply,capacity,"staleCatalogue");return true;}
        size_t index=0;for(;index<a->count;index++) if(!strcmp(a->descriptors[index].id,tokens[3])) break;
        if(index==a->count) {error(reply,capacity,"unknownAction");return true;}
        const osg_action_descriptor_t *d=&a->descriptors[index];
        if(!d->readable || d->semantics!=OSG_ACTION_SETTER || count!=6u+d->parameter_count) {error(reply,capacity,"invalidArgument");return true;}
        osg_action_value_t v[OSG_ACTION_MAX_PARAMETERS]={0};
        for(size_t i=0;i<d->parameter_count;i++) if(!parse_value(&d->parameters[i],tokens[6+i],&v[i])) {error(reply,capacity,"invalidArgument");return true;}
        if(!valid_values(d,v)) {error(reply,capacity,"invalidArgument");return true;}
        osg_action_lease_t *lease=&a->leases[endpoint_index(a,index)];
        if(lease->owned && (strcmp(lease->token,secret) || lease->scope!=scope)) {error(reply,capacity,"outputBusy");return true;}
        if(!lease->owned) {if(a->lease_counter==UINT32_MAX) {error(reply,capacity,"leaseExhausted");return true;}lease->id=++a->lease_counter;}
        strcpy(lease->token,secret);memcpy(lease->fallback,v,sizeof v);lease->scope=scope;lease->owned=true;
        lease->action_index=(uint8_t)index;lease->fallback_status=0;lease->expires_at=now+duration;
        lease_reply(&w,a,lease,false);
    } else if(!strcmp(tokens[1],"invoke") || !strcmp(tokens[1],"invoke_owned")) {
        bool leased=!strcmp(tokens[1],"invoke_owned");size_t base=leased?6:4;
        osg_action_lease_t *lease=NULL;
        if(leased) {
            uint32_t id;char secret[OSG_ACTION_TOKEN_CAPACITY];
            if(!a->ownership_enabled) {error(reply,capacity,"ownershipUnsupported");return true;}
            if((unsigned)scope>OSG_ACTION_SCOPE_CAN || count<base || !token(tokens[2],secret) || !unsigned_number(tokens[3],&id) || !id) {error(reply,capacity,"invalidArgument");return true;}
            const char *why;lease=owned(a,secret,id,scope,&why);if(!lease) {error(reply,capacity,why);return true;}
        }
        uint32_t revision;
        if(count<base || !unsigned_number(tokens[base-2],&revision)) { error(reply,capacity,"invalidArgument"); return true; }
        if(revision!=a->catalogue_revision) { error(reply,capacity,"staleCatalogue"); return true; }
        const osg_action_descriptor_t *d=NULL;
        size_t index=0;for(;index<a->count;index++) if(!strcmp(a->descriptors[index].id,tokens[base-1])) {d=&a->descriptors[index];break;}
        if(!d) { error(reply,capacity,"unknownAction"); return true; }
        if(count!=base+d->parameter_count) { error(reply,capacity,"invalidArgument"); return true; }
        osg_action_lease_t *endpoint=&a->leases[endpoint_index(a,index)];
        if(leased && lease!=endpoint) {error(reply,capacity,"outputNotOwned");return true;}
        if(!leased && endpoint->owned) {error(reply,capacity,"outputBusy");return true;}
        osg_action_value_t v[OSG_ACTION_MAX_PARAMETERS]={0};
        for(size_t i=0;i<d->parameter_count;++i) if(!parse_value(&d->parameters[i],tokens[base+i],&v[i])) { error(reply,capacity,"invalidArgument"); return true; }
        if(!valid_values(d,v)) { error(reply,capacity,"invalidArgument"); return true; }
        if(!a->invoke(a->context,d->id,v,d->parameter_count) ||
           (d->readable && (!a->read(a->context,d->id,v,d->parameter_count) || !valid_values(d,v)))) { error(reply,capacity,"failed"); return true; }
        header(&w,a); literal(&w,",\"id\":"); quoted(&w,d->id); literal(&w,",\"values\":"); values(&w,v,d->parameter_count); literal(&w,"}");
    } else { error(reply,capacity,"unknownCommand"); return true; }
    if(w.failed) error(reply,capacity,"replyTooSmall");
    return true;
}
bool osg_actions_command(osg_actions_t *a,const char *line,char *reply,size_t capacity) {
    return osg_actions_command_at(a,line,a?a->now:0,OSG_ACTION_SCOPE_LOCAL,reply,capacity);
}
