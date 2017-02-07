#include "evas_engine_filter.h"

#include <math.h>
#include <time.h>

// FIXME: Add proper stride support

static int
_box_blur_auto_radius(int *radii, int r)
{
   if (r <= 2)
     {
        radii[0] = r;
        radii[1] = 0;
        WRN("Radius is too small for auto box blur: %d", r);
        return 1;
     }
   else if (r <= 6)
     {
        radii[0] = r / 2;
        radii[1] = r - radii[0] - 1;
        radii[2] = 0;
        XDBG("Using auto radius for %d: %d %d", r, radii[0], radii[1]);
        return 2;
     }
   else
     {
        radii[0] = (r + 3) / 3;
        radii[1] = (r + 2) / 3;
        radii[2] = r - radii[0] - radii[1];
        radii[3] = 0;
        XDBG("Using auto radius for %d: %d %d %d", r, radii[0], radii[1], radii[2]);
        return 3;
     }
}

#include "./blur/blur_box_rgba_.c"
#ifdef BUILD_MMX
#include "./blur/blur_box_rgba_i386.c"
#endif
#ifdef BUILD_SSE3
#include "./blur/blur_box_rgba_sse3.c"
#endif
#ifdef BUILD_NEON
#include "./blur/blur_box_rgba_neon.c"
#endif

static void
_box_blur_horiz_rgba(uint32_t *src, uint32_t *dst, int* radii, int w, int h)
{
   DEBUG_TIME_BEGIN();

#ifdef BUILD_SSE3
   if (eina_cpu_features_get() & EINA_CPU_SSE3)
     {
        _box_blur_rgba_horiz_step_sse3(src, dst, radii, w, h);
        goto end;
     }
#endif
#ifdef BUILD_MMX
   if (eina_cpu_features_get() & EINA_CPU_MMX)
     {
        _box_blur_rgba_horiz_step_mmx(src, dst, radii, w, h);
        goto end;
     }
#endif
#ifdef BUILD_NEON
   if (eina_cpu_features_get() & EINA_CPU_NEON)
     {
        _box_blur_rgba_horiz_step_neon(src, dst, radii, w, h);
        goto end;
     }
#endif
   _box_blur_rgba_horiz_step(src, dst, radii, w, h);

end:
   DEBUG_TIME_END();
}

static void
_box_blur_vert_rgba(uint32_t *src, uint32_t *dst, int* radii, int w, int h)
{
   DEBUG_TIME_BEGIN();

#ifdef BUILD_SSE3
   if (eina_cpu_features_get() & EINA_CPU_SSE3)
     {
        _box_blur_rgba_vert_step_sse3(src, dst, radii, h, w);
        goto end;
     }
#endif
#ifdef BUILD_MMX
   if (eina_cpu_features_get() & EINA_CPU_MMX)
     {
        _box_blur_rgba_vert_step_mmx(src, dst, radii, h, w);
        goto end;
     }
#endif
#ifdef BUILD_NEON
   if (eina_cpu_features_get() & EINA_CPU_NEON)
     {
        _box_blur_rgba_vert_step_neon(src, dst, radii, h, w);
        goto end;
     }
#endif
   _box_blur_rgba_vert_step(src, dst, radii, h, w);

end:
   DEBUG_TIME_END();
}

#include "./blur/blur_box_alpha_.c"
#ifdef BUILD_MMX
#include "./blur/blur_box_alpha_i386.c"
#endif
#ifdef BUILD_SSE3
#include "./blur/blur_box_alpha_sse3.c"
#endif
#ifdef BUILD_NEON
#include "./blur/blur_box_alpha_neon.c"
#endif

static void
_box_blur_horiz_alpha(const DATA8 *src, DATA8 *dst, int* radii, int w, int h)
{
   DEBUG_TIME_BEGIN();

#ifdef BUILD_SSE3
   if (eina_cpu_features_get() & EINA_CPU_SSE3)
     {
        _box_blur_alpha_horiz_step_sse3(src, dst, radii, w, h);
        goto end;
     }
#endif
#ifdef BUILD_MMX
   if (eina_cpu_features_get() & EINA_CPU_MMX)
     {
        _box_blur_alpha_horiz_step_mmx(src, dst, radii, w, h);
        goto end;
     }
#endif
#ifdef BUILD_NEON
   if (eina_cpu_features_get() & EINA_CPU_NEON)
     {
        _box_blur_alpha_horiz_step_neon(src, dst, radii, w, h);
        goto end;
     }
#endif
   _box_blur_alpha_horiz_step(src, dst, radii, w, h);

end:
   DEBUG_TIME_END();
}

