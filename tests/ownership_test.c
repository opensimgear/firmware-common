#include "opensimgear/actions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *A="00112233-4455-6677-8899-aabbccddeeff",*B="11223344-5566-7788-99aa-bbccddeeff00";
static const osg_action_parameter_t boolean={.id="value",.label="Value",.type=OSG_ACTION_BOOLEAN};
static const osg_action_parameter_t rgb={.id="color",.label="Color",.type=OSG_ACTION_RGB};
static const osg_action_descriptor_t descriptors[]={
    {.id="led.a",.label="LED A",.endpoint="led.a",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&boolean,.parameter_count=1},
    {.id="led.alias",.label="LED alias",.endpoint="led.a",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&boolean,.parameter_count=1},
    {.id="pulse",.label="Pulse",.endpoint="led.a",.semantics=OSG_ACTION_ONE_SHOT},
    {.id="led.b",.label="LED B",.endpoint="led.b",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&rgb,.parameter_count=1},
};
typedef struct {bool on,fail_write,fail_read,mismatch,write_then_fail,read_after_write_fail;uint32_t color;unsigned writes;} device_t;
static bool invoke(void *context,const char *id,const osg_action_value_t *values,size_t count) {
    device_t *s=context;s->writes++;if(s->fail_write)return false;
    if(!strcmp(id,"led.b")){assert(count==1);s->color=values[0].value.rgb;}
    else if(!strcmp(id,"pulse")){assert(!count);s->on=true;}
    else {assert(count==1);s->on=values[0].value.boolean;}
    if(s->read_after_write_fail)s->fail_read=true;
    return !s->write_then_fail;
}
static bool read_value(void *context,const char *id,osg_action_value_t *values,size_t count) {
    device_t *s=context;assert(count==1);if(s->fail_read)return false;
    if(!strcmp(id,"led.b"))values[0]=(osg_action_value_t){.type=OSG_ACTION_RGB,.value.rgb=s->color};
    else values[0]=(osg_action_value_t){.type=OSG_ACTION_BOOLEAN,.value.boolean=s->mismatch?!s->on:s->on};
    return true;
}
static char reply[4096];
static void command(osg_actions_t *a,uint32_t now,osg_action_scope_t scope,const char *line,const char *expected) {
    assert(osg_actions_command_at(a,line,now,scope,reply,sizeof reply));
    if(!strstr(reply,expected)){fprintf(stderr,"%s expected%s in%s\n",line,expected,reply);assert(false);}
    assert(strlen(reply)<=4091 && !strstr(reply,A) && !strstr(reply,B));
}
static void claim(osg_actions_t *a,uint32_t now,osg_action_scope_t scope,const char *id,const char *token,unsigned ttl,const char *value,const char *expected) {
    char line[384];snprintf(line,sizeof line,"action claim 1 %s %s %u %s",id,token,ttl,value);command(a,now,scope,line,expected);
}
static void owned(osg_actions_t *a,uint32_t now,osg_action_scope_t scope,const char *token,unsigned lease,const char *id,const char *value,const char *expected) {
    char line[384];snprintf(line,sizeof line,"action invoke_owned %s %u 1 %s %s",token,lease,id,value);command(a,now,scope,line,expected);
}

