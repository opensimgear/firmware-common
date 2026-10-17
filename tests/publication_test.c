#include "opensimgear/publication.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t OWNER[16]={1},OTHER[16]={2};
static osg_publication_scope_t scope(void) {
    osg_publication_scope_t s={.publisher={9},.boot=11,.route_generation=12};return s;
}
static osg_publication_view_t view(bool enabled) {
    osg_publication_view_t v={.count=1};
    v.targets[0]=(osg_publication_target_t){.identity={3},.boot=20,.configuration_generation=21,.profile_revision=22,
        .input_capable=true,.enabled=enabled,.report_id=1,.order=enabled?0:255};return v;
}
static osg_publication_ticket_t claim(osg_publication_t *s,const osg_publication_view_t *original,
    const osg_publication_view_t *desired,uint32_t now,uint32_t ttl) {
    osg_publication_ticket_t t;osg_publication_scope_t route=scope();
    assert(osg_publication_claim_current(s,&route,OWNER,original,desired,now,ttl,&t)==OSG_PUBLICATION_OK);return t;
}
static void apply(osg_publication_t *s,const osg_publication_ticket_t *ticket,
    const osg_publication_view_t *before,const osg_publication_view_t *after,uint32_t now) {
    osg_publication_scope_t route=scope();
    assert(osg_publication_apply_allowed(s,&route,OWNER,ticket,before,now));
    assert(osg_publication_begin_apply(s,&route,OWNER,ticket,before,now));
    assert(!osg_publication_begin_apply(s,&route,OWNER,ticket,before,now));
    assert(osg_publication_confirm_applied(s,&route,OWNER,ticket,after,now));
}
static void lifecycle(void) {
    for(unsigned enabled=0;enabled<2;enabled++) {
        osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope();
        osg_publication_view_t original=view(enabled),desired=view(!enabled),actual=original;
        osg_publication_ticket_t t=claim(&s,&original,&desired,100,5000);assert(t.lease_id==1 && t.transition==1);
        /* Firmware claim itself cannot change the driver/configuration snapshot. */
        assert(!memcmp(&actual,&original,sizeof actual));
        apply(&s,&t,&actual,&desired,110);actual=desired;
        osg_publication_ticket_t duplicate;
        assert(osg_publication_claim_current(&s,&route,OWNER,&actual,&desired,120,5000,&duplicate)==OSG_PUBLICATION_OK);
        assert(duplicate.lease_id==t.lease_id && duplicate.transition==t.transition && duplicate.expires_at==t.expires_at);
        assert(!memcmp(&s.original,&original,sizeof original)); /* No stale host fallback or recapture. */
        assert(osg_publication_claim_current(&s,&route,OTHER,&actual,&desired,120,5000,&duplicate)==OSG_PUBLICATION_BUSY);
        assert(osg_publication_release(&s,&route,OTHER,t.lease_id,&actual,130)==OSG_PUBLICATION_NOT_OWNED);
        assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&actual,130)==OSG_PUBLICATION_RESTORE);
        osg_publication_ticket_t cleanup=s.ticket;
        assert(!osg_publication_apply_allowed(&s,&route,OWNER,&t,&actual,130));
        assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&actual,131)==OSG_PUBLICATION_RESTORE);
        assert(s.ticket.transition==cleanup.transition);
        assert(!osg_publication_restore_allowed(&s,&t,&actual));
        assert(osg_publication_restore_allowed(&s,&cleanup,&actual));
        assert(!osg_publication_confirm_restored(&s,&cleanup,&actual)); /* Scheduling is not driver acknowledgement. */
        actual=original;
        assert(osg_publication_confirm_restored(&s,&cleanup,&actual));
        assert(!osg_publication_restore_allowed(&s,&cleanup,&actual));
        assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&actual,140)==OSG_PUBLICATION_NO_CHANGE);
        osg_publication_ticket_t newer=claim(&s,&original,&desired,150,250);
        assert(newer.lease_id>t.lease_id && newer.transition>cleanup.transition);
        assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&actual,151)==OSG_PUBLICATION_NOT_OWNED);
        assert(!osg_publication_confirm_applied(&s,&route,OWNER,&t,&desired,151));
    }
}
static void lost_ack_and_expiry(void) {
    osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope();
    osg_publication_view_t original=view(true),desired=view(false);
    osg_publication_ticket_t t=claim(&s,&original,&desired,100,250),renewed;
    /* A lost accepted claim can be read/retried exactly without creating work. */
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,110,250,&renewed)==OSG_PUBLICATION_OK);
    assert(renewed.lease_id==t.lease_id && renewed.transition==t.transition);
    assert(osg_publication_tick(&s,&original,349)==OSG_PUBLICATION_OK);
    assert(!osg_publication_apply_allowed(&s,&route,OWNER,&t,&original,350));
    assert(osg_publication_renew(&s,&route,OWNER,t.lease_id,350,250,&renewed)==OSG_PUBLICATION_EXPIRED);
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,350,250,&renewed)==OSG_PUBLICATION_EXPIRED);
    assert(osg_publication_tick(&s,&original,350)==OSG_PUBLICATION_NO_CHANGE); /* Never applied: cancel only. */
    assert(!osg_publication_confirm_applied(&s,&route,OWNER,&t,&desired,350));
    t=claim(&s,&original,&desired,400,250);apply(&s,&t,&original,&desired,410);
    assert(osg_publication_renew(&s,&route,OWNER,t.lease_id,500,250,&renewed)==OSG_PUBLICATION_OK);
    assert(renewed.expires_at==750 && renewed.transition==t.transition);
    assert(!osg_publication_begin_apply(&s,&route,OWNER,&t,&desired,700));
    assert(osg_publication_tick(&s,&desired,750)==OSG_PUBLICATION_RESTORE);
    assert(s.phase==OSG_PUBLICATION_RESTORING);
    assert(osg_publication_restore_allowed(&s,&s.ticket,&desired));
    assert(osg_publication_confirm_restored(&s,&s.ticket,&original));
}
static void capture_is_immutable(void) {
    osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope();
    osg_publication_view_t original=view(true),desired=view(false),actual=original;
    osg_publication_ticket_t t=claim(&s,&original,&desired,100,250);
    /* Caller-owned command/read buffers may be reused immediately. */
    original.targets[0].configuration_generation++;original.targets[0].order=7;
    desired.targets[0].enabled=true;desired.targets[0].order=9;route.boot++;
    assert(s.original.targets[0].configuration_generation==21 && s.original.targets[0].order==0);
    assert(!s.desired.targets[0].enabled && s.desired.targets[0].order==255);
    route=scope();
    assert(osg_publication_begin_apply(&s,&route,OWNER,&t,&actual,110));
    actual=s.desired;
    assert(osg_publication_confirm_applied(&s,&route,OWNER,&t,&actual,111));
    actual.targets[0].order=3; /* Later corruption cannot change last-owned capture. */
    assert(s.last_owned.targets[0].order==255);
}
static void admitted_but_unconfirmed(void) {
    for(unsigned cause=0;cause<3;cause++) {
        osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope();
        osg_publication_view_t original=view(true),desired=view(false),actual=original;
        osg_publication_ticket_t t=claim(&s,&original,&desired,100,250),duplicate;
        assert(!osg_publication_confirm_applied(&s,&route,OWNER,&t,&desired,110)); /* No admitted IO. */
        assert(osg_publication_begin_apply(&s,&route,OWNER,&t,&actual,110));
        actual=desired; /* Physical descriptor changed; confirmation is delayed/lost. */
        assert(!s.apply_confirmed && s.last_owned.targets[0].enabled);
        assert(!osg_publication_begin_apply(&s,&route,OWNER,&t,&actual,120)); /* Never replay IO. */
        assert(osg_publication_claim_current(&s,&route,OWNER,&actual,&desired,120,250,&duplicate)==OSG_PUBLICATION_OK);
        assert(duplicate.transition==t.transition && duplicate.expires_at==t.expires_at);
        assert(s.original.targets[0].enabled && !s.apply_confirmed);
        osg_publication_result_t result=cause==0?osg_publication_tick(&s,&actual,350):
            cause==1?osg_publication_release(&s,&route,OWNER,t.lease_id,&actual,130):
                     osg_publication_invalidate(&s,&route,&actual,130);
        assert(result==OSG_PUBLICATION_RESTORE && s.phase==OSG_PUBLICATION_RESTORING);
        assert(!osg_publication_confirm_applied(&s,&route,OWNER,&t,&actual,350));
        assert(!osg_publication_begin_apply(&s,&route,OWNER,&t,&actual,350));
        assert(osg_publication_claim_current(&s,&route,OTHER,&actual,&desired,350,250,&duplicate)==OSG_PUBLICATION_BUSY);
        assert(osg_publication_restore_allowed(&s,&s.ticket,&actual));
        actual=original;assert(osg_publication_confirm_restored(&s,&s.ticket,&actual));
        assert(s.original.targets[0].enabled && !s.apply_confirmed); /* No fictitious write acknowledgement. */
    }
    /* Revocation before physical application cancels the admitted transition. */
    osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope();
    osg_publication_view_t original=view(true),desired=view(false);
    osg_publication_ticket_t t=claim(&s,&original,&desired,100,250);
    assert(osg_publication_begin_apply(&s,&route,OWNER,&t,&original,110));
    assert(osg_publication_tick(&s,&original,350)==OSG_PUBLICATION_NO_CHANGE);
    assert(!osg_publication_begin_apply(&s,&route,OWNER,&t,&original,350));
    assert(!osg_publication_confirm_applied(&s,&route,OWNER,&t,&desired,350));
    /* Unknown outcome blocks new owners; exact lease reconciliation can restore. */
    t=claim(&s,&original,&desired,400,250);
    assert(osg_publication_begin_apply(&s,&route,OWNER,&t,&original,410));
    assert(osg_publication_tick(&s,NULL,650)==OSG_PUBLICATION_UNVERIFIED);
    assert(s.phase==OSG_PUBLICATION_ATTENTION);
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&desired,660)==OSG_PUBLICATION_RESTORE);
    assert(osg_publication_restore_allowed(&s,&s.ticket,&desired));
    assert(osg_publication_confirm_restored(&s,&s.ticket,&original));
}
static void preserve_and_unknown(void) {
    osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope();
    osg_publication_view_t original=view(true),desired=view(false),external=desired;
    osg_publication_ticket_t t=claim(&s,&original,&desired,100,250);apply(&s,&t,&original,&desired,110);
    external.targets[0].configuration_generation++;
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&external,120)==OSG_PUBLICATION_PRESERVED);
    assert(s.phase==OSG_PUBLICATION_IDLE && !osg_publication_restore_allowed(&s,&s.ticket,&external));
    t=claim(&s,&original,&desired,130,250);apply(&s,&t,&original,&desired,140);
    assert(osg_publication_invalidate(&s,&route,NULL,150)==OSG_PUBLICATION_UNVERIFIED);
    assert(s.phase==OSG_PUBLICATION_ATTENTION);
    osg_publication_ticket_t rejected;
    assert(osg_publication_claim_current(&s,&route,OWNER,&desired,&desired,151,250,&rejected)==OSG_PUBLICATION_BUSY);
    assert(!osg_publication_apply_allowed(&s,&route,OWNER,&t,&desired,151));
    /* Exact existing lease recovery reads again; it never reclaims/replays. */
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&desired,160)==OSG_PUBLICATION_RESTORE);
    assert(!osg_publication_restore_allowed(&s,&s.ticket,NULL));assert(s.phase==OSG_PUBLICATION_ATTENTION);
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&desired,170)==OSG_PUBLICATION_RESTORE);
    assert(!osg_publication_restore_allowed(&s,&s.ticket,&external));assert(s.phase==OSG_PUBLICATION_IDLE);
    t=claim(&s,&original,&desired,180,250);apply(&s,&t,&original,&desired,190);
    assert(osg_publication_invalidate(&s,&route,&desired,200)==OSG_PUBLICATION_RESTORE);
    assert(osg_publication_invalidate(&s,&route,&desired,201)==OSG_PUBLICATION_RESTORE);
    assert(osg_publication_confirm_restored(&s,&s.ticket,&original));
}
static void scope_and_transition_fences(void) {
    osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope(),changed=route;
    osg_publication_view_t original=view(true),desired=view(false),foreign=original;
    osg_publication_ticket_t t=claim(&s,&original,&desired,100,250),other;
    changed.route_generation++;
    assert(!osg_publication_apply_allowed(&s,&changed,OWNER,&t,&original,110));
    assert(osg_publication_renew(&s,&changed,OWNER,t.lease_id,110,250,&other)==OSG_PUBLICATION_NOT_OWNED);
    assert(osg_publication_invalidate(&s,&changed,&original,110)==OSG_PUBLICATION_NOT_OWNED);
    changed=route;changed.boot++;
    assert(!osg_publication_confirm_applied(&s,&changed,OWNER,&t,&desired,110));
    changed=route;changed.publisher[0]++;
    assert(osg_publication_claim_current(&s,&changed,OWNER,&original,&desired,110,250,&other)==OSG_PUBLICATION_BUSY);
    foreign.targets[0].profile_revision++;
    assert(!osg_publication_apply_allowed(&s,&route,OWNER,&t,&foreign,110));
    other=t;other.transition++;
    assert(!osg_publication_apply_allowed(&s,&route,OWNER,&other,&original,110));
    assert(!osg_publication_confirm_applied(&s,&route,OWNER,&t,&original,110));
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&original,120)==OSG_PUBLICATION_NO_CHANGE);
    assert(!osg_publication_apply_allowed(&s,&route,OWNER,&t,&original,121));
}
static void multi_target(void) {
    osg_publication_t s;osg_publication_init(&s);osg_publication_view_t original=view(true);
    original.count=32;
    for(unsigned i=1;i<32;i++) {original.targets[i]=original.targets[0];original.targets[i].identity[0]=(uint8_t)(i+10);original.targets[i].order=(uint8_t)i;original.targets[i].report_id=(uint8_t)(i+1);}
    assert(osg_publication_view_valid(&original));
    osg_publication_view_t desired=original;
    for(unsigned i=0;i<32;i++) {desired.targets[i].enabled=false;desired.targets[i].order=255;}
    osg_publication_ticket_t t=claim(&s,&original,&desired,100,250);apply(&s,&t,&original,&desired,110);
    osg_publication_scope_t route=scope();
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&desired,120)==OSG_PUBLICATION_RESTORE);
    assert(osg_publication_restore_allowed(&s,&s.ticket,&desired));assert(osg_publication_confirm_restored(&s,&s.ticket,&original));
    desired=original;desired.targets[0].report_id=2;assert(!osg_publication_view_valid(&desired));
    desired=original;desired.targets[0].order=1;assert(!osg_publication_view_valid(&desired));
    desired=original;desired.targets[1].identity[0]=3;assert(!osg_publication_view_valid(&desired));
}
static void bounds_clock_and_exhaustion(void) {
    osg_publication_t s;osg_publication_init(&s);osg_publication_scope_t route=scope(),bad=route;
    osg_publication_view_t original=view(true),desired=view(false),invalid=original;osg_publication_ticket_t t;
    assert(!osg_publication_view_valid(NULL));invalid.count=0;assert(!osg_publication_view_valid(&invalid));invalid.count=33;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].boot=0;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].configuration_generation=0;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].profile_revision=0;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].identity[0]=0;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].input_capable=false;assert(!osg_publication_view_valid(&invalid));
    invalid=desired;invalid.targets[0].order=0;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].order=32;assert(!osg_publication_view_valid(&invalid));
    invalid=original;invalid.targets[0].report_id=33;assert(!osg_publication_view_valid(&invalid));
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,100,249,&t)==OSG_PUBLICATION_INVALID);
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,100,5001,&t)==OSG_PUBLICATION_INVALID);
    bad.boot=0;assert(osg_publication_claim_current(&s,&bad,OWNER,&original,&desired,100,250,&t)==OSG_PUBLICATION_INVALID);
    bad=route;bad.route_generation=0;assert(osg_publication_claim_current(&s,&bad,OWNER,&original,&desired,100,250,&t)==OSG_PUBLICATION_INVALID);
    assert(osg_publication_claim_current(&s,NULL,OWNER,&original,&desired,100,250,&t)==OSG_PUBLICATION_INVALID);
    uint8_t zero[16]={0};assert(osg_publication_claim_current(&s,&route,zero,&original,&desired,100,250,&t)==OSG_PUBLICATION_INVALID);
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,100,250,NULL)==OSG_PUBLICATION_INVALID);
    desired.targets[0].configuration_generation++;
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,100,250,&t)==OSG_PUBLICATION_INVALID);
    desired=view(false);t=claim(&s,&original,&desired,UINT32_MAX-100,250);
    assert(t.expires_at==149);apply(&s,&t,&original,&desired,UINT32_MAX-90);
    assert(osg_publication_tick(&s,&desired,148)==OSG_PUBLICATION_OK);
    assert(osg_publication_tick(&s,&desired,149)==OSG_PUBLICATION_RESTORE);
    assert(osg_publication_confirm_restored(&s,&s.ticket,&original));
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,148,250,&t)==OSG_PUBLICATION_STALE_TIME);
    t=claim(&s,&original,&desired,200,250);
    assert(osg_publication_renew(&s,&route,OWNER,t.lease_id,199,250,&t)==OSG_PUBLICATION_STALE_TIME);
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&original,199)==OSG_PUBLICATION_STALE_TIME);
    assert(osg_publication_tick(&s,&original,199)==OSG_PUBLICATION_STALE_TIME);
    assert(osg_publication_invalidate(&s,&route,&original,199)==OSG_PUBLICATION_STALE_TIME);
    s.transition=UINT64_MAX;
    assert(osg_publication_release(&s,&route,OWNER,t.lease_id,&desired,201)==OSG_PUBLICATION_EXHAUSTED);
    assert(!osg_publication_apply_allowed(&s,&route,OWNER,&t,&original,201));
    osg_publication_init(&s);s.lease_counter=UINT32_MAX;
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,1,250,&t)==OSG_PUBLICATION_EXHAUSTED);
    osg_publication_init(&s);s.transition=UINT64_MAX;
    assert(osg_publication_claim_current(&s,&route,OWNER,&original,&desired,1,250,&t)==OSG_PUBLICATION_EXHAUSTED);
    assert(osg_publication_release(NULL,&route,OWNER,1,&original,1)==OSG_PUBLICATION_INVALID);
    assert(osg_publication_tick(NULL,&original,1)==OSG_PUBLICATION_INVALID);
    assert(osg_publication_invalidate(NULL,&route,&original,1)==OSG_PUBLICATION_INVALID);
    assert(osg_publication_renew(NULL,&route,OWNER,1,1,250,&t)==OSG_PUBLICATION_INVALID);
    assert(!osg_publication_apply_allowed(NULL,&route,OWNER,&t,&original,1));
    assert(!osg_publication_begin_apply(NULL,&route,OWNER,&t,&original,1));
    assert(!osg_publication_confirm_applied(NULL,&route,OWNER,&t,&desired,1));
    assert(!osg_publication_restore_allowed(NULL,&t,&desired));assert(!osg_publication_confirm_restored(NULL,&t,&original));
    osg_publication_init(NULL);
}
int main(void) {
    lifecycle();lost_ack_and_expiry();capture_is_immutable();admitted_but_unconfirmed();preserve_and_unknown();scope_and_transition_fences();multi_target();bounds_clock_and_exhaustion();
    puts("Volatile publication ownership: capture, expiry, fencing and compare restoration passed");return 0;
}