static void
_box_blur_vert_alpha(const DATA8 *src, DATA8 *dst, int* radii, int w, int h)
{
   DEBUG_TIME_BEGIN();

#ifdef BUILD_SSE3
   if (eina_cpu_features_get() & EINA_CPU_SSE3)
     {
        _box_blur_alpha_vert_step_sse3(src, dst, radii, h, w);
        goto end;
     }
#endif
#ifdef BUILD_MMX
   if (eina_cpu_features_get() & EINA_CPU_MMX)
     {
        _box_blur_alpha_vert_step_mmx(src, dst, radii, h, w);
        goto end;
     }
#endif
#ifdef BUILD_NEON
   if (eina_cpu_features_get() & EINA_CPU_NEON)
     {
        _box_blur_alpha_vert_step_neon(src, dst, radii, h, w);
        goto end;
     }
#endif
   _box_blur_alpha_vert_step(src, dst, radii, h, w);

end:
   DEBUG_TIME_END();
}

static Eina_Bool
_box_blur_apply(Evas_Filter_Command *cmd, Eina_Bool vert, Eina_Bool rgba)
{
   unsigned int src_len, src_stride, dst_len, dst_stride;
   Eina_Bool ret = EINA_TRUE;
   int radii[7] = {0};
   unsigned int r;
   void *src, *dst;

   r = abs(vert ? cmd->blur.dy : cmd->blur.dx);
   src = _buffer_map_all(cmd->input->buffer, &src_len, E_READ, rgba ? E_ARGB : E_ALPHA, &src_stride);
   dst = _buffer_map_all(cmd->output->buffer, &dst_len, E_WRITE, rgba ? E_ARGB : E_ALPHA, &dst_stride);

   if (cmd->blur.auto_count)
     _box_blur_auto_radius(radii, r);
   else for (int k = 0; k < cmd->blur.count; k++)
     radii[k] = r;

   if (src && dst)
     {
        if (rgba)
          {
             if (!vert)
               _box_blur_horiz_rgba(src, dst, radii, cmd->input->w, cmd->input->h);
             else
                _box_blur_vert_rgba(src, dst, radii, cmd->input->w, cmd->input->h);
          }
        else
          {
             if (!vert)
               _box_blur_horiz_alpha(src, dst, radii, cmd->input->w, cmd->input->h);
             else
                _box_blur_vert_alpha(src, dst, radii, cmd->input->w, cmd->input->h);
          }
     }
   else ret = EINA_FALSE;

   ector_buffer_unmap(cmd->input->buffer, src, src_len);
   ector_buffer_unmap(cmd->output->buffer, dst, dst_len);

   return ret;
}

static Eina_Bool
_box_blur_horiz_apply_alpha(Evas_Filter_Command *cmd)
{
   return _box_blur_apply(cmd, 0, 0);
}

static Eina_Bool
_box_blur_vert_apply_alpha(Evas_Filter_Command *cmd)
{
   return _box_blur_apply(cmd, 1, 0);
}

static Eina_Bool
_box_blur_horiz_apply_rgba(Evas_Filter_Command *cmd)
{
   return _box_blur_apply(cmd, 0, 1);
}

static Eina_Bool
_box_blur_vert_apply_rgba(Evas_Filter_Command *cmd)
{
   return _box_blur_apply(cmd, 1, 1);
}

/* Gaussian blur */

static void
_sin_blur_weights_get(int *weights, int *pow2_divider, int radius)
{
   const int diameter = 2 * radius + 1;
   double x, divider, sum = 0.0;
   double dweights[diameter];
   int k, nextpow2, isum = 0;
   const int FAKE_PI = 3.0;

   /* Base curve:
    * f(x) = sin(x+pi/2)/2+1/2
    */

   for (k = 0; k < diameter; k++)
     {
        x = ((double) k / (double) (diameter - 1)) * FAKE_PI * 2.0 - FAKE_PI;
        dweights[k] = ((sin(x + M_PI_2) + 1.0) / 2.0) * 1024.0;
        sum += dweights[k];
     }

   // Now we need to normalize to have a 2^N divider.
   nextpow2 = log2(2 * sum);
   divider = (double) (1 << nextpow2);

   for (k = 0; k < diameter; k++)
     {
        weights[k] = round(dweights[k] * divider / sum);
        isum += weights[k];
     }

   // Final correction. The difference SHOULD be small...
   weights[radius] += (int) divider - isum;

   if (pow2_divider)
     *pow2_divider = nextpow2;
}

#define FUNCTION_NAME _gaussian_blur_horiz_alpha_step
#define STEP 1
#include "./blur/blur_gaussian_alpha_.c"

