/* SPDX-License-Identifier: MIT
 * Pipeline validation follows executable NIR, not every AIR declaration.
 * Direct texture/sampler/image declarations are filtered by the same rule.
 * Argument-buffer layouts remain intact. Dynamic/unknown indexing keeps all
 * declarations, and every load/store/atomic to a known slot keeps that slot.
 */
static void
native_optimize_bindings(nir_shader *s)
{
   bool progress;
   do {
      progress = false;
      NIR_PASS(progress, s, nir_lower_vars_to_ssa);
      NIR_PASS(progress, s, nir_copy_prop);
      NIR_PASS(progress, s, nir_opt_dce);
      NIR_PASS(progress, s, nir_opt_constant_folding);
      NIR_PASS(progress, s, nir_opt_algebraic);
      NIR_PASS(progress, s, nir_opt_dead_cf);
      NIR_PASS(progress, s, nir_opt_cse);
      NIR_PASS(progress, s, nir_opt_if, 0);
      NIR_PASS(progress, s, nir_opt_peephole_select, 8, true, true);
   } while (progress);
}

struct native_resource_use {
   uint64_t ubo, ssbo, texture, sampler, image;
   bool any_ubo, any_ssbo, any_texture, any_sampler, any_image;
};

static struct native_resource_use
native_resource_usage(nir_shader *s)
{
   struct native_resource_use use = {0};
   nir_foreach_function_impl(impl, s) {
      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type == nir_instr_type_tex) {
               nir_tex_instr *t = nir_instr_as_tex(instr);
               bool sampled = t->op == nir_texop_tex || t->op == nir_texop_txb ||
                              t->op == nir_texop_txl || t->op == nir_texop_txd ||
                              t->op == nir_texop_tg4 || t->op == nir_texop_lod;
               if (!sampled && t->op != nir_texop_txf && t->op != nir_texop_txf_ms &&
                   t->op != nir_texop_txs && t->op != nir_texop_query_levels &&
                   t->op != nir_texop_texture_samples && t->op != nir_texop_samples_identical)
                  use.any_sampler = true; /* an unfamiliar operation may need a sampler */
               use.any_texture |= t->texture_index >= 64;
               if (t->texture_index < 64) use.texture |= UINT64_C(1)<<t->texture_index;
               if (sampled) {
                  use.any_sampler |= t->sampler_index >= 64;
                  if (t->sampler_index < 64) use.sampler |= UINT64_C(1)<<t->sampler_index;
               }
               for (unsigned i=0;i<t->num_srcs;i++) {
                  switch (t->src[i].src_type) {
                  case nir_tex_src_texture_offset:
                  case nir_tex_src_texture_handle:
                  case nir_tex_src_texture_deref: use.any_texture=true;break;
                  case nir_tex_src_sampler_offset:
                  case nir_tex_src_sampler_handle:
                  case nir_tex_src_sampler_deref: use.any_sampler=true;break;
                  default: break;
                  }
               }
               continue;
            }
            if (instr->type != nir_instr_type_intrinsic)
               continue;
            nir_intrinsic_instr *in = nir_instr_as_intrinsic(instr);
            const char *name = nir_intrinsic_infos[in->intrinsic].name;
            if (strstr(name,"image")) {
               /* Direct image intrinsics carry their image index in src0.
                * Deref/bindless or unfamiliar forms conservatively retain all. */
               if (!strncmp(name,"image_",6) && nir_intrinsic_infos[in->intrinsic].num_srcs &&
                   nir_src_is_const(in->src[0]) && nir_src_as_uint(in->src[0])<64)
                  use.image |= UINT64_C(1)<<nir_src_as_uint(in->src[0]);
               else use.any_image=true;
            }
            bool ubo = strstr(name, "ubo") != NULL;
            bool ssbo = strstr(name, "ssbo") != NULL;
            if (!ubo && !ssbo)
               continue;
            int source = -1;
            switch (in->intrinsic) {
            case nir_intrinsic_load_ubo:
            case nir_intrinsic_load_ubo_vec4:
            case nir_intrinsic_load_ssbo:
            case nir_intrinsic_get_ssbo_size:
            case nir_intrinsic_ssbo_atomic:
            case nir_intrinsic_ssbo_atomic_swap:
               source = 0; break;
            case nir_intrinsic_store_ssbo:
               source = 1; break;
            default: break;
            }
            if (source < 0 || !nir_src_is_const(in->src[source])) {
               use.any_ubo |= ubo;
               use.any_ssbo |= ssbo;
               continue;
            }
            unsigned index = nir_src_as_uint(in->src[source]);
            if (index >= 64) {
               use.any_ubo |= ubo;
               use.any_ssbo |= ssbo;
            } else if (ubo) {
               use.ubo |= UINT64_C(1) << index;
            } else {
               use.ssbo |= UINT64_C(1) << index;
            }
         }
      }
   }
   return use;
}

static bool
native_resource_active(struct native_resource_use use, const struct air_resource *r)
{
   if (r->index >= 64 || r->argbuf_set != ~0u)
      return true;
   if (r->kind == AIR_RESOURCE_CONSTANT_BUFFER)
      return use.any_ubo || (use.ubo & (UINT64_C(1) << r->index));
   if (r->kind == AIR_RESOURCE_DEVICE_BUFFER)
      return use.any_ssbo || (use.ssbo & (UINT64_C(1) << r->index));
   if (r->kind == AIR_RESOURCE_TEXTURE)
      return use.any_texture || (use.texture & (UINT64_C(1)<<r->index));
   if (r->kind == AIR_RESOURCE_SAMPLER)
      return use.any_sampler || (use.sampler & (UINT64_C(1)<<r->index));
   if (r->kind == AIR_RESOURCE_IMAGE)
      return use.any_image || (use.image & (UINT64_C(1)<<r->index));
   return true;
}

static unsigned
native_prune_direct_resources(nir_shader *s, struct air_shader_info *info)
{
   native_optimize_bindings(s);
   struct native_resource_use use = native_resource_usage(s);
   unsigned kept = 0, old_count = info->num_resources;
   for (unsigned i = 0; i < old_count; i++)
      if (native_resource_active(use, &info->resources[i]))
         info->resources[kept++] = info->resources[i];
   info->num_resources = kept;
   /* NIR resource counts/binding numbers are unchanged: no compaction of slots. */
   return old_count - kept;
}
