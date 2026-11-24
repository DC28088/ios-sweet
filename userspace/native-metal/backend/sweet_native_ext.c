/* SPDX-License-Identifier: MIT
 * Included after air_metal.c; Gallium/NIR stay inside this library. */
#include <errno.h>
unsigned rwa_native_abi(void) { return SWEET_NATIVE_METAL_ABI; }
struct rwa_library {
   void *ctx;
   void *data;
   struct air_metallib lib;
};
/*
 * iOS 26 ships its system metallibs (QuartzCore default.metallib) as a fat container: big-endian
 * 0xCAFEBABE, nfat_arch, then {cputype, cpusubtype, offset, size, align} per slice; one slice is the
 * generic AIR library (CPU_TYPE_AIR64 0x01000017, an MTLB container) and one an Apple GPU binary.
 * Use the AIR slice; a plain MTLB file (iOS 16) is unchanged. Bounds are checked per slice.
 */
static uint32_t rwa_be32(const unsigned char *p) { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static const void *rwa_air_slice(const void *bytes, size_t *size)
{
   const unsigned char *b=bytes;
   if (*size < 8 || rwa_be32(b) != 0xcafebabeu) return bytes;
   uint32_t count=rwa_be32(b+4);
   if (count == 0 || count > 16 || 8 + (size_t)count * 20 > *size) return bytes;
   for (uint32_t i=0;i<count;i++) {
      const unsigned char *a=b+8+(size_t)i*20;
      uint32_t cputype=rwa_be32(a), off=rwa_be32(a+8), len=rwa_be32(a+12);
      if (cputype != 0x01000017u || off > *size || len > *size - off || len < 4) continue;
      if (memcmp(b+off,"MTLB",4)) continue;
      *size=len; return b+off;
   }
   return bytes;
}
struct rwa_library *rwa_library_open(const void *bytes, size_t size, char *why, size_t n)
{
   if (bytes) bytes=rwa_air_slice(bytes,&size);
   if (!bytes || size < 4 || memcmp(bytes,"MTLB",4)) {
      say(why,n,"reader: expected a Metal library container"); return NULL;
   }
   struct rwa_library *lib=calloc(1,sizeof(*lib));
   if (!lib) { say(why,n,"out of memory reading metallib");return NULL; }
   lib->ctx=ralloc_context(NULL);lib->data=malloc(size);
   if (!lib->ctx || !lib->data) { rwa_library_close(lib);return NULL; }
   memcpy(lib->data,bytes,size);
   const char *error=NULL;
   if (!air_metallib_parse(lib->ctx,lib->data,size,&lib->lib,&error)) {
      say(why,n,"reader: %s",error?error:"invalid metallib");rwa_library_close(lib);return NULL;
   }
   return lib;
}
void rwa_library_close(struct rwa_library *lib)
{
   if (lib) {ralloc_free(lib->ctx);free(lib->data);free(lib);}
}
unsigned rwa_library_count(const struct rwa_library *lib) {return lib?lib->lib.num_functions:0;}
const char *rwa_library_name(const struct rwa_library *lib,unsigned index)
{ return lib && index<lib->lib.num_functions?lib->lib.functions[index].name:NULL; }
int rwa_library_function(const struct rwa_library *lib,unsigned index,const void **bytes,size_t *size,unsigned *type)
{
   if(!lib || index>=lib->lib.num_functions || !bytes || !size || !type)return -1;
   const struct air_metallib_function *f=&lib->lib.functions[index];
   if(f->type<0 || f->type>2)return -1;
   *bytes=f->bitcode;*size=f->bitcode_size;*type=f->type+1;return 0;
}
int rwa_set_constants(struct rwa_device *d,unsigned stage,const struct rwa_constant *values,
                      unsigned count,char *why,size_t n)
{
   if(!d || stage>=3 || count>256 || (count&&!values)) {say(why,n,"invalid specialization table");return -1;}
   for(unsigned i=0;i<count;i++) if(!values[i].bytes || values[i].bytes>8) {
      say(why,n,"interface: function constant exceeds AIR's 64-bit value representation");return -1;
   }
   for(unsigned i=0;i<d->constant_count[stage];i++)free((void*)d->constants[stage][i].name);
   d->constant_count[stage]=0;
   for(unsigned i=0;i<count;i++) {
      d->constants[stage][i]=values[i];
      d->constants[stage][i].name=values[i].name?strdup(values[i].name):NULL;
      if(values[i].name&&!d->constants[stage][i].name){say(why,n,"out of memory copying constant name");return -1;}
      d->constant_count[stage]++;
   }
   return 0;
}
static unsigned native_constants(struct rwa_device *d,unsigned stage,void *ctx,const void *bytes,
                                  size_t size,struct air_constant_value **out)
{
   struct air_constant_value *declared=NULL;
   unsigned declarations=air_module_function_constants(ctx,bytes,size,&declared);
   *out=rzalloc_array(ctx,struct air_constant_value,declarations);
   unsigned count=0;
   for(unsigned i=0;i<declarations;i++) {
      for(unsigned j=0;j<d->constant_count[stage];j++) {
         const struct rwa_constant *v=&d->constants[stage][j];
         if(v->name ? declared[i].name && !strcmp(v->name,declared[i].name) : declared[i].index==v->index) {
            (*out)[count]=declared[i];(*out)[count++].bits=v->bits;break;
         }
      }
   }
   return count; // absent constants stay absent; never silently specialize to zero
}
int rwa_barrier(struct rwa_device *d,char *why,size_t n)
{
   if(!d || d->lost){say(why,n,"device lost while encoding memory barrier");return -1;}
   native_invalidate_constants(d);
   d->pipe->memory_barrier(d->pipe,PIPE_BARRIER_ALL);return 0;
}
static uint64_t native_now_ns(void)
{
   struct timespec now;clock_gettime(CLOCK_MONOTONIC_RAW,&now);
   return (uint64_t)now.tv_sec*1000000000ull+now.tv_nsec;
}
int rwa_render_draw(struct rwa_render *render,const struct rwa_draw *draw,char *why,size_t n)
{
   uint64_t began=native_now_ns();
   render->device->native_buffer_writes=false;
   int result=native_render_draw(render,draw,why,n);
   if(render->device->native_buffer_writes)
      native_invalidate_constants(render->device);
   render->device->native_draw_ns+=native_now_ns()-began;
   render->device->native_draw_count++;
   return result;
}
/* Which core runs the render thread. The ADT describes the six Cortex-A55
 * and two Cortex-A76 as one homogeneous cluster, so XNU places it anywhere.
 * TPIDR_EL0 bits 0-11 hold the current CPU number on this kernel. */
static unsigned native_cpu(void)
{
   uint64_t v;__asm__ volatile("mrs %0, TPIDR_EL0":"=r"(v));
   return (unsigned)(v&0xfff)&7;
}
static unsigned native_begin_cpu;
static uint64_t native_cpu_commands[8],native_cpu_encode_ns[8],native_cpu_draws[8],native_cpu_draw_count_at_begin;
int rwa_command_begin(struct rwa_device *d,char *why,size_t n)
{
   native_begin_cpu=native_cpu();native_cpu_draw_count_at_begin=d?d->native_draw_count:0;
   if(!d || d->lost || d->in_command){say(why,n,"device lost or command buffer already executing");return -1;}
   if(sweet_transport_command_begin()){say(why,n,"transport transaction already active");return -1;}
   native_invalidate_constants(d);
   memset(d->native_views_valid,0,sizeof(d->native_views_valid));
   memset(d->native_images_valid,0,sizeof(d->native_images_valid));
   memset(d->native_samplers_valid,0,sizeof(d->native_samplers_valid));
   d->native_begin_ns=native_now_ns();
   if(!d->native_window_ns)d->native_window_ns=d->native_begin_ns;
   d->in_command=true;return 0;
}
static int native_command_end(struct rwa_device *d,uint64_t *submitted,char *why,size_t n)
{
   if(!d || !d->in_command){say(why,n,"no command buffer is executing");return -1;}
   uint64_t encoded=native_now_ns();
   d->native_encode_ns+=encoded-d->native_begin_ns;
   native_cpu_commands[native_begin_cpu]++;
   native_cpu_encode_ns[native_begin_cpu]+=encoded-d->native_begin_ns;
   native_cpu_draws[native_begin_cpu]+=d->native_draw_count-native_cpu_draw_count_at_begin;
   // Changing/unbinding framebuffer closes its Gallium batch; no per-draw flush.
   d->pipe->set_framebuffer_state(d->pipe,&(struct pipe_framebuffer_state){0});
   util_unreference_framebuffer_state(&d->batch_fb);
   int result=0;
   if(submitted) {
      struct pipe_fence_handle *fence=NULL;
      d->pipe->flush(d->pipe,&fence,0);
      if(fence) {
         /* Timeout 0 still flushes freedreno's deferred submits into the
          * kernel (non-threaded msm fences are always "ready"); it only skips
          * the wait. The completion thread waits for the kernel fence. */
         d->screen->fence_finish(d->screen,NULL,fence,0);
         d->screen->fence_reference(d->screen,&fence,NULL);
      }
   } else result=finish_or_say_why(d,false,"command buffer",why,n);
   sweet_transport_command_end();
   if(submitted)*submitted=sweet_transport_submitted_fence();
   for(unsigned i=0;i<d->native_upload_count;i++)
      pipe_resource_reference(&d->native_uploads[i].cb.buffer,NULL);
   d->native_upload_count=0;
   // Work objects still retain every sampler/texture here. Clear raw sampler
   // state pointers before returning control to callers that can destroy them.
   d->in_command=false;
   for(unsigned stage=0;stage<PIPE_SHADER_TYPES;stage++) {
      struct rwa_bound pending={0};
      pending.num_views=d->native_pending_views[stage];
      pending.num_images=d->native_pending_images[stage];
      pending.num_samplers=d->native_pending_samplers[stage];
      unbind_stage(d,(enum pipe_shader_type)stage,&pending);
      d->native_pending_views[stage]=d->native_pending_images[stage]=d->native_pending_samplers[stage]=0;
   }
   uint64_t ended=native_now_ns();
   d->native_finish_ns+=ended-encoded;d->native_command_count++;
   if(ended-d->native_window_ns>=5000000000ull) {
      fprintf(stderr,"SweetNativeMetal: frontend window_ms=%llu commands=%llu draws=%llu encode_us=%llu draw_us=%llu finish_us=%llu\n",
         (unsigned long long)((ended-d->native_window_ns)/1000000),
         (unsigned long long)d->native_command_count,(unsigned long long)d->native_draw_count,
         (unsigned long long)(d->native_encode_ns/1000),(unsigned long long)(d->native_draw_ns/1000),
         (unsigned long long)(d->native_finish_ns/1000));
      fprintf(stderr,"SweetNativeMetal: render cpu commands=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu encode_ms=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu draws=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu (cpu0-5 A55, cpu6-7 A76)\n",
         (unsigned long long)native_cpu_commands[0],(unsigned long long)native_cpu_commands[1],(unsigned long long)native_cpu_commands[2],(unsigned long long)native_cpu_commands[3],
         (unsigned long long)native_cpu_commands[4],(unsigned long long)native_cpu_commands[5],(unsigned long long)native_cpu_commands[6],(unsigned long long)native_cpu_commands[7],
         (unsigned long long)(native_cpu_encode_ns[0]/1000000),(unsigned long long)(native_cpu_encode_ns[1]/1000000),(unsigned long long)(native_cpu_encode_ns[2]/1000000),(unsigned long long)(native_cpu_encode_ns[3]/1000000),
         (unsigned long long)(native_cpu_encode_ns[4]/1000000),(unsigned long long)(native_cpu_encode_ns[5]/1000000),(unsigned long long)(native_cpu_encode_ns[6]/1000000),(unsigned long long)(native_cpu_encode_ns[7]/1000000),
         (unsigned long long)native_cpu_draws[0],(unsigned long long)native_cpu_draws[1],(unsigned long long)native_cpu_draws[2],(unsigned long long)native_cpu_draws[3],
         (unsigned long long)native_cpu_draws[4],(unsigned long long)native_cpu_draws[5],(unsigned long long)native_cpu_draws[6],(unsigned long long)native_cpu_draws[7]);
      memset(native_cpu_commands,0,sizeof native_cpu_commands);memset(native_cpu_encode_ns,0,sizeof native_cpu_encode_ns);memset(native_cpu_draws,0,sizeof native_cpu_draws);
      fprintf(stderr,"SweetNativeMetal: draw phases validation_surface_us=%llu fixed_state_us=%llu bindings_us=%llu draw_vbo_us=%llu\n",
         (unsigned long long)(d->native_phase_ns[0]/1000),(unsigned long long)(d->native_phase_ns[1]/1000),
         (unsigned long long)(d->native_phase_ns[2]/1000),(unsigned long long)(d->native_phase_ns[3]/1000));
      fprintf(stderr,"SweetNativeMetal: constant bindings calls=%llu reused=%llu\n",
         (unsigned long long)d->native_cb_calls,(unsigned long long)d->native_cb_reused);
      fprintf(stderr,"SweetNativeMetal: binding driver_us constants=%llu views=%llu images=%llu samplers=%llu reused_states=%llu\n",
         (unsigned long long)(d->native_cb_ns/1000),(unsigned long long)(d->native_views_ns/1000),
         (unsigned long long)(d->native_images_ns/1000),(unsigned long long)(d->native_samplers_ns/1000),
         (unsigned long long)d->native_binding_reused);
      fprintf(stderr,"SweetNativeMetal: constant uploads hits=%llu misses=%llu copy_us=%llu bytes=%llu\n",
         (unsigned long long)d->native_upload_hits,(unsigned long long)d->native_upload_misses,
         (unsigned long long)(d->native_upload_ns/1000),(unsigned long long)d->native_upload_bytes);
      fprintf(stderr,"SweetNativeMetal: bounded constant uploads count=%llu avoided_bytes=%llu\n",
         (unsigned long long)d->native_uniform_trimmed,(unsigned long long)d->native_uniform_trim_bytes);
      d->native_uniform_trimmed=d->native_uniform_trim_bytes=0;
      d->native_upload_hits=d->native_upload_misses=d->native_upload_ns=d->native_upload_bytes=0;
      d->native_cb_ns=d->native_views_ns=d->native_images_ns=d->native_samplers_ns=d->native_binding_reused=0;
      d->native_cb_calls=d->native_cb_reused=0;
      memset(d->native_phase_ns,0,sizeof(d->native_phase_ns));
      d->native_window_ns=ended;d->native_command_count=d->native_draw_count=0;
      d->native_encode_ns=d->native_draw_ns=d->native_finish_ns=0;
   }
   d->in_command=false;
   if(result)d->lost=true;
   return result;
}
int rwa_command_end(struct rwa_device *d,char *why,size_t n)
{
   return native_command_end(d,NULL,why,n);
}
int rwa_command_submit(struct rwa_device *d,uint64_t *fence,char *why,size_t n)
{
   if(!fence){say(why,n,"asynchronous command end needs a fence result");return -1;}
   *fence=0;
   return native_command_end(d,fence,why,n);
}
int rwa_command_poll(struct rwa_device *d,uint64_t fence,bool give_up,char *why,size_t n)
{
   if(!d){say(why,n,"no device for command completion");return -1;}
   int e=sweet_transport_poll_fence(fence);
   if(!e)return 0;
   if(e==ETIMEDOUT && !give_up)return 1;
   d->lost=true;
   if(e==ETIMEDOUT)
      say(why,n,"the GPU did not finish a command buffer within twenty seconds; its "
                "results are not what the shaders computed and the GPU is wedged");
   else
      say(why,n,"waiting for a command buffer's GPU fence failed (errno %d)",e);
   return -1;
}
int rwa_clear(struct rwa_device *d,struct rwa_texture *const *color,unsigned count,unsigned mask,
               const float values[8][4],struct rwa_texture *depth,const float *z,const unsigned *s,
               const unsigned *levels,const unsigned *layers,char *why,size_t n)
{
   if(!d || d->lost || count>8 || (mask>>count)){say(why,n,"invalid clear attachments");return -1;}
   for(unsigned i=0;i<count;i++)if(mask&(1u<<i)) {
      if(!color || !color[i]){say(why,n,"clear attachment %u is missing",i);return -1;}
      struct pipe_resource *res=color[i]->res;
      if(!(res->bind&PIPE_BIND_RENDER_TARGET) || util_format_is_pure_integer(res->format)) {
         say(why,n,"interface: clear requires a floating-point or normalized render target");return -1;
      }
      const unsigned lv=levels?levels[i]:0,ly=layers?layers[i]:0,alv=lv+color[i]->native_base_level;
      if(alv>res->last_level){say(why,n,"clear attachment %u level %u is beyond its %u levels",i,alv,res->last_level+1);return -1;}
      struct pipe_surface *surface=(lv||ly)?native_render_surface_at(color[i],lv,ly):native_render_surface(color[i]);
      if(!surface){say(why,n,"failed to create clear surface");return -1;}
      union pipe_color_union value;
      memcpy(value.f,values[i],sizeof(value.f));
      d->pipe->clear_render_target(d->pipe,surface,&value,0,0,u_minify(res->width0,alv),u_minify(res->height0,alv),false);
      pipe_surface_reference(&surface,NULL);
   }
   if(z || s) {
      if(!depth){say(why,n,"depth/stencil clear has no attachment");return -1;}
      struct pipe_resource *res=depth->res;
      unsigned flags=(z?PIPE_CLEAR_DEPTH:0)|(s?PIPE_CLEAR_STENCIL:0);
      if((z&&!format_has_depth(res->format)) || (s&&!format_has_stencil(res->format))) {
         say(why,n,"depth/stencil clear aspect absent from format");return -1;
      }
      struct pipe_surface *surface=native_render_surface(depth);
      if(!surface){say(why,n,"failed to create depth clear surface");return -1;}
      d->pipe->clear_depth_stencil(d->pipe,surface,flags,z?*z:0,s?*s:0,0,0,res->width0,res->height0,false);
      pipe_surface_reference(&surface,NULL);
   }
   return d->in_command?0:finish_or_say_why(d,true,"clear",why,n);
}

/*
 * 26c: MTLBlitCommandEncoder over Gallium. QuartzCore copies the backdrop of a Liquid Glass / blur element out of
 * the frame it is drawing with a blit, then downsamples and blurs that copy. The donor answered
 * -blitCommandEncoder with nil, so every copy was a message to nil: the effects sampled a never-written texture
 * (USER: dock and Control Centre blur missing, the glass grey).
 */
/* pipe_box filled by hand (Mesa 25.0.7 has no util/u_box.h on this include path) */
static void native_box(struct pipe_box *b,int x,int y,int z,int w,int h,int d)
{
   memset(b,0,sizeof *b);b->x=x;b->y=y;b->z=z;b->width=w;b->height=h;b->depth=d;
}
static unsigned native_layers(const struct pipe_resource *r,unsigned level)
{
   return r->target==PIPE_TEXTURE_3D?u_minify(r->depth0,level):r->array_size;
}
unsigned rwa_texture_levels(const struct rwa_texture *t){return t&&t->res?(t->native_levels?t->native_levels:t->res->last_level+1):0;}
unsigned rwa_texture_slices(const struct rwa_texture *t){return t&&t->res?native_layers(t->res,0):0;}
static unsigned native_blit_told;
int rwa_copy_texture(struct rwa_device *d,struct rwa_texture *src,unsigned src_slice,unsigned src_level,
                     const unsigned so[3],const unsigned size[3],struct rwa_texture *dst,unsigned dst_slice,
                     unsigned dst_level,const unsigned dor[3],char *why,size_t n)
{
   if(!d || d->lost || !src || !dst || !src->res || !dst->res){say(why,n,"blit: invalid texture copy");return -1;}
   struct pipe_resource *s=src->res,*t=dst->res;
   src_level+=src->native_base_level;src_slice+=src->native_base_layer;   /* 26c views */
   dst_level+=dst->native_base_level;dst_slice+=dst->native_base_layer;
   if(src_level>s->last_level || dst_level>t->last_level){say(why,n,"blit: copy level outside the texture");return -1;}
   if(!size[0] || !size[1] || !size[2])return 0;
   const unsigned sw=u_minify(s->width0,src_level),sh=u_minify(s->height0,src_level);
   const unsigned tw=u_minify(t->width0,dst_level),th=u_minify(t->height0,dst_level);
   if(so[0]+size[0]>sw || so[1]+size[1]>sh || dor[0]+size[0]>tw || dor[1]+size[1]>th){
      say(why,n,"blit: copy rectangle %ux%u at %u,%u -> %u,%u outside %ux%u / %ux%u",size[0],size[1],so[0],so[1],
          dor[0],dor[1],sw,sh,tw,th);return -1;
   }
   /* 2D arrays and cubes: z is the slice; 3D: z is the depth origin and the slice is 0 */
   const unsigned sz=s->target==PIPE_TEXTURE_3D?so[2]:src_slice,tz=t->target==PIPE_TEXTURE_3D?dor[2]:dst_slice;
   struct pipe_box box;native_box(&box,so[0],so[1],sz,size[0],size[1],size[2]);
   if(s->format==t->format || util_format_get_blocksize(s->format)==util_format_get_blocksize(t->format)) {
      /* Metal requires copy-compatible formats: a plain byte copy, as Metal defines it */
      d->pipe->resource_copy_region(d->pipe,t,dst_level,dor[0],dor[1],tz,s,src_level,&box);
   } else {
      struct pipe_blit_info b;memset(&b,0,sizeof b);
      b.src.resource=s;b.src.level=src_level;b.src.format=s->format;b.src.box=box;
      b.dst.resource=t;b.dst.level=dst_level;b.dst.format=t->format;native_box(&b.dst.box,dor[0],dor[1],tz,size[0],size[1],size[2]);
      b.mask=util_format_get_mask(t->format);b.filter=PIPE_TEX_FILTER_NEAREST;
      d->pipe->blit(d->pipe,&b);
   }
   if(native_blit_told++<12)
      fprintf(stderr,"SweetNativeMetal: blit copy %ux%ux%u level %u slice %u (%s) -> level %u slice %u (%s)\n",
              size[0],size[1],size[2],src_level,src_slice,util_format_short_name(s->format),dst_level,dst_slice,
              util_format_short_name(t->format));
   d->native_buffer_writes=true;
   return d->in_command?0:finish_or_say_why(d,true,"blit copy",why,n);
}
int rwa_generate_mipmaps(struct rwa_device *d,struct rwa_texture *tex,char *why,size_t n)
{
   if(!d || d->lost || !tex || !tex->res){say(why,n,"blit: invalid mipmap texture");return -1;}
   struct pipe_resource *r=tex->res;
   const unsigned base=tex->native_base_level,last=tex->native_levels?base+tex->native_levels-1:r->last_level;
   for(unsigned l=base+1;l<=last;l++) {
      struct pipe_blit_info b;memset(&b,0,sizeof b);
      b.src.resource=r;b.src.level=l-1;b.src.format=r->format;
      b.dst.resource=r;b.dst.level=l;b.dst.format=r->format;
      native_box(&b.src.box,0,0,0,u_minify(r->width0,l-1),u_minify(r->height0,l-1),native_layers(r,l-1));
      native_box(&b.dst.box,0,0,0,u_minify(r->width0,l),u_minify(r->height0,l),native_layers(r,l));
      b.mask=util_format_get_mask(r->format);
      b.filter=util_format_is_pure_integer(r->format)?PIPE_TEX_FILTER_NEAREST:PIPE_TEX_FILTER_LINEAR;
      d->pipe->blit(d->pipe,&b);
   }
   static unsigned told;
   if(told++<6)fprintf(stderr,"SweetNativeMetal: blit mipmaps %ux%u %s, %u levels\n",r->width0,r->height0,
                       util_format_short_name(r->format),r->last_level+1);
   return d->in_command?0:finish_or_say_why(d,true,"mipmaps",why,n);
}
int rwa_copy_buffer(struct rwa_device *d,struct rwa_buffer *src,size_t so,struct rwa_buffer *dst,size_t dof,
                    size_t size,char *why,size_t n)
{
   if(!d || d->lost || !src || !dst){say(why,n,"blit: invalid buffer copy");return -1;}
   if(!size)return 0;
   if(so>src->size || size>src->size-so || dof>dst->size || size>dst->size-dof || size>UINT32_MAX){
      say(why,n,"blit: buffer copy of %zu bytes outside its buffers",size);return -1;
   }
   struct pipe_box box;native_box(&box,(int)so,0,0,(int)size,1,1);
   d->pipe->resource_copy_region(d->pipe,dst->res,0,(unsigned)dof,0,0,src->res,0,&box);
   native_invalidate_constants(d);d->native_buffer_writes=true;
   return d->in_command?0:finish_or_say_why(d,true,"buffer copy",why,n);
}
int rwa_fill_buffer(struct rwa_device *d,struct rwa_buffer *buf,size_t off,size_t size,uint8_t value,char *why,size_t n)
{
   if(!d || d->lost || !buf){say(why,n,"blit: invalid buffer fill");return -1;}
   if(!size)return 0;
   if(off>buf->size || size>buf->size-off || size>UINT32_MAX){say(why,n,"blit: fill outside its buffer");return -1;}
   d->pipe->clear_buffer(d->pipe,buf->res,(unsigned)off,(unsigned)size,&value,1);
   native_invalidate_constants(d);d->native_buffer_writes=true;
   return d->in_command?0:finish_or_say_why(d,true,"buffer fill",why,n);
}

struct rwa_texture *rwa_texture_view(struct rwa_texture *p,unsigned first_level,unsigned levels,unsigned first_layer,
                                     unsigned layers,char *why,size_t n)
{
   if(!p || !p->res){say(why,n,"view of nothing");return NULL;}
   const unsigned pl=p->native_levels?p->native_levels:p->res->last_level+1;
   const unsigned pa=p->native_layers?p->native_layers:(p->layers?p->layers:1);
   if(!levels || first_level+levels>pl){say(why,n,"view levels %u+%u outside the texture's %u",first_level,levels,pl);return NULL;}
   if(!layers || first_layer+layers>pa){say(why,n,"view slices %u+%u outside the texture's %u",first_layer,layers,pa);return NULL;}
   struct rwa_texture *v=malloc(sizeof *v);
   if(!v){say(why,n,"out of memory for a texture view");return NULL;}
   *v=*p;
   v->view=NULL;v->native_surface=NULL;v->res=NULL;
   pipe_resource_reference(&v->res,p->res);
   v->native_base_level=p->native_base_level+first_level;v->native_levels=levels;
   v->native_base_layer=p->native_base_layer+first_layer;v->native_layers=layers;
   v->layers=layers;
   v->shape.width=u_minify(p->res->width0,v->native_base_level);
   v->shape.height=u_minify(p->res->height0,v->native_base_level);
   v->shape.mip_levels=levels;v->shape.array_length=layers;
   return v;
}