// Step size is w (row by row), loops = w, so STEP = 'loops'
#define FUNCTION_NAME _gaussian_blur_vert_alpha_step
#define STEP loops
#include "./blur/blur_gaussian_alpha_.c"

#define FUNCTION_NAME _gaussian_blur_horiz_rgba_step
#define STEP 1
#include "./blur/blur_gaussian_rgba_.c"

#define FUNCTION_NAME _gaussian_blur_vert_rgba_step
#define STEP loops
#include "./blur/blur_gaussian_rgba_.c"

static Eina_Bool
_gaussian_blur_apply(Evas_Filter_Command *cmd, Eina_Bool vert, Eina_Bool rgba)
{
   unsigned int src_len, src_stride, dst_len, dst_stride, radius;
   Eina_Bool ret = EINA_TRUE;
   int pow2_div = 0, w, h;
   void *src, *dst;
   int *weights;

   radius = abs(vert ? cmd->blur.dy : cmd->blur.dx);
   src = _buffer_map_all(cmd->input->buffer, &src_len, E_READ, rgba ? E_ARGB : E_ALPHA, &src_stride);
   dst = _buffer_map_all(cmd->output->buffer, &dst_len, E_WRITE, rgba ? E_ARGB : E_ALPHA, &dst_stride);
   w = cmd->input->w;
   h = cmd->input->h;

   weights = alloca((2 * radius + 1) * sizeof(int));
   _sin_blur_weights_get(weights, &pow2_div, radius);

   if (src && dst)
     {
        DEBUG_TIME_BEGIN();
        if (rgba)
          {
             if (!vert)
               _gaussian_blur_horiz_rgba_step(src, dst, radius, w, h, w, weights, pow2_div);
             else
               _gaussian_blur_vert_rgba_step(src, dst, radius, h, w, 1, weights, pow2_div);
          }
        else
          {
             if (!vert)
               _gaussian_blur_horiz_alpha_step(src, dst, radius, w, h, w, weights, pow2_div);
             else
               _gaussian_blur_vert_alpha_step(src, dst, radius, h, w, 1, weights, pow2_div);
          }
        DEBUG_TIME_END();
     }
   else ret = EINA_FALSE;

   ector_buffer_unmap(cmd->input->buffer, src, src_len);
   ector_buffer_unmap(cmd->output->buffer, dst, dst_len);

   return ret;
}

static Eina_Bool
_gaussian_blur_horiz_apply_alpha(Evas_Filter_Command *cmd)
{
   return _gaussian_blur_apply(cmd, 0, 0);
}

static Eina_Bool
_gaussian_blur_vert_apply_alpha(Evas_Filter_Command *cmd)
{
   return _gaussian_blur_apply(cmd, 1, 0);
}

static Eina_Bool
_gaussian_blur_horiz_apply_rgba(Evas_Filter_Command *cmd)
{
   return _gaussian_blur_apply(cmd, 0, 1);
}

static Eina_Bool
_gaussian_blur_vert_apply_rgba(Evas_Filter_Command *cmd)
{
   return _gaussian_blur_apply(cmd, 1, 1);
}

/* Main entry point */

Software_Filter_Func
eng_filter_blur_func_get(Evas_Filter_Command *cmd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd->input, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd->output, NULL);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(cmd->mode == EVAS_FILTER_MODE_BLUR, NULL);

   switch (cmd->blur.type)
     {
      case EVAS_FILTER_BLUR_BOX:
        if (!cmd->output->alpha_only)
          {
             if (cmd->blur.dx)
               return _box_blur_horiz_apply_rgba;
             else if (cmd->blur.dy)
               return _box_blur_vert_apply_rgba;
          }
        else
          {
             if (cmd->blur.dx)
               return _box_blur_horiz_apply_alpha;
             else if (cmd->blur.dy)
               return _box_blur_vert_apply_alpha;
          }
        break;
      case EVAS_FILTER_BLUR_GAUSSIAN:
        if (!cmd->output->alpha_only)
          {
             if (cmd->blur.dx)
               return _gaussian_blur_horiz_apply_rgba;
             else if (cmd->blur.dy)
               return _gaussian_blur_vert_apply_rgba;
          }
        else
          {
             if (cmd->blur.dx)
               return _gaussian_blur_horiz_apply_alpha;
             else if (cmd->blur.dy)
               return _gaussian_blur_vert_apply_alpha;
          }
        break;
      default:
        ERR("Unsupported blur type %d", cmd->blur.type);
        return NULL;
     }

   return NULL;
}
