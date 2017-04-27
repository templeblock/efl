/**
 * Simple Evas example illustrating basic map utils.
 *
 * You'll need at least one engine built for it (excluding the buffer
 * one). See stdout/stderr for output.
 *
 * @verbatim
 * gcc -o evas_map_utils_eo evas-map-utils-eo.c `pkg-config --cflags --libs evas ecore ecore-evas`
 * @endverbatim
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_EXAMPLES_DIR "."
#endif

#ifndef EFL_BETA_API_SUPPORT
# define EFL_BETA_API_SUPPORT
#endif

#ifndef EFL_EO_API_SUPPORT
# define EFL_EO_API_SUPPORT
#endif

#include <Ecore.h>
#include <Ecore_Evas.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "evas-common.h"

#define WIDTH  480
#define HEIGHT 480

typedef struct
{
   Ecore_Evas *ee;
   Evas       *canvas;
   struct {
        int r, g, b, a;
   } colors[6];
   int colors_index;
   int frame;
   Eina_Bool   alpha : 1;
   Eina_Bool   smooth : 1;
   Eina_Bool   backface_culling : 1;
   Eina_Bool   apply_perspective : 1;
   Eina_Bool   apply_lighting : 1;
} App_Data;

static const char *img1_path = PACKAGE_EXAMPLES_DIR EVAS_IMAGE_FOLDER "/cube1.png";
static const char *img2_path = PACKAGE_EXAMPLES_DIR EVAS_IMAGE_FOLDER "/enlightenment.png";
static const char *img3_path = PACKAGE_EXAMPLES_DIR EVAS_IMAGE_FOLDER "/im1.png";

static const char *help_string =
        "Valid commands:\n"
        "\ta - toggle alpha for maps\n"
        "\ts - toggle smooth for maps\n"
        "\tc - switch map color\n"
        "\tb - toggle backface culling\n"
        "\tp - toggle perspective\n"
        "\tl - toggle lighting\n"
        "\th - prints this help\n";

static Eina_Bool
_anim_cb(void *data)
{
   App_Data *ad = data;
   Evas_Object *o, *ref;
   int r, g, b, a;
   int win_w, win_h, img_w, img_h;
   Evas_Coord x, y, w, h;

   evas_output_size_get(ad->canvas, &win_w, &win_h);

   r = ad->colors[ad->colors_index].r;
   g = ad->colors[ad->colors_index].g;
   b = ad->colors[ad->colors_index].b;
   a = ad->colors[ad->colors_index].a;

   o = evas_object_name_find(ad->canvas, "obj1");
   evas_object_geometry_get(o, &x, &y, &w, &h);

   efl_gfx_map_populate(o, 0);
   efl_gfx_map_rotate(o, 3 * ad->frame, x + (w / 2), y + (h / 2));
   efl_gfx_map_smooth_set(o, ad->smooth);
   efl_gfx_map_alpha_set(o, ad->alpha);
   efl_gfx_map_color_set(o, -1, r, g, b, a);
   efl_gfx_map_enable_set(o, EINA_TRUE);
   ref = o;

   o = evas_object_name_find(ad->canvas, "obj2");
   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_image_size_get(o, &img_w, &img_h);

   efl_gfx_map_dup(o, ref);
   efl_gfx_map_populate(o, 100);
   efl_gfx_map_point_image_uv_set(o, 0, 0, 0);
   efl_gfx_map_point_image_uv_set(o, 1, img_w, 0);
   efl_gfx_map_point_image_uv_set(o, 2, img_w, img_h);
   efl_gfx_map_point_image_uv_set(o, 3, 0, img_h);
   efl_gfx_map_rotate_3d(o, ad->frame * 6, ad->frame * 6, ad->frame * 6,
                         x + (w / 3), y + 10, 0);
   if (ad->apply_lighting)
     efl_gfx_map_lightning_3d(o, win_w / 2, win_h / 2, -100,
                              255, 255, 255, 0, 0, 0);
   efl_gfx_map_enable_set(o, EINA_TRUE);

   o = evas_object_name_find(ad->canvas, "obj3");
   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_image_size_get(o, &img_w, &img_h);

   efl_gfx_map_dup(o, ref);
   efl_gfx_map_populate_manual(o, x, y + (h / 2), w, h, -20);
   efl_gfx_map_point_image_uv_set(o, 0, 0, 0);
   efl_gfx_map_point_image_uv_set(o, 1, img_w, 0);
   efl_gfx_map_point_image_uv_set(o, 2, img_w, img_h);
   efl_gfx_map_point_image_uv_set(o, 3, 0, img_h);
   efl_gfx_map_rotate_3d(o, 20, ad->frame * 6, 0,
                         x + (w / 2), y + (w / 2), w / 2);

   if (ad->apply_perspective)
     efl_gfx_map_perspective_3d(o, x + (w / 2), y + (h / 2), 0, 256);
   if (ad->apply_lighting)
     {
        Evas_Coord mx, my;
        evas_pointer_canvas_xy_get(ad->canvas, &mx, &my);
        efl_gfx_map_lightning_3d(o, mx, my, -256, 255, 255, 255, 0, 0, 0);
     }
   if (ad->backface_culling)
     {
        if (efl_gfx_map_clockwise_get(o))
          evas_object_show(o);
        else
          evas_object_hide(o);
     }
   else
     evas_object_show(o);
   efl_gfx_map_enable_set(o, EINA_TRUE);

   o = evas_object_name_find(ad->canvas, "obj4");
   efl_gfx_geometry_get(o, &x, &y, &w, &h);
   efl_gfx_view_size_get(o, &img_w, &img_h);

   efl_gfx_map_dup(o, ref);
   efl_gfx_map_point_coord_set(o, 0, x, y + h, 0);
   efl_gfx_map_point_coord_set(o, 1, x + w, y + h, 0);
   efl_gfx_map_point_coord_set(o, 2, win_w - 10, win_h - 30, 0);
   efl_gfx_map_point_coord_set(o, 3, (win_w / 2) + 10, win_h - 30, 0);
   efl_gfx_map_point_image_uv_set(o, 0, 0, img_h);
   efl_gfx_map_point_image_uv_set(o, 1, img_w, img_h);
   efl_gfx_map_point_image_uv_set(o, 2, img_w, 2 * (img_h / 3));
   efl_gfx_map_point_image_uv_set(o, 3, 0, 2 * (img_h / 3));
   efl_gfx_map_color_set(o, 0, 200, 200, 200, 150);
   efl_gfx_map_color_set(o, 1, 200, 200, 200, 150);
   efl_gfx_map_color_set(o, 2, 0, 0, 0, 0);
   efl_gfx_map_color_set(o, 3, 0, 0, 0, 0);
   efl_gfx_map_enable_set(o, EINA_TRUE);

   ad->frame = (ad->frame + 1) % 60;

   return EINA_TRUE;
}

static void
_on_keydown(void *data, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event)
{
   App_Data *ad = data;
   Evas_Event_Key_Down *ev = event;
   switch (ev->key[0])
     {
      case 'a':
         ad->alpha = !ad->alpha;
         break;
      case 's':
         ad->smooth = !ad->smooth;
         break;
      case 'c':
         ad->colors_index = (ad->colors_index + 1) % 6;
         break;
      case 'b':
         ad->backface_culling = !ad->backface_culling;
         break;
      case 'p':
         ad->apply_perspective = !ad->apply_perspective;
         break;
      case 'l':
         ad->apply_lighting = !ad->apply_lighting;
         break;
      case 'h':
         puts(help_string);
         break;
      default:
         break;
     }
}

static void
_objs_fit(Evas *e)
{
   Evas_Object *o;
   int w, h;

   evas_output_size_get(e, &w, &h);
   w /= 2;
   h /= 2;

   o = evas_object_name_find(e, "obj1");
   evas_object_move(o, w / 4, h / 4);
   evas_object_resize(o, w / 2, h / 2);

   o = evas_object_name_find(e, "obj2");
   evas_object_move(o, 5 * w / 4, h / 4);
   evas_object_resize(o, w / 2, h / 2);

   o = evas_object_name_find(e, "obj3");
   evas_object_move(o, w / 4, 5 * h / 4);
   evas_object_resize(o, w / 2, h / 2);

   o = evas_object_name_find(e, "obj4_source");
   evas_object_move(o, 5 * w / 4, 5 * h / 4);
   evas_object_resize(o, w / 2, h / 2);

   o = evas_object_name_find(e, "obj4");
   evas_object_move(o, 5 * w / 4, 5 * h / 4);
   evas_object_resize(o, w / 2, h / 2);
}

static void
_on_resize(void *data EINA_UNUSED, Evas *e, Evas_Object *o EINA_UNUSED, void *event EINA_UNUSED)
{
   _objs_fit(e);
}

static void
_on_free(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event EINA_UNUSED)
{
   ecore_main_loop_quit();
}

int
main(void)
{
   Evas_Object *bg, *o, *osrc;
   static App_Data d = {
        .ee = NULL,
        .canvas = NULL,
        .colors = {
             { 255, 255, 255, 255 },
             { 128, 128, 0, 128 },
             { 255, 0, 0, 255 },
             { 64, 128, 255, 255 },
             { 11, 23, 58, 132 },
             { 0, 0, 0, 255 }
        },
        .colors_index = 0,
        .frame = 0,
        .alpha = EINA_FALSE,
        .smooth = EINA_FALSE,
        .backface_culling = EINA_FALSE,
        .apply_perspective = EINA_TRUE,
        .apply_lighting = EINA_TRUE
   };

   if (!ecore_evas_init())
     return EXIT_FAILURE;

   d.ee = ecore_evas_new(NULL, 0, 0, WIDTH, HEIGHT, NULL);
   if (!d.ee)
     goto error;

   d.canvas = ecore_evas_get(d.ee);

   bg = evas_object_image_filled_add(d.canvas);
   evas_object_image_file_set(bg, img1_path, NULL);
   ecore_evas_object_associate(d.ee, bg, 0);
   evas_object_focus_set(bg, EINA_TRUE);
   evas_object_move(bg, 0, 0);
   evas_object_resize(bg, WIDTH, HEIGHT);
   evas_object_show(bg);

   o = evas_object_rectangle_add(d.canvas);
   evas_object_name_set(o, "obj1");
   evas_object_color_set(o, 128, 0, 200, 200);
   evas_object_show(o);

   o = evas_object_image_filled_add(d.canvas);
   evas_object_name_set(o, "obj2");
   evas_object_image_file_set(o, img2_path, NULL);
   evas_object_show(o);

   o = evas_object_image_filled_add(d.canvas);
   evas_object_name_set(o, "obj3");
   evas_object_image_file_set(o, img2_path, NULL);
   evas_object_show(o);

   osrc = evas_object_image_filled_add(d.canvas);
   evas_object_image_file_set(osrc, img3_path, NULL);
   evas_object_name_set(osrc, "obj4_source");
   evas_object_show(osrc);

   o = evas_object_image_filled_add(d.canvas);
   evas_object_image_source_set(o, osrc);
   evas_object_name_set(o, "obj4");
   evas_object_show(o);

   _objs_fit(d.canvas);

   evas_object_event_callback_add(bg, EVAS_CALLBACK_KEY_DOWN, _on_keydown, &d);
   evas_object_event_callback_add(bg, EVAS_CALLBACK_RESIZE, _on_resize, NULL);
   evas_object_event_callback_add(bg, EVAS_CALLBACK_FREE, _on_free, NULL);

   ecore_animator_add(_anim_cb, &d);

   ecore_main_loop_begin();

   ecore_evas_shutdown();
   return 0;

error:
   fprintf(stderr, "you got to have at least one evas engine built and linked"
                   " up to ecore-evas for this example to run properly.\n");
   ecore_evas_shutdown();
   return -1;
}
