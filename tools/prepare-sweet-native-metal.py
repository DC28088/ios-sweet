#!/usr/bin/env python3
"""Adapt pinned HakiOS AIR/Gallium sources to Sweet; never touch a device.

The input is a disposable export of inputs.json's donor revision. Every edit
checks its source context so a changed donor cannot silently make another ABI.
"""
from pathlib import Path
import argparse
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'userspace/native-metal'

def replace(s,old,new,count=1):
    actual=s.count(old)
    if actual!=count:raise RuntimeError(f'expected {count} matches, found {actual}: {old[:110]!r}')
    return s.replace(old,new)

def adapt_backend(donor):
    air=donor/'tools/mesa/air'
    p=air/'nir_air.h';s=p.read_text()
    s=replace(s,'   uint64_t bits;      /* its value, as the bits of its type */',
                '   uint64_t bits;      /* little-endian packed scalar/small-vector value */\n   const char *name;    /* declared Metal function-constant name */')
    p.write_text(s)
    p=air/'air_metallib.c';s=p.read_text()
    s=replace(s,'   if (total > size || funcs + 4 > size || bc_off + bc_len > size) {',
                '   if (total > size || funcs > size || size - funcs < 4 || bc_off > size || bc_len > size - bc_off) {')
    s=replace(s,'   p += 4;\n   lib->functions =','''   p += 4;
   if (count > (size - funcs - 4) / 4) {
      *error = "function count exceeds container extent";return false;
   }
   lib->functions =''')
    s=replace(s,'      if (p + 4 > b + size) {','      if ((size_t)(b + size - p) < 4) {')
    s=replace(s,'      const uint8_t *q = p + 4, *end = p + len;\n      if (len < 4 || end > b + size) {',
                '      if (len < 4 || len > (size_t)(b + size - p)) {')
    s=replace(s,'      struct air_metallib_function *f = &lib->functions[lib->num_functions++];',
                '      const uint8_t *q = p + 4, *end = p + len;\n      struct air_metallib_function *f = &lib->functions[lib->num_functions++];')
    s=replace(s,'      while (q + 6 <= end && memcmp(q, "ENDT", 4) != 0) {',
                '      while ((size_t)(end - q) >= 6 && memcmp(q, "ENDT", 4) != 0) {')
    s=replace(s,'         if (v + vlen > end) {','         if (vlen > (size_t)(end - v)) {')
    s=replace(s,'      if (module_off + module_size > bc_len) {',
                '      if (module_off > bc_len || module_size > bc_len - module_off) {')
    p.write_text(s)
    p=air/'air_private.h';s=p.read_text()
    s=replace(s,'   AIR_VAL_PTR_SELECT,','''   AIR_VAL_PTR_SELECT,
   /* A translation-time pointer cell (air_pointer_cells.inc): .elems[0] is the
    * one stored resolution, or .cell_src a constant global's pointer
    * initializer, resolved where it is read. Never bytes in memory.
    */
   AIR_VAL_PTR_CELL,''')
    s=replace(s,'   unsigned num_elems;\n   struct air_val **elems;\n};',
                '   unsigned num_elems;\n   struct air_val **elems;\n   const struct air_value *cell_src;\n};')
    s=replace(s,'   const struct air_function *fn;\n','''   const struct air_function *fn;
   /* translating the static initializers -- and what they call, which is
    * cached and may be reused by the body: no shadow answers then */
   bool in_static_init;
   /* the executable edges of the function translate_body is in */
   struct air_edges *edges;
''')
    s=replace(s,'      bool skip;            /* llvm.* lists, and anything that failed */\n   } *global_plan;','''      bool skip;            /* llvm.* lists, and anything that failed */
      /* air_pointer_cells.inc */
      bool cell;            /* a constant wrapping one pointer: .cell_leaf */
      const struct air_value *cell_leaf;
      /* program-scope bytes known at translation time: plan_static_constants */
      uint8_t *shadow, *shadow_known;
      uint64_t shadow_size;
   } *global_plan;''')
    p.write_text(s)
    shutil.copy2(NATIVE/'backend/air_pointer_cells.inc',air/'air_pointer_cells.inc')
    shutil.copy2(NATIVE/'tests/air_specialization_check.c',air/'air_specialization_check.c')
    p=air/'meson.build';s=p.read_text()
    s=replace(s,"air2nir = executable(","""# Sweet: generic QuartzCore functions against Apple's own specializations
# (userspace/native-metal/tests/air_specialization_check.c). Host-only.
air_specialization_check = executable(
  'air_specialization_check',
  files('air_specialization_check.c'),
  dependencies : [dep_m, idep_air],
  include_directories : [inc_include, inc_src],
  build_by_default : false,
  install : false,
)

air2nir = executable(""")
    p.write_text(s)
    p=air/'air_to_nir.c';s=p.read_text()
    s=replace(s,'               (*out)[num++].index = idx;',
        '               const char *constant_name = md_str(node, 2);\n               (*out)[num].name = constant_name ? ralloc_strdup(mem_ctx, constant_name) : NULL;\n               (*out)[num++].index = idx;')
    s=replace(s,'   return v && (v->kind == AIR_VAL_PTR || v->kind == AIR_VAL_PTR_SELECT);',
                '   return v && (v->kind == AIR_VAL_PTR || v->kind == AIR_VAL_PTR_SELECT ||\n                v->kind == AIR_VAL_PTR_CELL);')
    s=replace(s,'/* -- instructions -','#include "air_pointer_cells.inc"\n\n/* -- instructions -')
    s=replace(s,'   if (base->kind == AIR_VAL_PTR_SELECT) {\n      struct air_val *a = NULL, *b = NULL;','''   if (base->kind == AIR_VAL_PTR_CELL) {
      if (!gep_indices_zero(in))
         return air_error(c, "instruction: getelementptr at an offset into a pointer cell");
      *out = base;
      return true;
   }
   if (base->kind == AIR_VAL_PTR_SELECT) {
      struct air_val *a = NULL, *b = NULL;''')
    s=replace(s,'   if (in->sub == AIR_CAST_BITCAST && dt->kind == AIR_TYPE_POINTER) {\n      struct air_val *v = air_get(c, in->operands[0]);','''   if ((in->sub == AIR_CAST_BITCAST || in->sub == AIR_CAST_ADDRSPACECAST) &&
       dt->kind == AIR_TYPE_POINTER) {
      struct air_val *v = air_get(c, in->operands[0]);
      if (v && (v->kind == AIR_VAL_PTR_CELL ||
                (v->kind == AIR_VAL_PTR_SELECT && in->sub == AIR_CAST_BITCAST))) {
         struct air_val *r = retype_pointer(c, v, dt->elem);
         if (!r)
            return air_error(c, "instruction: a bitcast of a choice with a side that is "
                                "not a pointer");
         air_set(c, &in->base, r);
         return true;
      }
   }
   if (in->sub == AIR_CAST_BITCAST && dt->kind == AIR_TYPE_POINTER) {
      struct air_val *v = air_get(c, in->operands[0]);''')
    s=replace(s,'         /* A choice between two members of an argument buffer cannot be kept','''         /* A condition known at translation time -- function constants, read
          * through the static initializer -- is no choice: take that side.
          */
         nir_def *known_cond = air_get_ssa(c, in->operands[0]);
         uint64_t take;
         if (!known_cond)
            return false;
         if (fold_scalar(c, known_cond, &take)) {
            air_set(c, &in->base, take ? pa : pb);
            return true;
         }
         /* A choice between two members of an argument buffer cannot be kept''')
    s=replace(s,'      /* A local holding a POINTER. Scratch memory cannot hold one',
        '      if (pointer_cell_alloca(c, in)) {\n         air_set(c, &in->base, air_val_cell(c, in->aux_type, NULL));\n         return true;\n      }\n      /* A local holding a POINTER. Scratch memory cannot hold one')
    s=replace(s,'   case AIR_OP_LOAD: {\n      struct air_val *p = air_get(c, in->operands[0]);\n      if (!p)\n         return false;\n','''   case AIR_OP_LOAD: {
      struct air_val *p = air_get(c, in->operands[0]);
      if (!p)
         return false;
      struct air_val *known = p->kind == AIR_VAL_PTR ? shadow_load(c, in, p->ptr) : NULL;
      if (known) {
         air_set(c, &in->base, known);
         return true;
      }
      if (p->kind == AIR_VAL_PTR_CELL ||
          (p->kind == AIR_VAL_PTR_SELECT && in->base.type->kind == AIR_TYPE_POINTER)) {
         if (in->base.type->kind != AIR_TYPE_POINTER)
            return air_error(c, "instruction: a number loaded from a pointer cell");
         struct air_val *v = load_pointer(c, p, in->base.type);
         if (!v)
            return false;
         air_set(c, &in->base, v);
         return true;
      }
''')
    s=replace(s,'      if (val_is_pointerish(sv))\n         return air_error(c, "instruction: a store of a pointer into memory, which this "','''      if (p->kind == AIR_VAL_PTR_CELL) {
         if (p->cell_src)
            return air_error(c, "instruction: a store into a constant pointer");
         if (p->elems[0])
            return air_error(c, "instruction: a second store into a pointer cell");
         if (sv->kind == AIR_VAL_PTR_CELL || !val_is_pointerish(sv))
            return air_error(c, "instruction: a pointer cell stored with something that is "
                                "not a resolution");
         p->elems[0] = sv;
         return true;
      }
      if (val_is_pointerish(sv))
         return air_error(c, "instruction: a store of a pointer into memory, which this "''')
    s=replace(s,'      return air_store_ptr(c, p->ptr, v, in->align);',
                '      shadow_store(c, in, p->ptr, v);\n      return air_store_ptr(c, p->ptr, v, in->align);')
    s=replace(s,'      } else if (g->addrspace == 3) {\n         /* a static threadgroup array */',
                '      } else if (g->is_const && g->init && plan_constant_global(c, pl, g)) {\n         /* air_pointer_cells.inc: constant data, or a pointer cell */\n      } else if (g->addrspace == 3) {\n         /* a static threadgroup array */')
    s=replace(s,'         c->scratch_size += air_type_alloc_size(c->m, g->value_type);\n      }\n   }\n}\n',
                '         c->scratch_size += air_type_alloc_size(c->m, g->value_type);\n      }\n   }\n   if (!c->error[0])\n      plan_static_constants(c);\n}\n')
    s=replace(s,'      if (pl->skip)\n         continue;\n      const struct air_global *g = c->m->globals[i];\n      struct air_ptr p = {','''      if (pl->skip)
         continue;
      const struct air_global *g = c->m->globals[i];
      if (pl->cell) {
         air_set(c, &g->base, air_val_cell(c, g->value_type, pl->cell_leaf));
         continue;
      }
      struct air_ptr p = {''')
    s=replace(s,'      if (!v)\n         return air_error(c, "interface: function constant %u has no value", ptr.binding), NULL;\n','''      if (!v) {
         /* Not supplied: what is read is the global's own initializer, which
          * for a Metal function constant is `undef` -- the uber shaders load
          * all 58 in their static initializer and guard each optional one
          * with is_function_constant_defined. Any other initializer is
          * refused rather than guessed.
          */
         bool declared_undef = false;
         for (unsigned g = 0; c->global_plan && g < c->m->num_globals; g++) {
            const struct air_global_plan *pl = &c->global_plan[g];
            const struct air_constant *k = air_value_as_constant(c->m->globals[g]->init);
            if (!pl->skip && pl->class == AIR_PTR_FUNCTION_CONSTANT &&
                pl->binding == ptr.binding)
               declared_undef = k && (k->ckind == AIR_CONST_UNDEF ||
                                      k->ckind == AIR_CONST_POISON);
         }
         if (!declared_undef)
            return air_error(c, "interface: function constant %u has no value", ptr.binding), NULL;
         res = nir_undef(&c->b, comps, bits);
         break;
      }
''')
    s=replace(s,'resolve_pointer_phi(struct air_ctx *c, const struct air_inst *in, const bool *reachable)',
                'resolve_pointer_phi(struct air_ctx *c, const struct air_inst *in,\n                    const struct air_edges *edges)')
    s=replace(s,'      if (!in->targets[k] || !reachable[in->targets[k]->index])\n         continue;',
                '      if (!in->targets[k] ||\n          !edge_possible(edges, in->targets[k]->index, in->parent->index))\n         continue;')
    s=replace(s,'   for (unsigned oi = 0; oi < num_order; oi++)\n      reachable[order[oi]] = true;\n',
                '   for (unsigned oi = 0; oi < num_order; oi++)\n      reachable[order[oi]] = true;\n   struct air_edges edges;\n   edges_init(c, &edges, f, reachable);\n   struct air_edges *outer_edges = c->edges;\n   c->edges = &edges;\n')
    s=replace(s,'      unsigned bi = order[oi];\n      const struct air_block *blk = &f->blocks[bi];\n      nir_block *nb = nir_block_of(c, blk);','''      unsigned bi = order[oi];
      const struct air_block *blk = &f->blocks[bi];
      if (!block_executable(&edges, bi)) {
         /* dead under the function constants: no instruction, no edge out */
         edges.processed[bi] = true;
         continue;
      }
      nir_block *nb = nir_block_of(c, blk);''')
    s=replace(s,'            if (!resolve_pointer_phi(c, in, reachable))','            if (!resolve_pointer_phi(c, in, &edges))')
    s=replace(s,'         struct hash_entry *e = _mesa_hash_table_search(c->phi_regs, in);\n         air_set(c, &in->base,\n','''         const struct air_value *only = single_incoming(&edges, in);
         if (only) {
            struct air_val *ov = air_get(c, only);
            if (!ov)
               return false;
            air_set(c, &in->base, ov);
            continue;
         }
         struct hash_entry *e = _mesa_hash_table_search(c->phi_regs, in);
         air_set(c, &in->base,
''')
    s=replace(s,'''         if (term->num_targets == 1) {
            nir_goto(&c->b, nir_block_of(c, term->targets[0]));
         } else {
            nir_def *cond = air_get_ssa(c, term->operands[0]);
            if (!cond)
               return false;
            if (term->targets[0] == term->targets[1])
               nir_goto(&c->b, nir_block_of(c, term->targets[0]));
            else
               nir_goto_if(&c->b, nir_block_of(c, term->targets[0]), cond,
                           nir_block_of(c, term->targets[1]));
         }''','''         if (term->num_targets == 1) {
            edge_take(&edges, bi, term->targets[0]);
            nir_goto(&c->b, nir_block_of(c, term->targets[0]));
         } else {
            nir_def *cond = air_get_ssa(c, term->operands[0]);
            uint64_t known = 0;
            if (!cond)
               return false;
            if (term->targets[0] == term->targets[1] || fold_scalar(c, cond, &known)) {
               const struct air_block *to = term->targets[0] == term->targets[1] || known
                                               ? term->targets[0] : term->targets[1];
               edge_take(&edges, bi, to);
               nir_goto(&c->b, nir_block_of(c, to));
            } else {
               edge_take(&edges, bi, term->targets[0]);
               edge_take(&edges, bi, term->targets[1]);
               nir_goto_if(&c->b, nir_block_of(c, term->targets[0]), cond,
                           nir_block_of(c, term->targets[1]));
            }
         }''')
    s=replace(s,'         nir_def *cond = air_get_ssa(c, term->operands[0]);\n         if (!cond)\n            return false;\n         nir_block *deflt = nir_block_of(c, term->targets[0]);','''         nir_def *cond = air_get_ssa(c, term->operands[0]);
         if (!cond)
            return false;
         uint64_t known;
         if (fold_scalar(c, cond, &known)) {
            const struct air_block *to = switch_known_target(term, known, cond->bit_size);
            if (to) {
               edge_take(&edges, bi, to);
               nir_goto(&c->b, nir_block_of(c, to));
               break;
            }
         }
         for (unsigned k = 0; k < term->num_targets; k++)
            edge_take(&edges, bi, term->targets[k]);
         nir_block *deflt = nir_block_of(c, term->targets[0]);''')
    s=replace(s,'      default:\n         return air_error(c, "instruction: block %u ends with opcode %u", bi, term->op);\n      }\n   }\n',
                '      default:\n         return air_error(c, "instruction: block %u ends with opcode %u", bi, term->op);\n      }\n      edges_decided(&edges, bi);\n   }\n   c->edges = outer_edges;\n   if (!edges_check(c, &edges))\n      return false;\n')
    s=replace(s,'            if (!ce)\n               continue; /* an edge from a block the entry cannot reach */\n',
                '            if (!ce)\n               continue; /* an edge from a block the entry cannot reach */\n            if (!edge_possible(&edges, in->targets[k]->index, bi))\n               continue; /* an edge the function constants rule out */\n')
    s=replace(s,'      if (!nir_src_is_const(nir_src_for_ssa(i)))\n         return air_error(c, "instruction: insertelement at a run-time index");','''      if (!nir_src_is_const(nir_src_for_ssa(i))) {
         /* each component compared against the index and selected: exact */
         air_set(c, &in->base,
                 air_val_ssa(c, nir_vector_insert(&c->b, v, x, nir_u2u32(&c->b, i))));
         return true;
      }''')
    s=replace(s,'static void\nrun_static_initializers(struct air_ctx *c)\n{\n',
                'static void\nrun_static_initializers(struct air_ctx *c)\n{\n   store_initializers(c);\n')
    s=replace(s,'   nir_cursor saved_consts = c->const_cursor;\n',
                '   nir_cursor saved_consts = c->const_cursor;\n   struct air_edges *saved_edges = c->edges;\n')
    s=replace(s,'   c->const_cursor = saved_consts;\n','   c->const_cursor = saved_consts;\n   c->edges = saved_edges;\n')
    s=replace(s,'   if (!c.error[0])\n      run_static_initializers(&c);\n',
                '   if (!c.error[0]) {\n      c.in_static_init = true;\n      run_static_initializers(&c);\n      c.in_static_init = false;\n   }\n')
    s=replace(s,'   nir_index_ssa_defs(nir_shader_get_entrypoint(c.nir));\n',
                '   drop_unread_scratch(c.nir);\n   nir_index_ssa_defs(nir_shader_get_entrypoint(c.nir));\n')
    s=replace(s,'      if (ptr && argv[i]->kind == AIR_VAL_PTR && argv[i]->ptr.offset) {',
                '      uint64_t const_offset;\n      if (ptr && argv[i]->kind == AIR_VAL_PTR && argv[i]->ptr.offset &&\n          !fold_scalar(c, argv[i]->ptr.offset, &const_offset)) {')
    s=replace(s,'         air_set(c, &f->args[i], argv[i]);   /* the caller\'s own resolution */','''         struct air_val *a = argv[i];
         if (a->kind == AIR_VAL_PTR && a->ptr.offset) {
            /* a constant byte offset is an immediate: rebuilt in this impl */
            struct air_ptr rp = a->ptr;
            uint64_t bytes = 0;
            fold_scalar(c, a->ptr.offset, &bytes);   /* checked by the caller above */
            rp.offset = nir_imm_int(&c->b, bytes);
            a = air_val_ptr(c, rp);
         }
         air_set(c, &f->args[i], a);   /* the caller's own resolution */''')
    p.write_text(s)
    p=air/'air_intrinsics.c';s=p.read_text()
    s=replace(s,'   struct air_tex_name t;\n   if (!parse_tex_name(n->part[0], &t))\n','''   /* metal::texture2d() and its kin: a NULL handle, which is no binding. Every
    * member but is_null_texture is undefined on one, so a use that reaches
    * tex_handle is refused there; the QuartzCore uber shaders only phi it with
    * their texture arguments, on edges their function constants decide.
    */
   if (!strncmp(n->part[0], "get_null_", 9) && in->base.type->kind == AIR_TYPE_POINTER) {
      struct air_ptr p = { .class = AIR_PTR_NONE };
      air_set(c, &in->base, air_val_ptr(c, p));
      return true;
   }
   struct air_tex_name t;
   if (!parse_tex_name(n->part[0], &t))
''')
    p.write_text(s)
    for name in ('sweet_native_ext.h','sweet_native_ext.c'):shutil.copy2(NATIVE/'backend'/name,air/name)
    p=air/'air_metal.h';s=p.read_text()
    s=replace(s,'\tunsigned fragment_buffer_table_count;\n};', '''\tunsigned fragment_buffer_table_count;
    /* Native ABI extension, shared by frontend and backend in this build. */
    float native_viewport[4];  /* x,y,width,height */
    unsigned native_scissor[4]; /* x,y,width,height */
    float native_blend[4];
    unsigned native_flags; /* bit0 viewport, bit1 scissor, bit2 blend */
    unsigned native_cull, native_winding, native_fill;
};''')
    p.write_text(s)
    p=air/'air_metal.c';s=p.read_text()
    s=replace(s,'#include "air_metal.h"','#include "sweet_native_ext.h"\n#include "util/u_framebuffer.h"\n#include <math.h>')
    s=replace(s,'   bool software;','''   bool software;
   bool in_command, lost;
   struct pipe_framebuffer_state batch_fb;
   struct rwa_constant constants[3][256];
   unsigned constant_count[3];''')
    s=replace(s,'/* PRINTFLIKE,', '''static unsigned native_constants(struct rwa_device *, unsigned, void *, const void *,
                                  size_t, struct air_constant_value **);

/* PRINTFLIKE,''')
    # GPU-only build. Keeping a CPU fallback here would disguise failed bring-up.
    s=replace(s,'   bool software = debug_get_bool_option("RWA_SOFTWARE", false);','   bool software = false;')
    s=replace(s,'   if (!device->screen)\n      device->screen = sw_screen_create(null_sw_create());','')
    s=s.replace('#include "target-helpers/inline_sw_helper.h"','').replace('#include "null/null_sw_winsys.h"','')
    s=replace(s,'      air_module_function_constants(compute->mem_ctx, bitcode, size, &constants);',
                '      native_constants(device, 2, compute->mem_ctx, bitcode, size, &constants);')
    s=replace(s,'      air_module_function_constants(mem_ctx, bitcode, size, &constants);',
                '      native_constants(device, 2, mem_ctx, bitcode, size, &constants);',count=2)
    s=replace(s,'      air_module_function_constants(render->mem_ctx, fragment_bitcode, fragment_size, &ffc);',
                '      native_constants(device, 1, render->mem_ctx, fragment_bitcode, fragment_size, &ffc);')
    s=replace(s,'         air_module_function_constants(render->mem_ctx, vertex_bitcode, vertex_size, &vfc);',
                '         native_constants(device, 0, render->mem_ctx, vertex_bitcode, vertex_size, &vfc);')
    s=replace(s,'   pipe->launch_grid(pipe, &grid);','   pipe->launch_grid(pipe, &grid);\n   pipe->memory_barrier(pipe, PIPE_BARRIER_ALL);')
    s=replace(s,'   int rc = finish_or_say_why(device, true, "dispatch", why, why_size);',
                '   int rc = device->in_command ? 0 : finish_or_say_why(device, true, "dispatch", why, why_size);')
    s=replace(s,'   rc = finish_or_say_why(device, true, "draw", why, why_size);',
                '   rc = device->in_command ? 0 : finish_or_say_why(device, true, "draw", why, why_size);')
    s=replace(s,'   pipe->set_framebuffer_state(pipe, &fb);', '''   bool same = device->in_command && device->batch_fb.width == fb.width &&
      device->batch_fb.height == fb.height && device->batch_fb.samples == fb.samples &&
      device->batch_fb.nr_cbufs == fb.nr_cbufs;
   for (unsigned i=0; same && i<fb.nr_cbufs; i++)
      same = device->batch_fb.cbufs[i] &&
             device->batch_fb.cbufs[i]->texture == fb.cbufs[i]->texture;
   if (same) same = (!fb.zsbuf && !device->batch_fb.zsbuf) ||
      (fb.zsbuf && device->batch_fb.zsbuf && fb.zsbuf->texture == device->batch_fb.zsbuf->texture);
   if (!same) {
      pipe->set_framebuffer_state(pipe, &fb);
      if(device->in_command)util_copy_framebuffer_state(&device->batch_fb,&fb);
   }''')
    s=replace(s,'   pipe->set_framebuffer_state(pipe, &(struct pipe_framebuffer_state){ 0 });',
                '   if (!device->in_command) pipe->set_framebuffer_state(pipe, &(struct pipe_framebuffer_state){ 0 });')
    # Context and batch_fb own references when a draw is deferred.
    s=s.replace('pipe->surface_destroy(pipe, surf[i]);','pipe_surface_reference(&surf[i], NULL);')
    s=s.replace('pipe->surface_destroy(pipe, surf[j]);','pipe_surface_reference(&surf[j], NULL);')
    s=replace(s,'      pipe->surface_destroy(pipe, zsurf);','      pipe_surface_reference(&zsurf, NULL);')
    s=replace(s,'   if (!done) {\n      say(why, why_size,', '   if (!done) {\n      device->lost = true;\n      say(why, why_size,')
    # Do not destroy a timed-out context and its in-flight allocations.
    marker='rwa_device_close(struct rwa_device *device)\n{'
    s=replace(s,marker,marker+'\n   if(device && device->lost)return;\n   if(device)for(unsigned k=0;k<3;k++)for(unsigned i=0;i<device->constant_count[k];i++)free((void*)device->constants[k][i].name);')
    s=replace(s,'.depth_clip_near = 1, .depth_clip_far = 1, .clip_halfz = zs_format != 0,',
                '.depth_clip_near = 1, .depth_clip_far = 1, .clip_halfz = 1, .scissor = 1,')
    s=replace(s,'   /* The attachments of the pass must be the attachments the pipeline was compiled for:','''   if(device->lost) {say(why,why_size,"device lost");return -1;}
   if(draw->native_flags&1)for(unsigned i=0;i<4;i++) {
      if(!isfinite(draw->native_viewport[i]) || (i>=2 && draw->native_viewport[i]<0)) {
         say(why,why_size,"invalid viewport");return -1;
      }
   }
   /* The attachments of the pass must be the attachments the pipeline was compiled for:''')
    s=replace(s,'   pipe->set_viewport_states(pipe, 0, 1, &(struct pipe_viewport_state){', '''   float vx=0, vy=0, vw=w, vh=h;
   if(draw->native_flags&1) {
      vx=draw->native_viewport[0];vy=draw->native_viewport[1];
      vw=draw->native_viewport[2];vh=draw->native_viewport[3];
   }
   pipe->set_viewport_states(pipe, 0, 1, &(struct pipe_viewport_state){''')
    s=replace(s,'      .scale = { w / 2.0f, -(h / 2.0f), zfar - znear },\n      .translate = { w / 2.0f, h / 2.0f, znear } });', '''      .scale = { vw / 2.0f, -(vh / 2.0f), zfar - znear },
      .translate = { vx + vw / 2.0f, vy + vh / 2.0f, znear } });
   struct pipe_scissor_state scissor={.minx=0,.miny=0,.maxx=w,.maxy=h};
   if(draw->native_flags&2) {
      scissor.minx=MIN2(draw->native_scissor[0],w);scissor.miny=MIN2(draw->native_scissor[1],h);
      scissor.maxx=scissor.minx+MIN2(draw->native_scissor[2],w-scissor.minx);
      scissor.maxy=scissor.miny+MIN2(draw->native_scissor[3],h-scissor.miny);
   }
   pipe->set_scissor_states(pipe,0,1,&scissor);
   pipe->set_blend_color(pipe,&(struct pipe_blend_color){.color={draw->native_blend[0],draw->native_blend[1],draw->native_blend[2],draw->native_blend[3]}});''')
    # Cull/fill are explicitly checked by frontend; don't silently ignore them.
    s+='\n#include "sweet_native_ext.c"\n'
    p.write_text(s)

