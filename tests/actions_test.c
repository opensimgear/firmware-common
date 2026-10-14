#include "opensimgear/actions.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const char *const modes[]={"auto","on","off"};
static const osg_action_parameter_t boolean={.id="enabled",.label="Enabled",.type=OSG_ACTION_BOOLEAN};
static const osg_action_parameter_t rgb={.id="color",.label="Color",.type=OSG_ACTION_RGB};
static const osg_action_parameter_t mode={.id="mode",.label="Mode",.type=OSG_ACTION_ENUM,.options=modes,.option_count=3};
static const osg_action_parameter_t integer={.id="value",.label="Value",.type=OSG_ACTION_INTEGER,.minimum=-20,.maximum=100};
static const osg_action_parameter_t string={.id="text",.label="Display \"text\" \\ ",.type=OSG_ACTION_STRING,.max_bytes=63};
static const osg_action_descriptor_t catalogue[]={
    {.id="led.boolean.set_enabled",.label="Boolean LED",.endpoint="led.boolean",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&boolean,.parameter_count=1},
    {.id="led.rgb.set_color",.label="RGB LED",.endpoint="led.rgb",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&rgb,.parameter_count=1},
    {.id="led.status.set_mode",.label="Status LED",.endpoint="led.status",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&mode,.parameter_count=1},
    {.id="gauge.set_value",.label="Gauge",.endpoint="gauge",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&integer,.parameter_count=1},
    {.id="display.set_text",.label="Display",.endpoint="display",.semantics=OSG_ACTION_SETTER,.readable=true,.parameters=&string,.parameter_count=1},
    {.id="pulse",.label="Pulse",.endpoint="led.boolean",.semantics=OSG_ACTION_ONE_SHOT},
};
typedef struct { osg_action_value_t stored[6]; unsigned invokes; bool fail_invoke,fail_read,invalid_read; } device_t;
static size_t index_of(const char *id) { for(size_t i=0;i<6;++i) if(!strcmp(id,catalogue[i].id)) return i; assert(false); return 0; }
static bool invoke(void *context,const char *id,const osg_action_value_t *values,size_t count) {
    device_t *d=context; ++d->invokes; if(d->fail_invoke) return false;
    if(count) d->stored[index_of(id)]=values[0];
    return true;
}
static bool read(void *context,const char *id,osg_action_value_t *values,size_t count) {
    device_t *d=context; if(d->fail_read) return false;
    assert(count==1); values[0]=d->stored[index_of(id)]; if(d->invalid_read) values[0].type=OSG_ACTION_STRING;
    return true;
}
static char reply[OSG_ACTION_REPLY_CAPACITY];
static void command(osg_actions_t *actions,const char *s,const char *expected) {
    assert(osg_actions_command(actions,s,reply,sizeof reply));
    assert(strlen(reply)<OSG_ACTION_REPLY_CAPACITY-4); assert(strstr(reply,expected));
}
static void rejection(osg_actions_t *a,device_t *d,const char *s,const char *error) {
    unsigned before=d->invokes; command(a,s,error); assert(d->invokes==before);
}
static void invalid_catalogues(void) {
    osg_actions_t a; osg_action_descriptor_t c=catalogue[0]; osg_action_parameter_t p=boolean;
    assert(!osg_actions_init(NULL,&c,1,1,invoke,read,NULL));
    assert(!osg_actions_init(&a,&c,1,0,invoke,read,NULL));
    assert(!osg_actions_init(&a,&c,17,1,invoke,read,NULL));
    assert(!osg_actions_init(&a,NULL,1,1,invoke,read,NULL));
    assert(!osg_actions_init(&a,&c,1,1,NULL,read,NULL));
    assert(!osg_actions_init(&a,&c,1,1,invoke,NULL,NULL));
    c.id=NULL; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c=catalogue[0]; c.id=""; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.id="bad name"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.id="12345678901234567890123456789012"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c=catalogue[0]; c.endpoint="bad!"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c=catalogue[0]; c.label=NULL; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label=""; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xC2\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\1"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\x7F"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xC0\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xC2"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xE0\x80\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xF0\x80\x80\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xED\xA0\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xF4\x90\x80\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="\xE2x\x80"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c.label="Valid \xC2\xA3 \xE2\x82\xAC \xF0\x9F\x98\x80"; assert(osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c=catalogue[0]; c.semantics=(osg_action_semantics_t)9; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c=catalogue[0]; c.parameter_count=5; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    c=catalogue[0]; c.parameters=NULL; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    osg_action_descriptor_t duplicate[]={catalogue[0],catalogue[0]}; assert(!osg_actions_init(&a,duplicate,2,1,invoke,read,NULL));
    c=catalogue[0]; c.parameters=&p; p.id="bad id"; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=boolean; p.label=NULL; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=boolean; p.label=""; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=boolean; p.type=(osg_action_type_t)-1; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=boolean; p.type=(osg_action_type_t)9; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=integer; p.minimum=101; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=mode; p.option_count=0; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=mode; p.option_count=17; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=mode; p.options=NULL; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    const char *const bad[]={"!"}; p=mode; p.options=bad; p.option_count=1; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    const char *const dup[]={"on","on"}; p.options=dup; p.option_count=2; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p=string; p.max_bytes=64; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    p.max_bytes=0; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    osg_action_parameter_t params[]={boolean,boolean}; c.parameters=params; c.parameter_count=2; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    params[0]=string; params[1]=string; params[1].id="other"; assert(osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    osg_action_parameter_t many[]={params[0],params[1],params[0],params[1]}; many[2].id="third"; many[3].id="fourth";
    c.parameters=many; c.parameter_count=4; assert(!osg_actions_init(&a,&c,1,1,invoke,read,NULL));
    osg_action_descriptor_t too_many[16]; char ids[16][8];
    c.parameters=params; c.parameter_count=2;
    for(size_t i=0;i<16;++i) { snprintf(ids[i],sizeof ids[i],"a%u",(unsigned)i); too_many[i]=c; too_many[i].id=ids[i]; }
    assert(!osg_actions_init(&a,too_many,16,1,invoke,read,NULL));
}
static void display_test(void) {
    device_t device={.stored={[4]={.type=OSG_ACTION_STRING}}};
    osg_action_display_t display={.columns=3,.rows=2,.encoding=OSG_ACTION_DISPLAY_UTF8};
    osg_action_descriptor_t descriptor=catalogue[4];descriptor.display=&display;
    osg_actions_t actions;assert(osg_actions_init(&actions,&descriptor,1,2,invoke,read,&device));
    command(&actions,"action describe 0","\"display\":{\"columns\":3,\"rows\":2,\"encoding\":\"utf8\"}");
    command(&actions,"action invoke 2 display.set_text 414243444546","ABCDEF");
    rejection(&actions,&device,"action invoke 2 display.set_text 41424344454647","invalidArgument");
    /* Scalar cells and byte limits are independent; UTF-8 stays intact. */
    command(&actions,"action invoke 2 display.set_text C2A3E282ACF09F9880414243","\xC2\xA3\xE2\x82\xAC\xF0\x9F\x98\x80" "ABC");
    rejection(&actions,&device,"action invoke 2 display.set_text C2A3E282ACF09F988041424344","invalidArgument");
    strcpy(device.stored[4].value.text,"1234567");command(&actions,"action state","failed");
    display.encoding=OSG_ACTION_DISPLAY_ASCII;
    assert(osg_actions_init(&actions,&descriptor,1,3,invoke,read,&device));
    rejection(&actions,&device,"action invoke 3 display.set_text C2A3","invalidArgument");
    command(&actions,"action invoke 3 display.set_text 225C20202020","\"values\":[\"\\\"\\\\    \"]");
    command(&actions,"action invoke 3 display.set_text -","\"values\":[\"\"]");
    strcpy(device.stored[4].value.text,"\xC2\xA3");command(&actions,"action state","failed");
    osg_action_parameter_t parameter=string;parameter.max_bytes=6;descriptor.parameters=&parameter;
    display.encoding=OSG_ACTION_DISPLAY_UTF8;
    assert(osg_actions_init(&actions,&descriptor,1,4,invoke,read,&device));
    rejection(&actions,&device,"action invoke 4 display.set_text F09F9880F09F9880","invalidArgument");
    for(unsigned trial=0;trial<9;trial++) {
        descriptor=catalogue[4];descriptor.display=&display;
        display=(osg_action_display_t){.columns=3,.rows=2,.encoding=OSG_ACTION_DISPLAY_UTF8};
        switch(trial) {
            case 0:display.columns=0;break;
            case 1:display.columns=64;break;
            case 2:display.rows=0;break;
            case 3:display.rows=5;break;
            case 4:display.columns=16;display.rows=4;break;
            case 5:display.encoding=(osg_action_display_encoding_t)-1;break;
            case 6:descriptor.readable=false;break;
            case 7:descriptor.semantics=OSG_ACTION_ONE_SHOT;break;
            default:descriptor.parameters=&boolean;break;
        }
        assert(!osg_actions_init(&actions,&descriptor,1,1,invoke,read,&device));
    }
    descriptor=catalogue[4];descriptor.display=&display;descriptor.parameters=&parameter;
    display=(osg_action_display_t){.columns=7,.rows=1,.encoding=OSG_ACTION_DISPLAY_ASCII};
    assert(!osg_actions_init(&actions,&descriptor,1,1,invoke,read,&device));
    /* Legacy one-string descriptors still accept text with no geometry. */
    descriptor.display=NULL;
    assert(osg_actions_init(&actions,&descriptor,1,1,invoke,read,&device));
    command(&actions,"action describe 0","\"maxBytes\":6");assert(!strstr(reply,"\"display\":"));
}
int main(void) {
    device_t device={.stored={{.type=OSG_ACTION_BOOLEAN},{.type=OSG_ACTION_RGB},
        {.type=OSG_ACTION_ENUM,.value.text="auto"},{.type=OSG_ACTION_INTEGER},
        {.type=OSG_ACTION_STRING},{.type=OSG_ACTION_BOOLEAN}}};
    osg_actions_t actions; assert(osg_actions_init(&actions,catalogue,6,1,invoke,read,&device));
    strcpy(reply,"unchanged"); assert(!osg_actions_command(&actions,"config get",reply,sizeof reply)); assert(!strcmp(reply,"unchanged"));
    assert(!osg_actions_command(&actions,NULL,reply,sizeof reply));
    assert(!osg_actions_command(&actions,"actions state",reply,sizeof reply));
    command(&actions,"action describe 0","\"next\":4"); assert(strstr(reply,"\"type\":\"boolean\"")); assert(strstr(reply,"\"options\":[\"auto\",\"on\",\"off\"]"));
    command(&actions,"action describe 4","\"next\":null"); assert(strstr(reply,"Display \\\"text\\\" \\\\"));
    command(&actions,"action describe 6","\"actions\":[]");
    command(&actions,"action state","\"values\":[\"auto\"]");
    command(&actions,"action invoke 1 led.boolean.set_enabled 1","\"values\":[true]");
    command(&actions,"action invoke 1 led.boolean.set_enabled 0","\"values\":[false]");
    command(&actions,"action invoke 1 led.rgb.set_color aB09fF","\"values\":[\"AB09FF\"]");
    command(&actions,"action invoke 1 led.status.set_mode off","\"values\":[\"off\"]");
    command(&actions,"action invoke 1 gauge.set_value -20","\"values\":[-20]");
    command(&actions,"action invoke 1 gauge.set_value 100","\"values\":[100]");
    command(&actions,"action invoke 1 display.set_text 48656c6c6f20C2A3","Hello \xC2\xA3");
    command(&actions,"action invoke 1 display.set_text 225C","\\\"\\\\");
    command(&actions,"action invoke 1 display.set_text -","\"values\":[\"\"]");
    command(&actions,"action invoke 1 pulse","\"values\":[]");
    command(&actions,"action\t state  ","\"states\":[");
    const char *invalid[]={"action", "action unknown", "action state 1", "action describe", "action describe -1", "action describe 7", "action describe 4294967296", "action describe x", "action invoke", "action invoke x pulse", "action invoke 1 unknown", "action invoke 1 pulse extra", "action invoke 1 led.boolean.set_enabled", "action invoke 1 led.boolean.set_enabled true", "action invoke 1 led.rgb.set_color FF", "action invoke 1 led.rgb.set_color GGGGGG", "action invoke 1 led.status.set_mode wrong", "action invoke 1 gauge.set_value +1", "action invoke 1 gauge.set_value -21", "action invoke 1 gauge.set_value 101", "action invoke 1 gauge.set_value 2147483648", "action invoke 1 gauge.set_value -2147483649", "action invoke 1 display.set_text 1", "action invoke 1 display.set_text GG", "action invoke 1 display.set_text 00", "action invoke 1 display.set_text 01", "action invoke 1 display.set_text C080", "action invoke 1 display.set_text C280", "action invoke 1 pulse 1 2 3 4 5"};
    for(size_t i=0;i<sizeof invalid/sizeof invalid[0];++i) rejection(&actions,&device,invalid[i],"\"ok\":false");
    rejection(&actions,&device,"action invoke 2 pulse","staleCatalogue");
    unsigned before=device.invokes; assert(osg_actions_command(&actions,"action invoke 1 pulse",reply,64)); assert(strstr(reply,"replyTooSmall")); assert(device.invokes==before);
    assert(osg_actions_command(&actions,"action state",NULL,0));
    char tiny[1]; assert(osg_actions_command(&actions,"action state",tiny,sizeof tiny)); assert(!*tiny);
    char large[400]; memset(large,'x',sizeof large); memcpy(large,"action ",7); large[399]=0; rejection(&actions,&device,large,"invalidArgument");
    device.fail_invoke=true; command(&actions,"action invoke 1 pulse","failed"); device.fail_invoke=false;
    device.fail_read=true; command(&actions,"action state","failed"); command(&actions,"action invoke 1 led.boolean.set_enabled 1","failed"); device.fail_read=false;
    device.invalid_read=true; command(&actions,"action state","failed"); device.invalid_read=false;
    osg_actions_t empty; assert(osg_actions_init(&empty,NULL,0,UINT32_MAX,NULL,NULL,NULL)); command(&empty,"action describe 0","\"actions\":[]"); command(&empty,"action state","\"states\":[]");
    command(NULL,"action state","invalidCatalogue"); empty.valid=false; command(&empty,"action state","invalidCatalogue");
    /* The byte bound can end a page before the entry-count bound. */
    char options[16][32], label[64]; const char *option_ptrs[16];
    memset(label,'"',63); label[63]=0;
    for(size_t i=0;i<16;++i) {
        memset(options[i],'a',31); options[i][30]="0123456789abcdef"[i]; options[i][31]=0;
        option_ptrs[i]=options[i];
    }
    osg_action_parameter_t fat_params[4];
    const char *const names[]={"first","second","third","fourth"};
    for(size_t i=0;i<4;++i) fat_params[i]=(osg_action_parameter_t){.id=names[i],.label=label,
        .type=OSG_ACTION_ENUM,.options=option_ptrs,.option_count=16};
    osg_action_descriptor_t fat[]={
        {.id="first",.label=label,.endpoint="led",.parameters=fat_params,.parameter_count=4},
        {.id="second",.label=label,.endpoint="led",.parameters=fat_params,.parameter_count=4}};
    osg_actions_t paginated; assert(osg_actions_init(&paginated,fat,2,1,invoke,NULL,NULL));
    command(&paginated,"action describe 0","\"next\":1");
    command(&paginated,"action describe 1","\"next\":null");
    invalid_catalogues();display_test(); puts("device action behavior passed");
}
