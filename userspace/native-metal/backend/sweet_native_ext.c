/* SPDX-License-Identifier: MIT
 * Included after air_metal.c; Gallium/NIR stay inside this library. */
unsigned rwa_native_abi(void) { return SWEET_NATIVE_METAL_ABI; }
struct rwa_library {
   void *ctx;
   void *data;
   struct air_metallib lib;
};
struct rwa_library *rwa_library_open(const void *bytes, size_t size, char *why, size_t n)
{
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
   d->pipe->memory_barrier(d->pipe,PIPE_BARRIER_ALL);return 0;
}
int rwa_command_begin(struct rwa_device *d,char *why,size_t n)
{
   if(!d || d->lost || d->in_command){say(why,n,"device lost or command buffer already executing");return -1;}
   d->in_command=true;return 0;
}
int rwa_command_end(struct rwa_device *d,char *why,size_t n)
{
   if(!d || !d->in_command){say(why,n,"no command buffer is executing");return -1;}
   // Changing/unbinding framebuffer closes its Gallium batch; no per-draw flush.
   d->pipe->set_framebuffer_state(d->pipe,&(struct pipe_framebuffer_state){0});
   util_unreference_framebuffer_state(&d->batch_fb);
   int result=finish_or_say_why(d,false,"command buffer",why,n);
   d->in_command=false;
   if(result)d->lost=true;
   return result;
}
int rwa_clear(struct rwa_device *d,struct rwa_texture *const *color,unsigned count,unsigned mask,
               const float values[8][4],struct rwa_texture *depth,const float *z,const unsigned *s,
               char *why,size_t n)
{
   if(!d || d->lost || count>8 || (mask>>count)){say(why,n,"invalid clear attachments");return -1;}
   for(unsigned i=0;i<count;i++)if(mask&(1u<<i)) {
      if(!color || !color[i]){say(why,n,"clear attachment %u is missing",i);return -1;}
      struct pipe_resource *res=color[i]->res;
      if(!(res->bind&PIPE_BIND_RENDER_TARGET) || util_format_is_pure_integer(res->format)) {
         say(why,n,"interface: clear requires a floating-point or normalized render target");return -1;
      }
      struct pipe_surface *surface=d->pipe->create_surface(d->pipe,res,&(struct pipe_surface){.format=res->format});
      if(!surface){say(why,n,"failed to create clear surface");return -1;}
      union pipe_color_union value;
      memcpy(value.f,values[i],sizeof(value.f));
      d->pipe->clear_render_target(d->pipe,surface,&value,0,0,res->width0,res->height0,false);
      pipe_surface_reference(&surface,NULL);
   }
   if(z || s) {
      if(!depth){say(why,n,"depth/stencil clear has no attachment");return -1;}
      struct pipe_resource *res=depth->res;
      unsigned flags=(z?PIPE_CLEAR_DEPTH:0)|(s?PIPE_CLEAR_STENCIL:0);
      if((z&&!format_has_depth(res->format)) || (s&&!format_has_stencil(res->format))) {
         say(why,n,"depth/stencil clear aspect absent from format");return -1;
      }
      struct pipe_surface *surface=d->pipe->create_surface(d->pipe,res,&(struct pipe_surface){.format=res->format});
      if(!surface){say(why,n,"failed to create depth clear surface");return -1;}
      d->pipe->clear_depth_stencil(d->pipe,surface,flags,z?*z:0,s?*s:0,0,0,res->width0,res->height0,false);
      pipe_surface_reference(&surface,NULL);
   }
   return d->in_command?0:finish_or_say_why(d,true,"clear",why,n);
}