def adapt_frontend(donor):
    src=donor/'userland/src'
    p=src/'redwoodmetal.m';s=p.read_text()
    begin=s.index('struct rwa_device;')
    end=s.index('_Static_assert(',begin)
    s=s[:begin]+'#include "sweet_native_ext.h"\n#include <pthread.h>\n#include <math.h>\n\n'+s[end:]
    # Resolve the same-build C ABI directly at load time. No weak or stale backend.
    s=replace(s,'\tmesa.handle = dlopen("/redwood/lib/libredwoodair.dylib", RTLD_NOW);',
                '\tmesa.handle = rwa_native_abi()==SWEET_NATIVE_METAL_ABI ? (void *)1 : NULL;')
    s,n=re.subn(r'\*\(void \*\*\)&mesa\.(\w+) = dlsym\(mesa.handle,\s*"(rwa_\w+)"\);',r'mesa.\1 = \2;',s)
    if n!=29:raise RuntimeError(f'incomplete symbol adaptation: {n}')
    s=s.replace('dlclose(mesa.handle);','/* backend is a required @loader_path dependency */')
    # Remove foreign kernel selectors: resource allocation must use this backend.
    begin=s.index('static bool\ngpu_param(');end=s.index('/* The kernel\'s gpu interface',begin)
    s=s[:begin]+'''static bool gpu_param(io_connect_t gpu, uint32_t which, uint64_t *value)
{
    (void)gpu;(void)which;(void)value;return false;
}

'''+s[end:]
    begin=s.index('\t/* The gpu this driver would actually drive.')
    end=s.index('\t/* The struct Metal reads',begin)
    s=s[:begin]+'''    _gpu=IO_OBJECT_NULL;
    char nativeWhy[512]={0};
    if(mesa_available())_mesa=mesa.device_open(nativeWhy,sizeof nativeWhy);
    if(!_mesa) {
        rw_print("native A618 backend unavailable: %s\\n",nativeWhy);
        [self release];return nil;
    }
    rw_print("native renderer %s; AIR -> NIR -> freedreno, no Vulkan\\n",mesa.device_name(_mesa));

'''+s[end:]
    begin=s.index('\tif (gpu == IO_OBJECT_NULL || length == 0)',s.index('@implementation RedwoodMetalBuffer'))
    end=s.index('\n- (struct rwa_buffer *)mesaBuffer',begin)
    s=s[:begin]+'''    rw_print("native buffer allocation requires the AIR/freedreno backend\\n");
    [self release];return nil;
}
'''+s[end:]
    begin=s.index('\tif (_contents != NULL) {',s.index('@implementation RedwoodMetalBuffer'))
    end=s.index('\n\t[super dealloc];',begin)
    s=s[:begin]+s[end:]
    s=replace(s,'@property (readonly) struct rwa_buffer *mesaBuffer;', '@property (readonly) struct rwa_buffer *mesaBuffer;\n- (void *)contents;\n- (NSUInteger)length;')
    s=replace(s,'@interface RedwoodMetalCommandQueue : _MTLCommandQueue', '@interface RedwoodMetalCommandQueue : _MTLCommandQueue\n- (NSUInteger)maxCommandBufferCount;')
    s=replace(s,'\t    [[[MTLCommandQueueDescriptor alloc] init] autorelease];','''        [[[(NSClassFromString(@"MTLCommandQueueDescriptorInternal") ?: NSClassFromString(@"MTLCommandQueueDescriptor")) alloc] init] autorelease];
    if(!use){rw_print("Metal command queue descriptor unavailable\\n");return nil;}''')
    s=replace(s,'- (id)newCommandQueueWithDescriptor:(id)descriptor\n{','''- (id)newCommandQueue {
    return [self newCommandQueueWithDescriptor:nil];
}
- (id)newCommandQueueWithDescriptor:(id)descriptor
{''')
    # Increment and read were previously unsequenced in a logging macro.
    s=replace(s,'if ((++rw_times_ & (rw_times_ - 1)) == 0)', 'if ((rw_times_++, (rw_times_ & (rw_times_ - 1))) == 0)')
    # Real library and function objects retain exact specialization bytes.
    marker='@implementation RedwoodMetalDevice\n'
    s=replace(s,marker,'#include "SweetNativeLibrary.inc"\n\n'+marker)
    at=s.index('- (struct rwa_device *)mesaDevice',s.index(marker))
    s=s[:at]+'''- (id)newLibraryWithFile:(NSString *)path error:(NSError **)error {
    NSData *data=[NSData dataWithContentsOfFile:path options:0 error:error];
    return data?[[SweetNativeLibrary alloc] initWithDevice:self data:data error:error]:nil;
}
- (id)newLibraryWithURL:(NSURL *)url error:(NSError **)error {
    return [self newLibraryWithFile:[url path] error:error];
}
- (id)newLibraryWithData:(dispatch_data_t)data error:(NSError **)error {
    if(!data){if(error)*error=native_error("empty metallib data");return nil;}
    const void *bytes=NULL;size_t length=0;
    dispatch_data_t mapped=dispatch_data_create_map(data,&bytes,&length);
    NSData *copy=[NSData dataWithBytes:bytes length:length];
    id library=[[SweetNativeLibrary alloc] initWithDevice:self data:copy error:error];
    dispatch_release(mapped);return library;
}
- (id)newDefaultLibrary {
    NSString *path=[[NSBundle mainBundle] pathForResource:@"default" ofType:@"metallib"];
    return path?[self newLibraryWithFile:path error:NULL]:nil;
}
- (id)newDefaultLibraryWithBundle:(NSBundle *)bundle error:(NSError **)error {
    NSString *path=[bundle pathForResource:@"default" ofType:@"metallib"];
    if(!path){if(error)*error=native_error("bundle has no default.metallib");return nil;}
    return [self newLibraryWithFile:path error:error];
}
- (id)newLibraryWithSource:(NSString *)source options:(id)options error:(NSError **)error {
    (void)source;(void)options;if(error)*error=native_error("Metal source compilation is unavailable; supply AIR/metallib");return nil;
}
'''+s[at:]
    # The exact call sites can be found by the C-ABI calls, with the descriptor's
    # function objects still in scope. Never replace specialized values with zeros.
    at=s.index('\tstruct rwa_render *render = mesa.render_create(')
    s=s[:at]+'''    char nativeWhy[512]={0};
    if(!native_specialize(_mesa,vertex,0,nativeWhy,sizeof nativeWhy) ||
       !native_specialize(_mesa,fragment,1,nativeWhy,sizeof nativeWhy))
        return [self fail:error with:[NSString stringWithUTF8String:nativeWhy]];
'''+s[at:]
    # Inspect compute context for its actual function variable name.
    pattern=r'(\tstruct rwa_compute \*compute = mesa\.compute_create\()'
    match=re.search(pattern,s)
    if not match:raise RuntimeError('missing compute create')
    at=match.start()
    s=s[:at]+'''    char nativeWhy[512]={0};
    if(!native_specialize(_mesa,function,2,nativeWhy,sizeof nativeWhy))
        return [self fail:error with:[NSString stringWithUTF8String:nativeWhy]];
'''+s[at:]
    s=replace(s,'\tif (mesa.module_argbufs(_mesa, [air bytes], [air length],','''    if(!native_specialize(_mesa,function,2,why,sizeof why)){rw_print("argument encoder: %s\\n",why);return nil;}
	if (mesa.module_argbufs(_mesa, [air bytes], [air length],''')
    # Hold every referenced object through the actual command-buffer fence.
    s=replace(s,'@implementation RedwoodMetalWork\n@end','''@implementation RedwoodMetalWork
- (void)dealloc {
    [pipeline release];[render release];[depth release];[depthStencilState release];[indexBuffer release];
    for(unsigned i=0;i<8;i++)[color[i] release];
    for(unsigned i=0;i<RW_MAX_BUFFERS;i++){[vertexBuffers[i] release];[buffers[i] release];[textures[i] release];[samplers[i] release];}
    [super dealloc];
}
@end''')
    s=replace(s,'\t[_work addObject:work];','''    [work->pipeline retain];[work->render retain];[work->depth retain];[work->depthStencilState retain];[work->indexBuffer retain];
    for(unsigned i=0;i<8;i++)[work->color[i] retain];
    for(unsigned i=0;i<RW_MAX_BUFFERS;i++){[work->vertexBuffers[i] retain];[work->buffers[i] retain];[work->textures[i] retain];[work->samplers[i] retain];}
\t[_work addObject:work];''')
    # Every commit is one context transaction. This initial implementation waits
    # once at commit, including on partial failure, before completion callbacks.
    s=replace(s,'\t_status = RW_CB_COMMITTED;\n\tfor (RedwoodMetalWork *work in _work) {', '''    if(_status>=RW_CB_COMMITTED){[self failWith:"command buffer committed twice"];return;}
    _status = RW_CB_COMMITTED;
    if(rwa_command_begin([[_queue device] mesaDevice],why,sizeof why)) {
        [self failWith:why];[self redwoodReturnSlot];[self redwoodDeliverHandlers];return;
    }
\tfor (RedwoodMetalWork *work in _work) {''')
    s=replace(s,'done:\n\t/* The queue\'s slot goes back here', '''done:
    {
        char completionWhy[512]={0};
        if(rwa_command_end([[_queue device] mesaDevice],completionWhy,sizeof completionWhy))
            [self failWith:completionWhy];
        else if(_status!=RW_CB_ERROR)_status=RW_CB_COMPLETED;
    }
\t/* The queue's slot goes back here''')
    s=replace(s,'\tif (_status == RW_CB_ERROR) {\n\t\treturn;\n\t}',
                '\tif (_status == RW_CB_ERROR || _status == RW_CB_COMPLETED) {\n\t\treturn;\n\t}')
    s=replace(s,'\tif (_status >= RW_CB_SCHEDULED) {\n\t\t[self redwoodDeliver:handler];\n\t\treturn;\n\t}\n\tif (_completed == nil)',
                '\tif (_status >= RW_CB_COMPLETED) {\n\t\t[self redwoodDeliver:handler];\n\t\treturn;\n\t}\n\tif (_completed == nil)')
    # Draw state is appended to the shared C ABI and recorded at encode time.
    s=replace(s,'\tBOOL hasDepthRange;','''\tBOOL hasDepthRange;
    float nativeViewport[4], nativeBlend[4];
    unsigned nativeScissor[4], nativeFlags;
    BOOL nativeClearOnly, nativeBarrier;
    unsigned nativeClearMask;
    float nativeClearColors[8][4];''')
    s=replace(s,'\tBOOL _hasDepthRange;','''\tBOOL _hasDepthRange;
    float _nativeViewport[4], _nativeBlend[4];
    unsigned _nativeScissor[4], _nativeFlags;
    unsigned _nativeClearMask;
    float _nativeClearColors[8][4];''')
    begin=s.index('- (void)setViewport:(MTLViewport)viewport\n{')
    end=s.index('/* The depth/stencil state,',begin)
    s=s[:begin]+'''- (void)setViewport:(MTLViewport)v {
    if(!isfinite(v.originX)||!isfinite(v.originY)||!isfinite(v.width)||!isfinite(v.height)||v.width<0||v.height<0||
       !isfinite(v.znear)||!isfinite(v.zfar)) {[_commandBuffer failWith:"invalid viewport"];return;}
    _nativeViewport[0]=v.originX;_nativeViewport[1]=v.originY;
    _nativeViewport[2]=v.width;_nativeViewport[3]=v.height;_nativeFlags|=1;
    _depthRange[0]=v.znear;_depthRange[1]=v.zfar;_hasDepthRange=YES;
}
- (void)setScissorRect:(MTLScissorRect)r {
    if(r.x>UINT32_MAX||r.y>UINT32_MAX||r.width>UINT32_MAX||r.height>UINT32_MAX){[_commandBuffer failWith:"scissor exceeds 32-bit range"];return;}
    _nativeScissor[0]=r.x;_nativeScissor[1]=r.y;_nativeScissor[2]=r.width;_nativeScissor[3]=r.height;_nativeFlags|=2;
}
- (void)setCullMode:(NSUInteger)mode {if(mode)[_commandBuffer failWith:"interface: culling is not implemented"];}
- (void)setFrontFacingWinding:(NSUInteger)mode {if(mode)[_commandBuffer failWith:"interface: non-default winding is not implemented"];}
- (void)setTriangleFillMode:(NSUInteger)mode {if(mode)[_commandBuffer failWith:"interface: wireframe is not implemented"];}

'''+s[end:]
    s=replace(s,'\t(void)r; (void)g; (void)b; (void)a;',
                '    _nativeBlend[0]=r;_nativeBlend[1]=g;_nativeBlend[2]=b;_nativeBlend[3]=a;_nativeFlags|=4;')
    s=replace(s,'\twork->hasDepthRange = _hasDepthRange;','''\twork->hasDepthRange = _hasDepthRange;
    memcpy(work->nativeViewport,_nativeViewport,sizeof _nativeViewport);
    memcpy(work->nativeScissor,_nativeScissor,sizeof _nativeScissor);
    memcpy(work->nativeBlend,_nativeBlend,sizeof _nativeBlend);
    work->nativeFlags=_nativeFlags;
    work->nativeClearMask=_nativeClearMask;
    memcpy(work->nativeClearColors,_nativeClearColors,sizeof _nativeClearColors);
    _nativeClearMask=0;_clears=NO;_clearsDepth=NO;_clearsStencil=NO;''')
    s=replace(s,'\t\t.fragment_buffer_table_count = work->count,','''\t\t.fragment_buffer_table_count = work->count,
        .native_flags=work->nativeFlags,
        .native_viewport={work->nativeViewport[0],work->nativeViewport[1],work->nativeViewport[2],work->nativeViewport[3]},
        .native_scissor={work->nativeScissor[0],work->nativeScissor[1],work->nativeScissor[2],work->nativeScissor[3]},
        .native_blend={work->nativeBlend[0],work->nativeBlend[1],work->nativeBlend[2],work->nativeBlend[3]},''')
    # Independent attachment clear values and exactly one clear per pass.
    s=replace(s,'\t\tif (i == 0 && [attachment loadAction] == MTLLoadActionClear) {',
                '\t\tif ([attachment loadAction] == MTLLoadActionClear) {')
    s=replace(s,'\t\t\t_clears = YES;', '''            _nativeClearMask|=1u<<i;
            _nativeClearColors[i][0]=c.red;_nativeClearColors[i][1]=c.green;
            _nativeClearColors[i][2]=c.blue;_nativeClearColors[i][3]=c.alpha;
\t\t\t_clears = YES;''')
    # Clear via Gallium before the first draw. EndEncoding records clear-only passes.
    s=replace(s,'\t\tif (work->render != nil) {','''        if(work->nativeBarrier) {
            if(rwa_barrier([[_queue device] mesaDevice],why,sizeof why)){[self failWith:why];goto done;}
            continue;
        }
        if(work->nativeClearMask || work->clearsDepth || work->clearsStencil) {
            struct rwa_texture *targets[8]={0};
            for(unsigned i=0;i<work->colorCount;i++)targets[i]=[work->color[i] mesaTexture];
            if(rwa_clear([[_queue device] mesaDevice],targets,work->colorCount,work->nativeClearMask,
                         work->nativeClearColors,[work->depth mesaTexture],
                         work->clearsDepth?&work->clearDepth:NULL,work->clearsStencil?&work->clearStencil:NULL,
                         why,sizeof why)){[self failWith:why];goto done;}
        }
        if(work->nativeClearOnly)continue;
\t\tif (work->render != nil) {''')
    s=replace(s,'\t\t.clear_color = work->clears ? work->clearColor : NULL,','\t\t.clear_color = NULL,')
    s=replace(s,'\t\t.clear_depth = work->clearsDepth ? &work->clearDepth : NULL,','\t\t.clear_depth = NULL,')
    s=replace(s,'\t\t.clear_stencil = work->clearsStencil ? &work->clearStencil : NULL,','\t\t.clear_stencil = NULL,')
    marker='- (void)endEncoding\n{\n\t[super endEncoding];\n}'
    # first is render encoder, second compute: only edit first.
    at=s.index(marker)
    s=s[:at]+s[at:].replace(marker,'''- (void)endEncoding
{
    if(_nativeClearMask || _clearsDepth || _clearsStencil) {
        RedwoodMetalWork *work=[RedwoodMetalWork new];work->nativeClearOnly=YES;
        work->colorCount=_colorCount;for(unsigned i=0;i<8;i++)work->color[i]=_color[i];
        work->nativeClearMask=_nativeClearMask;memcpy(work->nativeClearColors,_nativeClearColors,sizeof _nativeClearColors);
        work->depth=_depth;work->clearDepth=_clearDepth;work->clearStencil=_clearStencil;
        work->clearsDepth=_clearsDepth;work->clearsStencil=_clearsStencil;
        [_commandBuffer recordWork:work];[work release];
        _nativeClearMask=0;_clearsDepth=NO;_clearsStencil=NO;
    }
    [super endEncoding];
}''',1)
    at=s.index('- (void)recordWork:(RedwoodMetalWork *)work\n{')
    s=s[:at]+'''- (void)nativeRecordBarrier {
    RedwoodMetalWork *work=[RedwoodMetalWork new];work->nativeBarrier=YES;
    [self recordWork:work];[work release];
}
'''+s[at:]
    # Barriers became necessary once draws stopped waiting individually. Keep
    # their ordering in the recorded command stream, not at encoding time.
    edits=[]
    for m in re.finditer(r'(?m)^- \(void\)memoryBarrier',s):
        brace=s.index('{',m.start());end=s.index('}',brace)
        edits.append((brace+1,end,' [_commandBuffer nativeRecordBarrier]; '))
    for begin,end,body in reversed(edits):s=s[:begin]+body+s[end:]
    for operation in ('encodeSignalEvent','encodeWaitForEvent'):
        old='- (void)'+operation+':(id)event value:(uint64_t)value { (void)event; (void)value; }'
        new='- (void)'+operation+':(id)event value:(uint64_t)value { (void)value;if(event)[self failWith:"interface: shared events are not implemented"]; }'
        s=replace(s,old,new)
    # Errors encountered during encoding must survive commit.
    s=replace(s,'    if(_status>=RW_CB_COMMITTED){[self failWith:"command buffer committed twice"];return;}',
                '    if(_status==RW_CB_ERROR){[self redwoodReturnSlot];[self redwoodDeliverHandlers];return;}\n    if(_status>=RW_CB_COMMITTED){[self failWith:"command buffer committed twice"];return;}')
    s=replace(s,'- (id)iosurface                    { return nil; }',
                '- (id)iosurface                    { return (id)_nativeSurface; }')
    s=replace(s,'\tstruct rwa_texture_shape _shape;','\tstruct rwa_texture_shape _shape;\n    CFTypeRef _nativeSurface;')
    # keep exact IOSurface object for Metal's iosurface accessor as well as winsys ownership
    # Locate the creation/wrapping code in the donor and add via object_setIvar is
    # unnecessary: private ivars are available to a category setter below.
    begin=s.index('- (NSString *)name\n{',s.index('@implementation RedwoodMetalDevice'))
    end=s.index('\n/*',begin)
    s=s[:begin]+'''- (NSString *)name
{
    return @"Adreno 618 (Sweet Native Metal)";
}
'''+s[end:]
    s=replace(s,'@property (readonly) struct rwa_texture *mesaTexture;',
                '@property (readonly) struct rwa_texture *mesaTexture;\n- (void)nativeAttachSurface:(void *)surface;')
    begin=s.index('\treturn [[RedwoodMetalTexture alloc] initWithRedwoodDevice:self',s.index('- (id)newTextureWithDescriptor:(MTLTextureDescriptor *)d iosurface:'))
    end=s.index('\n}',begin)
    s=s[:begin]+'''    RedwoodMetalTexture *result=[[RedwoodMetalTexture alloc] initWithRedwoodDevice:self texture:texture shape:&shape];
    [result nativeAttachSurface:s];return result;'''+s[end:]
    at=s.index('+ (void)load',s.index('@implementation RedwoodMetalTexture'))
    s=s[:at]+'''- (void)nativeAttachSurface:(void *)surface {
    if(_nativeSurface)CFRelease(_nativeSurface);
    _nativeSurface=surface?CFRetain(surface):NULL;
}
'''+s[at:]
    begin=s.index('- (void)dealloc',s.index('@implementation RedwoodMetalTexture'))
    at=s.index('\n\t[super dealloc];',begin)
    s=s[:at]+'\n    if(_nativeSurface)CFRelease(_nativeSurface);'+s[at:]
    # Resources retain their owning device even when the application releases it.
    for cls in ('RedwoodMetalBuffer','RedwoodMetalTexture','RedwoodMetalComputePipeline','RedwoodMetalRenderPipeline','RedwoodMetalArgumentEncoder','RedwoodMetalCommandQueue'):
        begin=s.index('@implementation '+cls)
        end=s.index('\n@end',begin)
        part=s[begin:end]
        part=part.replace('_owner = device;','_owner = [device retain];')
        if '_owner = [device retain];' in part:
            part=part.replace('[super dealloc];','[_owner release];[super dealloc];')
        s=s[:begin]+part+s[end:]
    # Never globally replace methods on Apple's function class. Native functions
    # implement their own argument-encoder entrypoints.
    at=s.index('static void\nensure_function_encoder(void)\n{')
    s=s[:at]+'''static void
ensure_function_encoder(void)
{
    /* SweetNativeFunction owns this API; no process-wide Objective-C swizzle. */
}
'''
    s=s.replace('RedwoodMetal','SweetNativeMetal')
    s=s.replace('redwoodmetal[','SweetNativeMetal[')
    p.write_text(s)
    shutil.copy2(NATIVE/'frontend/SweetNativeLibrary.inc',src/'SweetNativeLibrary.inc')
    shutil.copy2(NATIVE/'frontend/SweetNativeDrawable.m',src/'redwooddrawable.m')
    # The donor includes structs by hand in two files; only the drawable shim's
    # geometry record remains local. The AIR header itself is shared above.
    return p