typedef struct {osg_action_value_t value[4];bool mismatch;} typed_t;
static bool typed_write(void *context,const char *id,const osg_action_value_t *v,size_t count) {
    (void)id;typed_t *s=context;memcpy(s->value,v,count*sizeof *v);return true;
}
static bool typed_read(void *context,const char *id,osg_action_value_t *v,size_t count) {
    (void)id;typed_t *s=context;memcpy(v,s->value,count*sizeof *v);
    if(s->mismatch) v[0].value.integer++;
    return true;
}
static void typed_and_paged_test(void) {
    const char *options[]={"off","on"};
    osg_action_parameter_t params[]={
        {.id="integer",.label="Integer",.type=OSG_ACTION_INTEGER,.minimum=-10,.maximum=10},
        {.id="mode",.label="Mode",.type=OSG_ACTION_ENUM,.options=options,.option_count=2},
        {.id="text",.label="Text",.type=OSG_ACTION_STRING,.max_bytes=63},
    };
    osg_action_descriptor_t d[6];char ids[6][8];
    for(unsigned i=0;i<6;i++) {
        snprintf(ids[i],sizeof ids[i],"out.%u",i);
        d[i]=(osg_action_descriptor_t){.id=ids[i],.label="Output",.endpoint=ids[i],.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=params,.parameter_count=3};
    }
    typed_t state={.value={{.type=OSG_ACTION_INTEGER,.value.integer=0},{.type=OSG_ACTION_ENUM,.value.text="off"},{.type=OSG_ACTION_STRING,.value.text=""}}};osg_actions_t a;
    assert(osg_actions_init(&a,d,6,1,typed_write,typed_read,&state) && osg_actions_enable_ownership(&a));
    command(&a,0,OSG_ACTION_SCOPE_CAN,"action ownership 0","\"next\":4");
    assert(strstr(reply,"\"fallback\":[]") && strstr(reply,"\"leaseId\":0"));
    command(&a,0,OSG_ACTION_SCOPE_CAN,"action ownership 4","\"next\":null");
    claim(&a,1,OSG_ACTION_SCOPE_CAN,"out.0","00112233-4455-6677-8899-AABBCCDDEEFF",500,"-2 off 225c","\"leaseId\":1");
    owned(&a,2,OSG_ACTION_SCOPE_CAN,A,1,"out.0","3 on c3a9","\"values\":[3,\"on\",\"\xc3\xa9\"]");
    /* Renewing an existing owner's claim keeps its lease ID and replaces fallback. */
    claim(&a,3,OSG_ACTION_SCOPE_CAN,"out.0",A,500,"-4 on 225c","\"leaseId\":1");
    char line[384];snprintf(line,sizeof line,"action release %s 1",A);
    command(&a,4,OSG_ACTION_SCOPE_CAN,line,"\"released\":true");
    assert(state.value[0].value.integer==-4 && !strcmp(state.value[1].value.text,"on") && !strcmp(state.value[2].value.text,"\"\\"));
    command(&a,5,OSG_ACTION_SCOPE_CAN,"action ownership 0","\"fallbackStatus\":\"applied\"");
    claim(&a,6,OSG_ACTION_SCOPE_CAN,"out.0",A,500,"-2 off -","\"leaseId\":2");
    state.mismatch=true;snprintf(line,sizeof line,"action release %s 2",A);
    command(&a,7,OSG_ACTION_SCOPE_CAN,line,"\"fallbackStatus\":\"preserved\"");assert(!a.leases[0].owned);state.mismatch=false;
    /* Valid legacy catalogues can be too wide for owned command envelopes. */
    osg_action_parameter_t wide[3];for(unsigned i=0;i<3;i++){wide[i]=params[2];wide[i].id=params[i].id;}
    d[0].parameters=wide;assert(!osg_actions_init(&a,d,1,1,typed_write,typed_read,&state));
    wide[2].max_bytes=20;assert(osg_actions_init(&a,d,1,1,typed_write,typed_read,&state));assert(!osg_actions_enable_ownership(&a));
}
static void restore_if_unchanged_test(void) {
    device_t device={0};osg_actions_t a;char line[384];
    assert(osg_actions_init(&a,descriptors,4,1,invoke,read_value,&device) && osg_actions_enable_ownership(&a));
    command(&a,0,OSG_ACTION_SCOPE_USB,"action describe 0","\"restorePolicy\":\"if-unchanged\"");
    /* Claims capture real readable state without invoking a setter. */
    device.fail_read=true;claim(&a,1,OSG_ACTION_SCOPE_USB,"led.a",A,500,"0","failed");assert(!a.leases[0].owned && !device.writes);
    device.fail_read=false;claim(&a,2,OSG_ACTION_SCOPE_USB,"led.a",A,500,"0","\"leaseId\":1");
    owned(&a,3,OSG_ACTION_SCOPE_USB,A,1,"led.alias","1","\"values\":[true]");
    device.on=false;unsigned writes=device.writes;
    snprintf(line,sizeof line,"action release %s 1",A);command(&a,4,OSG_ACTION_SCOPE_USB,line,"\"fallbackStatus\":\"preserved\"");
    assert(!device.on && device.writes==writes && !a.leases[0].owned && !a.leases[0].token[0]);
    command(&a,5,OSG_ACTION_SCOPE_USB,"action ownership 0","\"fallbackStatus\":\"preserved\"");
    /* A new owner may adopt the preserved value; the old lease cannot release it. */
    claim(&a,6,OSG_ACTION_SCOPE_CAN,"led.a",B,500,"1","\"leaseId\":2");
    snprintf(line,sizeof line,"action release %s 1",A);command(&a,7,OSG_ACTION_SCOPE_USB,line,"outputNotOwned");
    assert(a.leases[0].owned && !device.on && device.writes==writes);
    osg_actions_invalidate_scope(&a,OSG_ACTION_SCOPE_USB,8);assert(a.leases[0].owned);
    snprintf(line,sizeof line,"action release %s 2",B);command(&a,9,OSG_ACTION_SCOPE_CAN,line,"\"fallbackStatus\":\"applied\"");assert(device.on && device.writes==writes+1);
    /* Expiry preserves external changes even without another host request. */
    claim(&a,10,OSG_ACTION_SCOPE_CAN,"led.a",A,250,"0","\"leaseId\":3");device.on=false;writes=device.writes;
    osg_actions_tick(&a,260);assert(!a.leases[0].owned && a.leases[0].fallback_status==3 && !device.on && device.writes==writes);
    /* Owned one-shot aliases refresh the claimed readable Setter's exact shape. */
    claim(&a,261,OSG_ACTION_SCOPE_CAN,"led.a",A,250,"0","\"leaseId\":4");
    owned(&a,262,OSG_ACTION_SCOPE_CAN,A,4,"pulse","","\"values\":[]");assert(device.on && a.leases[0].last_owned[0].value.boolean);
    osg_actions_invalidate_scope(&a,OSG_ACTION_SCOPE_CAN,263);assert(!device.on && a.leases[0].fallback_status==1);
    /* Transport loss uses the same comparison and never overwrites external RGB. */
    claim(&a,264,OSG_ACTION_SCOPE_USB,"led.b",B,250,"000000","\"leaseId\":5");
    owned(&a,265,OSG_ACTION_SCOPE_USB,B,5,"led.b","112233","112233");device.color=0x445566;writes=device.writes;
    osg_actions_invalidate_scope(&a,OSG_ACTION_SCOPE_USB,266);assert(device.color==0x445566 && a.leases[3].fallback_status==3 && device.writes==writes);
    /* Ambiguous mutation/readback failures revoke restoration authority, even
       if a later read happens to equal the value preceding the failed command. */
    claim(&a,267,OSG_ACTION_SCOPE_USB,"led.a",A,250,"0","\"leaseId\":6");device.write_then_fail=true;
    owned(&a,268,OSG_ACTION_SCOPE_USB,A,6,"led.a","1","failed");assert(device.on && !a.leases[0].last_owned_valid);device.write_then_fail=false;writes=device.writes;
    snprintf(line,sizeof line,"action release %s 6",A);command(&a,269,OSG_ACTION_SCOPE_USB,line,"fallbackFailed");assert(device.on && device.writes==writes && !a.leases[0].owned);
    claim(&a,270,OSG_ACTION_SCOPE_USB,"led.a",A,250,"0","\"leaseId\":7");device.read_after_write_fail=true;
    owned(&a,271,OSG_ACTION_SCOPE_USB,A,7,"led.a","0","failed");device.read_after_write_fail=false;device.fail_read=false;writes=device.writes;
    snprintf(line,sizeof line,"action release %s 7",A);command(&a,272,OSG_ACTION_SCOPE_USB,line,"fallbackFailed");assert(device.writes==writes);
    /* Compare read failures do not call a fallback setter. */
    claim(&a,273,OSG_ACTION_SCOPE_CAN,"led.a",A,250,"1","\"leaseId\":8");device.fail_read=true;writes=device.writes;
    osg_actions_tick(&a,523);assert(a.leases[0].fallback_status==2 && !a.leases[0].owned && device.writes==writes);device.fail_read=false;
    /* Restoration that writes but cannot confirm is failed once, without retry. */
    claim(&a,524,OSG_ACTION_SCOPE_CAN,"led.a",A,250,"1","\"leaseId\":9");device.read_after_write_fail=true;
    snprintf(line,sizeof line,"action release %s 9",A);command(&a,525,OSG_ACTION_SCOPE_CAN,line,"fallbackFailed");assert(!a.leases[0].owned && device.on);
    writes=device.writes;osg_actions_tick(&a,1000);assert(device.writes==writes);
}
int main(void) {
    device_t device={0};osg_actions_t a;assert(osg_actions_init(&a,descriptors,4,1,invoke,read_value,&device));
    assert(!osg_actions_enable_ownership(NULL));osg_actions_t invalid={0};assert(!osg_actions_enable_ownership(&invalid));
    command(&a,0,OSG_ACTION_SCOPE_CAN,"action ownership 0","ownershipUnsupported");
    command(&a,0,OSG_ACTION_SCOPE_CAN,"action renew bad 1 500","ownershipUnsupported");
    command(&a,0,OSG_ACTION_SCOPE_CAN,"action invoke_owned bad 1 1 led.a 0","ownershipUnsupported");
    claim(&a,0,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","ownershipUnsupported");
    assert(osg_actions_enable_ownership(&a));
    command(&a,0,OSG_ACTION_SCOPE_CAN,"action describe 0","\"ownership\":{\"schemaVersion\":1");
    claim(&a,10,OSG_ACTION_SCOPE_CAN,"led.a",A,1000,"0","\"leaseId\":1");assert(!device.writes);
    claim(&a,11,OSG_ACTION_SCOPE_USB,"led.alias",B,500,"1","outputBusy");assert(!device.writes);
    command(&a,12,OSG_ACTION_SCOPE_USB,"action invoke 1 pulse","outputBusy");
    claim(&a,13,OSG_ACTION_SCOPE_USB,"led.b",B,500,"000000","\"leaseId\":2");
    owned(&a,20,OSG_ACTION_SCOPE_CAN,A,1,"led.alias","1","\"values\":[true]");assert(device.on && device.writes==1);
    owned(&a,21,OSG_ACTION_SCOPE_USB,A,1,"led.a","0","outputNotOwned");
    owned(&a,22,OSG_ACTION_SCOPE_CAN,A,1,"led.b","ffffff","outputNotOwned");
    owned(&a,23,OSG_ACTION_SCOPE_CAN,B,2,"led.b","ffffff","outputNotOwned");
    owned(&a,24,OSG_ACTION_SCOPE_USB,B,2,"led.b","123456","123456");assert(device.color==0x123456);
    char line[384];snprintf(line,sizeof line,"action renew %s 1 250",A);command(&a,200,OSG_ACTION_SCOPE_CAN,line,"\"remainingMs\":250");
    osg_actions_tick(&a,449);assert(device.on);
    osg_actions_tick(&a,450);assert(!device.on && a.leases[0].fallback_status==1);
    claim(&a,451,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"1","\"leaseId\":3");
    owned(&a,452,OSG_ACTION_SCOPE_CAN,A,1,"led.a","0","staleOutputLease");
    osg_actions_invalidate_scope(&a,OSG_ACTION_SCOPE_USB,453);assert(a.leases[0].owned && !a.leases[3].owned && !device.color);
    claim(&a,454,OSG_ACTION_SCOPE_USB,"led.b",B,500,"000000","\"leaseId\":4");
    osg_actions_invalidate_scope(&a,OSG_ACTION_SCOPE_CAN,455);assert(device.on && !a.leases[0].owned && a.leases[3].owned);
    command(&a,456,OSG_ACTION_SCOPE_CAN,"action ownership 0","\"total\":2");assert(strstr(reply,"\"next\":null") && strstr(reply,"applied"));
    command(&a,457,OSG_ACTION_SCOPE_CAN,"action ownership 2","\"ownership\":[]");
    command(&a,458,OSG_ACTION_SCOPE_CAN,"action ownership 3","invalidArgument");
    snprintf(line,sizeof line,"action release %s 4",B);command(&a,459,OSG_ACTION_SCOPE_USB,line,"\"released\":true");assert(!a.leases[3].owned);
    claim(&a,UINT32_MAX-99,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","\"leaseId\":5");
    osg_actions_tick(&a,399);assert(a.leases[0].owned);osg_actions_tick(&a,400);assert(!a.leases[0].owned && !device.on);
    claim(&a,401,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","\"leaseId\":6");device.fail_read=true;
    snprintf(line,sizeof line,"action release %s 6",A);command(&a,402,OSG_ACTION_SCOPE_CAN,line,"fallbackFailed");
    assert(!a.leases[0].owned && a.leases[0].fallback_status==2);unsigned writes=device.writes;osg_actions_tick(&a,1000);assert(device.writes==writes);device.fail_read=false;
    const char *bad[]={"action claim 2 led.a 00112233-4455-6677-8899-aabbccddeeff 500 0","action claim 1 pulse 00112233-4455-6677-8899-aabbccddeeff 500",
        "action claim 1 led.a wrong 500 0","action claim 1 led.a 00112233-4455-6677-8899-aabbccddeeff 249 0",
        "action claim 1 led.a 00112233-4455-6677-8899-aabbccddeeff 5001 0","action claim 1 led.a 00112233-4455-6677-8899-aabbccddeeff 500 2",
        "action renew wrong 1 500","action renew 00112233-4455-6677-8899-aabbccddeeff 0 500","action release wrong 1","action claim 1 led.a 00000000-0000-0000-0000-000000000000 500 0","action renew 00000000-0000-0000-0000-000000000000 1 500","action invoke_owned 00000000-0000-0000-0000-000000000000 1 1 led.a 0","action ownership"};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){command(&a,1001,OSG_ACTION_SCOPE_CAN,bad[i],"\"ok\":false");assert(device.writes==writes);}
    claim(&a,1002,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","\"leaseId\":7");device.mismatch=true;
    snprintf(line,sizeof line,"action release %s 7",A);command(&a,1003,OSG_ACTION_SCOPE_CAN,line,"\"fallbackStatus\":\"preserved\"");device.mismatch=false;
    claim(&a,1004,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","\"leaseId\":8");device.fail_write=true;
    osg_actions_tick(&a,1504);assert(!a.leases[0].owned && a.leases[0].fallback_status==2);device.fail_write=false;
    claim(&a,1505,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","\"leaseId\":9");
    snprintf(line,sizeof line,"action renew %s 9 500",A);command(&a,1506,(osg_action_scope_t)9,line,"invalidArgument");
    owned(&a,1506,OSG_ACTION_SCOPE_CAN,A,0,"led.a","1","invalidArgument");
    osg_actions_invalidate_scope(&a,OSG_ACTION_SCOPE_CAN,1507);
    typed_and_paged_test();
    restore_if_unchanged_test();
    a.lease_counter=UINT32_MAX;claim(&a,1002,OSG_ACTION_SCOPE_CAN,"led.a",A,500,"0","leaseExhausted");
    osg_actions_tick(NULL,0);osg_actions_invalidate_scope(NULL,OSG_ACTION_SCOPE_CAN,0);
    puts("endpoint aliases, competing transports, independent leases, expiry/wrap/fallback, secret/redaction and stale lease guards passed");
}
