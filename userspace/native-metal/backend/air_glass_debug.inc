/* SPDX-License-Identifier: MIT
 * Diagnostic only: /var/local/sweet-glass-dbg holds a mode number, read when a glass_background_no_bleed pipeline is
 * made (so it needs a backboardd restart). The fragment's colour is replaced to bisect why the draw leaves nothing:
 *   1 red                          2 the shape map's coordinates (r=x g=y, b=both inside 0..1)
 *   3 the shape map sample (r: distance < 0, g: alpha, b: distance == 0)
 *   4 coverage recomputed at 32 bits                     5 the shader's own colour, discards removed
 *   6 own colour classified (r: not finite, g: alpha, b: any rgb > 0)     7 red, discards kept
 * Modes 1-6 remove the discards. Without the file the shader is untouched.
 */
static void
native_glass_debug(nir_shader *fs, const char *entry)
{
   if (!entry || !strstr(entry, "glass_background_no_bleed"))
      return;
   FILE *f = fopen("/var/local/sweet-glass-dbg", "r");
   if (!f)
      return;
   unsigned mode = 0;
   if (fscanf(f, "%u", &mode) != 1)
      mode = 0;
   fclose(f);
   if (!mode)
      return;

   nir_function_impl *impl = nir_shader_get_entrypoint(fs);
   nir_tex_instr *map = NULL;
   unsigned stores = 0, demotes = 0;
   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type == nir_instr_type_tex) {
            nir_tex_instr *t = nir_instr_as_tex(instr);
            if (!map && t->texture_index == 4)
               map = t;
            continue;
         }
         if (instr->type != nir_instr_type_intrinsic)
            continue;
         nir_intrinsic_instr *in = nir_instr_as_intrinsic(instr);
         if (in->intrinsic == nir_intrinsic_demote || in->intrinsic == nir_intrinsic_demote_if ||
             in->intrinsic == nir_intrinsic_terminate || in->intrinsic == nir_intrinsic_terminate_if) {
            demotes++;
            if (mode != 7)
               nir_instr_remove(instr);
            continue;
         }
         if (in->intrinsic != nir_intrinsic_store_deref)
            continue;
         nir_variable *var = nir_intrinsic_get_var(in, 0);
         if (!var || var->data.mode != nir_var_shader_out ||
             (var->data.location != FRAG_RESULT_DATA0 && var->data.location != FRAG_RESULT_COLOR) ||
             in->num_components != 4)
            continue;
         stores++;
         if (mode == 5)
            continue;
         nir_builder b = nir_builder_at(nir_before_instr(instr));
         nir_def *old = in->src[1].ssa;
         const unsigned bits = old->bit_size;
         nir_def *one = nir_imm_float(&b, 1.0f), *zero = nir_imm_float(&b, 0.0f);
         nir_def *r = one, *g = zero, *bl = zero;
         int ci = map ? nir_tex_instr_src_index(map, nir_tex_src_coord) : -1;
         if (mode == 2 && ci >= 0) {
            nir_def *c = map->src[ci].src.ssa;
            nir_def *x = nir_f2fN(&b, nir_channel(&b, c, 0), 32), *y = nir_f2fN(&b, nir_channel(&b, c, 1), 32);
            nir_def *in01 = nir_iand(&b, nir_iand(&b, nir_fge(&b, x, zero), nir_fge(&b, one, x)),
                                     nir_iand(&b, nir_fge(&b, y, zero), nir_fge(&b, one, y)));
            r = nir_fsat(&b, x); g = nir_fsat(&b, y); bl = nir_bcsel(&b, in01, one, zero);
         } else if ((mode == 3 || mode == 4) && map) {
            nir_def *d = nir_f2fN(&b, nir_channel(&b, &map->def, 0), 32);
            nir_def *a = nir_f2fN(&b, nir_channel(&b, &map->def, 3), 32);
            if (mode == 3) {
               r = nir_bcsel(&b, nir_flt(&b, d, zero), one, zero);
               g = nir_fsat(&b, a);
               bl = nir_bcsel(&b, nir_feq(&b, d, zero), one, zero);
            } else {
               nir_builder at = nir_builder_at(nir_after_instr(&map->instr));
               nir_def *d1 = nir_f2fN(&at, nir_channel(&at, &map->def, 0), 32);
               nir_def *w = nir_fadd(&at, nir_fabs(&at, nir_ddx(&at, d1)), nir_fabs(&at, nir_ddy(&at, d1)));
               nir_def *cov = nir_fsat(&b, nir_fadd_imm(&b, nir_fdiv(&b, nir_fneg(&b, d),
                                                                    nir_fmax(&b, w, nir_imm_float(&b, 1e-4f))), 0.5));
               r = cov; g = nir_fsat(&b, a); bl = nir_fmul(&b, cov, g);
            }
         } else if (mode == 6) {
            nir_def *c = nir_f2fN(&b, old, 32);
            nir_def *bad = nir_imm_false(&b), *pos = nir_imm_false(&b);
            for (unsigned k = 0; k < 4; k++) {
               nir_def *v = nir_channel(&b, c, k);
               bad = nir_ior(&b, bad, nir_inot(&b, nir_flt(&b, nir_fabs(&b, v), nir_imm_float(&b, 1e30f))));
               if (k < 3)
                  pos = nir_ior(&b, pos, nir_flt(&b, zero, v));
            }
            r = nir_bcsel(&b, bad, one, zero);
            g = nir_fsat(&b, nir_channel(&b, c, 3));
            bl = nir_bcsel(&b, pos, one, zero);
         }
         nir_src_rewrite(&in->src[1], nir_f2fN(&b, nir_vec4(&b, r, g, bl, one), bits));
      }
   }
   if (mode != 7)
      fs->info.fs.uses_discard = false;
   nir_metadata_preserve(impl, nir_metadata_none);
   fprintf(stderr, "SweetNativeMetal: glass-dbg mode %u on %s: %u colour stores, %u discards, map %s tex %u samp %u bits %u\n",
           mode, entry, stores, demotes, map ? "found" : "MISSING", map ? map->texture_index : 0,
           map ? map->sampler_index : 0, map ? map->def.bit_size : 0);
}
