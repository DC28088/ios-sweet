/* SPDX-License-Identifier: MIT */
/* APR ASM/ADM layouts: Qualcomm APR ABI, verified against Linux q6asm/q6adm.
 * Board-specific AFE readiness is the caller's prerequisite. */
#include "sweet_pcm.h"
#include <string.h>
#define ASM 7u
#define ADM 8u
#define MAP 0x10d92u
#define MAP_RSP 0x10d93u
#define UNMAP 0x10d94u
#define ADM_OPEN 0x10326u
#define ADM_OPEN_RSP 0x10329u
#define ADM_CLOSE 0x10327u
#define ROUTE 0x10325u
#define OPEN 0x10db3u
#define CLOSE 0x10bcdu
#define FORMAT 0x10d98u
#define RUN 0x10daau
#define PAUSE 0x10bd3u
#define WRITE 0x10dabu
#define WRITE_DONE 0x10d99u
#define STREAM 0x101u
#define WTOKEN 0x40000000u /* | sequence<<3 | slot (SWEET_PCM_SLOTS<=8) */
/* Teardown is ordered; if any teardown times out/fails, keep DMA allocated. */
enum { P_MAP,P_OPEN,P_ADM,P_ROUTE,P_FORMAT,P_RUN,P_PLAY,P_PAUSE,P_CLOSE,P_ADM_CLOSE,P_UNMAP,P_DONE };
static void p16(uint8_t *p,uint16_t x){p[0]=x;p[1]=x>>8;}
static void p32(uint8_t *p,uint32_t x){p16(p,x);p16(p+2,x>>16);}
static uint32_t g32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
void sweet_pcm_init(struct sweet_pcm *s,uint64_t dsp)
{
    memset(s,0,sizeof(*s));s->dsp=dsp;s->token=0x140000;s->limit=SWEET_PCM_LIMIT;
    if(!dsp || (dsp&4095u) || dsp>UINT64_MAX-SWEET_PCM_MAP_BYTES){s->failed=s->done=1;s->status=1;}
}
int sweet_pcm_set_limit(struct sweet_pcm *s,unsigned periods)
{
    if(!s || s->step!=P_MAP || s->waiting || s->mapped || s->failed ||
       !periods || (periods>10000 && periods!=SWEET_PCM_STREAM_LIMIT))return -1;
    s->limit=periods;return 0;
}
void sweet_pcm_abort(struct sweet_pcm *s,uint32_t status)
{
    s->failed=1;s->status=status;s->waiting=0;
    if(s->step>=P_PAUSE){s->done=1;return;}
    s->step=P_PAUSE;
}
void sweet_pcm_poll(struct sweet_pcm *s,uint64_t now)
{
    if(!s->done && ((s->waiting || s->inflight) && now>=s->deadline))sweet_pcm_abort(s,0xffffffffu);
}
static size_t pack(struct sweet_pcm *s,uint8_t *out,size_t cap,uint64_t now,uint8_t svc,uint16_t dest,uint32_t op,uint8_t *p,uint16_t len)
{
    uint16_t src=svc==ASM?dest:0;
    if(svc==ADM && (op==ADM_OPEN || op==ADM_CLOSE))src=AFE_PORT_ID_PRIMARY_MI2S_RX;
    uint32_t token=svc==ADM && op==ADM_OPEN?((AFE_PORT_INDEX_PRIMARY_MI2S_RX<<16)|s->adm_default):s->token+1;
    size_t n=sweet_apr_pack(out,cap,APR_MSG_TYPE_SEQ_CMD,svc,src,dest,token,op,p,len);
    if(n){s->token=token;s->waiting=1;s->pending_op=op;s->deadline=now+3000;}
    return n;
}
size_t sweet_pcm_next(struct sweet_pcm *s,uint8_t *out,size_t cap,uint64_t now)
{
    uint8_t p[32]={0};
    if(s->done || s->waiting)return 0;
    for(;;)switch(s->step){
    case P_MAP:
        p16(p,3);p16(p+2,1);p32(p+8,s->dsp);p32(p+12,s->dsp>>32);p32(p+16,SWEET_PCM_MAP_BYTES);
        return pack(s,out,cap,now,ASM,0,MAP,p,20);
    case P_ADM:
        p16(p+2,1);p16(p+4,AFE_PORT_ID_PRIMARY_MI2S_RX);p16(p+6,0xffff);p32(p+8,s->adm_default?0x10314:0x10312);
        p16(p+12,2);p16(p+14,16);p32(p+16,48000);p[20]=1;p[21]=2;
        return pack(s,out,cap,now,ADM,AFE_PORT_ID_PRIMARY_MI2S_RX,ADM_OPEN,p,28);
    case P_OPEN:
        p16(p+6,16);p32(p+8,0x10c68);p32(p+12,0x10da5);
        return pack(s,out,cap,now,ASM,STREAM,OPEN,p,16);
    case P_ROUTE:
        p32(p+4,1);p16(p+8,1);p16(p+10,1);p16(p+12,s->copp);
        return pack(s,out,cap,now,ADM,0,ROUTE,p,16);
    case P_FORMAT:
        p32(p,20);p16(p+4,2);p16(p+6,16);p32(p+8,48000);p16(p+12,1);p[16]=1;p[17]=2;
        return pack(s,out,cap,now,ASM,STREAM,FORMAT,p,24);
    case P_RUN:return pack(s,out,cap,now,ASM,STREAM,RUN,p,12);
    case P_PLAY:{
        if(s->sent==s->limit){if(s->inflight)return 0;s->step=P_PAUSE;continue;}
        unsigned slot=0;while(slot<SWEET_PCM_SLOTS && (s->busy&(1u<<slot)))++slot;
        if(slot==SWEET_PCM_SLOTS)return 0;
        uint64_t a=s->dsp+(uint64_t)slot*SWEET_PCM_STRIDE;
        p32(p,a);p32(p+4,a>>32);p32(p+8,s->handle);p32(p+12,SWEET_PCM_PERIOD);p32(p+16,slot);
        size_t n=sweet_apr_pack(out,cap,APR_MSG_TYPE_SEQ_CMD,ASM,STREAM,STREAM,WTOKEN|((s->sent&0x07ffffffu)<<3)|slot,WRITE,p,32);
        if(n){s->write_token[slot]=WTOKEN|((s->sent&0x07ffffffu)<<3)|slot;s->busy|=1u<<slot;++s->sent;++s->inflight;s->deadline=now+3000;}
        return n;
    }
    case P_PAUSE:if(!s->stream_open){s->step=P_CLOSE;continue;}return pack(s,out,cap,now,ASM,STREAM,PAUSE,p,0);
    case P_CLOSE:if(!s->stream_open){s->step=P_ADM_CLOSE;continue;}return pack(s,out,cap,now,ASM,STREAM,CLOSE,p,0);
    case P_ADM_CLOSE:if(!s->copp_open){s->step=P_UNMAP;continue;}return pack(s,out,cap,now,ADM,s->copp,ADM_CLOSE,p,0);
    case P_UNMAP:if(!s->mapped){s->done=1;return 0;}p32(p,s->handle);return pack(s,out,cap,now,ASM,0,UNMAP,p,4);
    default:s->done=1;return 0;
    }
}
int sweet_pcm_receive(struct sweet_pcm *s,const uint8_t *packet,size_t n)
{
    struct sweet_apr_msg m;
    if(sweet_apr_parse(packet,n,&m))return -1;
    if(s->done || m.src_domain!=APR_DOMAIN_ADSP || m.dest_domain!=APR_DOMAIN_APPS)return 0;
    if(m.opcode==WRITE_DONE && m.src_svc==ASM && m.dest_svc==ASM && m.dest_port==STREAM && (m.token&0xc0000000u)==WTOKEN){
        unsigned slot=m.token&7u;uint64_t a=s->dsp+(uint64_t)slot*SWEET_PCM_STRIDE;
        if(!(s->busy&(1u<<slot)) || s->write_token[slot]!=m.token)return 0;
        if(m.payload_len<16 || g32(m.payload)!=(uint32_t)a || g32(m.payload+4)!=(uint32_t)(a>>32) || g32(m.payload+8)!=s->handle){sweet_pcm_abort(s,0xfffffffeu);return -1;}
        s->busy&=~(1u<<slot);--s->inflight;++s->completed;
        if(g32(m.payload+12)){sweet_pcm_abort(s,g32(m.payload+12));return -1;}
        return 1;
    }
    if(!s->waiting || m.token!=s->token)return 0;
    unsigned svc=s->step==P_ADM || s->step==P_ROUTE || s->step==P_ADM_CLOSE?ADM:ASM;
    uint16_t port=svc==ASM && s->step!=P_MAP && s->step!=P_UNMAP?STREAM:0;
    if(s->step==P_ADM || s->step==P_ADM_CLOSE)port=AFE_PORT_ID_PRIMARY_MI2S_RX;
    if(m.src_svc!=svc || m.dest_svc!=svc || m.dest_port!=port)return 0;
    uint32_t status=0,op=0;
    if(m.opcode==MAP_RSP && s->step==P_MAP){
        if(m.payload_len<4 || !(s->handle=g32(m.payload)))return -1;
        s->mapped=1;
    }else if(m.opcode==ADM_OPEN_RSP && s->step==P_ADM){
        if(m.payload_len<8){sweet_pcm_abort(s,0xfffffffeu);return -1;}
        /* ABI: u32 status, u16 copp_id, u16 reserved. Firmware leaves
         * reserved nonzero; it is not part of the device id. */
        status=g32(m.payload);s->copp=g32(m.payload+4)&0xffffu;
        if(!status && s->copp<0xff)s->copp_open=1;else if(!status)status=1;
    }else{
        if(sweet_apr_basic_result(&m,&op,&status) || op!=s->pending_op)return 0;
        /* Map and ADM open require their handle-bearing response, not basic success. */
        if(!status && (s->step==P_MAP || s->step==P_ADM))return 0;
    }
    /* A legacy Sweet COPP uses stock DEFAULT topology 0x10314. Try it once
     * after an explicitly rejected null topology; no new map or AFE reset. */
    if(status && s->step==P_ADM && !s->adm_default && status<=3){
        s->adm_default=1;s->waiting=0;s->status=status;return 1;
    }
    if(status){sweet_pcm_abort(s,status);return -1;}
    if(s->step==P_OPEN)s->stream_open=1;
    if(s->step==P_CLOSE){s->stream_open=0;s->busy=s->inflight=0;}
    if(s->step==P_ADM_CLOSE)s->copp_open=0;
    if(s->step==P_UNMAP)s->mapped=0;
    if(!s->failed)s->status=0;
    s->waiting=0;++s->step;return 1;
}