def serialize_methods(path):
    s=path.read_text()
    support='''
// Gallium has one context per device. Serialize all object entrypoints, not just
// commits, because pipeline creation and CPU texture transfers also use it.
static pthread_mutex_t nativeObjectLock;
static pthread_once_t nativeLockOnce=PTHREAD_ONCE_INIT;
static void nativeLockInit(void){pthread_mutexattr_t a;pthread_mutexattr_init(&a);pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);pthread_mutex_init(&nativeObjectLock,&a);pthread_mutexattr_destroy(&a);}
static int nativeLock(void){pthread_once(&nativeLockOnce,nativeLockInit);pthread_mutex_lock(&nativeObjectLock);return 0;}
static void nativeUnlock(int *unused){(void)unused;pthread_mutex_unlock(&nativeObjectLock);}
#define NATIVE_OBJECT_LOCK __attribute__((cleanup(nativeUnlock))) int nativeGuard = nativeLock()
'''
    at=s.index('static struct {\n\tvoid *handle;')
    s=s[:at]+support+s[at:]
    # Only definitions, not @interface declarations. A method's next semicolon
    # appears before its opening brace for declarations.
    insert=[]
    for m in re.finditer(r'(?m)^[+-] \(',s):
        brace=s.find('{',m.start());semi=s.find(';',m.start())
        if brace>=0 and (semi<0 or brace<semi):
            signature=s[m.start():brace]
            # Apple's command-buffer constructor waits for a queue slot. Holding
            # this lock across that wait would prevent another thread's commit
            # from returning a slot. These factories do not access Gallium.
            if 'initWithRedwoodQueue:' in signature or re.match(r'- \(id\)commandBuffer(?:\s|WithUnretainedReferences)',signature):
                continue
            insert.append(brace+1)
    for at in reversed(insert):s=s[:at]+' NATIVE_OBJECT_LOCK; '+s[at:]
    path.write_text(s)
    p=path.parent/'SweetNativeLibrary.inc';s=p.read_text();insert=[]
    for m in re.finditer(r'(?m)^[+-] \(',s):
        brace=s.find('{',m.start());semi=s.find(';',m.start())
        if brace>=0 and (semi<0 or brace<semi):insert.append(brace+1)
    for at in reversed(insert):s=s[:at]+' NATIVE_OBJECT_LOCK; '+s[at:]
    p.write_text(s)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('donor',type=Path,help='disposable export of the pinned donor')
    args=parser.parse_args()
    adapt_backend(args.donor)
    frontend=adapt_frontend(args.donor)
    serialize_methods(frontend)
    print('Prepared native Sweet Metal sources:',args.donor)

if __name__=='__main__':main()
