#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define ELM_INTERFACE_ATSPI_ACCESSIBLE_PROTECTED
#define ELM_INTERFACE_ATSPI_WIDGET_ACTION_PROTECTED
#define EFL_INPUT_EVENT_PROTECTED
#define EFL_GFX_SIZE_HINT_PROTECTED
#define EFL_UI_WIN_BETA

#include <Elementary.h>
#include <Elementary_Cursor.h>

#include "elm_priv.h"
#include "elm_widget_menu.h"
#ifdef HAVE_ELEMENTARY_WL2
# include "ecore_evas_wayland_private.h"
#endif

#include "../evas/canvas/evas_box.eo.h"

#define EFL_INTERNAL_UNSTABLE
#include "interfaces/efl_common_internal.h"

#include "elm_part_helper.h"
#include "efl_ui_win_internal_part.eo.h"

#define MY_CLASS EFL_UI_WIN_CLASS
#define MY_CLASS_NAME "Efl.Ui.Win"
#define MY_CLASS_NAME_LEGACY "elm_win"

#define FRAME_OBJ_THEME_MIN_VERSION 119

static const Elm_Win_Trap *trap = NULL;

#define TRAP(sd, name, ...)                                             \
  do                                                                    \
    {                                                                   \
       if (sd->type != ELM_WIN_FAKE)                                    \
         if ((!trap) || (!trap->name) ||                                \
             ((trap->name) &&                                           \
              (trap->name(sd->trap_data, sd->obj, ## __VA_ARGS__))))    \
           ecore_evas_##name(sd->ee, ##__VA_ARGS__);                    \
    }                                                                   \
  while (0)

#define ELM_WIN_DATA_GET(o, sd) \
  Efl_Ui_Win_Data * sd = efl_data_scope_get(o, MY_CLASS)

#define ELM_WIN_DATA_GET_OR_RETURN(o, ptr, ...)      \
  ELM_WIN_DATA_GET(o, ptr);                          \
  if (!ptr)                                          \
    {                                                \
       CRI("No widget data for object %p (%s)",      \
            o, efl_class_name_get(o));                \
       return __VA_ARGS__;                           \
    }

#define ELM_WIN_CHECK(obj)                                             \
  if (!obj || !efl_isa(obj, MY_CLASS)) \
    return

#define ENGINE_GET() (_elm_preferred_engine ? _elm_preferred_engine : _elm_config->engine)

typedef struct _Efl_Ui_Win_Data Efl_Ui_Win_Data;
typedef struct _Input_Pointer_Iterator Input_Pointer_Iterator;

struct _Efl_Ui_Win_Data
{
   Ecore_Evas           *ee;
   Evas                 *evas;
   Evas_Object          *parent; /* parent *window* object*/
   Evas_Object          *img_obj, *frame_obj;
   Eo /* wref */        *bg, *content;
   Evas_Object          *obj; /* The object itself */
#ifdef HAVE_ELEMENTARY_X
   struct
   {
      Ecore_X_Window       xwin;
      Ecore_Event_Handler *client_message_handler;
      Ecore_Event_Handler *property_handler;
   } x;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   struct
   {
      Ecore_Wl2_Window *win;
      Ecore_Event_Handler *configure_handler;
      Eina_Bool opaque_dirty : 1;
   } wl;
#endif
#ifdef HAVE_ELEMENTARY_COCOA
   struct {
      Ecore_Cocoa_Window *win;
   } cocoa;
#endif
#ifdef HAVE_ELEMENTARY_WIN32
   struct
   {
      Ecore_Win32_Window *win;
   } win32;
#endif

   Efl_Ui_Win_Type                   type;
   Efl_Ui_Win_Keyboard_Mode          kbdmode;
   Elm_Win_Indicator_Mode         indmode;
   Elm_Win_Indicator_Opacity_Mode ind_o_mode;
   struct
   {
      const char  *info;
      Ecore_Timer *timer;
      int          repeat_count;
      int          shot_counter;
   } shot;
   int                           *autodel_clear, rot;
   struct
   {
      int x, y;
   } screen;
   struct
   {
#ifdef HAVE_ELEMENTARY_WL2
      Ecore_Wl2_Window *win;
      struct wl_surface *surf;
#endif
      Ecore_Evas  *ee;
      Evas_Object *obj, *hot_obj;
      int          hot_x, hot_y;
      Eina_Bool    visible : 1;
   } pointer;
   struct
   {
      Evas_Object *fobj; /* focus highlight edje object */

      struct
      {
         Evas_Object *target;
         Eina_Bool    visible : 1;
         Eina_Bool    in_theme: 1; /**< focus highlight is handled by theme.
                                     this is set true if edc data item "focus_highlight" is set to "on" during focus in callback. */
      } cur, prev;

      const char  *style;
      Ecore_Job   *reconf_job;

      Eina_Bool    enabled : 1;
      Eina_Bool    theme_changed : 1; /* set true when the focus theme is changed */
      Eina_Bool    animate : 1; /* set true when the focus highlight animate is enabled */
      Eina_Bool    animate_supported : 1; /* set true when the focus highlight animate is supported by theme */
      Eina_Bool    geometry_changed : 1;
      Eina_Bool    auto_enabled : 1;
      Eina_Bool    auto_animate : 1;
   } focus_highlight;

   Evas_Object *icon;
   const char  *title;
   const char  *icon_name;
   const char  *role;
   const char  *stack_id;
   const char  *stack_master_id;
   Eina_Stringshare *name;
   Eina_Stringshare *accel_pref;

   Evas_Object *main_menu;

   struct
   {
      const char  *name;
      const char **available_list;
      unsigned int count;
   } profile;
   struct
   {
      int          preferred_rot; /* indicates preferred rotation value, -1 means invalid. */
      int         *rots; /* indicates available rotations */
      unsigned int count; /* number of elements in available rotations */
      Eina_Bool    wm_supported : 1; /* set true when the window manager support window rotation */
      Eina_Bool    use : 1; /* set ture when application use window manager rotation. */
   } wm_rot;

   void *trap_data;

   double       aspect; /* defined as w/h or 0 */
   int          aspect_w, aspect_h; /* used for the get API */
   int          size_base_w, size_base_h;
   int          size_step_w, size_step_h;
   int          max_w, max_h;
   int          norender;
   int          modal_count;
   int          response;

   struct {
      short     pointer_move;
      short     pointer_down;
      short     pointer_up;
      short     pointer_in;
      short     pointer_out;
      short     pointer_cancel;
      short     pointer_wheel;
      short     finger_move;
      short     finger_down;
      short     finger_up;
      short     key_down;
      short     key_up;
      short     render_pre;
      short     render_post;
      short     focus_in;
      short     focus_out;
      short     object_focus_in;
      short     object_focus_out;
      short     device_changed;
   } event_forward;

   struct {
      /* frame_obj is always used except for FAKE */
      Eina_Bool need : 1; /**< if true, application draws its own csd */
      Eina_Bool need_shadow : 1; /**< if true, application draws its csd and shadow */
      Eina_Bool need_borderless : 1;
      Eina_Bool need_bg_solid : 1;
      Eina_Bool need_bg_standard : 1;
      Eina_Bool need_menu : 1;
      Eina_Bool need_unresizable : 1;
      Eina_Bool cur_borderless : 1;
      Eina_Bool cur_shadow : 1;
      Eina_Bool cur_focus : 1;
      Eina_Bool cur_maximized : 1;
      Eina_Bool cur_bg_solid : 1;
      Eina_Bool cur_bg_standard : 1;
      Eina_Bool cur_menu : 1;
      Eina_Bool cur_unresizable : 1;
      Eina_Bool wayland : 1;
   } csd;

   struct {
      Evas_Object *box, *edje;
      Eina_Bool    forbidden : 1; /**< Marks some legacy APIs as not allowed. */
      Eina_Bool    bg_must_swallow : 1; /**< Legacy theme compatibility (elm_bg for standard window) */
      Eina_Bool    bg_must_swallow_init : 1;
   } legacy;

   Eina_Bool    first_draw : 1;
   Eina_Bool    deferred_resize_job : 1;
   Eina_Bool    urgent : 1;
   Eina_Bool    modal : 1;
   Eina_Bool    demand_attention : 1;
   Eina_Bool    autodel : 1;
   Eina_Bool    autohide : 1;
   Eina_Bool    constrain : 1;
   Eina_Bool    resizing : 1;
   Eina_Bool    iconified : 1;
   Eina_Bool    withdrawn : 1;
   Eina_Bool    sticky : 1;
   Eina_Bool    fullscreen : 1;
   Eina_Bool    maximized : 1;
   Eina_Bool    skip_focus : 1;
   Eina_Bool    floating : 1;
   Eina_Bool    noblank : 1;
   Eina_Bool    theme_alpha : 1; /**< alpha value fetched by a theme. this has higher priority than application_alpha */
   Eina_Bool    application_alpha : 1; /**< alpha value set by an elm_win_alpha_set() api. this has lower priority than theme_alpha */
   Eina_Bool    tmp_updating_hints : 1;
   Eina_Bool    single_edje_content: 1; /* hack for E */
   Eina_Bool    shown : 1;
   Eina_Bool    stack_base : 1;
};

struct _Input_Pointer_Iterator
{
   Eina_Iterator  iterator;
   Eina_List     *list;
   Eina_Iterator *real_iterator;
   const Eo      *object;
};

static const char SIG_DELETE_REQUEST[] = "delete,request";
static const char SIG_FOCUS_OUT[] = "focus,out"; // deprecated. use "unfocused" instead.
static const char SIG_FOCUS_IN[] = "focus,in"; // deprecated. use "focused" instead.
static const char SIG_MOVED[] = "moved";
static const char SIG_WITHDRAWN[] = "withdrawn";
static const char SIG_ICONIFIED[] = "iconified";
static const char SIG_NORMAL[] = "normal";
static const char SIG_STICK[] = "stick";
static const char SIG_UNSTICK[] = "unstick";
static const char SIG_FULLSCREEN[] = "fullscreen";
static const char SIG_UNFULLSCREEN[] = "unfullscreen";
static const char SIG_MAXIMIZED[] = "maximized";
static const char SIG_UNMAXIMIZED[] = "unmaximized";
static const char SIG_IOERR[] = "ioerr";
static const char SIG_INDICATOR_PROP_CHANGED[] = "indicator,prop,changed";
static const char SIG_ROTATION_CHANGED[] = "rotation,changed";
static const char SIG_PROFILE_CHANGED[] = "profile,changed";
static const char SIG_WM_ROTATION_CHANGED[] = "wm,rotation,changed";

static const Evas_Smart_Cb_Description _smart_callbacks[] = {
   {SIG_DELETE_REQUEST, ""},
   {SIG_FOCUS_OUT, ""},
   {SIG_FOCUS_IN, ""},
   {SIG_MOVED, ""},
   {SIG_WITHDRAWN, ""},
   {SIG_ICONIFIED, ""},
   {SIG_NORMAL, ""},
   {SIG_STICK, ""},
   {SIG_UNSTICK, ""},
   {SIG_FULLSCREEN, ""},
   {SIG_UNFULLSCREEN, ""},
   {SIG_MAXIMIZED, ""},
   {SIG_UNMAXIMIZED, ""},
   {SIG_IOERR, ""},
   {SIG_INDICATOR_PROP_CHANGED, ""},
   {SIG_ROTATION_CHANGED, ""},
   {SIG_PROFILE_CHANGED, ""},
   {SIG_WM_ROTATION_CHANGED, ""},
   {SIG_WIDGET_FOCUSED, ""}, /**< handled by elm_widget */
   {SIG_WIDGET_UNFOCUSED, ""}, /**< handled by elm_widget */
   {NULL, NULL}
};

static Eina_Bool _key_action_return(Evas_Object *obj, const char *params);
static Eina_Bool _key_action_move(Evas_Object *obj, const char *params);

static const Elm_Action key_actions[] = {
   {"return", _key_action_return},
   {"move", _key_action_move},
   {NULL, NULL}
};

Eina_List *_elm_win_list = NULL;
int _elm_win_deferred_free = 0;

static Eina_Bool _elm_win_throttle_ok = EINA_FALSE;
static int _elm_win_count = 0;

static Eina_Bool _elm_win_auto_throttled = EINA_FALSE;

static Ecore_Timer *_elm_win_state_eval_timer = NULL;

static void _elm_win_legacy_init(Efl_Ui_Win_Data *sd);
static void
_elm_win_on_resize_obj_changed_size_hints(void *data,
                                          Evas *e,
                                          Evas_Object *obj,
                                          void *event_info);
static void
_elm_win_img_callbacks_del(Evas_Object *obj, Evas_Object *imgobj);
static Elm_Theme_Apply _elm_win_theme_internal(Eo *obj, Efl_Ui_Win_Data *sd);
static void _elm_win_frame_add(Efl_Ui_Win_Data *sd, const char *element, const char *style);
static void _elm_win_frame_style_update(Efl_Ui_Win_Data *sd, Eina_Bool force_emit, Eina_Bool calc);
static inline void _elm_win_need_frame_adjust(Efl_Ui_Win_Data *sd, const char *engine);
static void _elm_win_resize_objects_eval(Evas_Object *obj);
static void _elm_win_opaque_update(Efl_Ui_Win_Data *sd, Eina_Bool force_alpha);
static void _elm_win_frame_obj_update(Efl_Ui_Win_Data *sd);

#ifdef HAVE_ELEMENTARY_X
static void _elm_win_xwin_update(Efl_Ui_Win_Data *sd);
#endif

EAPI double _elm_startup_time = 0;

static void
_elm_win_first_frame_do(void *data, Evas *e EINA_UNUSED, void *event_info EINA_UNUSED)
{
   double end = ecore_time_unix_get();
   char *first = data;

   switch (*first)
     {
      case 'A': abort();
      case 'E':
      case 'D': exit(-1);
      case 'T': fprintf(stderr, "Startup time: '%f' - '%f' = '%f' sec\n", end, _elm_startup_time, end - _elm_startup_time);
                break;
     }

   evas_event_callback_del_full(e, EVAS_CALLBACK_RENDER_POST, _elm_win_first_frame_do, data);
}

static void
_win_noblank_eval(void)
{
#ifdef HAVE_ELEMENTARY_X
   Eina_List *l;
   Evas_Object *obj;
   int noblanks = 0;
   Eina_Bool change = EINA_FALSE;

   EINA_LIST_FOREACH(_elm_win_list, l, obj)
     {
        ELM_WIN_DATA_GET(obj, sd);

        if (sd->x.xwin)
          {
             if ((sd->noblank) && (!sd->iconified) && (!sd->withdrawn) &&
                 evas_object_visible_get(obj))
               noblanks++;

             change = EINA_TRUE;
          }
     }

   if (!change) return;

   if (noblanks > 0) ecore_x_screensaver_suspend();
   else ecore_x_screensaver_resume();
#endif
#ifdef HAVE_ELEMENTARY_WL2
   // XXX: no wl implementation of this yet - maybe higher up at prop level
#endif
}

static Elm_Process_State _elm_process_state = ELM_PROCESS_STATE_FOREGROUND;

EAPI Elm_Process_State
elm_process_state_get(void)
{
   return _elm_process_state;
}

static void
_elm_win_apply_alpha(Eo *obj, Efl_Ui_Win_Data *sd)
{
   Eina_Bool enabled;

   if (!sd->ee) return;

   enabled = sd->theme_alpha | sd->application_alpha;
   _elm_win_opaque_update(sd, EINA_TRUE);
   if (sd->img_obj)
     {
        evas_object_image_alpha_set(sd->img_obj, enabled);
        ecore_evas_alpha_set(sd->ee, enabled);
     }
   else
     {
#ifdef HAVE_ELEMENTARY_X
        if (sd->x.xwin)
          {
             enabled |= (sd->csd.need && !sd->fullscreen);
             if (enabled)
               {
                  if (!ecore_x_screen_is_composited(0))
                    elm_win_shaped_set(obj, enabled);
                  else
                    TRAP(sd, alpha_set, enabled);
               }
             else
               TRAP(sd, alpha_set, enabled);
             _elm_win_xwin_update(sd);
          }
        else
#else
          (void)obj;
#endif
          TRAP(sd, alpha_set, enabled);
     }
}

static Eina_Bool
_elm_win_state_eval(void *data EINA_UNUSED)
{
   Eina_List *l;
   Evas_Object *obj;
   int _elm_win_count_shown = 0;
   int _elm_win_count_iconified = 0;
   int _elm_win_count_withdrawn = 0;
   Eina_Bool throttle = EINA_FALSE;

   _elm_win_state_eval_timer = NULL;

   EINA_LIST_FOREACH(_elm_win_list, l, obj)
     {
        if (_elm_config->auto_norender_withdrawn)
          {
             if ((elm_win_withdrawn_get(obj)) ||
                 ((elm_win_iconified_get(obj) &&
                   (_elm_config->auto_norender_iconified_same_as_withdrawn))))
               {
                  if (!evas_object_data_get(obj, "__win_auto_norender"))
                    {
                       Evas *evas = evas_object_evas_get(obj);

                       elm_win_norender_push(obj);
                       evas_object_data_set(obj, "__win_auto_norender", obj);

                       if (_elm_config->auto_flush_withdrawn)
                         {
                            edje_file_cache_flush();
                            edje_collection_cache_flush();
                            evas_image_cache_flush(evas);
                            evas_font_cache_flush(evas);
                         }
                       if (_elm_config->auto_dump_withdrawn)
                         {
                            evas_render_dump(evas);
                         }
                    }
                  continue;
               }
          }
        if (evas_object_data_get(obj, "__win_auto_norender"))
          {
             elm_win_norender_pop(obj);
             evas_object_data_del(obj, "__win_auto_norender");
          }
     }
   if (((_elm_config->auto_throttle) &&
        (elm_policy_get(ELM_POLICY_THROTTLE) != ELM_POLICY_THROTTLE_NEVER)) ||
        (elm_policy_get(ELM_POLICY_THROTTLE) == ELM_POLICY_THROTTLE_HIDDEN_ALWAYS))
     throttle = EINA_TRUE;
   if (_elm_win_count == 0)
     {
        if ((_elm_win_throttle_ok) && (_elm_win_auto_throttled))
          {
             _elm_process_state = ELM_PROCESS_STATE_FOREGROUND;
             ecore_event_add(ELM_EVENT_PROCESS_FOREGROUND, NULL, NULL, NULL);
             if (throttle)
               ecore_throttle_adjust(-_elm_config->auto_throttle_amount);
             _elm_win_auto_throttled = EINA_FALSE;
          }
     }
   else
     {
        EINA_LIST_FOREACH(_elm_win_list, l, obj)
          {
             if (elm_win_withdrawn_get(obj)) _elm_win_count_withdrawn++;
             else if (elm_win_iconified_get(obj)) _elm_win_count_iconified++;
             else if (evas_object_visible_get(obj)) _elm_win_count_shown++;
          }
        if (_elm_win_count_shown <= 0)
          {
             if ((_elm_win_throttle_ok) && (!_elm_win_auto_throttled))
               {
                  _elm_process_state = ELM_PROCESS_STATE_BACKGROUND;
                  ecore_event_add(ELM_EVENT_PROCESS_BACKGROUND, NULL, NULL, NULL);
                  if (throttle)
                    ecore_throttle_adjust(_elm_config->auto_throttle_amount);
                  _elm_win_auto_throttled = EINA_TRUE;
               }
          }
        else
          {
             if ((_elm_win_throttle_ok) && (_elm_win_auto_throttled))
               {
                  _elm_process_state = ELM_PROCESS_STATE_FOREGROUND;
                  ecore_event_add(ELM_EVENT_PROCESS_FOREGROUND, NULL, NULL, NULL);
                  if (throttle)
                    ecore_throttle_adjust(-_elm_config->auto_throttle_amount);
                  _elm_win_auto_throttled = EINA_FALSE;
               }
          }
     }
   _win_noblank_eval();
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_policy_quit_triggered(Eo* triggering_obj)
{
   if ((!_elm_win_list) &&
       (elm_policy_get(ELM_POLICY_QUIT) == ELM_POLICY_QUIT_LAST_WINDOW_CLOSED))
     {
        return EINA_TRUE;
     }

   if (elm_policy_get(ELM_POLICY_QUIT) == ELM_POLICY_QUIT_LAST_WINDOW_HIDDEN)
     {
        Eina_List *l;
        Evas_Object *win;

        EINA_LIST_FOREACH(_elm_win_list, l, win)
          if (win != triggering_obj && evas_object_visible_get(win) == EINA_TRUE)
            {
               return EINA_FALSE;
            }
        return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_elm_win_flush_cache_and_exit(Eo *obj)
{
   edje_file_cache_flush();
   edje_collection_cache_flush();
   evas_image_cache_flush(evas_object_evas_get(obj));
   evas_font_cache_flush(evas_object_evas_get(obj));
   elm_exit();
}

static void
_elm_win_state_eval_queue(void)
{
   if (_elm_win_state_eval_timer) ecore_timer_del(_elm_win_state_eval_timer);
   _elm_win_state_eval_timer = ecore_timer_add(0.5, _elm_win_state_eval, NULL);
}

// example shot spec (wait 0.1 sec then save as my-window.png):
// ELM_ENGINE="shot:delay=0.1:file=my-window.png"

static double
_shot_delay_get(Efl_Ui_Win_Data *sd)
{
   char *p, *pd;
   char *d = strdup(sd->shot.info);

   if (!d) return 0.5;
   for (p = (char *)sd->shot.info; *p; p++)
     {
        if (!strncmp(p, "delay=", 6))
          {
             double v;

             for (pd = d, p += 6; (*p) && (*p != ':'); p++, pd++)
               {
                  *pd = *p;
               }
             *pd = 0;
             v = _elm_atof(d);
             free(d);
             return v;
          }
     }
   free(d);

   return 0.5;
}

static char *
_shot_file_get(Efl_Ui_Win_Data *sd)
{
   char *p;
   char *tmp = strdup(sd->shot.info);
   char *repname = NULL;

   if (!tmp) return NULL;

   for (p = (char *)sd->shot.info; *p; p++)
     {
        if (!strncmp(p, "file=", 5))
          {
             strcpy(tmp, p + 5);
             if (!sd->shot.repeat_count) return tmp;
             else
               {
                  char *dotptr = strrchr(tmp, '.');
                  if (dotptr)
                    {
                       size_t size = sizeof(char) * (strlen(tmp) + 16);
                       repname = malloc(size);
                       strncpy(repname, tmp, dotptr - tmp);
                       snprintf(repname + (dotptr - tmp), size -
                                (dotptr - tmp), "%03i",
                                sd->shot.shot_counter + 1);
                       strcat(repname, dotptr);
                       free(tmp);
                       return repname;
                    }
               }
          }
     }
   free(tmp);
   if (!sd->shot.repeat_count) return strdup("out.png");

   repname = malloc(sizeof(char) * 24);
   snprintf(repname, sizeof(char) * 24, "out%03i.png",
            sd->shot.shot_counter + 1);

   return repname;
}

static int
_shot_repeat_count_get(Efl_Ui_Win_Data *sd)
{
   char *p, *pd;
   char *d = strdup(sd->shot.info);

   if (!d) return 0;
   for (p = (char *)sd->shot.info; *p; p++)
     {
        if (!strncmp(p, "repeat=", 7))
          {
             int v;

             for (pd = d, p += 7; (*p) && (*p != ':'); p++, pd++)
               {
                  *pd = *p;
               }
             *pd = 0;
             v = atoi(d);
             if (v < 0) v = 0;
             if (v > 1000) v = 999;
             free(d);
             return v;
          }
     }
   free(d);

   return 0;
}

static char *
_shot_key_get(Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   return NULL;
}

static char *
_shot_flags_get(Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   return NULL;
}

static void
_shot_do(Efl_Ui_Win_Data *sd)
{
   Ecore_Evas *ee;
   Evas_Object *o;
   unsigned int *pixels;
   int w, h;
   char *file, *key, *flags;

   ecore_evas_manual_render(sd->ee);
   pixels = (void *)ecore_evas_buffer_pixels_get(sd->ee);
   if (!pixels) return;

   ecore_evas_geometry_get(sd->ee, NULL, NULL, &w, &h);
   if ((w < 1) || (h < 1)) return;

   file = _shot_file_get(sd);
   if (!file) return;

   key = _shot_key_get(sd);
   flags = _shot_flags_get(sd);
   ee = ecore_evas_buffer_new(1, 1);
   o = evas_object_image_add(ecore_evas_get(ee));
   evas_object_image_alpha_set(o,
                               sd->theme_alpha | sd->application_alpha);
   evas_object_image_size_set(o, w, h);
   evas_object_image_data_set(o, pixels);
   if (!evas_object_image_save(o, file, key, flags))
     {
        ERR("Cannot save window to '%s' (key '%s', flags '%s')",
            file, key, flags);
     }
   free(file);
   free(key);
   free(flags);
   ecore_evas_free(ee);
   if (sd->shot.repeat_count) sd->shot.shot_counter++;
}

static Eina_Bool
_shot_delay(void *data)
{
   ELM_WIN_DATA_GET(data, sd);

   _shot_do(sd);
   if (sd->shot.repeat_count)
     {
        int remainshot = (sd->shot.repeat_count - sd->shot.shot_counter);
        if (remainshot > 0) return EINA_TRUE;
     }
   sd->shot.timer = NULL;
   elm_exit();

   return EINA_FALSE;
}

static void
_shot_init(Efl_Ui_Win_Data *sd)
{
   if (!sd->shot.info) return;

   sd->shot.repeat_count = _shot_repeat_count_get(sd);
   sd->shot.shot_counter = 0;
}

static void
_shot_handle(Efl_Ui_Win_Data *sd)
{
   if (!sd->shot.info) return;

   if (!sd->shot.timer)
     sd->shot.timer = ecore_timer_add(_shot_delay_get(sd), _shot_delay,
                                      sd->obj);
}

/* elm-win specific associate, does the trap while ecore_evas_object_associate()
 * does not.
 */
static Efl_Ui_Win_Data *
_elm_win_associate_get(const Ecore_Evas *ee)
{
   Evas_Object *obj = ecore_evas_data_get(ee, "elm_win");
   if (!obj) return NULL;
   ELM_WIN_DATA_GET(obj, sd);
   return sd;
}

/* Interceptors Callbacks */
static void
_elm_win_obj_intercept_raise(void *data, Evas_Object *obj EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);
   TRAP(sd, raise);
}

static void
_elm_win_obj_intercept_lower(void *data, Evas_Object *obj EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);
   TRAP(sd, lower);
}

static void
_elm_win_obj_intercept_stack_above(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, Evas_Object *above EINA_UNUSED)
{
   INF("TODO: %s", __FUNCTION__);
}

static void
_elm_win_obj_intercept_stack_below(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, Evas_Object *below EINA_UNUSED)
{
   INF("TODO: %s", __FUNCTION__);
}

static void
_elm_win_obj_intercept_layer_set(void *data, Evas_Object *obj EINA_UNUSED, int l)
{
   ELM_WIN_DATA_GET(data, sd);
   TRAP(sd, layer_set, l);
}

/* Event Callbacks */

static void
_elm_win_size_hints_update(Efl_Ui_Win *win, Efl_Ui_Win_Data *sd)
{
   Evas_Coord minw, minh, maxw, maxh;

   efl_gfx_size_hint_combined_min_get(win, &minw, &minh);
   efl_gfx_size_hint_max_get(win, &maxw, &maxh);
   if (maxw < 1) maxw = -1;
   if (maxh < 1) maxh = -1;

   TRAP(sd, size_min_set, minw, minh);
   TRAP(sd, size_max_set, maxw, maxh);
}

static void
_elm_win_obj_callback_changed_size_hints(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                                         Evas_Object *obj, void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(obj, sd);

   if (sd->tmp_updating_hints) return;
   _elm_win_size_hints_update(obj, sd);
}
/* end of elm-win specific associate */

static void
_elm_win_move(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   int x, y;

   if (!sd) return;

   ecore_evas_geometry_get(ee, &x, &y, NULL, NULL);
   sd->screen.x = x;
   sd->screen.y = y;
   efl_event_callback_legacy_call(sd->obj, EFL_GFX_EVENT_MOVE, NULL);
   evas_nochange_push(evas_object_evas_get(sd->obj));
   sd->response++;
   evas_object_move(sd->obj, x, y);
   sd->response--;
   evas_nochange_pop(evas_object_evas_get(sd->obj));
}

static void
_elm_win_resize_job(void *data)
{
   ELM_WIN_DATA_GET(data, sd);
   int w, h;

   sd->deferred_resize_job = EINA_FALSE;
   ecore_evas_geometry_get(sd->ee, NULL, NULL, &w, &h);
   if (sd->constrain)
     {
        int sw, sh;
        ecore_evas_screen_geometry_get(sd->ee, NULL, NULL, &sw, &sh);
        w = MIN(w, sw);
        h = MIN(h, sh);
     }

   if (sd->frame_obj)
     {
        int fx, fy, fw, fh;

        evas_output_framespace_get(sd->evas, &fx, &fy, &fw, &fh);
        evas_object_move(sd->frame_obj, -fx, -fy);
        evas_object_resize(sd->frame_obj, w + fw, h + fh);
     }
   else
     _elm_win_opaque_update(sd, 0);

   if (sd->main_menu)
     {
        int mx, my;

        efl_gfx_position_get(sd->main_menu, &mx, &my);
        elm_menu_move(sd->main_menu, mx, my);
     }

   sd->response++;
   evas_object_resize(sd->obj, w, h);
   evas_object_resize(sd->legacy.edje, w, h);
   sd->response--;
}

static void
_elm_win_pre_render(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   if (!sd) return;

   _elm_win_throttle_ok = EINA_TRUE;
   if (!sd->first_draw)
     {
        sd->first_draw = EINA_TRUE;
        _elm_win_frame_obj_update(sd);
     }
   if (sd->deferred_resize_job)
     _elm_win_resize_job(sd->obj);
}

static void
_elm_win_resize(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   if (!sd) return;

   sd->deferred_resize_job = EINA_TRUE;
}

static void
_elm_win_mouse_in(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   if (!sd) return;

   _elm_win_throttle_ok = EINA_TRUE;
   sd->resizing = EINA_FALSE;
#ifdef HAVE_ELEMENTARY_WL2
   if ((sd->wl.win) && (sd->pointer.ee))
     {
        sd->pointer.visible = EINA_TRUE;
        ecore_evas_show(sd->pointer.ee);
        sd->pointer.surf = ecore_wl2_window_surface_get(sd->pointer.win);

        _elm_win_wl_cursor_set(sd->obj, NULL);
     }
#endif
}

static void
_elm_win_mouse_out(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   if (!sd) return;

#ifdef HAVE_ELEMENTARY_WL2
   if ((sd->wl.win) && (sd->pointer.ee))
     sd->pointer.visible = EINA_FALSE;
#endif
}

static void
_elm_win_focus_highlight_reconfigure_job_stop(Efl_Ui_Win_Data *sd)
{
   ELM_SAFE_FREE(sd->focus_highlight.reconf_job, ecore_job_del);
}

static void
_elm_win_focus_highlight_visible_set(Efl_Ui_Win_Data *sd,
                                     Eina_Bool visible)
{
   Evas_Object *fobj = sd->focus_highlight.fobj;
   if (!fobj) return;

   if (visible)
     {
        evas_object_show(fobj);
        edje_object_signal_emit(fobj, "elm,action,focus,show", "elm");
     }
   else
     {
        edje_object_signal_emit(fobj, "elm,action,focus,hide", "elm");
     }
}

Evas_Object *
_elm_win_focus_highlight_object_get(Evas_Object *obj)
{
   ELM_WIN_DATA_GET(obj, sd);

   return sd->focus_highlight.fobj;
}

static void
_elm_win_focus_highlight_anim_setup(Efl_Ui_Win_Data *sd,
                                    Evas_Object *obj)
{
   Evas_Coord tx, ty, tw, th;
   Evas_Coord px, py, pw, ph;
   Edje_Message_Int_Set *m;
   Evas_Object *target = sd->focus_highlight.cur.target;

   evas_object_geometry_get(obj, &px, &py, &pw, &ph);
   elm_widget_focus_highlight_geometry_get(target, &tx, &ty, &tw, &th);
   evas_object_move(obj, tx, ty);
   evas_object_resize(obj, tw, th);

   if ((px == tx) && (py == ty) && (pw == tw) && (ph == th)) return;

   if (!_elm_config->focus_highlight_clip_disable)
     evas_object_clip_unset(obj);

   m = alloca(sizeof(*m) + (sizeof(int) * 8));
   m->count = 8;
   m->val[0] = px - tx;
   m->val[1] = py - ty;
   m->val[2] = pw;
   m->val[3] = ph;
   m->val[4] = 0;
   m->val[5] = 0;
   m->val[6] = tw;
   m->val[7] = th;
   edje_object_message_send(obj, EDJE_MESSAGE_INT_SET, 1, m);
}

static void
_elm_win_focus_highlight_simple_setup(Efl_Ui_Win_Data *sd,
                                      Evas_Object *obj)
{
   Evas_Object *clip, *target = sd->focus_highlight.cur.target;
   Evas_Coord x, y, w, h;

   elm_widget_focus_highlight_geometry_get(target, &x, &y, &w, &h);

   evas_object_move(obj, x, y);
   evas_object_resize(obj, w, h);

   if (!_elm_config->focus_highlight_clip_disable)
     {
        clip = evas_object_clip_get(target);
        if (clip) evas_object_clip_set(obj, clip);
     }

   edje_object_signal_emit(obj, "elm,state,anim,stop", "elm");
}

static void
_elm_win_focus_prev_target_del(void *data,
                               Evas *e EINA_UNUSED,
                               Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);
   sd->focus_highlight.prev.target = NULL;
}

static void
_elm_win_focus_highlight_reconfigure_job(void *data)
{
   ELM_WIN_DATA_GET(data, sd);
   Evas_Object *target = sd->focus_highlight.cur.target;
   Evas_Object *previous = sd->focus_highlight.prev.target;
   Evas_Object *fobj = sd->focus_highlight.fobj;
   Eina_Bool visible_changed;
   Eina_Bool common_visible;
   const char *sig = NULL;
   const char *focus_style_target = NULL;
   const char *focus_style_previous = NULL;

   _elm_win_focus_highlight_reconfigure_job_stop(sd);

   visible_changed = (sd->focus_highlight.cur.visible !=
                      sd->focus_highlight.prev.visible);

   if ((target == previous) && (!visible_changed) &&
       (!sd->focus_highlight.geometry_changed) &&
       (!sd->focus_highlight.theme_changed))
     return;

   if (previous)
     {
        evas_object_event_callback_del_full
           (previous, EVAS_CALLBACK_DEL, _elm_win_focus_prev_target_del, data);
        if (sd->focus_highlight.prev.in_theme)
          elm_widget_signal_emit
             (previous, "elm,action,focus_highlight,hide", "elm");
     }

   if (!target)
     common_visible = EINA_FALSE;
   else if (sd->focus_highlight.cur.in_theme)
     {
        common_visible = EINA_FALSE;
        if (sd->focus_highlight.cur.visible)
          sig = "elm,action,focus_highlight,show";
        else
          sig = "elm,action,focus_highlight,hide";
     }
   else
     common_visible = sd->focus_highlight.cur.visible;

   if (sig)
     elm_widget_signal_emit(target, sig, "elm");

   if ((!target) || (!common_visible) || (sd->focus_highlight.cur.in_theme))
     {
        if (target)
          _elm_win_focus_highlight_simple_setup(sd, fobj);
        goto the_end;
     }

   if (previous)
     focus_style_previous = elm_widget_focus_highlight_style_get(previous);
   focus_style_target = elm_widget_focus_highlight_style_get(target);

   if (sd->focus_highlight.theme_changed ||
       (focus_style_target != focus_style_previous))
     {
        const char *str;

        if (focus_style_target)
          str = focus_style_target;
        else if (sd->focus_highlight.style)
          str = sd->focus_highlight.style;
        else
          str = "default";

        elm_widget_theme_object_set
          (sd->obj, fobj, "focus_highlight", "top", str);
        sd->focus_highlight.theme_changed = EINA_FALSE;

        if ((sd->focus_highlight.animate) || (sd->focus_highlight.auto_animate))
          {
             str = edje_object_data_get(sd->focus_highlight.fobj, "animate");
             sd->focus_highlight.animate_supported = ((str) && (!strcmp(str, "on")));
          }
        else
          sd->focus_highlight.animate_supported = EINA_FALSE;
     }

   if ((sd->focus_highlight.animate_supported) && (previous) &&
       (!sd->focus_highlight.prev.in_theme))
     _elm_win_focus_highlight_anim_setup(sd, fobj);
   else
     _elm_win_focus_highlight_simple_setup(sd, fobj);
   evas_object_raise(fobj);

the_end:
   _elm_win_focus_highlight_visible_set(sd, common_visible);
   sd->focus_highlight.geometry_changed = EINA_FALSE;
   sd->focus_highlight.prev = sd->focus_highlight.cur;
   if (sd->focus_highlight.prev.target)
     {
        evas_object_event_callback_add
              (sd->focus_highlight.prev.target,
               EVAS_CALLBACK_DEL, _elm_win_focus_prev_target_del, data);
     }
}

static void
_elm_win_focus_highlight_reconfigure_job_start(Efl_Ui_Win_Data *sd)
{
   ecore_job_del(sd->focus_highlight.reconf_job);

   sd->focus_highlight.reconf_job = ecore_job_add(
       _elm_win_focus_highlight_reconfigure_job, sd->obj);
}

static void
_elm_win_focus_in(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   Evas_Object *obj;

   if ((!sd) || (sd->modal_count)) return;

   _elm_win_throttle_ok = EINA_TRUE;
   obj = sd->obj;

   _elm_widget_top_win_focused_set(obj, EINA_TRUE);
   if (sd->type != ELM_WIN_FAKE)
     {
        if (!elm_widget_focus_order_get(obj))
          {
             elm_widget_focus_steal(obj, NULL);
          }
        else
          {
             Evas_Object *newest = NULL;
             unsigned int newest_focus_order = 0;

             newest = elm_widget_newest_focus_order_get
                (obj, &newest_focus_order, EINA_TRUE);
             if (newest &&
                 (_elm_widget_onscreen_is(newest) || (newest == obj)))
               elm_widget_focus_restore(obj);
             else
               evas_object_focus_set(obj, EINA_TRUE);
          }
     }
   // FIXME: the event is deprecated but still in use.
   // Has to be removed in EFL2.0
   evas_object_smart_callback_call(obj, SIG_FOCUS_IN, NULL);
   sd->focus_highlight.cur.visible = EINA_TRUE;
   _elm_win_focus_highlight_reconfigure_job_start(sd);
   _elm_win_frame_style_update(sd, 0, 1);

   if (_elm_config->atspi_mode)
     {
        elm_interface_atspi_window_activated_signal_emit(obj);
        elm_interface_atspi_accessible_state_changed_signal_emit(obj, ELM_ATSPI_STATE_ACTIVE, EINA_TRUE);
     }

   /* do nothing */
   /* else if (sd->img_obj) */
   /*   { */
   /*   } */
}

static void
_elm_win_focus_out(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   Evas_Object *obj;

   if (!sd) return;

   obj = sd->obj;

   _elm_widget_top_win_focused_set(obj, EINA_FALSE);
   // FIXME: the event is deprecated but still in use.
   // Has to be removed in EFL2.0
   evas_object_smart_callback_call(obj, SIG_FOCUS_OUT, NULL);
   sd->focus_highlight.cur.visible = EINA_FALSE;
   _elm_win_focus_highlight_reconfigure_job_start(sd);
   if (!sd->resizing)
     _elm_win_frame_style_update(sd, 0, 1);

   /* access */
   _elm_access_object_highlight_disable(evas_object_evas_get(obj));

   if (_elm_config->atspi_mode)
     {
        elm_interface_atspi_window_deactivated_signal_emit(obj);
        elm_interface_atspi_accessible_state_changed_signal_emit(obj, ELM_ATSPI_STATE_ACTIVE, EINA_FALSE);
     }

   /* do nothing */
   /* if (sd->img_obj) */
   /*   { */
   /*   } */
}

static void
_elm_win_available_profiles_del(Efl_Ui_Win_Data *sd)
{
   if (!sd->profile.available_list) return;

   unsigned int i;
   for (i = 0; i < sd->profile.count; i++)
     ELM_SAFE_FREE(sd->profile.available_list[i], eina_stringshare_del);
   sd->profile.count = 0;
   ELM_SAFE_FREE(sd->profile.available_list, free);
}

static void
_elm_win_profile_del(Efl_Ui_Win_Data *sd)
{
   ELM_SAFE_FREE(sd->profile.name, eina_stringshare_del);
}

static Eina_Bool
_internal_elm_win_profile_set(Efl_Ui_Win_Data *sd, const char *profile)
{
   Eina_Bool changed = EINA_FALSE;
   if (profile)
     {
        if (sd->profile.name)
          {
             if (strcmp(sd->profile.name, profile) != 0)
               {
                  eina_stringshare_replace(&(sd->profile.name), profile);
                  changed = EINA_TRUE;
               }
          }
        else
          {
             sd->profile.name = eina_stringshare_add(profile);
             changed = EINA_TRUE;
          }
     }
   else
     _elm_win_profile_del(sd);

   return changed;
}

static void
_elm_win_profile_update(Efl_Ui_Win_Data *sd)
{
   if (getenv("ELM_PROFILE")) return;

   if (sd->profile.available_list)
     {
        Eina_Bool found = EINA_FALSE;
        if (sd->profile.name)
          {
             unsigned int i;
             for (i = 0; i < sd->profile.count; i++)
               {
                  if (!strcmp(sd->profile.name,
                              sd->profile.available_list[i]))
                    {
                       found = EINA_TRUE;
                       break;
                    }
               }
          }

        /* If current profile is not present in an available profiles,
         * change current profile to the 1st element of an array.
         */
        if (!found)
          _internal_elm_win_profile_set(sd, sd->profile.available_list[0]);
     }

   _config_profile_lock = EINA_TRUE;
   _elm_config_profile_set(sd->profile.name);

   /* update sub ee */
   Ecore_Evas *ee2;
   Eina_List *sub, *l = NULL;

   sub = ecore_evas_sub_ecore_evas_list_get(sd->ee);
   EINA_LIST_FOREACH(sub, l, ee2)
     ecore_evas_window_profile_set(ee2, sd->profile.name);

   efl_event_callback_legacy_call(sd->obj, EFL_UI_WIN_EVENT_PROFILE_CHANGED, NULL);
}

static inline void
_elm_win_opaque_dirty(Efl_Ui_Win_Data *sd)
{
#ifdef HAVE_ELEMENTARY_WL2
   sd->wl.opaque_dirty = 1;
#else
   (void)sd;
#endif
}

static void
_elm_win_opaque_update(Efl_Ui_Win_Data *sd, Eina_Bool force_alpha)
{
#ifdef HAVE_ELEMENTARY_WL2
   int ox, oy, ow, oh;
   Eina_Bool alpha;
   Ecore_Evas_Engine_Wl_Data *wdata;

   if (!sd->wl.win) return;
   if (!sd->shown) return;

   wdata = sd->ee->engine.data;
   alpha = ecore_evas_alpha_get(sd->ee) || force_alpha;
   if (sd->fullscreen || !sd->frame_obj)
     {
        ecore_evas_geometry_get(sd->ee, NULL, NULL, &ow, &oh);
        if (!alpha)
          ecore_wl2_window_opaque_region_set(sd->wl.win, 0, 0, ow, oh);
        else
          ecore_wl2_window_opaque_region_set(sd->wl.win, 0, 0, 0, 0);
        wdata->content.x = wdata->content.y = 0;
        wdata->content.w = ow;
        wdata->content.h = oh;
        ecore_wl2_window_geometry_set(sd->wl.win, 0, 0, ow, oh);
        ecore_wl2_window_input_region_set(sd->wl.win, 0, 0, ow, oh);
        return;
     }

   edje_object_part_geometry_get(sd->frame_obj, "elm.spacer.opaque",
                                 &ox, &oy, &ow, &oh);
   edje_object_part_geometry_get(sd->frame_obj, "elm.swallow.client",
                                 &wdata->content.x, &wdata->content.y,
                                 &wdata->content.w, &wdata->content.h);
   if (!alpha)
     ecore_wl2_window_opaque_region_set(sd->wl.win, ox, oy, ow, oh);
   else
     ecore_wl2_window_opaque_region_set(sd->wl.win, 0, 0, 0, 0);

   /* FIXME: Replace with call to ecore_evas_shadow_geometry_set(). */
   ecore_wl2_window_geometry_set(sd->wl.win, ox, oy, ow, oh);
   ecore_wl2_window_input_region_set(sd->wl.win, ox, oy, ow, oh);
#else
   (void)sd;
   (void)force_alpha;
#endif
}

static inline void
_elm_win_frame_geometry_adjust(Efl_Ui_Win_Data *sd)
{
   int l = 0, t = 0, r = 0, b = 0;

   if (sd->frame_obj && sd->csd.need && !sd->fullscreen)
     {
        int fw, fh, ox, oy, ow, oh;
        evas_object_geometry_get(sd->frame_obj, NULL, NULL, &fw, &fh);
        edje_object_part_geometry_get(sd->frame_obj, "elm.spacer.opaque",
                                      &ox, &oy, &ow, &oh);
        l = ox;
        t = oy;
        r = fw - ow - l;
        b = fh - oh - t;
     }
   ecore_evas_shadow_geometry_set(sd->ee, l, r, t, b);
}

static inline Eina_Bool
_elm_win_framespace_set(Efl_Ui_Win_Data *sd, int x, int y, int w, int h)
{
   int fx, fy, fw, fh;

   evas_output_framespace_get(sd->evas, &fx, &fy, &fw, &fh);
   evas_output_framespace_set(sd->evas, x, y, w, h);

   // return true if framespace geometry changed
   return ((fx != x) || (fy != y) || (fw != w) || (fh != h));
}

static void
_elm_win_frame_obj_update(Efl_Ui_Win_Data *sd)
{
   int ox, oy, ow, oh;
   int cx, cy, cw, ch;

   if (!sd->frame_obj) return;
   _elm_win_opaque_dirty(sd);
   _elm_win_frame_geometry_adjust(sd);
   evas_object_geometry_get(sd->frame_obj, &ox, &oy, &ow, &oh);
   edje_object_part_geometry_get(sd->frame_obj, "elm.spacer.content", &cx, &cy, &cw, &ch);
   if (_elm_win_framespace_set(sd, cx, cy, ow - cw, oh - ch))
     _elm_win_resize_objects_eval(sd->obj);
}

static void
_elm_win_state_change(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   Evas_Object *obj;
   Eina_Bool ch_withdrawn = EINA_FALSE;
   Eina_Bool ch_sticky = EINA_FALSE;
   Eina_Bool ch_iconified = EINA_FALSE;
   Eina_Bool ch_fullscreen = EINA_FALSE;
   Eina_Bool ch_maximized = EINA_FALSE;
   Eina_Bool ch_profile = EINA_FALSE;
   Eina_Bool ch_wm_rotation = EINA_FALSE;
   const char *profile;

   if (!sd) return;

   obj = sd->obj;

   if (sd->withdrawn != ecore_evas_withdrawn_get(sd->ee))
     {
        sd->withdrawn = ecore_evas_withdrawn_get(sd->ee);
        ch_withdrawn = EINA_TRUE;
     }
   if (sd->sticky != ecore_evas_sticky_get(sd->ee))
     {
        sd->sticky = ecore_evas_sticky_get(sd->ee);
        ch_sticky = EINA_TRUE;
     }
   if (sd->iconified != ecore_evas_iconified_get(sd->ee))
     {
        sd->iconified = ecore_evas_iconified_get(sd->ee);
        ch_iconified = EINA_TRUE;
     }
   if (sd->fullscreen != ecore_evas_fullscreen_get(sd->ee))
     {
        sd->fullscreen = ecore_evas_fullscreen_get(sd->ee);
        ch_fullscreen = EINA_TRUE;
     }
   if (sd->maximized != ecore_evas_maximized_get(sd->ee))
     {
        sd->maximized = ecore_evas_maximized_get(sd->ee);
        ch_maximized = EINA_TRUE;
     }

   if (ecore_evas_window_profile_supported_get(sd->ee))
     {
        profile = ecore_evas_window_profile_get(sd->ee);
        ch_profile = _internal_elm_win_profile_set(sd, profile);
     }

   if (sd->wm_rot.use)
     {
        if (sd->rot != ecore_evas_rotation_get(sd->ee))
          {
             sd->rot = ecore_evas_rotation_get(sd->ee);
             ch_wm_rotation = EINA_TRUE;
          }
     }

   _elm_win_state_eval_queue();

   if ((ch_withdrawn) || (ch_iconified))
     {
        if (sd->withdrawn)
          efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_WITHDRAWN, NULL);
        else if (sd->iconified)
          {
             efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_ICONIFIED, NULL);
             if (_elm_config->atspi_mode)
               elm_interface_atspi_window_minimized_signal_emit(obj);
          }
        else
          {
             efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_NORMAL, NULL);
             if (_elm_config->atspi_mode)
               elm_interface_atspi_window_restored_signal_emit(obj);
          }
     }
   if (ch_sticky)
     {
        if (sd->sticky)
          efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_STICK, NULL);
        else
          efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_UNSTICK, NULL);
     }
   if (ch_fullscreen)
     {
        _elm_win_frame_style_update(sd, 0, 1);
        if (sd->fullscreen)
          {
             _elm_win_opaque_dirty(sd);
             efl_event_callback_legacy_call
               (obj, EFL_UI_WIN_EVENT_FULLSCREEN, NULL);
          }
        else
          {
             efl_event_callback_legacy_call
               (obj, EFL_UI_WIN_EVENT_UNFULLSCREEN, NULL);
          }
     }
   if (ch_maximized)
     {
        _elm_win_frame_style_update(sd, 0, 1);
        if (sd->maximized)
          {
             efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_MAXIMIZED, NULL);
             if (_elm_config->atspi_mode)
               elm_interface_atspi_window_maximized_signal_emit(obj);
          }
        else
          {
             efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_UNMAXIMIZED, NULL);
             if (_elm_config->atspi_mode)
               elm_interface_atspi_window_restored_signal_emit(obj);
          }
     }
   if (ch_profile)
     {
        _elm_win_profile_update(sd);
     }
   if (ch_wm_rotation)
     {
        efl_gfx_size_hint_restricted_min_set(obj, -1, -1);
        efl_gfx_size_hint_max_set(obj, -1, -1);
#ifdef HAVE_ELEMENTARY_X
        _elm_win_xwin_update(sd);
#endif
        elm_widget_orientation_set(obj, sd->rot);
        efl_event_callback_legacy_call
          (obj, EFL_UI_WIN_EVENT_ROTATION_CHANGED, NULL);
        efl_event_callback_legacy_call
          (obj, EFL_UI_WIN_EVENT_WM_ROTATION_CHANGED, NULL);
     }
}

EOLIAN static Eina_Bool
_efl_ui_win_elm_widget_focus_next_manager_is(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *_pd EINA_UNUSED)
{
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_win_elm_widget_focus_next(Eo *obj, Efl_Ui_Win_Data *_pd EINA_UNUSED, Elm_Focus_Direction dir, Evas_Object **next, Elm_Object_Item **next_item)
{
   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd, EINA_FALSE);

   const Eina_List *items;
   void *(*list_data_get)(const Eina_List *list);

   /* Focus chain */
   if (wd->subobjs)
     {
        if (!(items = elm_widget_focus_custom_chain_get(obj)))
          {
             items = wd->subobjs;
             if (!items)
               return EINA_FALSE;
          }
        list_data_get = eina_list_data_get;

        elm_widget_focus_list_next_get(obj, items, list_data_get, dir, next, next_item);

        if (*next) return EINA_TRUE;
     }
   *next = (Evas_Object *)obj;
   return EINA_FALSE;
}

EOLIAN static Eina_Bool
_efl_ui_win_elm_widget_focus_direction_manager_is(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *_pd EINA_UNUSED)
{
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_win_elm_widget_focus_direction(Eo *obj, Efl_Ui_Win_Data *_pd EINA_UNUSED, const Evas_Object *base, double degree, Evas_Object **direction, Elm_Object_Item **direction_item, double *weight)
{
   const Eina_List *items;
   void *(*list_data_get)(const Eina_List *list);

   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd, EINA_FALSE);

   /* Focus chain */
   if (wd->subobjs)
     {
        if (!(items = elm_widget_focus_custom_chain_get(obj)))
          items = wd->subobjs;

        list_data_get = eina_list_data_get;

        return elm_widget_focus_list_direction_get
                 (obj, base, items, list_data_get, degree, direction, direction_item, weight);
     }

   return EINA_FALSE;
}

EOLIAN static Eina_Bool
_efl_ui_win_elm_widget_on_focus(Eo *obj, Efl_Ui_Win_Data *sd, Elm_Object_Item *item EINA_UNUSED)
{
   Eina_Bool int_ret = EINA_FALSE;
   int_ret = elm_obj_widget_on_focus(efl_super(obj, MY_CLASS), NULL);
   if (!int_ret) return EINA_TRUE;

   if (sd->img_obj)
     evas_object_focus_set(sd->img_obj, elm_widget_focus_get(obj));
   else
     evas_object_focus_set(obj, elm_widget_focus_get(obj));

   return EINA_TRUE;
}

static Eina_Bool
_key_action_return(Evas_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   return EINA_FALSE;
}

static Eina_Bool
_key_action_move(Evas_Object *obj, const char *params)
{
   const char *dir = params;

   _elm_widget_focus_auto_show(obj);
   if (!strcmp(dir, "previous"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_PREVIOUS);
   else if (!strcmp(dir, "next"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_NEXT);
   else if (!strcmp(dir, "left"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_LEFT);
   else if (!strcmp(dir, "right"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_RIGHT);
   else if (!strcmp(dir, "up"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_UP);
   else if (!strcmp(dir, "down"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_DOWN);
   else return EINA_FALSE;

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_win_elm_widget_widget_event(Eo *obj, Efl_Ui_Win_Data *_pd EINA_UNUSED, Evas_Object *src EINA_UNUSED, Evas_Callback_Type type, void *event_info)
{
   Evas_Event_Key_Down *ev = event_info;

   if (elm_widget_disabled_get(obj)) return EINA_FALSE;
   if (type != EVAS_CALLBACK_KEY_DOWN) return EINA_FALSE;

   if (!_elm_config_key_binding_call(obj, MY_CLASS_NAME_LEGACY, ev, key_actions))
     return EINA_FALSE;

   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   return EINA_TRUE;
}

/* forward events sent to evas to the window */
static void
_evas_event_key_cb(void *data, const Efl_Event *ev)
{
   Eo *win = data;
   Eo *evt = ev->info;
   Efl_Input_Key_Data *evdata;

   evdata = efl_data_scope_get(evt, EFL_INPUT_KEY_CLASS);
   if (!evdata || evdata->win_fed)
     return;

   // evas_callbacks will send the event to the focussed object (ie. this win)
   if (evas_focus_get(evas_object_evas_get(win)) == win)
     return;

   efl_event_callback_call(win, ev->desc, evt);
}

static void
_evas_event_pointer_cb(void *data, const Efl_Event *ev)
{
   Eo *win = data;
   Eo *evt = ev->info;
   Efl_Input_Pointer_Data *evdata;

   evdata = efl_data_scope_get(evt, EFL_INPUT_POINTER_CLASS);
   if (!evdata || evdata->win_fed)
     return;

   efl_event_callback_call(win, ev->desc, evt);
}

/* feed events from the window to evas - for fake inputs */
static void
_evas_event_key_feed_fake_cb(void *data, const Efl_Event *ev)
{
   Eo *evas = data;
   Efl_Input_Event *evt = ev->info;
   Efl_Input_Key_Data *evdata;

   if (!efl_input_fake_get(evt))
     return;

   evdata = efl_data_scope_get(evt, EFL_INPUT_KEY_CLASS);
   if (!evdata || evdata->win_fed)
     return;
   evdata->win_fed = EINA_TRUE;

   efl_event_callback_call(evas, ev->desc, evt);
   evdata->win_fed = EINA_FALSE;
   evdata->evas_done = EINA_FALSE;
}

static void
_evas_event_pointer_feed_fake_cb(void *data, const Efl_Event *ev)
{
   Eo *evas = data;
   Efl_Input_Event *evt = ev->info;
   Efl_Input_Pointer_Data *evdata;

   if (!efl_input_fake_get(evt))
     return;

   evdata = efl_data_scope_get(evt, EFL_INPUT_POINTER_CLASS);
   if (!evdata || evdata->win_fed)
     return;
   evdata->win_fed = EINA_TRUE;

   efl_event_callback_call(evas, ev->desc, evt);
   evdata->win_fed = EINA_FALSE;
   evdata->evas_done = EINA_FALSE;
}

EFL_CALLBACKS_ARRAY_DEFINE(_elm_win_evas_feed_fake_callbacks,
{ EFL_EVENT_POINTER_MOVE, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_POINTER_DOWN, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_POINTER_UP, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_POINTER_IN, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_POINTER_OUT, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_POINTER_CANCEL, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_POINTER_WHEEL, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_FINGER_MOVE, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_FINGER_DOWN, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_FINGER_UP, _evas_event_pointer_feed_fake_cb },
{ EFL_EVENT_KEY_DOWN, _evas_event_key_feed_fake_cb },
{ EFL_EVENT_KEY_UP, _evas_event_key_feed_fake_cb })

static void
_elm_win_evas_render_post(void *data,
                          Evas *e EINA_UNUSED,
                          Evas_Object *obj EINA_UNUSED,
                          void *event_info)
{
   Efl_Gfx_Event_Render_Post *ev = event_info;
   Eo *win = data;

   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_RENDER_POST, ev);
}

static void
_elm_win_evas_render_pre(void *data,
                          Evas *e EINA_UNUSED,
                          Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Eo *win = data;

   _elm_win_throttle_ok = EINA_TRUE;
   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_RENDER_PRE, NULL);
}

static void
_elm_win_evas_focus_in(void *data,
                       Evas *e EINA_UNUSED,
                       Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Eo *win = data;

   _elm_win_throttle_ok = EINA_TRUE;
   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_FOCUS_IN, NULL);
}

static void
_elm_win_evas_focus_out(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Eo *win = data;

   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_FOCUS_OUT, NULL);
}

static void
_elm_win_evas_object_focus_in(void *data,
                              Evas *e EINA_UNUSED,
                              Evas_Object *obj EINA_UNUSED,
                              void *event_info)
{
   Eo *object = event_info;
   Eo *win = data;

   _elm_win_throttle_ok = EINA_TRUE;
   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_OBJECT_FOCUS_IN, object);
}

static void
_elm_win_evas_object_focus_out(void *data,
                               Evas *e EINA_UNUSED,
                               Evas_Object *obj EINA_UNUSED,
                               void *event_info)
{
   Eo *object = event_info;
   Eo *win = data;

   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_OBJECT_FOCUS_OUT, object);
}

static void
_elm_win_evas_device_changed(void *data,
                             Evas *e EINA_UNUSED,
                             Evas_Object *obj EINA_UNUSED,
                             void *event_info)
{
   Eo *device = event_info;
   Eo *win = data;

   efl_event_callback_legacy_call(win, EFL_CANVAS_EVENT_DEVICE_CHANGED, device);
}

static void
_win_event_add_cb(void *data, const Efl_Event *ev)
{
   const Efl_Callback_Array_Item *array = ev->info;
   Efl_Ui_Win_Data *sd = data;
   Efl_Ui_Win *win = ev->object;
   int i;

   for (i = 0; array[i].desc; i++)
     {
        if (array[i].desc == EFL_EVENT_POINTER_MOVE)
          {
             if (!(sd->event_forward.pointer_move++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_DOWN)
          {
             if (!(sd->event_forward.pointer_down++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_UP)
          {
             if (!(sd->event_forward.pointer_up++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_IN)
          {
             if (!(sd->event_forward.pointer_in++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_OUT)
          {
             if (!(sd->event_forward.pointer_out++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_CANCEL)
          {
             if (!(sd->event_forward.pointer_cancel++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_WHEEL)
          {
             if (!(sd->event_forward.pointer_wheel++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_FINGER_MOVE)
          {
             if (!(sd->event_forward.finger_move++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_FINGER_DOWN)
          {
             if (!(sd->event_forward.finger_down++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_FINGER_UP)
          {
             if (!(sd->event_forward.finger_up++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_KEY_DOWN)
          {
             if (!(sd->event_forward.key_down++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_key_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_KEY_UP)
          {
             if (!(sd->event_forward.key_up++))
               efl_event_callback_add(sd->evas, array[i].desc,
                                     _evas_event_key_cb, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_RENDER_POST)
          {
             if (!(sd->event_forward.render_post++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_RENDER_POST,
                                              _elm_win_evas_render_post, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_RENDER_PRE)
          {
             if (!(sd->event_forward.render_pre++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_RENDER_PRE,
                                              _elm_win_evas_render_pre, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_FOCUS_IN)
          {
             if (!(sd->event_forward.focus_in++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_FOCUS_IN,
                                              _elm_win_evas_focus_in, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_FOCUS_OUT)
          {
             if (!(sd->event_forward.focus_out++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_FOCUS_OUT,
                                              _elm_win_evas_focus_out, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_OBJECT_FOCUS_IN)
          {
             if (!(sd->event_forward.object_focus_in++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_IN,
                                              _elm_win_evas_object_focus_in, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_OBJECT_FOCUS_OUT)
          {
             if (!(sd->event_forward.object_focus_out++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_OUT,
                                              _elm_win_evas_object_focus_out, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_DEVICE_CHANGED)
          {
             if (!(sd->event_forward.device_changed++))
               evas_object_event_callback_add(sd->evas, EVAS_CALLBACK_DEVICE_CHANGED,
                                              _elm_win_evas_device_changed, win);
          }
     }
}

static void
_win_event_del_cb(void *data, const Efl_Event *ev)
{
   const Efl_Callback_Array_Item *array = ev->info;
   Efl_Ui_Win_Data *sd = data;
   Efl_Ui_Win *win = ev->object;
   int i;

   for (i = 0; array[i].desc; i++)
     {
        if (array[i].desc == EFL_EVENT_POINTER_MOVE)
          {
             if (!(--sd->event_forward.pointer_move))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_DOWN)
          {
             if (!(--sd->event_forward.pointer_down))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_UP)
          {
             if (!(--sd->event_forward.pointer_up))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_IN)
          {
             if (!(--sd->event_forward.pointer_in))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_OUT)
          {
             if (!(--sd->event_forward.pointer_out))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_CANCEL)
          {
             if (!(--sd->event_forward.pointer_cancel))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_POINTER_WHEEL)
          {
             if (!(--sd->event_forward.pointer_wheel))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_FINGER_MOVE)
          {
             if (!(--sd->event_forward.finger_move))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_FINGER_DOWN)
          {
             if (!(--sd->event_forward.finger_down))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_FINGER_UP)
          {
             if (!(--sd->event_forward.finger_up))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_pointer_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_KEY_DOWN)
          {
             if (!(--sd->event_forward.key_down))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_key_cb, win);
          }
        else if (array[i].desc == EFL_EVENT_KEY_UP)
          {
             if (!(--sd->event_forward.key_up))
               efl_event_callback_del(sd->evas, array[i].desc,
                                     _evas_event_key_cb, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_RENDER_POST)
          {
             if (!(--sd->event_forward.render_post))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_RENDER_POST,
                                              _elm_win_evas_render_post, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_RENDER_PRE)
          {
             if (!(--sd->event_forward.render_pre))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_RENDER_PRE,
                                              _elm_win_evas_render_pre, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_FOCUS_IN)
          {
             if (!(--sd->event_forward.focus_in))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_FOCUS_IN,
                                              _elm_win_evas_focus_in, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_FOCUS_OUT)
          {
             if (!(--sd->event_forward.focus_out))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_FOCUS_OUT,
                                              _elm_win_evas_focus_out, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_OBJECT_FOCUS_IN)
          {
             if (!(--sd->event_forward.object_focus_in))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_IN,
                                              _elm_win_evas_object_focus_in, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_OBJECT_FOCUS_OUT)
          {
             if (!(--sd->event_forward.object_focus_out))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_OUT,
                                              _elm_win_evas_object_focus_out, win);
          }
        else if (array[i].desc == EFL_CANVAS_EVENT_DEVICE_CHANGED)
          {
             if (!(--sd->event_forward.device_changed))
               evas_object_event_callback_del_full(sd->evas, EVAS_CALLBACK_DEVICE_CHANGED,
                                              _elm_win_evas_device_changed, win);
          }
     }
}

static void
_elm_win_cb_mouse_up(void *data, const Efl_Event *ev EINA_UNUSED)
{
   DBG("Evas mouse up event");
   /*Currently wayland server didn't send mouse up event after resize the window*/
   Efl_Ui_Win_Data *sd = data;
   if(sd->resizing) sd->resizing = EINA_FALSE;
}

static void
_deferred_ecore_evas_free(void *data)
{
   ecore_evas_free(data);
   _elm_win_deferred_free--;
}

static inline Edje_Object *
_elm_win_modal_blocker_edje_get(Efl_Ui_Win_Data *sd)
{
   /* Legacy theme compatibility */
   const char *version = edje_object_data_get(sd->legacy.edje, "version");
   int v = version ? atoi(version) : 0;
   if (v < FRAME_OBJ_THEME_MIN_VERSION)
     {
        DBG("Detected legacy theme (<1.19) for modal window blocker.");
        return sd->legacy.edje;
     }
   return sd->frame_obj;
}

static void
_elm_win_modality_increment(Efl_Ui_Win_Data *modalsd)
{
   Efl_Ui_Win *current;
   Eina_List *l;

   EINA_LIST_FOREACH(_elm_win_list, l, current)
     {
        ELM_WIN_DATA_GET_OR_RETURN(current, cursd);
        if (modalsd != cursd)
          cursd->modal_count++;
        if (cursd->modal_count > 0)
          {
             Edje_Object *ed = _elm_win_modal_blocker_edje_get(cursd);
             edje_object_signal_emit(ed, "elm,action,show_blocker", "elm");
             efl_event_callback_legacy_call
                   (cursd->main_menu, EFL_UI_WIN_EVENT_ELM_ACTION_BLOCK_MENU, NULL);
             _elm_win_frame_style_update(cursd, 0, 1);
          }
     }
}

static void
_elm_win_modality_decrement(Efl_Ui_Win_Data *modalsd)
{
   Efl_Ui_Win *current;
   Eina_List *l;

   EINA_LIST_FOREACH(_elm_win_list, l, current)
     {
        ELM_WIN_DATA_GET_OR_RETURN(current, cursd);
        if ((modalsd != cursd) && (cursd->modal_count > 0))
          cursd->modal_count--;
        if (cursd->modal_count == 0)
          {
             Edje_Object *ed = _elm_win_modal_blocker_edje_get(cursd);
             edje_object_signal_emit(ed, "elm,action,hide_blocker", "elm");
             efl_event_callback_legacy_call
                   (cursd->main_menu, ELM_MENU_EVENT_ELM_ACTION_UNBLOCK_MENU, NULL);
             _elm_win_frame_style_update(cursd, 0, 1);
          }
     }
}

static void
_efl_ui_win_show(Eo *obj, Efl_Ui_Win_Data *sd)
{
   Eina_Bool do_eval = EINA_FALSE;

   sd->shown = EINA_TRUE;
   if (sd->modal_count)
     {
        /* FIXME FIXME FIXME
         * Ugly code flow: legacy code had an early return in smart_show, ie.
         * evas object show would be processed but smart object show would be
         * aborted. This super call tries to simulate that. */
        efl_gfx_visible_set(efl_super(obj, EFL_CANVAS_GROUP_CLASS), EINA_TRUE);
        return;
     }

   if ((sd->modal) && (!evas_object_visible_get(obj)))
     _elm_win_modality_increment(sd);

   if (!evas_object_visible_get(obj)) do_eval = EINA_TRUE;
   efl_gfx_visible_set(efl_super(obj, MY_CLASS), EINA_TRUE);

   if (sd->deferred_resize_job)
     _elm_win_resize_job(sd->obj);
   evas_smart_objects_calculate(evas_object_evas_get(obj));

   TRAP(sd, show);

   if (_elm_config->atspi_mode && sd->type != ELM_WIN_SOCKET_IMAGE)
     {
        Eo *root = elm_interface_atspi_accessible_root_get(ELM_INTERFACE_ATSPI_ACCESSIBLE_MIXIN);
        elm_interface_atspi_accessible_parent_set(obj, root);
        elm_interface_atspi_window_created_signal_emit(obj);
     }

   if (do_eval)
     {
        if (_elm_win_state_eval_timer)
          {
             ecore_timer_del(_elm_win_state_eval_timer);
             _elm_win_state_eval_timer = NULL;
          }
        _elm_win_state_eval(NULL);
     }
   if (sd->shot.info) _shot_handle(sd);
}

static void
_efl_ui_win_hide(Eo *obj, Efl_Ui_Win_Data *sd)
{
   if (sd->modal_count)
     {
        /* FIXME FIXME FIXME
         * Ugly code flow: legacy code had an early return in smart_show, ie.
         * evas object show would be processed but smart object show would be
         * aborted. This super call tries to simulate that. */
        efl_gfx_visible_set(efl_super(obj, EFL_CANVAS_GROUP_CLASS), EINA_FALSE);
        return;
     }

   _elm_win_state_eval_queue();

   if ((sd->modal) && (evas_object_visible_get(obj)))
     _elm_win_modality_decrement(sd);

   efl_gfx_visible_set(efl_super(obj, MY_CLASS), EINA_FALSE);
   TRAP(sd, hide);

   if (sd->frame_obj)
     {
        evas_object_hide(sd->frame_obj);
     }
   if (sd->img_obj)
     {
        evas_object_hide(sd->img_obj);
     }
   if (sd->pointer.obj)
     {
        evas_object_hide(sd->pointer.obj);
     }

#ifdef HAVE_ELEMENTARY_WL2
   if (sd->pointer.ee)
     {
        ecore_evas_hide(sd->pointer.ee);
     }
#endif

   if (_elm_config->atspi_mode)
     {
        elm_interface_atspi_window_destroyed_signal_emit(obj);
        elm_interface_atspi_accessible_parent_set(obj, NULL);
     }

   if (_elm_win_policy_quit_triggered(obj))
     _elm_win_flush_cache_and_exit(obj);
}

EOLIAN static void
_efl_ui_win_efl_gfx_visible_set(Eo *obj, Efl_Ui_Win_Data *sd, Eina_Bool vis)
{
   if (_evas_object_intercept_call(obj, EVAS_OBJECT_INTERCEPT_CB_VISIBLE, 0, vis))
     return;

   if (vis) _efl_ui_win_show(obj, sd);
   else _efl_ui_win_hide(obj, sd);
}

EOLIAN static void
_efl_ui_win_efl_input_interface_pointer_xy_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int *x, int *y)
{
   evas_pointer_canvas_xy_get(sd->evas, x, y);
}

EOLIAN static Eina_Bool
_efl_ui_win_efl_input_interface_pointer_inside_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return evas_pointer_inside_get(sd->evas);
}

/* multi touch support */
static Eina_Bool
_input_pointer_iterator_next(Input_Pointer_Iterator *it, void **data)
{
   Eo *sub;

   if (!eina_iterator_next(it->real_iterator, (void **) &sub))
     return EINA_FALSE;

   if (data) *data = sub;
   return EINA_TRUE;
}

static Eo *
_input_pointer_iterator_get_container(Input_Pointer_Iterator *it)
{
   return (Eo *) it->object;
}

static void
_input_pointer_iterator_free(Input_Pointer_Iterator *it)
{
   Efl_Input_Pointer *ptr;

   EINA_LIST_FREE(it->list, ptr)
     efl_del(ptr);
   eina_iterator_free(it->real_iterator);
   free(it);
}

EOLIAN static Eina_Iterator *
_efl_ui_win_efl_input_interface_pointer_iterate(const Eo *obj, Efl_Ui_Win_Data *sd,
                                                Eina_Bool hover EINA_UNUSED)
{
   Input_Pointer_Iterator *it;
   Eina_List *list = NULL;
   int i, cnt;

   // Note: "hover" is here as a possible extension to this API. At the moment
   // I don't have any device that could track the position of hovering fingers
   // and Evas also wouldn't track those.

   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd, NULL);

   cnt = evas_canvas_touch_point_list_count(sd->evas);
   if (!cnt) return NULL;

   it = calloc(1, sizeof(*it));
   if (!it) return NULL;

   EINA_MAGIC_SET(&it->iterator, EINA_MAGIC_ITERATOR);

   for (i = 0; i < cnt; i++)
     {
        Efl_Input_Pointer_Data *ptrdata;
        Evas_Touch_Point_State state;
        Efl_Input_Pointer *ptr;
        double x, y;

        ptr = efl_input_instance_get(EFL_INPUT_POINTER_CLASS, (Eo *) obj, (void **) &ptrdata);
        if (!ptrdata) break;

        ptrdata->tool = evas_canvas_touch_point_list_nth_id_get(sd->evas, i);
        _efl_input_value_mark(ptrdata, EFL_INPUT_VALUE_TOOL);

        // Note that "still" maps to "down" here.
        state = evas_canvas_touch_point_list_nth_state_get(sd->evas, i);
        switch (state)
          {
           case EVAS_TOUCH_POINT_DOWN:   ptrdata->action = EFL_POINTER_ACTION_DOWN; break;
           case EVAS_TOUCH_POINT_UP:     ptrdata->action = EFL_POINTER_ACTION_UP; break;
           case EVAS_TOUCH_POINT_MOVE:   ptrdata->action = EFL_POINTER_ACTION_MOVE; break;
           case EVAS_TOUCH_POINT_STILL:  ptrdata->action = EFL_POINTER_ACTION_DOWN; break;
           case EVAS_TOUCH_POINT_CANCEL: ptrdata->action = EFL_POINTER_ACTION_CANCEL; break;
           default:                      ptrdata->action = EFL_POINTER_ACTION_NONE; break;
          }

        evas_canvas_touch_point_list_nth_xy_get(sd->evas, i, &x, &y);
        _efl_input_value_mark(ptrdata, EFL_INPUT_VALUE_X);
        _efl_input_value_mark(ptrdata, EFL_INPUT_VALUE_Y);
        ptrdata->cur.x = x;
        ptrdata->cur.y = y;
        ptrdata->prev = ptrdata->cur;

        list = eina_list_append(list, ptr);
     }

   it->list = list;
   it->real_iterator = eina_list_iterator_new(it->list);
   it->iterator.version = EINA_ITERATOR_VERSION;
   it->iterator.next = FUNC_ITERATOR_NEXT(_input_pointer_iterator_next);
   it->iterator.get_container = FUNC_ITERATOR_GET_CONTAINER(_input_pointer_iterator_get_container);
   it->iterator.free = FUNC_ITERATOR_FREE(_input_pointer_iterator_free);
   it->object = obj;

   return &it->iterator;
}

EOLIAN static Eina_Bool
_efl_ui_win_efl_canvas_image_max_size_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int *maxw, int *maxh)
{
   return evas_image_max_size_get(sd->evas, maxw, maxh);
}

EOLIAN static void
_efl_ui_win_efl_canvas_smart_objects_calculate(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   evas_smart_objects_calculate(sd->evas);
}

EOLIAN static Eina_Iterator *
_efl_ui_win_efl_canvas_objects_at_xy_get(const Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int x, int y, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   Eina_List *objs = NULL;
   objs = evas_objects_at_xy_get(sd->evas, x, y, include_pass_events_objects, include_hidden_objects);
   return eina_list_iterator_new(objs);
}

EOLIAN static Efl_Gfx *
_efl_ui_win_efl_canvas_object_top_at_xy_get(const Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int x, int y, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   return evas_object_top_at_xy_get(sd->evas, x, y, include_pass_events_objects, include_hidden_objects);
}

EOLIAN static Eina_Iterator *
_efl_ui_win_efl_canvas_objects_in_rectangle_get(const Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int x, int y, int w, int h, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   Eina_List *objs = NULL;
   objs = evas_objects_in_rectangle_get(sd->evas, x, y, w, h, include_pass_events_objects, include_hidden_objects);
   return eina_list_iterator_new(objs);
}

EOLIAN static Efl_Gfx *
_efl_ui_win_efl_canvas_object_top_in_rectangle_get(const Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int x, int y, int w, int h, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   return evas_object_top_in_rectangle_get(sd->evas, x, y, w, h, include_pass_events_objects, include_hidden_objects);
}

static void
_elm_win_on_parent_del(void *data,
                       Evas *e EINA_UNUSED,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   if (obj == sd->parent) sd->parent = NULL;
}

static void
_elm_win_focus_target_move(void *data,
                           Evas *e EINA_UNUSED,
                           Evas_Object *obj EINA_UNUSED,
                           void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   sd->focus_highlight.geometry_changed = EINA_TRUE;
   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

static void
_elm_win_focus_target_resize(void *data,
                             Evas *e EINA_UNUSED,
                             Evas_Object *obj EINA_UNUSED,
                             void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   sd->focus_highlight.geometry_changed = EINA_TRUE;
   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

static void
_elm_win_focus_target_del(void *data,
                          Evas *e EINA_UNUSED,
                          Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   sd->focus_highlight.cur.target = NULL;

   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

static Evas_Object *
_elm_win_focus_target_get(Evas_Object *obj)
{
   Evas_Object *o = obj;

   do
     {
        if (elm_widget_is(o))
          {
             if (!elm_widget_highlight_ignore_get(o))
               break;
             o = elm_widget_parent_get(o);
             if (!o)
               o = evas_object_smart_parent_get(o);
          }
        else
          {
             o = elm_widget_parent_widget_get(o);
             if (!o)
               o = evas_object_smart_parent_get(o);
          }
     }
   while (o);

   return o;
}

static void
_elm_win_focus_target_callbacks_add(Efl_Ui_Win_Data *sd)
{
   Evas_Object *obj = sd->focus_highlight.cur.target;
   if (!obj) return;

   evas_object_event_callback_add
     (obj, EVAS_CALLBACK_MOVE, _elm_win_focus_target_move, sd->obj);
   evas_object_event_callback_add
     (obj, EVAS_CALLBACK_RESIZE, _elm_win_focus_target_resize, sd->obj);
}

static void
_elm_win_focus_target_callbacks_del(Efl_Ui_Win_Data *sd)
{
   Evas_Object *obj = sd->focus_highlight.cur.target;

   evas_object_event_callback_del_full
     (obj, EVAS_CALLBACK_MOVE, _elm_win_focus_target_move, sd->obj);
   evas_object_event_callback_del_full
     (obj, EVAS_CALLBACK_RESIZE, _elm_win_focus_target_resize, sd->obj);
}

static void
_elm_win_object_focus_in(void *data,
                         Evas *e EINA_UNUSED,
                         void *event_info)
{
   Evas_Object *obj = event_info, *target;
   ELM_WIN_DATA_GET(data, sd);

   if (sd->focus_highlight.cur.target == obj)
     return;

   target = _elm_win_focus_target_get(obj);
   sd->focus_highlight.cur.target = target;

   if (target)
     {
        if (elm_widget_highlight_in_theme_get(target))
          sd->focus_highlight.cur.in_theme = EINA_TRUE;
        else
          _elm_win_focus_target_callbacks_add(sd);
        evas_object_event_callback_add
              (target, EVAS_CALLBACK_DEL, _elm_win_focus_target_del, sd->obj);
     }

   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

static void
_elm_win_object_focus_out(void *data,
                          Evas *e EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   if (!sd->focus_highlight.cur.target)
     return;

   if (!sd->focus_highlight.cur.in_theme)
     _elm_win_focus_target_callbacks_del(sd);

   evas_object_event_callback_del_full
      (sd->focus_highlight.cur.target,
       EVAS_CALLBACK_DEL, _elm_win_focus_target_del, sd->obj);

   sd->focus_highlight.cur.target = NULL;
   sd->focus_highlight.cur.in_theme = EINA_FALSE;

   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

static void
_elm_win_focus_highlight_shutdown(Efl_Ui_Win_Data *sd)
{
   _elm_win_focus_highlight_reconfigure_job_stop(sd);
   if (sd->focus_highlight.cur.target)
     {
        elm_widget_signal_emit(sd->focus_highlight.cur.target,
                               "elm,action,focus_highlight,hide", "elm");
        _elm_win_focus_target_callbacks_del(sd);
        evas_object_event_callback_del_full
           (sd->focus_highlight.cur.target,
            EVAS_CALLBACK_DEL, _elm_win_focus_target_del, sd->obj);
        sd->focus_highlight.cur.target = NULL;
     }
   ELM_SAFE_FREE(sd->focus_highlight.fobj, evas_object_del);

   evas_event_callback_del_full
     (sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_IN,
     _elm_win_object_focus_in, sd->obj);
   evas_event_callback_del_full
     (sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_OUT,
     _elm_win_object_focus_out, sd->obj);
}

static void
_win_img_hide(void *data,
              Evas *e EINA_UNUSED,
              Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   elm_widget_focus_hide_handle(data);
}

static void
_win_img_mouse_up(void *data,
                  Evas *e EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED,
                  void *event_info)
{
   Evas_Event_Mouse_Up *ev = event_info;
   if (!(ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD))
     elm_widget_focus_mouse_up_handle(data);
}

static void
_win_img_focus_in(void *data,
                  Evas *e EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   elm_widget_focus_steal(data, NULL);
}

static void
_win_img_focus_out(void *data,
                   Evas *e EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   elm_widget_focused_object_clear(data);
}

static void
_elm_win_on_img_obj_del(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);
   _elm_win_img_callbacks_del(sd->obj, sd->img_obj);
   sd->img_obj = NULL;
}

static void
_elm_win_img_callbacks_del(Evas_Object *obj, Evas_Object *imgobj)
{
   if (!imgobj) return;
   evas_object_event_callback_del_full
     (imgobj, EVAS_CALLBACK_DEL, _elm_win_on_img_obj_del, obj);
   evas_object_event_callback_del_full
     (imgobj, EVAS_CALLBACK_HIDE, _win_img_hide, obj);
   evas_object_event_callback_del_full
     (imgobj, EVAS_CALLBACK_MOUSE_UP, _win_img_mouse_up, obj);
   evas_object_event_callback_del_full
     (imgobj, EVAS_CALLBACK_FOCUS_IN, _win_img_focus_in, obj);
   evas_object_event_callback_del_full
     (imgobj, EVAS_CALLBACK_FOCUS_OUT, _win_img_focus_out, obj);
}

EOLIAN static void
_efl_ui_win_efl_canvas_group_group_del(Eo *obj, Efl_Ui_Win_Data *sd)
{
   if ((sd->modal) && (evas_object_visible_get(obj)))
     _elm_win_modality_decrement(sd);

   if ((sd->modal) && (sd->modal_count > 0)) 
     ERR("Deleted modal win was blocked by another modal win which was created after creation of that win.");

   evas_object_event_callback_del_full(sd->legacy.edje,
                                       EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                       _elm_win_on_resize_obj_changed_size_hints,
                                       obj);

   efl_event_callback_del(obj, EFL_EVENT_CALLBACK_ADD, _win_event_add_cb, sd);
   efl_event_callback_del(obj, EFL_EVENT_CALLBACK_DEL, _win_event_del_cb, sd);
   efl_event_callback_array_del(obj, _elm_win_evas_feed_fake_callbacks(), sd->evas);

   efl_event_callback_del(sd->evas, EFL_EVENT_POINTER_UP, _elm_win_cb_mouse_up, sd);
   evas_object_del(sd->legacy.box);
   evas_object_del(sd->legacy.edje);

   /* NB: child deletion handled by parent's smart del */

   if ((sd->type != ELM_WIN_FAKE) && (trap) && (trap->del))
     trap->del(sd->trap_data, obj);

   if (sd->parent)
     {
        evas_object_event_callback_del_full
          (sd->parent, EVAS_CALLBACK_DEL, _elm_win_on_parent_del, obj);
        sd->parent = NULL;
     }

   if (sd->autodel_clear) *(sd->autodel_clear) = -1;

   if (_elm_config->atspi_mode)
     elm_interface_atspi_window_destroyed_signal_emit(obj);

   _elm_win_list = eina_list_remove(_elm_win_list, obj);
   _elm_win_count--;
   _elm_win_state_eval_queue();

   if (sd->ee)
     {
        ecore_evas_callback_delete_request_set(sd->ee, NULL);
        ecore_evas_callback_resize_set(sd->ee, NULL);
     }

   eina_stringshare_del(sd->shot.info);
   ecore_timer_del(sd->shot.timer);

#ifdef HAVE_ELEMENTARY_X
   ecore_event_handler_del(sd->x.client_message_handler);
   ecore_event_handler_del(sd->x.property_handler);
#endif
#ifdef HAVE_ELEMENTARY_WL2
   ecore_event_handler_del(sd->wl.configure_handler);
   if (sd->pointer.obj) evas_object_del(sd->pointer.obj);
   if (sd->pointer.ee) ecore_evas_free(sd->pointer.ee);
   sd->pointer.surf = NULL;
#endif

   if (sd->type == ELM_WIN_INLINED_IMAGE)
     {
        _elm_win_img_callbacks_del(obj, sd->img_obj);
        sd->img_obj = NULL;
     }
   else
     {
        if (sd->ee && (sd->type != ELM_WIN_FAKE))
          {
             ecore_job_add(_deferred_ecore_evas_free, sd->ee);
             _elm_win_deferred_free++;
          }
     }

   _elm_win_focus_highlight_shutdown(sd);
   eina_stringshare_del(sd->focus_highlight.style);

   eina_stringshare_del(sd->title);
   eina_stringshare_del(sd->icon_name);
   eina_stringshare_del(sd->role);
   eina_stringshare_del(sd->name);
   eina_stringshare_del(sd->accel_pref);
   eina_stringshare_del(sd->stack_id);
   eina_stringshare_del(sd->stack_master_id);
   evas_object_del(sd->icon);
   evas_object_del(sd->main_menu);

   sd->focus_highlight.style = NULL;
   sd->title = NULL;
   sd->icon_name = NULL;
   sd->role = NULL;
   sd->name = NULL;
   sd->icon = NULL;
   sd->main_menu = NULL;

   _elm_win_profile_del(sd);
   _elm_win_available_profiles_del(sd);

   free(sd->wm_rot.rots);
   sd->wm_rot.rots = NULL;

   /* Don't let callback in the air that point to sd */
   ecore_evas_callback_delete_request_set(sd->ee, NULL);
   ecore_evas_callback_resize_set(sd->ee, NULL);
   ecore_evas_callback_mouse_in_set(sd->ee, NULL);
   ecore_evas_callback_focus_in_set(sd->ee, NULL);
   ecore_evas_callback_focus_out_set(sd->ee, NULL);
   ecore_evas_callback_move_set(sd->ee, NULL);
   ecore_evas_callback_state_change_set(sd->ee, NULL);
   ecore_evas_callback_pre_render_set(sd->ee, NULL);

   efl_canvas_group_del(efl_super(obj, MY_CLASS));

   if (_elm_win_policy_quit_triggered(obj))
     {
        _elm_win_flush_cache_and_exit(obj);
     }
}

static void
_elm_win_obj_intercept_show(void *data,
                            Evas_Object *obj)
{
   ELM_WIN_DATA_GET(data, sd);

   /* FIXME: this intercept needs to be implemented in proper EO */

   // this is called to make sure all smart containers have calculated their
   // sizes BEFORE we show the window to make sure it initially appears at
   // our desired size (ie min size is known first)
   evas_smart_objects_calculate(evas_object_evas_get(obj));
   if (sd->frame_obj)
     {
        evas_object_show(sd->frame_obj);
     }
   if (sd->img_obj)
     {
        evas_object_show(sd->img_obj);
     }
   if (sd->pointer.obj) evas_object_show(sd->pointer.obj);

   evas_object_show(obj);
#ifdef ELEMENTARY_X
   if (sd->type == ELM_WIN_TOOLTIP)
     ecore_x_window_shape_input_rectangle_set(sd->x.xwin, 0, 0, 0, 0);
#endif
}

EOLIAN static void
_efl_ui_win_efl_gfx_position_set(Eo *obj, Efl_Ui_Win_Data *sd, Evas_Coord x, Evas_Coord y)
{
   if (_evas_object_intercept_call(obj, EVAS_OBJECT_INTERCEPT_CB_MOVE, 0, x, y))
     return;

   if (sd->img_obj)
     {
        if ((x != sd->screen.x) || (y != sd->screen.y))
          {
             sd->screen.x = x;
             sd->screen.y = y;
             efl_event_callback_legacy_call(obj, EFL_GFX_EVENT_MOVE, NULL);
          }
        goto super_skip;
     }
   else
     {
        if (!sd->response) TRAP(sd, move, x, y);
        if (!ecore_evas_override_get(sd->ee)) goto super_skip;
     }

   efl_gfx_position_set(efl_super(obj, MY_CLASS), x, y);

   if (ecore_evas_override_get(sd->ee))
     {
        sd->screen.x = x;
        sd->screen.y = y;
        efl_event_callback_legacy_call(obj, EFL_GFX_EVENT_MOVE, NULL);
     }
   if (sd->frame_obj)
     {
#ifdef HAVE_ELEMENTARY_WL2
        /* TODO */
        /* ecore_wl_window_update_location(sd->wl.win, x, y); */
#endif
        sd->screen.x = x;
        sd->screen.y = y;
     }
   if (sd->img_obj)
     {
        sd->screen.x = x;
        sd->screen.y = y;
     }

   return;

super_skip:
   /* FIXME FIXME FIXME
    * Ugly code flow: legacy code had an early return in smart_move, ie.
    * evas object move would be processed but smart object move would be
    * aborted. This super call tries to simulate that. */
   efl_gfx_position_set(efl_super(obj, EFL_CANVAS_GROUP_CLASS), x, y);
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_set(Eo *obj, Efl_Ui_Win_Data *sd, Evas_Coord w, Evas_Coord h)
{
   if (_evas_object_intercept_call(obj, EVAS_OBJECT_INTERCEPT_CB_RESIZE, 0, w, h))
     return;

   if (sd->img_obj)
     {
        if (sd->constrain)
          {
             int sw, sh;

             ecore_evas_screen_geometry_get(sd->ee, NULL, NULL, &sw, &sh);
             w = MIN(w, sw);
             h = MIN(h, sh);
          }
        if (w < 1) w = 1;
        if (h < 1) h = 1;

        evas_object_image_size_set(sd->img_obj, w, h);
     }

   _elm_win_frame_geometry_adjust(sd);
   if (!sd->response) TRAP(sd, resize, w, h);

   efl_gfx_size_set(efl_super(obj, MY_CLASS), w, h);
}

static void
_elm_win_delete_request(Ecore_Evas *ee)
{
   Efl_Ui_Win_Data *sd = _elm_win_associate_get(ee);
   Evas_Object *obj;

   if (!sd) return;

   obj = sd->obj;

   int autodel = sd->autodel;
   sd->autodel_clear = &autodel;
   evas_object_ref(obj);
   efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_DELETE_REQUEST, NULL);
   if (sd->autohide)
     evas_object_hide(obj);
   // FIXME: if above callback deletes - then the below will be invalid
   if (_elm_config->atspi_mode)
     elm_interface_atspi_window_destroyed_signal_emit(obj);
   if (autodel) evas_object_del(obj);
   else sd->autodel_clear = NULL;
   evas_object_unref(obj);
}

Ecore_X_Window
_elm_ee_xwin_get(const Ecore_Evas *ee)
{
#ifdef HAVE_ELEMENTARY_X
   const char *engine_name;
   if (!ee) return 0;

   engine_name = ecore_evas_engine_name_get(ee);
   if (EINA_UNLIKELY(!engine_name)) return 0;

   if (!strcmp(engine_name, ELM_SOFTWARE_X11))
     {
        return ecore_evas_software_x11_window_get(ee);
     }
   else if (!strcmp(engine_name, ELM_OPENGL_X11))
     {
        return ecore_evas_gl_x11_window_get(ee);
     }
#else
   (void)ee;
#endif
   return 0;
}

#ifdef HAVE_ELEMENTARY_X
static void
_internal_elm_win_xwindow_get(Efl_Ui_Win_Data *sd)
{
   Ecore_X_Window pwin = sd->x.xwin;
   sd->x.xwin = _elm_ee_xwin_get(sd->ee);
   if (sd->x.xwin != pwin)
     {
        char buf[128];

        snprintf(buf, sizeof(buf), "%x", sd->x.xwin);
        eina_stringshare_del(sd->stack_id);
        sd->stack_id = eina_stringshare_add(buf);
     }
}
#endif

Ecore_Wl2_Window *
_elm_ee_wlwin_get(const Ecore_Evas *ee)
{
#ifdef HAVE_ELEMENTARY_WL2
   const char *engine_name;

   if (!ee) return NULL;

   engine_name = ecore_evas_engine_name_get(ee);
   if (EINA_UNLIKELY(!engine_name)) return NULL;

   if ((!strcmp(engine_name, ELM_WAYLAND_SHM)) ||
       (!strcmp(engine_name, ELM_WAYLAND_EGL)))
     {
        return ecore_evas_wayland2_window_get(ee);
     }
#else
   (void)ee;
#endif
   return NULL;
}

#ifdef HAVE_ELEMENTARY_WL2
static void
_elm_win_wlwindow_get(Efl_Ui_Win_Data *sd)
{
   sd->wl.win = _elm_ee_wlwin_get(sd->ee);
}

void
_elm_win_wl_cursor_set(Evas_Object *obj, const char *cursor)
{
   ELM_WIN_DATA_GET(obj, sd);

   if (!sd) return;

   if (sd->pointer.obj)
     {
        Evas_Coord mw = 1, mh = 1, hx = 0, hy = 0;

        if (cursor)
          {
             if (!_elm_theme_object_set(sd->obj, sd->pointer.obj,
                                        "cursor", cursor, "default"))
               {
                  _elm_theme_object_set(sd->obj, sd->pointer.obj,
                                        "pointer", "base", "default");
               }
          }
        else
          _elm_theme_object_set(sd->obj, sd->pointer.obj,
                                "pointer", "base", "default");

        edje_object_size_min_get(sd->pointer.obj, &mw, &mh);
        edje_object_size_min_restricted_calc(sd->pointer.obj, &mw, &mh, mw, mh);
        if ((mw < 32) || (mh < 32))
          {
             mw = 32;
             mh = 32;
          }
        evas_object_move(sd->pointer.obj, 0, 0);
        evas_object_resize(sd->pointer.obj, mw, mh);
        edje_object_part_geometry_get(sd->pointer.obj,
                                      "elm.swallow.hotspot",
                                      &hx, &hy, NULL, NULL);
        sd->pointer.hot_x = hx;
        sd->pointer.hot_y = hy;

        ecore_evas_resize(sd->pointer.ee, mw, mh);
     }

   if ((sd->wl.win) && (sd->pointer.surf) && (sd->pointer.visible))
     ecore_wl2_window_pointer_set(sd->wl.win, sd->pointer.surf,
                                  sd->pointer.hot_x, sd->pointer.hot_y);
}
#endif

Ecore_Cocoa_Window *
_elm_ee_cocoa_win_get(const Ecore_Evas *ee)
{
#ifdef HAVE_ELEMENTARY_COCOA
   const char *engine_name;

   if (!ee) return NULL;

   engine_name = ecore_evas_engine_name_get(ee);
   if (EINA_UNLIKELY(!engine_name)) return NULL;

   if (!strcmp(engine_name, "opengl_cocoa") ||
       !strcmp(engine_name, "gl_cocoa"))
     return ecore_evas_cocoa_window_get(ee);
#else
   (void)ee;
#endif
   return NULL;
}

Ecore_Win32_Window *
_elm_ee_win32win_get(const Ecore_Evas *ee)
{
#ifdef HAVE_ELEMENTARY_WIN32
   const char *engine_name;

   if (!ee) return NULL;

   engine_name = ecore_evas_engine_name_get(ee);
   if (EINA_UNLIKELY(!engine_name)) return NULL;

   if ((!strcmp(engine_name, ELM_SOFTWARE_WIN32)) ||
       (!strcmp(engine_name, ELM_SOFTWARE_DDRAW)))
     {
        return ecore_evas_win32_window_get(ee);
     }
#else
   (void)ee;
#endif
   return NULL;
}

#ifdef HAVE_ELEMENTARY_COCOA
static void
_elm_win_cocoawindow_get(Efl_Ui_Win_Data *sd)
{
   sd->cocoa.win = _elm_ee_cocoa_win_get(sd->ee);
}
#endif

#ifdef HAVE_ELEMENTARY_WIN32
static void
_internal_elm_win_win32window_get(Efl_Ui_Win_Data *sd)
{
   sd->win32.win = _elm_ee_win32win_get(sd->ee);
}
#endif

#ifdef HAVE_ELEMENTARY_X
static void
_elm_win_xwin_update(Efl_Ui_Win_Data *sd)
{
   const char *s;

   if (sd->type == ELM_WIN_FAKE) return;
   _internal_elm_win_xwindow_get(sd);

   if (!sd->x.xwin) return;  /* nothing more to do */

   if (sd->stack_master_id)
     {
        Ecore_X_Window win = strtol(sd->stack_master_id, NULL, 16);
        if (win)
          {
             ecore_x_icccm_transient_for_set(sd->x.xwin, win);
             if (sd->stack_base)
               ecore_x_e_stack_type_set(sd->x.xwin, ECORE_X_STACK_BASE);
             else
               ecore_x_e_stack_type_set(sd->x.xwin, ECORE_X_STACK_STANDARD);
          }
     }
   else
     {
        if (sd->parent)
          {
             ELM_WIN_DATA_GET(sd->parent, sdp);
             if (sdp) ecore_x_icccm_transient_for_set(sd->x.xwin, sdp->x.xwin);
          }
     }

   s = sd->title;
   if (!s) s = _elm_appname;
   if (!s) s = "";
   if (sd->icon_name) s = sd->icon_name;
   ecore_x_icccm_icon_name_set(sd->x.xwin, s);
   ecore_x_netwm_icon_name_set(sd->x.xwin, s);

   s = sd->role;
   if (s) ecore_x_icccm_window_role_set(sd->x.xwin, s);

   // set window icon
   if (sd->icon)
     {
        Eo *image = NULL;

        if (efl_isa(sd->icon, EFL_CANVAS_IMAGE_INTERNAL_CLASS))
          image = sd->icon;

        if (image)
          {
             int w = 0, h = 0, stride, x, y;
             Eina_Bool unmap = EINA_FALSE;
             Eina_Rw_Slice sl = {};

             if (efl_isa(image, EFL_CANVAS_IMAGE_CLASS))
               {
                  unmap = EINA_TRUE;
                  efl_gfx_buffer_size_get(image, &w, &h);
                  efl_gfx_buffer_map(image, &sl, EFL_GFX_BUFFER_ACCESS_MODE_READ,
                                     0, 0, w, h, EFL_GFX_COLORSPACE_ARGB8888, 0,
                                     &stride);
               }
             else
               {
                  evas_object_image_size_get(image, &w, &h);
                  stride = evas_object_image_stride_get(image);
                  sl.mem = evas_object_image_data_get(image, EINA_FALSE);
               }

             if (sl.mem)
               {
                  Ecore_X_Icon ic;

                  ic.width = w;
                  ic.height = h;
                  if ((w > 0) && (h > 0) &&
                      (stride >= (int)(w * sizeof(unsigned int))))
                    {
                       if (stride == (int)(w * sizeof(unsigned int)))
                         {
                            ic.data = sl.mem;
                            ecore_x_netwm_icons_set(sd->x.xwin, &ic, 1);
                         }
                       else
                         {
                            ic.data = malloc(w * h * sizeof(unsigned int));
                            if (ic.data)
                              {
                                 unsigned char *p = sl.mem;
                                 unsigned int *p2 = ic.data;

                                 for (y = 0; y < h; y++)
                                   {
                                      for (x = 0; x < w; x++)
                                        {
                                           *p2 = *((unsigned int *)p);
                                           p += sizeof(unsigned int);
                                           p2++;
                                        }
                                      p += (stride - (w * sizeof(unsigned int)));
                                   }
                                 ecore_x_netwm_icons_set(sd->x.xwin, &ic, 1);
                                 free(ic.data);
                              }
                         }
                    }
                  if (unmap) efl_gfx_buffer_unmap(image, &sl);
                  else evas_object_image_data_set(image, sl.mem);
               }
          }
     }

   switch (sd->type)
     {
      case ELM_WIN_BASIC:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_NORMAL);
        break;

      case ELM_WIN_DIALOG_BASIC:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_DIALOG);
        break;

      case ELM_WIN_DESKTOP:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_DESKTOP);
        break;

      case ELM_WIN_DOCK:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_DOCK);
        break;

      case ELM_WIN_TOOLBAR:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_TOOLBAR);
        break;

      case ELM_WIN_MENU:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_MENU);
        break;

      case ELM_WIN_UTILITY:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_UTILITY);
        break;

      case ELM_WIN_SPLASH:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_SPLASH);
        break;

      case ELM_WIN_DROPDOWN_MENU:
        ecore_x_netwm_window_type_set
          (sd->x.xwin, ECORE_X_WINDOW_TYPE_DROPDOWN_MENU);
        break;

      case ELM_WIN_POPUP_MENU:
        ecore_x_netwm_window_type_set
          (sd->x.xwin, ECORE_X_WINDOW_TYPE_POPUP_MENU);
        break;

      case ELM_WIN_TOOLTIP:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_TOOLTIP);
        ecore_x_window_shape_input_rectangle_set(sd->x.xwin, 0, 0, 0, 0);
        break;

      case ELM_WIN_NOTIFICATION:
        ecore_x_netwm_window_type_set
          (sd->x.xwin, ECORE_X_WINDOW_TYPE_NOTIFICATION);
        break;

      case ELM_WIN_COMBO:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_COMBO);
        break;

      case ELM_WIN_DND:
        ecore_x_netwm_window_type_set(sd->x.xwin, ECORE_X_WINDOW_TYPE_DND);
        break;

      default:
        break;
     }
   ecore_x_e_virtual_keyboard_state_set
     (sd->x.xwin, (Ecore_X_Virtual_Keyboard_State)sd->kbdmode);
   if (sd->indmode == ELM_WIN_INDICATOR_SHOW)
     ecore_x_e_illume_indicator_state_set
       (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_STATE_ON);
   else if (sd->indmode == ELM_WIN_INDICATOR_HIDE)
     ecore_x_e_illume_indicator_state_set
       (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_STATE_OFF);

   if ((sd->wm_rot.count) && (sd->wm_rot.rots))
     ecore_evas_wm_rotation_available_rotations_set(sd->ee,
                                                    sd->wm_rot.rots,
                                                    sd->wm_rot.count);
   if (sd->wm_rot.preferred_rot != -1)
     ecore_evas_wm_rotation_preferred_rotation_set(sd->ee,
                                                   sd->wm_rot.preferred_rot);

#ifdef HAVE_ELEMENTARY_X
   if (sd->csd.need && sd->x.xwin)
     TRAP(sd, borderless_set, EINA_TRUE);
#endif
}

#endif

/**
  * @internal
  *
  * Resize the window according to window layout's min and weight.
  * If the window layout's weight is 0.0, the window max is limited to layout's
  * min size.
  *
  * This is called when the window layout's weight hint is changed or when the
  * window is rotated.
  *
  * @param obj window object
  */
static void
_elm_win_resize_objects_eval(Evas_Object *obj)
{
   Efl_Ui_Win_Data *sd = efl_data_scope_get(obj, MY_CLASS);
   Evas_Coord w, h, minw, minh, maxw, maxh, ow, oh;
   Eina_Bool unresizable;
   double wx, wy;

   efl_gfx_size_hint_combined_min_get(sd->legacy.edje, &minw, &minh);
   if (minw < 1) minw = 1;
   if (minh < 1) minh = 1;

   // If content has a weight, make resizable
   efl_gfx_size_hint_weight_get(sd->legacy.edje, &wx, &wy);

   // Content max hint is ignored
   maxw = sd->max_w;
   maxh = sd->max_h;

   // Compatibility hack (for E)
   if (sd->single_edje_content && !wx && !wy)
     wx = wy = 1;

   if (!wx) maxw = minw;
   else if (maxw < 1) maxw = 32767;
   if (!wy) maxh = minh;
   else if (maxh < 1) maxh = 32767;
   if (maxw < minw) maxw = minw;
   if (maxh < minh) maxh = minh;
   if (maxw > 32767) maxw = 32767;
   if (maxh > 32767) maxh = 32767;

   unresizable = ((minw == maxw) && (minh == maxh));
   if (sd->csd.need_unresizable != unresizable)
     {
        sd->csd.need_unresizable = unresizable;
        _elm_win_frame_style_update(sd, 0, 1);
     }

   if (sd->frame_obj)
     {
        int fw, fh;

        evas_output_framespace_get(sd->evas, NULL, NULL, &fw, &fh);
        minw += fw;
        minh += fh;
        maxw += fw;
        maxh += fh;
     }

   sd->tmp_updating_hints = 1;
   efl_gfx_size_hint_restricted_min_set(obj, minw, minh);
   efl_gfx_size_hint_max_set(obj, maxw, maxh);
   sd->tmp_updating_hints = 0;
   _elm_win_size_hints_update(obj, sd);

   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   w = ow;
   h = oh;
   if (w < minw) w = minw;
   if (h < minh) h = minh;
   if (w > maxw) w = maxw;
   if (h > maxh) h = maxh;
   //if ((w == ow) && (h == oh)) return;
   if (sd->img_obj) evas_object_resize(obj, w, h);
   else
     {
        _elm_win_frame_geometry_adjust(sd);
        if (!sd->response) TRAP(sd, resize, w, h);
     }
}

static void
_elm_win_on_resize_obj_changed_size_hints(void *data,
                                          Evas *e EINA_UNUSED,
                                          Evas_Object *obj EINA_UNUSED,
                                          void *event_info EINA_UNUSED)
{
   _elm_win_resize_objects_eval(data);
}

void
_elm_win_shutdown(void)
{
   while (_elm_win_list)
     {
        Eina_List *itr = _elm_win_list;
        evas_object_del(itr->data);
        if (_elm_win_list == itr)
          {
             _elm_win_list = eina_list_remove_list(_elm_win_list, _elm_win_list);
          }
     }
   ELM_SAFE_FREE(_elm_win_state_eval_timer, ecore_timer_del);
}

void
_elm_win_rescale(Elm_Theme *th,
                 Eina_Bool use_theme)
{
   const Eina_List *l;
   Evas_Object *obj;

   if (!use_theme)
     {
        EINA_LIST_FOREACH(_elm_win_list, l, obj)
          elm_widget_theme(obj);
     }
   else
     {
        EINA_LIST_FOREACH(_elm_win_list, l, obj)
          elm_widget_theme_specific(obj, th, EINA_FALSE);
     }
}

void
_elm_win_access(Eina_Bool is_access)
{
   Evas *evas;
   const Eina_List *l;
   Evas_Object *obj;
   Evas_Object *fobj;

   EINA_LIST_FOREACH(_elm_win_list, l, obj)
     {
        elm_widget_access(obj, is_access);

         /* floating orphan object. if there are A, B, C objects and user does
            as below, then there would be floating orphan objects.

              1. elm_object_content_set(layout, A);
              2. elm_object_content_set(layout, B);
              3. elm_object_content_set(layout, C);

            now, the object A and B are floating orphan objects */

        fobj = obj;
        for (;;)
          {
             fobj = evas_object_below_get(fobj);
             if (!fobj) break;

             if (elm_widget_is(fobj) && !elm_widget_parent_get(fobj))
               {
                  elm_widget_access(fobj, is_access);
               }
          }

        if (!is_access)
          {
             evas = evas_object_evas_get(obj);
            if (evas) _elm_access_object_highlight_disable(evas);
          }
     }
}

void
_elm_win_translate(void)
{
   const Eina_List *l;
   Evas_Object *obj;

   EINA_LIST_FOREACH(_elm_win_list, l, obj)
     elm_widget_translate(obj);
}

void
_elm_win_focus_reconfigure(void)
{
   const Eina_List *l;
   Evas_Object *obj;

   EINA_LIST_FOREACH(_elm_win_list, l, obj)
     elm_widget_focus_reconfigure(obj);
}

#ifdef HAVE_ELEMENTARY_X
static Eina_Bool
_elm_win_client_message(void *data,
                        int type EINA_UNUSED,
                        void *event)
{
   ELM_WIN_DATA_GET(data, sd);
   Ecore_X_Event_Client_Message *e = event;

   if (e->format != 32) return ECORE_CALLBACK_PASS_ON;
   if (e->message_type == ECORE_X_ATOM_E_COMP_FLUSH)
     {
        if ((unsigned int)e->data.l[0] == sd->x.xwin)
          {
             Evas *evas = evas_object_evas_get(sd->obj);
             if (evas)
               {
                  edje_file_cache_flush();
                  edje_collection_cache_flush();
                  evas_image_cache_flush(evas);
                  evas_font_cache_flush(evas);
               }
          }
     }
   else if (e->message_type == ECORE_X_ATOM_E_COMP_DUMP)
     {
        if ((unsigned int)e->data.l[0] == sd->x.xwin)
          {
             Evas *evas = evas_object_evas_get(sd->obj);
             if (evas)
               {
                  edje_file_cache_flush();
                  edje_collection_cache_flush();
                  evas_image_cache_flush(evas);
                  evas_font_cache_flush(evas);
                  evas_render_dump(evas);
               }
          }
     }
   else if (e->message_type == ECORE_X_ATOM_E_ILLUME_ACCESS_CONTROL)
     {
        if ((unsigned int)e->data.l[0] == sd->x.xwin)
          {
             if ((unsigned int)e->data.l[1] ==
                 ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_NEXT)
               {
                  // XXX: call right access func
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_PREV)
               {
                  // XXX: call right access func
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_ACTIVATE)
               {
                  _elm_access_highlight_object_activate
                    (sd->obj, ELM_ACTIVATE_DEFAULT);
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ)
               {
                  /* there would be better way to read highlight object */
                  Evas *evas;
                  evas = evas_object_evas_get(sd->obj);
                  if (!evas) return ECORE_CALLBACK_PASS_ON;

                  _elm_access_mouse_event_enabled_set(EINA_TRUE);

                  evas_event_feed_mouse_in(evas, 0, NULL);
                  evas_event_feed_mouse_move
                    (evas, e->data.l[2], e->data.l[3], 0, NULL);

                  _elm_access_mouse_event_enabled_set(EINA_FALSE);
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ_NEXT)
               {
                  _elm_access_highlight_cycle(sd->obj, ELM_FOCUS_NEXT);
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ_PREV)
               {
                  _elm_access_highlight_cycle(sd->obj, ELM_FOCUS_PREVIOUS);
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_UP)
               {
                  _elm_access_highlight_object_activate
                    (sd->obj, ELM_ACTIVATE_UP);
               }
             else if ((unsigned int)e->data.l[1] ==
                      ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_DOWN)
               {
                  _elm_access_highlight_object_activate
                    (sd->obj, ELM_ACTIVATE_DOWN);
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_elm_win_property_change(void *data,
                         int type EINA_UNUSED,
                         void *event)
{
   ELM_WIN_DATA_GET(data, sd);
   Ecore_X_Event_Window_Property *e = event;

   if (e->atom == ECORE_X_ATOM_E_ILLUME_INDICATOR_STATE)
     {
        if (e->win == sd->x.xwin)
          {
             sd->indmode = (Elm_Win_Indicator_Mode)ecore_x_e_illume_indicator_state_get(e->win);
             efl_event_callback_legacy_call
               (sd->obj, EFL_UI_WIN_EVENT_INDICATOR_PROP_CHANGED, NULL);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}
#endif

static void
_elm_win_focus_highlight_hide(void *data EINA_UNUSED,
                              Evas_Object *obj,
                              const char *emission EINA_UNUSED,
                              const char *source EINA_UNUSED)
{
   evas_object_hide(obj);
}

static void
_elm_win_focus_highlight_anim_end(void *data,
                                  Evas_Object *obj,
                                  const char *emission EINA_UNUSED,
                                  const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   _elm_win_focus_highlight_simple_setup(sd, obj);
}

static void
_elm_win_focus_highlight_init(Efl_Ui_Win_Data *sd)
{
   evas_event_callback_add(sd->evas, EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_IN,
                           _elm_win_object_focus_in, sd->obj);
   evas_event_callback_add(sd->evas,
                           EVAS_CALLBACK_CANVAS_OBJECT_FOCUS_OUT,
                           _elm_win_object_focus_out, sd->obj);

   sd->focus_highlight.cur.target = _elm_win_focus_target_get(evas_focus_get(sd->evas));
   if (sd->focus_highlight.cur.target)
     {
        if (elm_widget_highlight_in_theme_get(sd->focus_highlight.cur.target))
          sd->focus_highlight.cur.in_theme = EINA_TRUE;
        else
          _elm_win_focus_target_callbacks_add(sd);

        evas_object_event_callback_add
           (sd->focus_highlight.cur.target,
            EVAS_CALLBACK_DEL, _elm_win_focus_target_del, sd->obj);
     }

   sd->focus_highlight.prev.target = NULL;
   sd->focus_highlight.theme_changed = EINA_TRUE;
   if (!sd->focus_highlight.fobj)
     {
        sd->focus_highlight.fobj = edje_object_add(sd->evas);
        edje_object_signal_callback_add(sd->focus_highlight.fobj,
                                        "elm,action,focus,hide,end", "*",
                                        _elm_win_focus_highlight_hide, NULL);
        edje_object_signal_callback_add(sd->focus_highlight.fobj,
                                        "elm,action,focus,anim,end", "*",
                                        _elm_win_focus_highlight_anim_end, sd->obj);
     }

   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

typedef struct _resize_info {
   const char *source;
   const char *cursor;
   Efl_Ui_Win_Move_Resize_Mode mode;
   int wl_location;
#ifdef HAVE_ELEMENTARY_X
#define XDIR(d) , ECORE_X_NETWM_DIRECTION_##d
   Ecore_X_Netwm_Direction x_dir;
#else
# define XDIR(d)
#endif
} resize_info;

static const resize_info _resize_infos[8] = {
   { "elm.event.resize.t",  ELM_CURSOR_TOP_SIDE, EFL_UI_WIN_MOVE_RESIZE_TOP, 1 XDIR(SIZE_T) },
   { "elm.event.resize.b",  ELM_CURSOR_BOTTOM_SIDE, EFL_UI_WIN_MOVE_RESIZE_BOTTOM, 2 XDIR(SIZE_B) },
   { "elm.event.resize.l",  ELM_CURSOR_LEFT_SIDE, EFL_UI_WIN_MOVE_RESIZE_LEFT, 4 XDIR(SIZE_L) },
   { "elm.event.resize.r",  ELM_CURSOR_RIGHT_SIDE, EFL_UI_WIN_MOVE_RESIZE_RIGHT, 8 XDIR(SIZE_R) },
   { "elm.event.resize.tl", ELM_CURSOR_TOP_LEFT_CORNER, EFL_UI_WIN_MOVE_RESIZE_TOP | EFL_UI_WIN_MOVE_RESIZE_LEFT, 5 XDIR(SIZE_TL) },
   { "elm.event.resize.bl", ELM_CURSOR_BOTTOM_LEFT_CORNER, EFL_UI_WIN_MOVE_RESIZE_BOTTOM | EFL_UI_WIN_MOVE_RESIZE_LEFT, 6 XDIR(SIZE_BL) },
   { "elm.event.resize.br", ELM_CURSOR_BOTTOM_RIGHT_CORNER, EFL_UI_WIN_MOVE_RESIZE_BOTTOM | EFL_UI_WIN_MOVE_RESIZE_RIGHT, 10 XDIR(SIZE_BR) },
   { "elm.event.resize.tr", ELM_CURSOR_TOP_RIGHT_CORNER, EFL_UI_WIN_MOVE_RESIZE_TOP | EFL_UI_WIN_MOVE_RESIZE_RIGHT, 9 XDIR(SIZE_TR) },
};

static inline Efl_Ui_Win_Move_Resize_Mode
_move_resize_mode_rotate(int rotation, Efl_Ui_Win_Move_Resize_Mode mode)
{
   const Efl_Ui_Win_Move_Resize_Mode edges[4] = {
      EFL_UI_WIN_MOVE_RESIZE_TOP,    EFL_UI_WIN_MOVE_RESIZE_LEFT,
      EFL_UI_WIN_MOVE_RESIZE_BOTTOM, EFL_UI_WIN_MOVE_RESIZE_RIGHT
   };

   const Efl_Ui_Win_Move_Resize_Mode corners[4] = {
      EFL_UI_WIN_MOVE_RESIZE_TOP    | EFL_UI_WIN_MOVE_RESIZE_LEFT,
      EFL_UI_WIN_MOVE_RESIZE_BOTTOM | EFL_UI_WIN_MOVE_RESIZE_LEFT,
      EFL_UI_WIN_MOVE_RESIZE_BOTTOM | EFL_UI_WIN_MOVE_RESIZE_RIGHT,
      EFL_UI_WIN_MOVE_RESIZE_TOP    | EFL_UI_WIN_MOVE_RESIZE_RIGHT,
   };

   const int i = rotation / 90;
   int k;

   for (k = 0; k < 4; k++)
     if (mode == edges[k])
       return edges[(k + i) % 4];

   for (k = 0; k < 4; k++)
     if (mode == corners[k])
       return corners[(k + i) % 4];

   return EFL_UI_WIN_MOVE_RESIZE_MOVE;
}

static const resize_info *
_resize_info_get(int rotation, Efl_Ui_Win_Move_Resize_Mode mode)
{
   if (rotation)
     return _resize_info_get(0, _move_resize_mode_rotate(rotation, mode));

   for (size_t k = 0; k < EINA_C_ARRAY_LENGTH(_resize_infos); k++)
     {
        if (_resize_infos[k].mode == mode)
          return &_resize_infos[k];
     }

   return NULL;
}

static Efl_Ui_Win_Move_Resize_Mode
_move_resize_mode_get(const char *source)
{
   for (size_t k = 0; k < EINA_C_ARRAY_LENGTH(_resize_infos); k++)
     if (!strcmp(source, _resize_infos[k].source))
       return _resize_infos[k].mode;

   return EFL_UI_WIN_MOVE_RESIZE_MOVE;
}

static void
_elm_win_frame_obj_move(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Efl_Ui_Win_Data *sd;

   if (!(sd = data)) return;
   if (!sd->legacy.edje) return;

   _elm_win_frame_obj_update(sd);
}

static void
_elm_win_frame_obj_resize(void *data,
                          Evas *e EINA_UNUSED,
                          Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Efl_Ui_Win_Data *sd;

   if (!(sd = data)) return;
   if (!sd->legacy.edje) return;

   _elm_win_frame_obj_update(sd);
}

static void
_elm_win_frame_cb_resize_show(void *data,
                              Evas_Object *obj EINA_UNUSED,
                              const char *sig EINA_UNUSED,
                              const char *source)
{
   ELM_WIN_DATA_GET(data, sd);

   if (!sd) return;
   if (sd->resizing) return;

#ifdef HAVE_ELEMENTARY_WL2
   if (sd->pointer.obj)
     {
        Efl_Ui_Win_Move_Resize_Mode mode = _move_resize_mode_get(source);
        const resize_info *ri = _resize_info_get(sd->rot, mode);
        if (ri) _elm_theme_object_set(sd->obj, sd->pointer.obj, "pointer", "base", ri->cursor);
     }
#else
   (void)source;
#endif
}

static void
_elm_win_frame_cb_resize_hide(void *data,
                              Evas_Object *obj EINA_UNUSED,
                              const char *sig EINA_UNUSED,
                              const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   if (!sd) return;
   if (sd->resizing) return;

#ifdef HAVE_ELEMENTARY_WL2
   if (sd->pointer.obj)
     _elm_theme_object_set(sd->obj, sd->pointer.obj,
                           "pointer", "base", "default");
#endif
}

static inline Eina_Bool
_win_move_start(Efl_Ui_Win_Data *sd)
{
#ifdef HAVE_ELEMENTARY_WL2
   if (sd->wl.win)
     {
        ecore_wl2_window_pointer_set(sd->wl.win, NULL,
                                     sd->pointer.hot_x, sd->pointer.hot_y);

        /* Note: Not passing any X,Y position as those don't make sense, only
         * the compositor can really handle the window & pointer position. */
        ecore_evas_wayland_move(sd->ee, 0, 0);
        return EINA_TRUE;
     }
#endif

#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin)
     {
        int x, y;

        sd->resizing = EINA_TRUE;
        ecore_x_pointer_ungrab();
        ecore_x_pointer_root_xy_get(&x, &y);
        ecore_x_netwm_moveresize_request_send(sd->x.xwin, x, y,
                                              ECORE_X_NETWM_DIRECTION_MOVE, 1);
        return EINA_TRUE;
     }
#endif

   INF("Window move request not supported for this window!");
   return EINA_FALSE;
}

static Eina_Bool
_win_move_resize_start(Efl_Ui_Win_Data *sd, Efl_Ui_Win_Move_Resize_Mode mode)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, EINA_FALSE);
   const resize_info *ri;

   // 1. move_resize can only be started after mouse down event
   if (evas_event_down_count_get(sd->evas) <= 0)
     {
        ERR("move_resize_start can only be called when a pointer is pressed.");
        return EINA_FALSE;
     }

   // 2. check move_resize already started
   if (sd->resizing)
     {
        ERR("Window is already being resized.");
        return EINA_FALSE;
     }

   if (mode == EFL_UI_WIN_MOVE_RESIZE_MOVE)
     return _win_move_start(sd);

   ri = _resize_info_get(sd->rot, mode);
   if (!ri)
     {
        ERR("Unsupported move_resize mode %#x", (int) mode);
        return EINA_FALSE;
     }

   sd->resizing = EINA_TRUE;

#ifdef HAVE_ELEMENTARY_WL2
   if (sd->wl.win)
     {
        ecore_evas_wayland_resize(sd->ee, ri->wl_location);
        return EINA_TRUE;
     }
#endif

#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin)
     {
        int x, y;
        ecore_x_pointer_ungrab();
        ecore_x_pointer_root_xy_get(&x, &y);
        ecore_x_netwm_moveresize_request_send(sd->x.xwin, x, y, ri->x_dir, 1);
        return EINA_TRUE;
     }
#endif

   INF("Window resize request not supported for this window!");
   return EINA_FALSE;
}

static void
_elm_win_frame_cb_move_start(void *data,
                             Evas_Object *obj EINA_UNUSED,
                             const char *sig EINA_UNUSED,
                             const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET_OR_RETURN(data, sd);

   _win_move_resize_start(sd, EFL_UI_WIN_MOVE_RESIZE_MOVE);
}

static void
_elm_win_frame_cb_move_stop(void *data,
                            Evas_Object *obj EINA_UNUSED,
                            const char *sig EINA_UNUSED,
                            const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET_OR_RETURN(data, sd);

#ifdef HAVE_ELEMENTARY_WL2
   _elm_win_wl_cursor_set(sd->obj, NULL);
#endif
}

static void
_elm_win_frame_cb_resize_start(void *data, Evas_Object *obj EINA_UNUSED,
                               const char *sig EINA_UNUSED, const char *source)
{
   ELM_WIN_DATA_GET_OR_RETURN(data, sd);
   Efl_Ui_Win_Move_Resize_Mode mode;

   mode = _move_resize_mode_get(source);
   if (mode == EFL_UI_WIN_MOVE_RESIZE_MOVE) return;

   _win_move_resize_start(sd, mode);
}

static void
_elm_win_frame_cb_minimize(void *data,
                           Evas_Object *obj EINA_UNUSED,
                           const char *sig EINA_UNUSED,
                           const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   if (!sd) return;
//   sd->iconified = EINA_TRUE;
   TRAP(sd, iconified_set, EINA_TRUE);
}

static void
_elm_win_frame_cb_maximize(void *data,
                           Evas_Object *obj EINA_UNUSED,
                           const char *sig EINA_UNUSED,
                           const char *source EINA_UNUSED)
{
   Eina_Bool value;
   ELM_WIN_DATA_GET(data, sd);

   if (!sd) return;
   if (sd->maximized) value = EINA_FALSE;
   else value = EINA_TRUE;

   efl_ui_win_maximized_set(sd->obj, value);
}

static void
_elm_win_frame_cb_menu(void *data,
                        Evas_Object *obj EINA_UNUSED,
                        const char *sig EINA_UNUSED,
                        const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);
#ifdef HAVE_ELEMENTARY_WL2
   Ecore_Wl2_Input *input;
   int x, y, wx, wy;

   if (!sd->wl.win) return;
   evas_canvas_pointer_canvas_xy_get(sd->evas, &x, &y);
   ecore_wl2_window_geometry_get(sd->wl.win, &wx, &wy, NULL, NULL);
   if (x < 0) x += wx;
   if (y < 0) y += wy;

   input = ecore_wl2_window_input_get(sd->wl.win);

   if (sd->wl.win->zxdg_toplevel)
     zxdg_toplevel_v6_show_window_menu(sd->wl.win->zxdg_toplevel,
                                       ecore_wl2_input_seat_get(input), 0, x, y);
   else if (sd->wl.win->xdg_surface)
     xdg_surface_show_window_menu(sd->wl.win->xdg_surface,
                                  ecore_wl2_input_seat_get(input), 0, x, y);
#else
   (void)sd;
#endif
}
static void
_elm_win_frame_cb_close(void *data,
                        Evas_Object *obj EINA_UNUSED,
                        const char *sig EINA_UNUSED,
                        const char *source EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);
   Evas_Object *win;

   /* FIXME: After the current freeze, this should be handled differently.
    *
    * Ideally, we would want to mimic the X11 backend and use something
    * like ECORE_WL_EVENT_WINDOW_DELETE and handle the delete_request
    * inside of ecore_evas. That would be the 'proper' way, but since we are
    * in a freeze right now, I cannot add a new event value, or a new
    * event structure to ecore_wayland.
    *
    * So yes, this is a temporary 'stop-gap' solution which will be fixed
    * when the freeze is over, but it does fix a trac bug for now, and in a
    * way which does not break API or the freeze. - dh
    */

   if (!sd) return;

   win = sd->obj;

   int autodel = sd->autodel;
   sd->autodel_clear = &autodel;
   evas_object_ref(win);
   efl_event_callback_legacy_call(win, EFL_UI_WIN_EVENT_DELETE_REQUEST, NULL);
   if (sd->autohide)
     evas_object_hide(win);
   // FIXME: if above callback deletes - then the below will be invalid
   if (autodel) evas_object_del(win);
   else sd->autodel_clear = NULL;
   evas_object_unref(win);
}

#ifdef HAVE_ELEMENTARY_WL2
static Eina_Bool
_elm_win_wl_configure(void *data, int t EINA_UNUSED, void *event)
{
   Ecore_Wl2_Event_Window_Configure *ev = event;
   ELM_WIN_DATA_GET(data, sd);
   if (ecore_wl2_window_id_get(sd->wl.win) != (int)ev->win) return ECORE_CALLBACK_RENEW;

   if (sd->resizing && (!ev->edges)) sd->resizing = EINA_FALSE;
   return ECORE_CALLBACK_RENEW;
}

static void
_elm_win_frame_pre_render(void *data, Evas *e EINA_UNUSED, void *ev EINA_UNUSED)
{
   Efl_Ui_Win_Data *sd = data;

   if (sd->wl.opaque_dirty)
     _elm_win_opaque_update(sd, EINA_FALSE);
   sd->wl.opaque_dirty = 0;
}
#endif

static inline void
_elm_object_part_cursor_set(Evas_Object *obj, Evas_Object *edj,
                            const char *part, const char *cursor)
{
   Evas_Object *sub = (Evas_Object *) edje_object_part_object_get(edj, part);
   if (!sub) return;

   elm_object_sub_cursor_set(sub, obj, cursor);
}

static char *
_efl_system_theme_path_get(void)
{
   // Find the default theme from EFL install. Quite ugly.
   const char *sysdir;
   char *version;
   char path[PATH_MAX];
   int v;

   sysdir = elm_theme_system_dir_get();
   if (!sysdir) return NULL;

   eina_file_path_join(path, PATH_MAX, sysdir, "default.edj");
   version = edje_file_data_get(path, "version");
   v = version ? atoi(version) : 0;
   free(version);
   if (v < FRAME_OBJ_THEME_MIN_VERSION)
     {
        ERR("Default system theme is too old, something is wrong with your installation of EFL.");
        return NULL;
     }

   return strdup(path);
}

static void
_elm_win_frame_add(Efl_Ui_Win_Data *sd, const char *element, const char *style)
{
   Evas_Object *obj = sd->obj;
   int w, h, mw, mh, v;
   const char *version;

   if (sd->frame_obj) return;

   sd->frame_obj = edje_object_add(sd->evas);

   // Verify theme version. Border requires an exact theme API.
   version = elm_theme_data_get(elm_widget_theme_get(sd->obj), "version");
   v = version ? atoi(version) : 0;
   if (EINA_LIKELY(v >= FRAME_OBJ_THEME_MIN_VERSION))
     {
        if (!elm_widget_theme_object_set
            (sd->obj, sd->frame_obj, "border", element, style))
          {
             ERR("Failed to set main border theme for the window.");
             ELM_SAFE_FREE(sd->frame_obj, evas_object_del);
             return;
          }

        // Verify border.edc version as well
        version = edje_object_data_get(sd->frame_obj, "version");
        v = version ? atoi(version) : 0;
     }

   if (v < FRAME_OBJ_THEME_MIN_VERSION)
     {
        // Theme compatibility
        const char *key  = "elm/border/base/default"; // FIXME?
        char *sys_theme;

        WRN("Selected theme does not support the required border theme API "
            "(version = %d, requires >= %d).",
            v, FRAME_OBJ_THEME_MIN_VERSION);
        sys_theme = _efl_system_theme_path_get();
        if (!sys_theme ||
            !edje_object_file_set(sd->frame_obj, sys_theme, key))
          {
             ERR("Failed to set main border theme for the window.");
             ELM_SAFE_FREE(sd->frame_obj, evas_object_del);
             free(sys_theme);
             return;
          }
        free(sys_theme);
     }

   /* Small hack: The special value 2 means this is the top frame object.
    * We propagate to the children now (the edc group contents), but subsequent
    * calls to smart_member_add will not propagate the flag further. Note that
    * this little hack will fall apart if edje creates and destroys objects on
    * the fly. */
   efl_canvas_object_is_frame_object_set(sd->frame_obj, 2);

   edje_object_part_swallow(sd->frame_obj, "elm.swallow.client", sd->legacy.edje);

   if (sd->icon)
     evas_object_show(sd->icon);
   else
     {
        Eina_Bool set = EINA_FALSE;

        sd->icon = elm_icon_add(sd->obj);

        if (sd->icon_name)
          set = elm_icon_standard_set(sd->icon, sd->icon_name);
        if ((!sd->icon_name) || (!set))
          {
             Efreet_Desktop *d;
             d = efreet_util_desktop_exec_find(_elm_appname);
             if (d)
               {
                  elm_icon_standard_set(sd->icon, d->icon);
                  efreet_desktop_free(d);
               }
          }
     }

   edje_object_part_swallow(sd->frame_obj, "elm.swallow.icon", sd->icon);
   efl_canvas_object_is_frame_object_set(sd->icon, EINA_TRUE);

   evas_object_event_callback_add
     (sd->frame_obj, EVAS_CALLBACK_MOVE, _elm_win_frame_obj_move, sd);
   evas_object_event_callback_add
     (sd->frame_obj, EVAS_CALLBACK_RESIZE, _elm_win_frame_obj_resize, sd);

   _elm_win_frame_style_update(sd, 1, 0);

   /* NB: Do NOT remove these calls !! Needed to calculate proper
    * framespace on initial show of the window */
   edje_object_size_min_calc(sd->frame_obj, &mw, &mh);
   evas_object_move(sd->frame_obj, 0, 0);
   evas_object_resize(sd->frame_obj, mw, mh);
   evas_object_smart_calculate(sd->frame_obj);

   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,move,start", "elm",
     _elm_win_frame_cb_move_start, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,move,stop", "elm",
     _elm_win_frame_cb_move_stop, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,resize,show", "*",
     _elm_win_frame_cb_resize_show, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,resize,hide", "*",
     _elm_win_frame_cb_resize_hide, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,resize,start", "*",
     _elm_win_frame_cb_resize_start, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,minimize", "elm",
     _elm_win_frame_cb_minimize, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,maximize", "elm",
     _elm_win_frame_cb_maximize, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,close", "elm", _elm_win_frame_cb_close, obj);
   edje_object_signal_callback_add
     (sd->frame_obj, "elm,action,menu", "elm", _elm_win_frame_cb_menu, obj);

   if (!sd->pointer.obj)
     {
        for (size_t k = 0; k < EINA_C_ARRAY_LENGTH(_resize_infos); k++)
          {
             const resize_info *ri = &_resize_infos[k];
             _elm_object_part_cursor_set(obj, sd->frame_obj, ri->source, ri->cursor);
          }
     }

   if (sd->title)
     {
        edje_object_part_text_escaped_set
          (sd->frame_obj, "elm.text.title", sd->title);
     }

   {
      // HACK: Force render mode of bg rect to COPY. This avoids render garbage.
      Eo *bgrect = (Eo *) edje_object_part_object_get(sd->frame_obj, "elm.rect.background");
      efl_canvas_object_render_op_set(bgrect, EFL_GFX_RENDER_OP_COPY);
   }

   _elm_win_frame_style_update(sd, 1, 1);
   _elm_win_frame_geometry_adjust(sd);
   ecore_evas_geometry_get(sd->ee, NULL, NULL, &w, &h);
   ecore_evas_resize(sd->ee, w, h);
}

static void
_elm_win_frame_style_update(Efl_Ui_Win_Data *sd, Eina_Bool force_emit, Eina_Bool calc)
{
   Eina_Bool borderless, maximized, shadow, focus, bg_solid, menu, unresizable,
         alpha, bg_standard;
   Eina_Bool changed = EINA_FALSE;

   if (!sd->frame_obj)
     {
        if (EINA_LIKELY(sd->type == ELM_WIN_FAKE)) return;
        CRI("Window has no frame object!");
        return;
     }

   if ((sd->type == ELM_WIN_INLINED_IMAGE) ||
       (sd->type == ELM_WIN_SOCKET_IMAGE))
     {
        sd->csd.need_shadow = EINA_FALSE;
        sd->csd.need_borderless = EINA_TRUE;
        sd->csd.need_unresizable = EINA_TRUE;
        sd->csd.need_menu = EINA_FALSE;
     }
   else
     {
        sd->csd.need_shadow = sd->csd.need && (!sd->maximized);
     }

   alpha = sd->application_alpha || sd->theme_alpha;
   borderless = sd->csd.need_borderless || (!sd->csd.need) || sd->fullscreen;
   maximized = sd->maximized;
   shadow = sd->csd.need_shadow && (!sd->fullscreen) && (!sd->maximized);
   if (alpha && borderless) shadow = 0;
   focus = ecore_evas_focus_get(sd->ee);
   bg_solid = sd->csd.need_bg_solid;
   bg_standard = sd->csd.need_bg_standard;
   unresizable = sd->csd.need_unresizable;
   menu = sd->csd.need_menu;

   /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
    * At the moment, E Wayland uses SSD for its internal windows. Which means
    * we must hide the shadow if the borderless flag is set. "trap" here means
    * we are likely to be running inside E compositor.
    * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
   if (trap && sd->csd.wayland && sd->csd.need_borderless)
     shadow = 0;

#define STATE_SET(state, s1, s2) do { \
   if (force_emit || (state != sd->csd.cur_##state)) \
     { \
        const char *sig = state ? s1 : s2; \
        edje_object_signal_emit(sd->frame_obj, sig, "elm"); \
        DBG("frame style emit: %p %s", sd->obj, sig); \
        sd->csd.cur_##state = state; \
        changed = EINA_TRUE; \
     } } while (0)

   STATE_SET(borderless, "elm,state,borderless,on", "elm,state,borderless,off");
   STATE_SET(shadow, "elm,state,shadow,on", "elm,state,shadow,off");
   STATE_SET(maximized, "elm,state,maximized", "elm,state,unmaximized");
   STATE_SET(focus, "elm,action,focus", "elm,action,unfocus");
   STATE_SET(bg_solid, "elm,state,background,solid,on", "elm,state,background,solid,off");
   STATE_SET(bg_standard, "elm,state,background,standard,on", "elm,state,background,standard,off");
   STATE_SET(unresizable, "elm,state,unresizable,on", "elm,state,unresizable,off");
   STATE_SET(menu, "elm,action,show_menu", "elm,action,hide_menu");

#undef STATE_SET

   if (changed)
     {
        if (calc || force_emit)
          edje_object_message_signal_process(sd->frame_obj);
        if (calc)
          evas_object_smart_calculate(sd->frame_obj);
        _elm_win_frame_obj_update(sd);
        _elm_win_opaque_update(sd, EINA_FALSE);
     }
}

#ifdef ELM_DEBUG
static void
_debug_key_down(void *data EINA_UNUSED,
                Evas *e EINA_UNUSED,
                Evas_Object *obj,
                void *event_info)
{
   Evas_Event_Key_Down *ev = event_info;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD)
     return;

   if ((strcmp(ev->key, "F12")) ||
       (!evas_key_modifier_is_set(ev->modifiers, "Control")))
     return;

   INF("Tree graph generated.");
   elm_object_tree_dot_dump(obj, "./dump.dot");
}

#endif

static void
_win_inlined_image_set(Efl_Ui_Win_Data *sd)
{
   evas_object_image_alpha_set(sd->img_obj, EINA_FALSE);
   evas_object_image_filled_set(sd->img_obj, EINA_TRUE);

   evas_object_event_callback_add
     (sd->img_obj, EVAS_CALLBACK_DEL, _elm_win_on_img_obj_del, sd->obj);
   evas_object_event_callback_add
     (sd->img_obj, EVAS_CALLBACK_HIDE, _win_img_hide, sd->obj);
   evas_object_event_callback_add
     (sd->img_obj, EVAS_CALLBACK_MOUSE_UP, _win_img_mouse_up, sd->obj);
   evas_object_event_callback_add
     (sd->img_obj, EVAS_CALLBACK_FOCUS_IN, _win_img_focus_in, sd->obj);
   evas_object_event_callback_add
     (sd->img_obj, EVAS_CALLBACK_FOCUS_OUT, _win_img_focus_out, sd->obj);
}

static void
_elm_win_on_icon_del(void *data,
                     Evas *e EINA_UNUSED,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   ELM_WIN_DATA_GET(data, sd);

   if (sd->icon == obj) sd->icon = NULL;
}

EOLIAN static void
_efl_ui_win_efl_canvas_group_group_add(Eo *obj, Efl_Ui_Win_Data *_pd EINA_UNUSED)
{
   efl_canvas_group_add(efl_super(obj, MY_CLASS));

   elm_widget_can_focus_set(obj, EINA_TRUE);

   elm_widget_highlight_ignore_set(obj, EINA_TRUE);
}

#ifdef HAVE_ELEMENTARY_X
static void
_elm_x_io_err(void *data EINA_UNUSED)
{
   Eina_List *l;
   Evas_Object *obj;

   EINA_LIST_FOREACH(_elm_win_list, l, obj)
     efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_IOERR, NULL);
   elm_exit();
}
#endif

EAPI Evas_Object *
elm_win_add(Evas_Object *parent,
            const char *name,
            Elm_Win_Type type)
{
   Evas_Object *obj = efl_add(MY_CLASS, parent, efl_ui_win_name_set(efl_added, name), efl_ui_win_type_set(efl_added, type));
   return obj;
}

EAPI Evas_Object *
elm_win_fake_add(Ecore_Evas *ee)
{
   return efl_add(MY_CLASS, NULL, elm_win_fake_canvas_set(efl_added, ee), efl_ui_win_name_set(efl_added, NULL), efl_ui_win_type_set(efl_added, ELM_WIN_FAKE));
}

static void
_elm_win_cb_hide(void *data EINA_UNUSED,
                 Evas *e EINA_UNUSED,
                 Evas_Object *obj EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   _elm_win_state_eval_queue();
}

static void
_elm_win_cb_show(void *data EINA_UNUSED,
                 Evas *e EINA_UNUSED,
                 Evas_Object *obj EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   _elm_win_state_eval_queue();
}

static inline const char *
_efl_ui_win_accel(Efl_Ui_Win_Data *sd)
{
   const char *str = sd->accel_pref;
   const char *env;

   /* current elm config */
   if (!str)
     {
        if (_elm_config->accel) str = _elm_config->accel;
        if (_elm_accel_preference) str = _elm_accel_preference;
     }

   /* global overrides */
   if ((_elm_config->accel_override) && (_elm_config->accel))
     str = _elm_config->accel;

   /* env var wins */
   env = getenv("ELM_ACCEL");
   if (env) str = env;

   return str;
}

static inline void
_elm_win_need_frame_adjust(Efl_Ui_Win_Data *sd, const char *engine)
{
   const char *s;

   /* this is for debug only - don't keep forever, it's not an api! */
   s = getenv("EFL_WIN_FRAME_MODE");
   sd->csd.wayland = (eina_streq(engine, ELM_WAYLAND_SHM) ||
                      eina_streq(engine, ELM_WAYLAND_EGL));

   if (sd->type == ELM_WIN_FAKE)
     sd->csd.need = EINA_FALSE;
   else if (eina_streq(s, "on"))
     sd->csd.need = EINA_TRUE;
   else if (eina_streq(s, "off"))
     sd->csd.need = EINA_FALSE;
   else
     sd->csd.need = sd->csd.wayland;
}

static Eo *
_elm_win_finalize_internal(Eo *obj, Efl_Ui_Win_Data *sd, const char *name, Elm_Win_Type type)
{
   sd->obj = obj; // in ctor

   Evas_Object *parent = NULL;
   Evas *e;
   const Eina_List *l;
   const char *fontpath, *engine = NULL, *enginelist[32], *disp;
   int gl_depth = _elm_config->gl_depth;
   int gl_stencil = _elm_config->gl_stencil;
   int gl_msaa = _elm_config->gl_msaa;
   Eina_Stringshare *accel = NULL;
   Eina_Bool is_gl_accel;
   int i, p = 0;

   Efl_Ui_Win_Data tmp_sd;

   parent = efl_parent_get(obj);

   /* just to store some data while trying out to create a canvas */
   memset(&tmp_sd, 0, sizeof(Efl_Ui_Win_Data));

   is_gl_accel = _elm_config_accel_preference_parse
         (_efl_ui_win_accel(sd), &accel, &gl_depth, &gl_stencil, &gl_msaa);

   switch (type)
     {
      case ELM_WIN_FAKE:
        tmp_sd.ee = sd->ee;
        break;
      case ELM_WIN_INLINED_IMAGE:
        if (!parent) break;
          {
             Ecore_Evas *ee;

             e = evas_object_evas_get(parent);
             if (!e) break;

             ee = ecore_evas_ecore_evas_get(e);
             if (!ee) break;

             tmp_sd.img_obj = ecore_evas_object_image_new(ee);
             if (!tmp_sd.img_obj) break;

             tmp_sd.ee = ecore_evas_object_ecore_evas_get(tmp_sd.img_obj);
             if (!tmp_sd.ee)
               {
                  ELM_SAFE_FREE(tmp_sd.img_obj, evas_object_del);
               }
          }
        break;

      case ELM_WIN_SOCKET_IMAGE:
        tmp_sd.ee = ecore_evas_extn_socket_new(1, 1);
        break;

      default:
        disp = getenv("ELM_DISPLAY");
        if ((disp) && (!strcmp(disp, "ews")))
          {
             enginelist[p++] = ELM_EWS;
          }
        else if ((disp) && (!strcmp(disp, "buffer")))
          {
             enginelist[p++] = ELM_BUFFER;
          }
        else if ((disp) && (!strcmp(disp, "shot")))
          {
             enginelist[p++] = ENGINE_GET();
          }
// welcome to ifdef hell! :)

#ifdef HAVE_ELEMENTARY_X
        else if ((disp) && (!strcmp(disp, "x11")))
          {
             if (is_gl_accel)
               {
                  enginelist[p++] = ELM_OPENGL_X11;
                  enginelist[p++] = ELM_SOFTWARE_X11;
               }
             else
               {
                  enginelist[p++] = ELM_SOFTWARE_X11;
                  enginelist[p++] = ELM_OPENGL_X11;
               }
          }
#endif

#ifdef HAVE_ELEMENTARY_WL2
        else if ((disp) && (!strcmp(disp, "wl")))
          {
             if (is_gl_accel)
               {
                  enginelist[p++] = ELM_WAYLAND_EGL;
                  enginelist[p++] = ELM_WAYLAND_SHM;
               }
             else
               {
                  enginelist[p++] = ELM_WAYLAND_SHM;
                  enginelist[p++] = ELM_WAYLAND_EGL;
               }
          }
#endif

#ifdef HAVE_ELEMENTARY_WIN32
        else if ((disp) && (!strcmp(disp, "win")))
          {
             enginelist[p++] = ELM_SOFTWARE_WIN32;
             enginelist[p++] = ELM_SOFTWARE_DDRAW;
          }
#endif

#ifdef HAVE_ELEMENTARY_SDL
        else if ((disp) && (!strcmp(disp, "sdl")))
          {
             if (is_gl_accel)
               {
                  enginelist[p++] = ELM_OPENGL_SDL;
                  enginelist[p++] = ELM_SOFTWARE_SDL;
               }
             else
               {
                  enginelist[p++] = ELM_SOFTWARE_SDL;
                  enginelist[p++] = ELM_OPENGL_SDL;
               }
          }
#endif

#ifdef HAVE_ELEMENTARY_COCOA
        else if ((disp) && (!strcmp(disp, "mac")))
          {
             enginelist[p++] = ELM_OPENGL_COCOA;
          }
#endif

#if defined(HAVE_ELEMENTARY_DRM) || defined(HAVE_ELEMENTARY_FB)
        else if ((disp) && (!strcmp(disp, "fb")))
          {
#ifdef HAVE_ELEMENTARY_DRM
             enginelist[p++] = ELM_DRM;
#endif
#ifdef HAVE_ELEMENTARY_FB
             enginelist[p++] = ELM_SOFTWARE_FB;
#endif
          }
#endif

#ifdef HAVE_ELEMENTARY_PSL1GHT
        else if ((disp) && (!strcmp(disp, "ps3")))
          {
             enginelist[p++] = ELM_SOFTWARE_PSL1GHT;
          }
#endif
#ifdef HAVE_ELEMENTARY_X
        else if (!_elm_preferred_engine &&
                 getenv("DISPLAY") && !getenv("ELM_ENGINE"))
          {
             if (is_gl_accel)
               {
                  enginelist[p++] = ELM_OPENGL_X11;
                  enginelist[p++] = ELM_SOFTWARE_X11;
               }
             else
               {
                  enginelist[p++] = ELM_SOFTWARE_X11;
                  enginelist[p++] = ELM_OPENGL_X11;
               }
          }
#endif
#ifdef HAVE_ELEMENTARY_WL2
        else if (!_elm_preferred_engine &&
                 getenv("WAYLAND_DISPLAY") && !getenv("ELM_ENGINE"))
          {
             if (is_gl_accel)
               {
                  enginelist[p++] = ELM_WAYLAND_EGL;
                  enginelist[p++] = ELM_WAYLAND_SHM;
               }
             else
               {
                  enginelist[p++] = ELM_WAYLAND_SHM;
                  enginelist[p++] = ELM_WAYLAND_EGL;
               }
          }
#endif
        else
          {
             if (is_gl_accel)
               {
// add all engines with selected engine first - if any
                  if (ENGINE_GET())
                    enginelist[p++] = ENGINE_GET();

// add all engines with gl/accelerated ones first - only engines compiled
#ifdef HAVE_ELEMENTARY_X
                  enginelist[p++] = ELM_OPENGL_X11;
#endif
#ifdef HAVE_ELEMENTARY_WL2
                  enginelist[p++] = ELM_WAYLAND_EGL;
#endif
#ifdef HAVE_ELEMENTARY_DRM
                  enginelist[p++] = ELM_DRM;
#endif
#ifdef HAVE_ELEMENTARY_FB
                  enginelist[p++] = ELM_SOFTWARE_FB;
#endif
#ifdef HAVE_ELEMENTARY_COCOA
                  enginelist[p++] = ELM_OPENGL_COCOA;
#endif
#ifdef HAVE_ELEMENTARY_SDL
                  enginelist[p++] = ELM_OPENGL_SDL;
#endif
#ifdef HAVE_ELEMENTARY_X
                  enginelist[p++] = ELM_SOFTWARE_X11;
#endif
#ifdef HAVE_ELEMENTARY_WL2
                  enginelist[p++] = ELM_WAYLAND_SHM;
#endif
#ifdef HAVE_ELEMENTARY_WIN32
                  enginelist[p++] = ELM_SOFTWARE_WIN32;
                  enginelist[p++] = ELM_SOFTWARE_DDRAW;
#endif
#ifdef HAVE_ELEMENTARY_SDL
                  enginelist[p++] = ELM_SOFTWARE_SDL;
#endif
#ifdef HAVE_ELEMENTARY_PSL1GHT
                  enginelist[p++] = ELM_SOFTWARE_PSL1GHT;
#endif
               }
             else
               {
// add all engines with selected engine first - if any
                  if (elm_config_preferred_engine_get())
                    enginelist[p++] = elm_config_preferred_engine_get();
// add check _elm_gl_preference whether "none" or not
                  else if (_elm_config->engine &&
                           ((elm_config_accel_preference_get() &&
                             !strcmp(elm_config_accel_preference_get(), "none"))
                            || (accel && !strcmp(accel, "none"))))
                    enginelist[p++] = _elm_config->engine;
// add all engines with gl/accelerated ones first - only engines compiled
#ifdef HAVE_ELEMENTARY_X
                  enginelist[p++] = ELM_SOFTWARE_X11;
#endif
#ifdef HAVE_ELEMENTARY_WL2
                  enginelist[p++] = ELM_WAYLAND_SHM;
#endif
#ifdef HAVE_ELEMENTARY_DRM
                  enginelist[p++] = ELM_DRM;
#endif
#ifdef HAVE_ELEMENTARY_FB
                  enginelist[p++] = ELM_SOFTWARE_FB;
#endif
#ifdef HAVE_ELEMENTARY_COCOA
                  enginelist[p++] = ELM_OPENGL_COCOA;
#endif
#ifdef HAVE_ELEMENTARY_WIN32
                  enginelist[p++] = ELM_SOFTWARE_WIN32;
                  enginelist[p++] = ELM_SOFTWARE_DDRAW;
#endif
#ifdef HAVE_ELEMENTARY_SDL
                  enginelist[p++] = ELM_SOFTWARE_SDL;
#endif
#ifdef HAVE_ELEMENTARY_X
                  enginelist[p++] = ELM_OPENGL_X11;
#endif
#ifdef HAVE_ELEMENTARY_WL2
                  enginelist[p++] = ELM_WAYLAND_EGL;
#endif
#ifdef HAVE_ELEMENTARY_DRM
                  enginelist[p++] = ELM_DRM;
#endif
#ifdef HAVE_ELEMENTARY_SDL
                  enginelist[p++] = ELM_OPENGL_SDL;
#endif
#ifdef HAVE_ELEMENTARY_PSL1GHT
                  enginelist[p++] = ELM_SOFTWARE_PSL1GHT;
#endif
               }
          }
        for (i = 0; i < p; i++)
          {
             if (!strcmp(enginelist[i], ELM_SOFTWARE_X11))
               tmp_sd.ee = ecore_evas_software_x11_new(NULL, 0, 0, 0, 1, 1);
             else if (!strcmp(enginelist[i], ELM_OPENGL_X11))
               {
                  int opt[20], opt_i = 0;

                  if (_elm_config->vsync)
                    {
                       opt[opt_i++] = ECORE_EVAS_GL_X11_OPT_VSYNC;
                       opt[opt_i++] = 1;
                    }
                  if (gl_depth)
                    {
                       opt[opt_i++] = ECORE_EVAS_GL_X11_OPT_GL_DEPTH;
                       opt[opt_i++] = gl_depth;
                    }
                  if (gl_stencil)
                    {
                       opt[opt_i++] = ECORE_EVAS_GL_X11_OPT_GL_STENCIL;
                       opt[opt_i++] = gl_stencil;
                    }
                  if (gl_msaa)
                    {
                       opt[opt_i++] = ECORE_EVAS_GL_X11_OPT_GL_MSAA;
                       opt[opt_i++] = gl_msaa;
                    }
                  opt[opt_i] = 0;
                  if (opt_i > 0)
                    tmp_sd.ee = ecore_evas_gl_x11_options_new(NULL, 0, 0, 0, 1, 1, opt);
                  else
                    tmp_sd.ee = ecore_evas_gl_x11_new(NULL, 0, 0, 0, 1, 1);
               }
             else if (!strcmp(enginelist[i], ELM_WAYLAND_SHM))
               tmp_sd.ee = ecore_evas_wayland_shm_new(NULL, 0, 0, 0, 1, 1, 0);
             else if (!strcmp(enginelist[i], ELM_WAYLAND_EGL))
               tmp_sd.ee = ecore_evas_wayland_egl_new(NULL, 0, 0, 0, 1, 1, 0);
             else if (!strcmp(enginelist[i], ELM_SOFTWARE_WIN32))
               tmp_sd.ee = ecore_evas_software_gdi_new(NULL, 0, 0, 1, 1);
             else if (!strcmp(enginelist[i], ELM_SOFTWARE_DDRAW))
               tmp_sd.ee = ecore_evas_software_ddraw_new(NULL, 0, 0, 1, 1);
             else if (!strcmp(enginelist[i], ELM_SOFTWARE_SDL))
               tmp_sd.ee = ecore_evas_sdl_new(NULL, 0, 0, 0, 0, 0, 1);
             else if (!strcmp(enginelist[i], ELM_OPENGL_SDL))
               tmp_sd.ee = ecore_evas_gl_sdl_new(NULL, 1, 1, 0, 0);
             else if (!strcmp(enginelist[i], ELM_OPENGL_COCOA))
               tmp_sd.ee = ecore_evas_cocoa_new(NULL, 1, 1, 0, 0);
             else if (!strcmp(enginelist[i], ELM_EWS))
               tmp_sd.ee = ecore_evas_ews_new(0, 0, 1, 1);
             else if (!strcmp(enginelist[i], ELM_SOFTWARE_FB))
               tmp_sd.ee = ecore_evas_fb_new(NULL, 0, 1, 1);
             else if (!strcmp(enginelist[i], ELM_BUFFER))
               tmp_sd.ee = ecore_evas_buffer_new(1, 1);
             else if (!strcmp(enginelist[i], ELM_SOFTWARE_PSL1GHT))
               tmp_sd.ee = ecore_evas_psl1ght_new(NULL, 1, 1);
             else if (!strcmp(enginelist[i], ELM_DRM))
               tmp_sd.ee = ecore_evas_drm_new(NULL, 0, 0, 0, 1, 1);
             else if (!strncmp(enginelist[i], "shot:", 5))
               {
                  tmp_sd.ee = ecore_evas_buffer_new(1, 1);
                  ecore_evas_manual_render_set(tmp_sd.ee, EINA_TRUE);
                  tmp_sd.shot.info = eina_stringshare_add(ENGINE_GET() + 5);
               }
             engine = enginelist[i];
             if (tmp_sd.ee) break;
          }
        break;
     }

   eina_stringshare_del(accel);
   if (!tmp_sd.ee)
     {
        ERR("Cannot create window.");
        return NULL;
     }

   if (!sd->accel_pref)
     eina_stringshare_replace(&sd->accel_pref, elm_config_accel_preference_get());

   efl_parent_set(obj, ecore_evas_get(tmp_sd.ee));

   /* FIXME: Major hack: calling the constructor in the middle of finalize. */
   efl_constructor(efl_super(obj, MY_CLASS));
   efl_canvas_object_type_set(obj, MY_CLASS_NAME_LEGACY);
   evas_object_smart_callbacks_descriptions_set(obj, _smart_callbacks);

   if (getenv("ELM_FIRST_FRAME"))
     evas_event_callback_add(ecore_evas_get(tmp_sd.ee), EVAS_CALLBACK_RENDER_POST,
                             _elm_win_first_frame_do, getenv("ELM_FIRST_FRAME"));

   /* copying possibly altered fields back */
#define SD_CPY(_field)             \
  do                               \
    {                              \
       sd->_field = tmp_sd._field; \
    } while (0)

   SD_CPY(ee);
   SD_CPY(img_obj);
   SD_CPY(shot.info);
#undef SD_CPY

   if ((type != ELM_WIN_FAKE) && (trap) && (trap->add))
     sd->trap_data = trap->add(obj);

   /* complementary actions, which depend on final smart data
    * pointer */
   if (type == ELM_WIN_INLINED_IMAGE)
     _win_inlined_image_set(sd);
#ifdef HAVE_ELEMENTARY_X
   else if ((engine) &&
            ((!strcmp(engine, ELM_SOFTWARE_X11)) ||
             (!strcmp(engine, ELM_OPENGL_X11))))
     {
        sd->x.client_message_handler = ecore_event_handler_add
            (ECORE_X_EVENT_CLIENT_MESSAGE, _elm_win_client_message, obj);
        sd->x.property_handler = ecore_event_handler_add
            (ECORE_X_EVENT_WINDOW_PROPERTY, _elm_win_property_change, obj);
     }
#endif
   else if ((engine) && (!strncmp(engine, "shot:", 5)))
     _shot_init(sd);

   sd->kbdmode = ELM_WIN_KEYBOARD_UNKNOWN;
   sd->indmode = ELM_WIN_INDICATOR_UNKNOWN;

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin) ecore_x_io_error_handler_set(_elm_x_io_err, NULL);
#endif

#ifdef HAVE_ELEMENTARY_WL2
   _elm_win_wlwindow_get(sd);
   if (eina_streq(engine, ELM_WAYLAND_SHM) || eina_streq(engine, ELM_WAYLAND_EGL))
     sd->wl.configure_handler =
       ecore_event_handler_add(ECORE_WL2_EVENT_WINDOW_CONFIGURE,
         _elm_win_wl_configure, obj);
#endif

#ifdef HAVE_ELEMENTARY_COCOA
   _elm_win_cocoawindow_get(sd);
#endif
#ifdef HAVE_ELEMENTARY_WIN32
   _internal_elm_win_win32window_get(sd);
#endif

   if ((_elm_config->bgpixmap)
#ifdef HAVE_ELEMENTARY_X
       &&
       (((sd->x.xwin) && (!ecore_x_screen_is_composited(0))) ||
           (!sd->x.xwin)))
#else
     )
#endif
     TRAP(sd, avoid_damage_set, ECORE_EVAS_AVOID_DAMAGE_EXPOSE);
   // bg pixmap done by x - has other issues like can be redrawn by x before it
   // is filled/ready by app
   //     TRAP(sd, avoid_damage_set, ECORE_EVAS_AVOID_DAMAGE_BUILT_IN);

   sd->type = type;
   sd->parent = parent;
   sd->modal_count = 0;
   sd->withdrawn = ecore_evas_withdrawn_get(sd->ee);

   if (sd->parent)
     evas_object_event_callback_add
       (sd->parent, EVAS_CALLBACK_DEL, _elm_win_on_parent_del, obj);

   sd->evas = ecore_evas_get(sd->ee);

   evas_object_color_set(obj, 0, 0, 0, 0);
   evas_object_move(obj, 0, 0);
   evas_object_resize(obj, 1, 1);
   evas_object_pass_events_set(obj, EINA_TRUE);

   if (type == ELM_WIN_INLINED_IMAGE)
     elm_widget_parent2_set(obj, parent);

   /* use own version of ecore_evas_object_associate() that does TRAP() */
   ecore_evas_data_set(sd->ee, "elm_win", obj);

   if (type != ELM_WIN_FAKE)
     {
        evas_object_event_callback_add(obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
           _elm_win_obj_callback_changed_size_hints, NULL);
        evas_object_intercept_raise_callback_add
          (obj, _elm_win_obj_intercept_raise, obj);
        evas_object_intercept_lower_callback_add
          (obj, _elm_win_obj_intercept_lower, obj);
        evas_object_intercept_stack_above_callback_add
          (obj, _elm_win_obj_intercept_stack_above, obj);
        evas_object_intercept_stack_below_callback_add
          (obj, _elm_win_obj_intercept_stack_below, obj);
        evas_object_intercept_layer_set_callback_add
          (obj, _elm_win_obj_intercept_layer_set, obj);
        evas_object_intercept_show_callback_add
          (obj, _elm_win_obj_intercept_show, obj);
     }

   TRAP(sd, name_class_set, name, _elm_appname);
   TRAP(sd, title_set, sd->title);
   ecore_evas_callback_delete_request_set(sd->ee, _elm_win_delete_request);
   ecore_evas_callback_state_change_set(sd->ee, _elm_win_state_change);
   ecore_evas_callback_focus_in_set(sd->ee, _elm_win_focus_in);
   ecore_evas_callback_focus_out_set(sd->ee, _elm_win_focus_out);
   ecore_evas_callback_resize_set(sd->ee, _elm_win_resize);
   ecore_evas_callback_move_set(sd->ee, _elm_win_move);
   ecore_evas_callback_pre_render_set(sd->ee, _elm_win_pre_render);
   if (type != ELM_WIN_FAKE)
     {
         ecore_evas_callback_mouse_in_set(sd->ee, _elm_win_mouse_in);
         ecore_evas_callback_mouse_out_set(sd->ee, _elm_win_mouse_out);
     }

   evas_object_event_callback_add(obj, EVAS_CALLBACK_HIDE, _elm_win_cb_hide, NULL);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_SHOW, _elm_win_cb_show, NULL);

   evas_image_cache_set(sd->evas, (_elm_config->image_cache * 1024));
   evas_font_cache_set(sd->evas, (_elm_config->font_cache * 1024));

   EINA_LIST_FOREACH(_elm_config->font_dirs, l, fontpath)
     evas_font_path_append(sd->evas, fontpath);

   if (!_elm_config->font_hinting)
     evas_font_hinting_set(sd->evas, EVAS_FONT_HINTING_NONE);
   else if (_elm_config->font_hinting == 1)
     evas_font_hinting_set(sd->evas, EVAS_FONT_HINTING_AUTO);
   else if (_elm_config->font_hinting == 2)
     evas_font_hinting_set(sd->evas, EVAS_FONT_HINTING_BYTECODE);

   sd->wm_rot.wm_supported = ecore_evas_wm_rotation_supported_get(sd->ee);
   sd->wm_rot.preferred_rot = -1; // it means that elm_win doesn't use preferred rotation.

#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
#ifdef HAVE_ELEMENTARY_WL2
   if (eina_streq(engine, ELM_WAYLAND_SHM) || eina_streq(engine, ELM_WAYLAND_EGL))
     evas_event_callback_add(sd->evas, EVAS_CALLBACK_RENDER_FLUSH_PRE, _elm_win_frame_pre_render, sd);
#endif

   //Prohibiting auto-rendering, until elm_win is shown.
   if (_elm_config->auto_norender_withdrawn)
     {
        if (elm_win_withdrawn_get(obj))
          {
             if (!evas_object_data_get(obj, "__win_auto_norender"))
               {
                  elm_win_norender_push(obj);
                  evas_object_data_set(obj, "__win_auto_norender", obj);
               }
          }
     }

#ifdef ELM_DEBUG
   Evas_Modifier_Mask mask = evas_key_modifier_mask_get(sd->evas, "Control");
   evas_object_event_callback_add
     (obj, EVAS_CALLBACK_KEY_DOWN, _debug_key_down, NULL);

   if (evas_object_key_grab(obj, "F12", mask, 0, EINA_TRUE))
     INF("Ctrl+F12 key combination exclusive for dot tree generation\n");
   else
     ERR("failed to grab F12 key to elm widgets (dot) tree generation");
#endif

   if (type != ELM_WIN_FAKE)
     {
        if ((_elm_config->softcursor_mode == ELM_SOFTCURSOR_MODE_ON) ||
            ((_elm_config->softcursor_mode == ELM_SOFTCURSOR_MODE_AUTO) &&
             ((engine) &&
              ((!strcmp(engine, ELM_SOFTWARE_FB)) || (!strcmp(engine, ELM_DRM))))))
          {
             Evas_Object *o;
             Evas_Coord mw = 1, mh = 1, hx = 0, hy = 0;

             sd->pointer.obj = o = edje_object_add(ecore_evas_get(tmp_sd.ee));
             _elm_theme_object_set(obj, o, "pointer", "base", "default");
             edje_object_size_min_calc(o, &mw, &mh);
             evas_object_resize(o, mw, mh);
             edje_object_part_geometry_get(o, "elm.swallow.hotspot",
                                           &hx, &hy, NULL, NULL);
             sd->pointer.hot_x = hx;
             sd->pointer.hot_y = hy;
             evas_object_show(o);
             ecore_evas_object_cursor_set(tmp_sd.ee, o, EVAS_LAYER_MAX, hx, hy);
          }
        else if (_elm_config->softcursor_mode == ELM_SOFTCURSOR_MODE_OFF)
          {
             // do nothing
          }
     }

   _elm_win_legacy_init(sd);
   _elm_win_need_frame_adjust(sd, engine);
   _elm_win_apply_alpha(obj, sd);

   /* do not append to list; all windows render as black rects */
   if (type != ELM_WIN_FAKE)
     {
        const char *element = "base";
        const char *style;

        _elm_win_list = eina_list_append(_elm_win_list, obj);
        _elm_win_count++;

        if ((engine) &&
            ((!strcmp(engine, ELM_SOFTWARE_FB)) || (!strcmp(engine, ELM_DRM))))
          {
             TRAP(sd, fullscreen_set, 1);
          }
        style = elm_widget_style_get(obj);
        if (!style) style = "default";
        switch (type)
          {
           case EFL_UI_WIN_DIALOG_BASIC:    element = "dialog"; break;
           case EFL_UI_WIN_NAVIFRAME_BASIC: element = "naviframe"; break;
           default: break;
          }
        _elm_win_frame_add(sd, element, style);

        if (_elm_config->focus_highlight_enable)
          elm_win_focus_highlight_enabled_set(obj, EINA_TRUE);
        if (_elm_config->focus_highlight_animate)
          elm_win_focus_highlight_animate_set(obj, EINA_TRUE);
     }

#ifdef HAVE_ELEMENTARY_WL2
   if ((type != ELM_WIN_FAKE) && (type != ELM_WIN_INLINED_IMAGE))
     {
        if ((engine) &&
            ((!strcmp(engine, ELM_WAYLAND_SHM) ||
              (!strcmp(engine, ELM_WAYLAND_EGL)))))
          {
             Evas *pevas;

             if (!strcmp(engine, ELM_WAYLAND_SHM))
               sd->pointer.ee = ecore_evas_wayland_shm_new(NULL, 0, 0, 0, 1, 1, 0);
             else if (!strcmp(engine, ELM_WAYLAND_EGL))
               sd->pointer.ee = ecore_evas_wayland_egl_new(NULL, 0, 0, 0, 1, 1, 0);

             pevas = ecore_evas_get(sd->pointer.ee);

             sd->pointer.obj = edje_object_add(pevas);

             sd->pointer.win = ecore_evas_wayland2_window_get(sd->pointer.ee);
             ecore_wl2_window_type_set(sd->pointer.win,
                                       ECORE_WL2_WINDOW_TYPE_NONE);
          }
     }
#endif

   elm_interface_atspi_accessible_role_set(obj, ELM_ATSPI_ROLE_WINDOW);
   if (_elm_config->atspi_mode)
     elm_interface_atspi_window_created_signal_emit(obj);

   // attach config API
   efl_composite_attach(obj, efl_provider_find(ecore_main_loop_get(), EFL_CONFIG_GLOBAL_CLASS));

   efl_event_callback_array_add(obj, _elm_win_evas_feed_fake_callbacks(), sd->evas);
   efl_event_callback_add(obj, EFL_EVENT_CALLBACK_ADD, _win_event_add_cb, sd);
   efl_event_callback_add(obj, EFL_EVENT_CALLBACK_DEL, _win_event_del_cb, sd);

   efl_event_callback_add(sd->evas, EFL_EVENT_POINTER_UP, _elm_win_cb_mouse_up, sd);
   evas_object_show(sd->legacy.edje);

   if (type == ELM_WIN_FAKE)
     {
        _elm_win_resize_job(obj);
        _elm_win_move(sd->ee);
     }
   return obj;
}

EOLIAN static Eo *
_efl_ui_win_efl_object_finalize(Eo *obj, Efl_Ui_Win_Data *_pd)
{
   obj = _elm_win_finalize_internal(obj, _pd, _pd->name, _pd->type);
   if (obj)
     {
        obj = efl_finalize(efl_super(obj, MY_CLASS));
     }
   return obj;
}

EOLIAN static Eo *
_efl_ui_win_efl_object_constructor(Eo *obj, Efl_Ui_Win_Data *_pd EINA_UNUSED)
{
   /* Do nothing. */
   /* XXX: We are calling the constructor chain from the finalizer. It's
    * really bad and hacky. Needs fixing. */

   return obj;
}

EOLIAN static void
_efl_ui_win_efl_text_text_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *title)
{
   if (!title) return;
   eina_stringshare_replace(&(sd->title), title);
   if (sd->ee)
     TRAP(sd, title_set, sd->title);
   if (sd->frame_obj)
     edje_object_part_text_escaped_set
       (sd->frame_obj, "elm.text.title", sd->title);
}

EOLIAN static const char*
_efl_ui_win_efl_text_text_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->title;
}

EOLIAN static void
_efl_ui_win_type_set(Eo *obj, Efl_Ui_Win_Data *sd, Elm_Win_Type type)
{
   if (efl_finalized_get(obj))
     {
        ERR("This function is only allowed during construction.");
        return;
     }
   sd->type = type;
}

EOLIAN static Efl_Ui_Win_Type
_efl_ui_win_type_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->type;
}

EOLIAN static void
_efl_ui_win_name_set(Eo *obj, Efl_Ui_Win_Data *sd, const char *name)
{
   if (efl_finalized_get(obj))
     {
        ERR("This function is only allowed during construction.");
        return;
     }
   sd->name = eina_stringshare_add(name);
}

EOLIAN static const char *
_efl_ui_win_name_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->name;
}

EOLIAN static void
_efl_ui_win_accel_preference_set(Eo *obj, Efl_Ui_Win_Data *pd, const char *accel)
{
   if (efl_finalized_get(obj))
     {
        ERR("This function is only allowed during construction.");
        return;
     }
   eina_stringshare_replace(&pd->accel_pref, accel);
}

EOLIAN static const char *
_efl_ui_win_accel_preference_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd)
{
   return pd->accel_pref;
}

EOLIAN static void
_efl_ui_win_noblank_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd, Eina_Bool noblank)
{
   noblank = !!noblank;
   if (pd->noblank == noblank) return;
   pd->noblank = noblank;
   _win_noblank_eval();
}

EOLIAN static Eina_Bool
_efl_ui_win_noblank_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd)
{
   return pd->noblank;
}

EOLIAN static void
_efl_ui_win_role_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *role)
{
   if (!role) return;
   eina_stringshare_replace(&(sd->role), role);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static const char*
_efl_ui_win_role_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->role;
}

EOLIAN static void
_efl_ui_win_icon_object_set(Eo *obj, Efl_Ui_Win_Data *sd, Evas_Object *icon)
{
   if (sd->icon)
     evas_object_event_callback_del_full(sd->icon, EVAS_CALLBACK_DEL,
       _elm_win_on_icon_del, obj);
   evas_object_del(sd->icon);
   sd->icon = icon;
   if (sd->icon)
     {
        evas_object_event_callback_add(sd->icon, EVAS_CALLBACK_DEL,
          _elm_win_on_icon_del, obj);
        if (sd->frame_obj)
          {
             edje_object_part_swallow(sd->frame_obj, "elm.swallow.icon", sd->icon);
             evas_object_is_frame_object_set(sd->icon, EINA_TRUE);
          }
     }
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static const Evas_Object*
_efl_ui_win_icon_object_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->icon;
}

EOLIAN static void
_efl_ui_win_autodel_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool autodel)
{
   sd->autodel = autodel;
}

EOLIAN static Eina_Bool
_efl_ui_win_autodel_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->autodel;
}

EOLIAN static void
_efl_ui_win_autohide_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool autohide)
{
   sd->autohide = autohide;
}

EOLIAN static Eina_Bool
_efl_ui_win_autohide_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->autohide;
}

EOLIAN static void
_efl_ui_win_activate(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   TRAP(sd, activate);
}

EOLIAN static void
_efl_ui_win_raise(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   TRAP(sd, raise);
}

EOLIAN static void
_efl_ui_win_center(Eo *obj, Efl_Ui_Win_Data *sd, Eina_Bool h, Eina_Bool v)
{
   int win_w, win_h, screen_x, screen_y, screen_w, screen_h, nx, ny;

   if ((trap) && (trap->center) && (!trap->center(sd->trap_data, obj, h, v)))
     return;

   ecore_evas_screen_geometry_get(sd->ee,
                                  &screen_x, &screen_y,
                                  &screen_w, &screen_h);
   if ((!screen_w) || (!screen_h)) return;

   evas_object_geometry_get(obj, &nx, &ny, &win_w, &win_h);
   if ((!win_w) || (!win_h)) return;

   if (h) nx = win_w >= screen_w ? 0 : (screen_w / 2) - (win_w / 2);
   if (v) ny = win_h >= screen_h ? 0 : (screen_h / 2) - (win_h / 2);

   evas_object_move(obj, screen_x + nx, screen_y + ny);
}

EOLIAN static void
_efl_ui_win_borderless_set(Eo *obj, Efl_Ui_Win_Data *sd, Eina_Bool borderless)
{
   sd->csd.need_borderless = borderless ? 1 : 0;
   _elm_win_frame_style_update(sd, 0, 1);

#ifdef HAVE_ELEMENTARY_X
   if (!sd->x.xwin || !sd->csd.need)
#endif
     TRAP(sd, borderless_set, borderless);

   _elm_win_resize_objects_eval(obj);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static Eina_Bool
_efl_ui_win_borderless_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   if (!sd->csd.need)
     return ecore_evas_borderless_get(sd->ee);
   return sd->csd.need_borderless;
}

EOLIAN static void
_efl_ui_win_alpha_set(Eo *obj, Efl_Ui_Win_Data *sd, Eina_Bool enabled)
{
   sd->application_alpha = enabled;
   _elm_win_apply_alpha(obj, sd);
   _elm_win_frame_style_update(sd, 0, 1);
}

EOLIAN static Eina_Bool
_efl_ui_win_alpha_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   if (sd->img_obj)
     {
        return evas_object_image_alpha_get(sd->img_obj);
     }
   else
     {
        return ecore_evas_alpha_get(sd->ee);
     }

   return EINA_FALSE;
}

EOLIAN static void
_efl_ui_win_fullscreen_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool fullscreen)
{
   const char *engine_name = ecore_evas_engine_name_get(sd->ee);
   // YYY: handle if sd->img_obj
   if (engine_name &&
       ((!strcmp(engine_name, ELM_SOFTWARE_FB)) ||
        (!strcmp(engine_name, ELM_DRM))))
     {
        // these engines... can ONLY be fullscreen
        return;
     }
   else if (sd->type == ELM_WIN_FAKE)
     sd->fullscreen = !!fullscreen;
   else
     {
//        sd->fullscreen = fullscreen;
        TRAP(sd, fullscreen_set, fullscreen);

#ifdef HAVE_ELEMENTARY_X
        _elm_win_xwin_update(sd);
#endif
     }
}

EOLIAN static Eina_Bool
_efl_ui_win_fullscreen_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   const char *engine_name = ecore_evas_engine_name_get(sd->ee);

   if (engine_name &&
       ((!strcmp(engine_name, ELM_SOFTWARE_FB)) ||
        (!strcmp(engine_name, ELM_DRM))))
     {
        // these engines... can ONLY be fullscreen
        return EINA_TRUE;
     }
   return sd->fullscreen;
}

static inline Eo *
_main_menu_swallow_get(Efl_Ui_Win_Data *sd)
{
   if (edje_object_part_exists(sd->legacy.edje, "elm.swallow.menu"))
     {
        DBG("Detected legacy theme, using legacy swallows.");
        return sd->legacy.edje;
     }
   return sd->frame_obj;
}

static void
_main_menu_resize_cb(void *data EINA_UNUSED, const Efl_Event *ev)
{
   // After resize, the framespace size has changed, so update the win geometry
   _elm_win_resize_objects_eval(ev->object);
   efl_event_callback_del(ev->object, EFL_GFX_EVENT_RESIZE, _main_menu_resize_cb, NULL);
}

static void
_dbus_menu_set(Eina_Bool dbus_connect, void *data)
{
   ELM_WIN_DATA_GET_OR_RETURN(data, sd);
   Eo *swallow = _main_menu_swallow_get(sd);

   if (dbus_connect)
     {
        DBG("Setting menu to D-Bus");
        edje_object_part_unswallow(swallow, sd->main_menu);
        sd->csd.need_menu = EINA_FALSE;
        _elm_menu_menu_bar_hide(sd->main_menu);
        _elm_win_resize_objects_eval(sd->obj);
     }
   else
     {
        DBG("Setting menu to local mode");
        efl_event_callback_add(sd->obj, EFL_GFX_EVENT_RESIZE, _main_menu_resize_cb, NULL);
        edje_object_part_swallow(swallow, "elm.swallow.menu", sd->main_menu);
        evas_object_show(sd->main_menu);
        if (swallow == sd->frame_obj)
          {
             efl_canvas_object_is_frame_object_set(sd->main_menu, EINA_TRUE);
             sd->csd.need_menu = EINA_TRUE;
          }
     }
   _elm_win_frame_style_update(sd, 0, 1);
   sd->deferred_resize_job = EINA_TRUE;
}

EOLIAN static void
_efl_ui_win_maximized_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool maximized)
{
   _elm_win_frame_style_update(sd, 0, 1);
   // YYY: handle if sd->img_obj
   TRAP(sd, maximized_set, maximized);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static Eina_Bool
_efl_ui_win_maximized_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->maximized;
}

EOLIAN static void
_efl_ui_win_iconified_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool iconified)
{
//   sd->iconified = iconified;
   TRAP(sd, iconified_set, iconified);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static Eina_Bool
_efl_ui_win_iconified_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->iconified;
}

EOLIAN static void
_efl_ui_win_available_profiles_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char **profiles, unsigned int count)
{
   Eina_Bool found = EINA_FALSE;

   _elm_win_available_profiles_del(sd);
   if ((profiles) && (count >= 1))
     {
        sd->profile.available_list = calloc(count, sizeof(char *));
        if (sd->profile.available_list)
          {
             if (!sd->profile.name) found = EINA_TRUE;

             unsigned int i;
             for (i = 0; i < count; i++)
               {
                  sd->profile.available_list[i] = eina_stringshare_add(profiles[i]);

                  /* check to see if a given array has a current profile of elm_win */
                  if ((sd->profile.name) &&
                      (!strcmp(sd->profile.name, profiles[i])))
                    {
                       found = EINA_TRUE;
                    }
               }
             sd->profile.count = count;
          }
     }

   if (ecore_evas_window_profile_supported_get(sd->ee))
     {
        ecore_evas_window_available_profiles_set(sd->ee,
                                                 sd->profile.available_list,
                                                 sd->profile.count);

        /* current profile of elm_win is wrong, change profile */
        if ((sd->profile.available_list) && (!found))
          {
             eina_stringshare_replace(&(sd->profile.name),
                                      sd->profile.available_list[0]);
             ecore_evas_window_profile_set(sd->ee, sd->profile.name);
          }

     }
   else
     {
        if (sd->profile.available_list)
          _elm_win_profile_update(sd);
     }
}

EOLIAN static Eina_Bool
_efl_ui_win_available_profiles_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, char ***profiles, unsigned int *count)
{
   if (ecore_evas_window_profile_supported_get(sd->ee))
     {
        return ecore_evas_window_available_profiles_get(sd->ee,
                                                       profiles,
                                                       count);
     }
   else
     {
        if (profiles) *profiles = (char **)sd->profile.available_list;
        if (count) *count = sd->profile.count;
        return EINA_TRUE;
     }
}

EOLIAN static void
_efl_ui_win_urgent_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Efl_Ui_Win_Urgent_Mode urgent)
{
   Eina_Bool urgent_tmp = !!urgent;

   if (sd->urgent == urgent_tmp) return;
   
   sd->urgent = urgent_tmp;
   TRAP(sd, urgent_set, urgent_tmp);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static Efl_Ui_Win_Urgent_Mode
_efl_ui_win_urgent_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   if (sd->urgent) return EFL_UI_WIN_URGENT_URGENT;
   return EFL_UI_WIN_URGENT_NONE;
}

EOLIAN static void
_efl_ui_win_modal_set(Eo *obj, Efl_Ui_Win_Data *sd, Efl_Ui_Win_Modal_Mode modal)
{
   Eina_Bool modal_tmp = !!modal;

   if (sd->modal_count) return;

   if ((modal_tmp) && (!sd->modal) && (evas_object_visible_get(obj)))
     _elm_win_modality_increment(sd);
   else if ((!modal_tmp) && (sd->modal) && (evas_object_visible_get(obj)))
     _elm_win_modality_decrement(sd);

   sd->modal = modal_tmp;
   TRAP(sd, modal_set, modal_tmp);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static Efl_Ui_Win_Modal_Mode
_efl_ui_win_modal_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   if (sd->modal) return EFL_UI_WIN_MODAL_MODAL;
   return EFL_UI_WIN_MODAL_NONE;
}

static void
_win_aspect_set(Efl_Ui_Win_Data *sd, double aspect)
{
   sd->aspect = aspect;
   TRAP(sd, aspect_set, aspect);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

static double
_win_aspect_get(Efl_Ui_Win_Data *sd)
{
   return sd->aspect;
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_aspect_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd,
                                              Efl_Gfx_Size_Hint_Aspect mode EINA_UNUSED, int w, int h)
{
   pd->aspect_w = w;
   pd->aspect_h = h;
   if (h) _win_aspect_set(pd, (double) w / (double) h);
   else _win_aspect_set(pd, 0.0);
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_aspect_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd,
                                              Efl_Gfx_Size_Hint_Aspect *mode, int *w, int *h)
{
   if (mode) *mode = EFL_GFX_SIZE_HINT_ASPECT_NONE;
   if (w) *w = pd->aspect_w;
   if (h) *h = pd->aspect_h;
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_base_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int w, int h)
{
   sd->size_base_w = w;
   sd->size_base_h = h;
   TRAP(sd, size_base_set, w, h);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_base_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int *w, int *h)
{
   if (w) *w = sd->size_base_w;
   if (h) *h = sd->size_base_h;
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_step_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int w, int h)
{
   sd->size_step_w = w;
   sd->size_step_h = h;
   TRAP(sd, size_step_set, w, h);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_max_set(Eo *obj, Efl_Ui_Win_Data *sd, int w, int h)
{
   if (sd->tmp_updating_hints)
     {
        efl_gfx_size_hint_max_set(efl_super(obj, MY_CLASS), w, h);
     }
   else
     {
        if (w < 1) w = -1;
        if (h < 1) h = -1;
        sd->max_w = w;
        sd->max_h = h;
        _elm_win_resize_objects_eval(obj);
     }
}

EOLIAN static void
_efl_ui_win_efl_gfx_size_hint_hint_step_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int *w, int *h)
{
   if (w) *w = sd->size_step_w;
   if (h) *h = sd->size_step_h;
}

EAPI void
elm_win_norender_push(Evas_Object *obj)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   sd->norender++;
   if (sd->norender == 1) ecore_evas_manual_render_set(sd->ee, EINA_TRUE);
}

EAPI void
elm_win_norender_pop(Evas_Object *obj)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (sd->norender <= 0) return;
   sd->norender--;
   if (sd->norender == 0) ecore_evas_manual_render_set(sd->ee, EINA_FALSE);
}

EAPI int
elm_win_norender_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) - 1;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, -1);
   return sd->norender;
}

EAPI void
elm_win_render(Evas_Object *obj)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);
   ecore_evas_manual_render(sd->ee);
}

static int
_win_rotation_degree_check(int rotation)
{
   if ((rotation > 360) || (rotation < 0))
     {
        WRN("Rotation degree should be 0 ~ 360 (passed degree: %d)", rotation);
        rotation %= 360;
        if (rotation < 0) rotation += 360;
     }
   return rotation;
}

/*
 * This API resizes the internal window(ex: X window) and evas_output.
 * But this does not resize the elm window object and its contents.
 */
static void
_win_rotate(Evas_Object *obj, Efl_Ui_Win_Data *sd, int rotation, Eina_Bool resize)
{
   rotation = _win_rotation_degree_check(rotation);
   if (sd->rot == rotation) return;
   sd->rot = rotation;
   if (resize) TRAP(sd, rotation_with_resize_set, rotation);
   else TRAP(sd, rotation_set, rotation);
   efl_gfx_size_hint_restricted_min_set(obj, -1, -1);
   efl_gfx_size_hint_max_set(obj, -1, -1);
   _elm_win_resize_objects_eval(obj);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
   _elm_win_frame_obj_update(sd);
   elm_widget_orientation_set(obj, rotation);
   efl_event_callback_legacy_call
     (obj, EFL_UI_WIN_EVENT_ROTATION_CHANGED, NULL);
}

EOLIAN static void
_efl_ui_win_wm_available_rotations_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const int *rotations, unsigned int count)
{
   unsigned int i;
   int r;

   if (!sd->wm_rot.use)
     sd->wm_rot.use = EINA_TRUE;

   ELM_SAFE_FREE(sd->wm_rot.rots, free);
   sd->wm_rot.count = 0;

   if (count > 0)
     {
        sd->wm_rot.rots = calloc(count, sizeof(int));
        if (!sd->wm_rot.rots) return;
        for (i = 0; i < count; i++)
          {
             r = _win_rotation_degree_check(rotations[i]);
             sd->wm_rot.rots[i] = r;
          }
     }

   sd->wm_rot.count = count;

   ecore_evas_wm_rotation_available_rotations_set(sd->ee,
                                                  sd->wm_rot.rots,
                                                  sd->wm_rot.count);
}

EOLIAN static Eina_Bool
_efl_ui_win_wm_available_rotations_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int **rotations, unsigned int *count)
{
   if (!sd->wm_rot.use) return EINA_FALSE;

   if (sd->wm_rot.count > 0)
     {
        if (rotations)
          {
             *rotations = calloc(sd->wm_rot.count, sizeof(int));
             if (*rotations)
               {
                  memcpy(*rotations,
                         sd->wm_rot.rots,
                         sizeof(int) * sd->wm_rot.count);
               }
          }
     }

   if (count) *count = sd->wm_rot.count;
   return EINA_TRUE;
}

EOLIAN static void
_efl_ui_win_sticky_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool sticky)
{
//   sd->sticky = sticky;
   TRAP(sd, sticky_set, sticky);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EOLIAN static Eina_Bool
_efl_ui_win_sticky_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->sticky;
}

EOLIAN static void
_efl_ui_win_keyboard_mode_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Elm_Win_Keyboard_Mode mode)
{
   if (mode == sd->kbdmode) return;
#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
#endif
   sd->kbdmode = mode;
#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin)
     ecore_x_e_virtual_keyboard_state_set
       (sd->x.xwin, (Ecore_X_Virtual_Keyboard_State)sd->kbdmode);
#endif
}

EOLIAN static Elm_Win_Keyboard_Mode
_efl_ui_win_keyboard_mode_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->kbdmode;
}

EOLIAN static void
_efl_ui_win_indicator_enabled_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd EINA_UNUSED, Eina_Bool enable EINA_UNUSED)
{
   //TODO: this mode will be implemented after removing the conformant.
   return;
}

EOLIAN static Eina_Bool
_efl_ui_win_indicator_enabled_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   //TODO: this mode will be implemented after removing the conformant.
   return EINA_FALSE;
}

EOLIAN static void
_efl_ui_win_indicator_type_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd EINA_UNUSED, Efl_Ui_Win_Indicator_Type type EINA_UNUSED)
{
   //TODO: this mode will be implemented after removing the conformant.
   return;
}

EOLIAN static Efl_Ui_Win_Indicator_Type
_efl_ui_win_indicator_type_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   //TODO: this mode will be implemented after removing the conformant.

   return EFL_UI_WIN_INDICATOR_TYPE_UNKNOWN;
}

EOLIAN static Eina_Bool
_efl_ui_win_focus_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return ecore_evas_focus_get(sd->ee);
}

EOLIAN static void
_efl_ui_win_screen_constrain_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool constrain)
{
   sd->constrain = !!constrain;
}

EOLIAN static Eina_Bool
_efl_ui_win_screen_constrain_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->constrain;
}

EOLIAN static void
_efl_ui_win_efl_screen_screen_size_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int *w, int *h)
{
   ecore_evas_screen_geometry_get(sd->ee, NULL, NULL, w, h);
}

EOLIAN static void
_efl_ui_win_efl_screen_screen_dpi_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, int *xdpi, int *ydpi)
{
   ecore_evas_screen_dpi_get(sd->ee, xdpi, ydpi);
}

EOLIAN static int
_efl_ui_win_efl_screen_screen_rotation_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   //TODO: query to wm about device's rotation
   (void)sd;

   return 0;
}

EOLIAN static void
_efl_ui_win_prop_focus_skip_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool skip)
{
   sd->skip_focus = skip;
   TRAP(sd, focus_skip_set, skip);
}

EOLIAN static Eina_Bool
_efl_ui_win_keygrab_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *key, Evas_Modifier_Mask modifiers EINA_UNUSED, Evas_Modifier_Mask not_modifiers EINA_UNUSED, int priority EINA_UNUSED, Efl_Ui_Win_Keygrab_Mode grab_mode)
{
   Eina_Bool ret = EINA_FALSE;
#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     {
        Ecore_X_Win_Keygrab_Mode x_grab_mode;
        switch (grab_mode)
          {
           case ELM_WIN_KEYGRAB_SHARED:
             x_grab_mode = ECORE_X_WIN_KEYGRAB_SHARED;
             break;
           case ELM_WIN_KEYGRAB_TOPMOST:
             x_grab_mode = ECORE_X_WIN_KEYGRAB_TOPMOST;
             break;
           case ELM_WIN_KEYGRAB_EXCLUSIVE:
             x_grab_mode = ECORE_X_WIN_KEYGRAB_EXCLUSIVE;
             break;
           case ELM_WIN_KEYGRAB_OVERRIDE_EXCLUSIVE:
             x_grab_mode = ECORE_X_WIN_KEYGRAB_OVERRIDE_EXCLUSIVE;
             break;
           default:
             return ret;
          }
         ret = ecore_x_window_keygrab_set(sd->x.xwin, key, 0, 0, 0, x_grab_mode);
     }
#else
   (void)sd;
   (void)key;
   (void)grab_mode;
#endif
   return ret;
}

EOLIAN static Eina_Bool
_efl_ui_win_keygrab_unset(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *key, Evas_Modifier_Mask modifiers EINA_UNUSED, Evas_Modifier_Mask not_modifiers EINA_UNUSED)
{
   Eina_Bool ret = EINA_FALSE;
#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     ret = ecore_x_window_keygrab_unset(sd->x.xwin, key, 0, 0);
#else
   (void)sd;
   (void)key;
#endif
   return ret;
}

EOLIAN static void
_efl_ui_win_focus_highlight_enabled_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool enabled)
{
   enabled = !!enabled;
   if (sd->focus_highlight.enabled == enabled)
     return;

   sd->focus_highlight.enabled = enabled;

   if ((sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled))
     _elm_win_focus_highlight_init(sd);
   else
     _elm_win_focus_highlight_shutdown(sd);
}

EOLIAN static Eina_Bool
_efl_ui_win_focus_highlight_enabled_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->focus_highlight.enabled;
}

static Elm_Theme_Apply
_elm_win_theme_internal(Eo *obj, Efl_Ui_Win_Data *sd)
{
   Elm_Theme_Apply int_ret = ELM_THEME_APPLY_FAILED;
   Eina_Bool ret = EINA_FALSE, prev_alpha;
   const char *s;

   int_ret = _elm_theme_object_set(obj, sd->legacy.edje, "win", "base",
                                   elm_widget_style_get(obj));
   if (!int_ret) return ELM_THEME_APPLY_FAILED;

   edje_object_mirrored_set(sd->legacy.edje, elm_widget_mirrored_get(obj));
   edje_object_scale_set(sd->legacy.edje,
                         elm_widget_scale_get(obj) * elm_config_scale_get());

   efl_event_callback_legacy_call(obj, EFL_UI_WIN_EVENT_THEME_CHANGED, NULL);
   ret = elm_obj_widget_disable(obj);

   if (!ret) int_ret = ELM_THEME_APPLY_FAILED;

   prev_alpha = sd->theme_alpha;
   s = edje_object_data_get(sd->legacy.edje, "alpha");
   sd->theme_alpha = (eina_streq(s, "1") || eina_streq(s, "true"));
   if (sd->theme_alpha != prev_alpha)
     _elm_win_apply_alpha(obj, sd);

   return int_ret;
}

EOLIAN static Elm_Theme_Apply
_efl_ui_win_elm_widget_theme_apply(Eo *obj, Efl_Ui_Win_Data *sd)
{
   Elm_Theme_Apply int_ret = ELM_THEME_APPLY_FAILED;
   int_ret = elm_obj_widget_theme_apply(efl_super(obj, MY_CLASS));
   if (!int_ret) return ELM_THEME_APPLY_FAILED;

   sd->focus_highlight.theme_changed = EINA_TRUE;

   int_ret = _elm_win_theme_internal(obj, sd) & int_ret;
   if (!int_ret) return ELM_THEME_APPLY_FAILED;
   _elm_win_focus_highlight_reconfigure_job_start(sd);

   return int_ret;
}

EOLIAN static void
_efl_ui_win_focus_highlight_style_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *style)
{
   eina_stringshare_replace(&sd->focus_highlight.style, style);
   sd->focus_highlight.theme_changed = EINA_TRUE;
   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

EOLIAN static const char*
_efl_ui_win_focus_highlight_style_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->focus_highlight.style;
}

EOLIAN static void
_efl_ui_win_focus_highlight_animate_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool animate)
{
   animate = !!animate;
   if (sd->focus_highlight.animate == animate)
     return;

   sd->focus_highlight.animate = animate;
   sd->focus_highlight.theme_changed = EINA_TRUE;
   _elm_win_focus_highlight_reconfigure_job_start(sd);
}

EOLIAN static Eina_Bool
_efl_ui_win_focus_highlight_animate_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->focus_highlight.animate;
}

EOLIAN static const char *
_efl_ui_win_stack_id_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->stack_id;
}

EOLIAN static void
_efl_ui_win_stack_master_id_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *id)
{
   if (sd->shown) return;
   eina_stringshare_replace(&(sd->stack_master_id), id);
#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin) _elm_win_xwin_update(sd);
#endif
}

EOLIAN static const char *
_efl_ui_win_stack_master_id_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->stack_master_id;
}

EOLIAN static void
_efl_ui_win_stack_base_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Eina_Bool base)
{
   if (sd->shown) return;
   sd->stack_base = !!base;
}

EOLIAN static Eina_Bool
_efl_ui_win_stack_base_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return sd->stack_base;
}

#ifdef HAVE_ELEMENTARY_X
// list transient from bottom to top by recursive walking
static void
_x_transients_for_list(Ecore_X_Window base, Ecore_X_Window transient,
                       Ecore_X_Window **wins, int *num)
{
   Ecore_X_Window t, *children, *w;
   int n, i;

   children = ecore_x_window_children_get(base, &n);
   if (children)
     {
        for (i = 0; i < n; i++)
          {
             if (children[i] != transient)
               {
                  t = ecore_x_icccm_transient_for_get(children[i]);
                  if (t == transient)
                    {
                       (*num)++;
                       w = realloc(*wins, *num * sizeof(Ecore_X_Window));
                       if (w)
                         {
                            *wins = w;
                            (*wins)[*num - 1] = children[i];
                         }
                    }
                  _x_transients_for_list(children[i], transient, wins, num);
               }
          }
        free(children);
     }
}
#endif

EOLIAN static void
_efl_ui_win_stack_pop_to(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   if (!sd->stack_master_id) return;
#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin)
     {
        Ecore_X_Window *wins = NULL;
        int i, num = 0;
        Eina_Bool del = EINA_FALSE;

        ecore_x_grab();
        _x_transients_for_list
          (ecore_x_window_root_get(sd->x.xwin),
           ecore_x_icccm_transient_for_get(sd->x.xwin),
           &wins, &num);
        if (wins)
          {
             for (i = 0; i < num; i++)
               {
                  if (del) ecore_x_window_delete_request_send(wins[i]);
                  if (wins[i] == sd->x.xwin) del = EINA_TRUE;
               }
             free(wins);
          }
        ecore_x_ungrab();
     }
#endif
   // wayland - needs to be a special compositor request
   // win32/osx ?
}

static void
_handle_msg(Ecore_Evas *ee, int domain, int msg, void *data, int size)
{
   int i = 1;
   ERR("Parent message recieved. %d %d. Answering...", domain, msg);
   ecore_evas_msg_send(ee, 123, 1, &msg, sizeof(msg));
}

static Eina_Bool
_timer_cb(void *data)
{
   Ecore_Evas *ee = data;
   int msg = 1;
   ERR("Send message to parent");
   ecore_evas_msg_send(ee, 123, 1, &msg, sizeof(msg));
   return EINA_TRUE;
}


EOLIAN static Eina_Bool
_efl_ui_win_socket_listen(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *svcname, int svcnum, Eina_Bool svcsys)
{
   if (!sd->ee) return EINA_FALSE;

   if (!ecore_evas_extn_socket_listen(sd->ee, svcname, svcnum, svcsys))
     return EINA_FALSE;

   ERR("Svcname: %s", svcname);
   ecore_evas_callback_msg_parent_handle_set(sd->ee, _handle_msg);

   return EINA_TRUE;
}

EAPI Eina_Bool
elm_win_trap_set(const Elm_Win_Trap *t)
{
   DBG("old %p, new %p", trap, t);

   if ((t) && (t->version != ELM_WIN_TRAP_VERSION))
     {
        CRI("trying to set a trap version %lu while %lu was expected!",
                 t->version, ELM_WIN_TRAP_VERSION);
        return EINA_FALSE;
     }

   trap = t;
   return EINA_TRUE;
}

EAPI void
elm_win_floating_mode_set(Evas_Object *obj, Eina_Bool floating)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   floating = !!floating;
   if (floating == sd->floating) return;
   sd->floating = floating;
#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     {
        if (sd->floating)
          ecore_x_e_illume_window_state_set
             (sd->x.xwin, ECORE_X_ILLUME_WINDOW_STATE_FLOATING);
        else
          ecore_x_e_illume_window_state_set
             (sd->x.xwin, ECORE_X_ILLUME_WINDOW_STATE_NORMAL);
     }
#endif
}

EAPI Eina_Bool
elm_win_floating_mode_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return sd->floating;
}

void
_elm_win_focus_highlight_in_theme_update(Evas_Object *obj, Eina_Bool in_theme)
{
   ELM_WIN_DATA_GET(obj, sd);
   sd->focus_highlight.cur.in_theme = !!in_theme;
}

void
_elm_win_focus_highlight_start(Evas_Object *obj)
{
   ELM_WIN_DATA_GET(obj, sd);

   if (!(sd->focus_highlight.enabled) && !(sd->focus_highlight.auto_enabled)) return;
   sd->focus_highlight.cur.visible = EINA_TRUE;
   sd->focus_highlight.geometry_changed = EINA_TRUE;
   _elm_win_focus_highlight_reconfigure_job(obj);
}

void
_elm_win_focus_auto_show(Evas_Object *obj)
{
   ELM_WIN_DATA_GET(obj, sd);
   Eina_Bool pfocus = (sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled);
   sd->focus_highlight.auto_enabled = _elm_config->win_auto_focus_enable;
   sd->focus_highlight.auto_animate = _elm_config->win_auto_focus_animate;
   if (pfocus != ((sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled)))
     {
        if ((sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled))
          _elm_win_focus_highlight_init(sd);
     }
}

void
_elm_win_focus_auto_hide(Evas_Object *obj)
{
   ELM_WIN_DATA_GET(obj, sd);
   Eina_Bool pfocus = (sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled);
   sd->focus_highlight.auto_enabled = EINA_FALSE;
   sd->focus_highlight.auto_animate = EINA_FALSE;
   if (pfocus != ((sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled)))
     {
        if (!((sd->focus_highlight.enabled) || (sd->focus_highlight.auto_enabled)))
          _elm_win_focus_highlight_shutdown(sd);
     }
}

static void
_on_atspi_bus_connected(void *data EINA_UNUSED, const Efl_Event *event EINA_UNUSED)
{
   Evas_Object *win;
   Eina_List *l;

   EINA_LIST_FOREACH(_elm_win_list, l, win)
     {
        /**
         * Reemit accessibility events when AT-SPI2 connection is begin
         * established. This assures that Assistive Technology clients will
         * receive all org.a11y.window events and could keep track of active
         * windows whithin system.
         */
        elm_interface_atspi_window_created_signal_emit(win);
        if (elm_win_focus_get(win))
          {
             elm_interface_atspi_window_activated_signal_emit(win);
             /** Reemit focused event to inform atspi clients about currently
              * focused object **/
             unsigned int order = 0;
             Evas_Object *target;
             target = elm_obj_widget_newest_focus_order_get(win, &order, EINA_TRUE);
             if (target)
               elm_interface_atspi_accessible_state_changed_signal_emit(target, ELM_ATSPI_STATE_FOCUSED, EINA_TRUE);
          }
        else
          elm_interface_atspi_window_deactivated_signal_emit(win);
     }
}

EOLIAN static void
_efl_ui_win_class_constructor(Efl_Class *klass)
{
   evas_smart_legacy_type_register(MY_CLASS_NAME_LEGACY, klass);

   if (_elm_config->atspi_mode)
     {
        Eo *bridge = _elm_atspi_bridge_get();
        if (bridge)
           efl_event_callback_add(bridge, ELM_ATSPI_BRIDGE_EVENT_CONNECTED, _on_atspi_bus_connected, NULL);
     }
}

EOLIAN static const Elm_Atspi_Action*
_efl_ui_win_elm_interface_atspi_widget_action_elm_actions_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   static Elm_Atspi_Action atspi_actions[] = {
          { "move,previous", "move", "previous", _key_action_move},
          { "move,next", "move", "next", _key_action_move},
          { "move,left", "move", "left", _key_action_move},
          { "move,right", "move", "right", _key_action_move},
          { "move,up", "move", "up", _key_action_move},
          { "move,down", "move", "down", _key_action_move},
          { NULL, NULL, NULL, NULL }
   };
   return &atspi_actions[0];
}

EOLIAN static Elm_Atspi_State_Set
_efl_ui_win_elm_interface_atspi_accessible_state_set_get(Eo *obj, Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   Elm_Atspi_State_Set ret;
   ret = elm_interface_atspi_accessible_state_set_get(efl_super(obj, MY_CLASS));

   if (elm_win_focus_get(obj))
     STATE_TYPE_SET(ret, ELM_ATSPI_STATE_ACTIVE);

   return ret;
}

EOLIAN static const char*
_efl_ui_win_elm_interface_atspi_accessible_name_get(Eo *obj, Efl_Ui_Win_Data *sd EINA_UNUSED)
{
   const char *ret;
   ret = elm_interface_atspi_accessible_name_get(efl_super(obj, EFL_UI_WIN_CLASS));
   if (ret) return ret;
   const char *name = elm_win_title_get(obj);
   return name;
}

EOLIAN static Eina_Bool
_efl_ui_win_efl_input_state_modifier_enabled_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd, const char *name)
{
   const Evas_Modifier *m = evas_key_modifier_get(pd->evas);
   return evas_key_modifier_is_set(m, name);
}

EOLIAN static Eina_Bool
_efl_ui_win_efl_input_state_lock_enabled_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *pd, const char *name)
{
   const Evas_Lock *m = evas_key_lock_get(pd->evas);
   return evas_key_lock_is_set(m, name);
}

// See evas_inline.x
#define _EVAS_COLOR_CLAMP(x, y) do { \
if (x > y) { x = y; bad = 1; } \
if (x < 0) { x = 0; bad = 1; } } while (0)

#define EVAS_COLOR_SANITIZE(r, g, b, a) \
({ int bad = 0; \
   _EVAS_COLOR_CLAMP(a, 255); \
   _EVAS_COLOR_CLAMP(r, a); \
   _EVAS_COLOR_CLAMP(g, a); \
   _EVAS_COLOR_CLAMP(b, a); \
   bad; })

/* Efl.Part APIs */

#define WIN_PART_ERR(part) ERR("No such part in window: '%s'. Supported parts are: 'background'.", part);

static Eina_Bool
_elm_win_bg_set(Efl_Ui_Win_Data *sd, Eo *bg)
{
   ELM_SAFE_DEL(sd->bg);
   if (!bg) return EINA_TRUE;

   if (!elm_widget_sub_object_add(sd->obj, bg))
     return EINA_FALSE;
   if (!edje_object_part_swallow(sd->frame_obj, "elm.swallow.background", bg))
     return EINA_FALSE;
   efl_gfx_visible_set(bg, 1);
   efl_gfx_size_hint_align_set(bg, -1, -1);
   efl_gfx_size_hint_weight_set(bg, 1, 1);
   efl_wref_add(bg, &sd->bg);
   return EINA_TRUE;
}

/* Legacy theme compatibility */
static Eina_Bool
_elm_win_bg_must_swallow(Efl_Ui_Win_Data *sd)
{
   if (EINA_UNLIKELY(!sd->legacy.bg_must_swallow_init))
     {
        /* Overkill: check which theme version the standard elm_bg uses */
        Elm_Widget_Smart_Data *wd;
        const char *version;
        Eo *bg;
        int v;

        sd->legacy.bg_must_swallow = 1;
        sd->legacy.bg_must_swallow_init = 1;

        bg = elm_bg_add(sd->obj);
        wd = efl_data_scope_get(bg, ELM_WIDGET_CLASS);
        if (wd)
          {
             version = edje_object_data_get(wd->resize_obj, "version");
             v = version ? atoi(version) : 0;
             if (v >= FRAME_OBJ_THEME_MIN_VERSION)
               sd->legacy.bg_must_swallow = 0;
          }
        evas_object_del(bg);
     }

   return sd->legacy.bg_must_swallow;
}

void
_elm_win_standard_init(Eo *obj)
{
   /* Support for elm_util_win_standard_add() and Efl.Ui.Win.Standard */
   Efl_Ui_Win_Data *sd = efl_data_scope_get(obj, MY_CLASS);

   ELM_SAFE_DEL(sd->bg);

   sd->csd.need_bg_standard = 1;
   if (!_elm_win_bg_must_swallow(sd))
     {
        sd->csd.need_bg_solid = EINA_TRUE;
     }
   else
     {
        /* Legacy theme compatibility */
        DBG("Detected legacy theme used for elm_bg. Swallowing object.");
        sd->csd.need_bg_solid = EINA_FALSE;
        _elm_win_bg_set(sd, efl_add(ELM_BG_CLASS, obj));
     }

   _elm_win_frame_style_update(sd, 0, 1);
}

static Eina_Bool
_efl_ui_win_content_set(Eo *obj, Efl_Ui_Win_Data *sd, const char *part, Eo *content)
{
   sd->legacy.forbidden = EINA_TRUE;
   if (!part) part = "content";
   if (eina_streq(part, "content"))
     {
        if (sd->content == content) return EINA_TRUE;
        if (!elm_widget_sub_object_add(obj, content))
          goto err;
        /* FIXME: Switch to swallow inside the frame
        if (!edje_object_part_swallow(sd->frame_obj, "elm.swallow.client", content))
          goto err;
        */
        evas_object_box_append(sd->legacy.box, content);
        evas_object_show(content);
        efl_wref_add(content, &sd->content);
        return EINA_TRUE;
     }
   else if (eina_streq(part, "background"))
     {
        sd->csd.need_bg_standard = 0;
        if (sd->bg == content) return EINA_TRUE;
        if (!_elm_win_bg_set(sd, content))
          goto err;
        return EINA_TRUE;
     }

   WIN_PART_ERR(part);
   return EINA_FALSE;

err:
   ERR("Failed to set object %p as %s for window %p", content, part, obj);
   return EINA_FALSE;
}

static Efl_Canvas_Object *
_efl_ui_win_content_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *part)
{
   sd->legacy.forbidden = EINA_TRUE;
   if (!part) part = "content";
   if (eina_streq(part, "content"))
     return sd->content;
   else if (eina_streq(part, "background"))
     return sd->bg;

   WIN_PART_ERR(part);
   return NULL;
}

static Efl_Canvas_Object *
_efl_ui_win_content_unset(Eo *obj, Efl_Ui_Win_Data *sd, const char *part)
{
   Eo *content;

   sd->legacy.forbidden = EINA_TRUE;
   content = _efl_ui_win_content_get(obj, sd, part);
   if (!content) return NULL;

   efl_ref(content);
   _efl_ui_win_content_set(obj, sd, part, NULL);
   return content;
}

static Eina_Bool
_efl_ui_win_part_color_set(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *part, int r, int g, int b, int a)
{
   sd->legacy.forbidden = EINA_TRUE;
   if (eina_streq(part, "background"))
     {
        sd->csd.need_bg_solid = EINA_TRUE;
        edje_object_color_class_set(sd->frame_obj, "elm/win/background", r, g, b, a, 0, 0, 0, 0, 0, 0, 0, 0);
        _elm_win_frame_style_update(sd, 0, 1);
        return EINA_TRUE;
     }

   WIN_PART_ERR(part);
   return EINA_FALSE;
}

static Eina_Bool
_efl_ui_win_part_color_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, const char *part, int *r, int *g, int *b, int *a)
{
   sd->legacy.forbidden = EINA_TRUE;
   if (eina_streq(part, "background"))
     {
        edje_object_color_class_get(sd->frame_obj, "elm/win/background", r, g, b, a, 0, 0, 0, 0, 0, 0, 0, 0);
        return EINA_TRUE;
     }

   WIN_PART_ERR(part);
   return EINA_FALSE;
}

static Eina_Bool
_efl_ui_win_part_file_set(Eo *obj, Efl_Ui_Win_Data *sd, const char *part, const char *file, const char *key)
{
   sd->legacy.forbidden = EINA_TRUE;
   if (eina_streq(part, "background"))
     {
        Eina_Bool ok = EINA_TRUE;
        Eo *bg = NULL;

        if (file)
          {
             bg = efl_add(EFL_UI_IMAGE_CLASS, obj);
             efl_ui_image_scale_type_set(bg, EFL_UI_IMAGE_SCALE_TYPE_FIT_OUTSIDE);
             ok = efl_file_set(bg, file, key);
             if (!ok) ELM_SAFE_DEL(bg);
             _elm_win_bg_set(sd, bg);
          }
        else
          {
             _elm_win_standard_init(obj);
          }

        return ok;
     }

   WIN_PART_ERR(part);
   return EINA_FALSE;
}

static Eina_Bool
_efl_ui_win_part_file_get(Eo *obj, Efl_Ui_Win_Data *sd, const char *part, const char **file, const char **key)
{
   sd->legacy.forbidden = EINA_TRUE;
   if (file) *file = NULL;
   if (key) *key = NULL;

   if (eina_streq(part, "background"))
     {
        const Eo *bg = _efl_ui_win_content_get(obj, sd, "background");
        efl_file_get(bg, file, key);
        return EINA_TRUE;
     }

   WIN_PART_ERR(part);
   return EINA_FALSE;
}

/* Efl.Part begin */

static void
_efl_ui_win_internal_part_efl_gfx_color_set(Eo *obj, Elm_Part_Data *pd, int r, int g, int b, int a)
{
   if (EVAS_COLOR_SANITIZE(r, g, b, a))
     ERR("Evas only handles premultiplied colors (0 <= R,G,B <= A <= 255)");

   _efl_ui_win_part_color_set(pd->obj, pd->sd, pd->part, r, g, b, a);
   ELM_PART_RETURN_VOID;
}

static void
_efl_ui_win_internal_part_efl_gfx_color_get(Eo *obj, Elm_Part_Data *pd, int *r, int *g, int *b, int *a)
{
   _efl_ui_win_part_color_get(pd->obj, pd->sd, pd->part, r, g, b, a);
   ELM_PART_RETURN_VOID;
}

static Eina_Bool
_efl_ui_win_internal_part_efl_file_file_set(Eo *obj, Elm_Part_Data *pd, const char *file, const char *key)
{
   ELM_PART_RETURN_VAL(_efl_ui_win_part_file_set(pd->obj, pd->sd, pd->part, file, key));
}

static void
_efl_ui_win_internal_part_efl_file_file_get(Eo *obj, Elm_Part_Data *pd, const char **file, const char **key)
{
   _efl_ui_win_part_file_get(pd->obj, pd->sd, pd->part, file, key);
   ELM_PART_RETURN_VOID;
}

EOLIAN static Eina_Bool
_efl_ui_win_efl_container_content_set(Eo *obj, Efl_Ui_Win_Data *sd, Evas_Object *content)
{
   return _efl_ui_win_content_set(obj, sd, NULL, content);
}

EOLIAN static Evas_Object*
_efl_ui_win_efl_container_content_get(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd)
{
   return _efl_ui_win_content_get(obj, sd, NULL);
}

EOLIAN static Evas_Object*
_efl_ui_win_efl_container_content_unset(Eo *obj, Efl_Ui_Win_Data *sd)
{
   return _efl_ui_win_content_unset(obj, sd, NULL);
}

ELM_PART_IMPLEMENT(efl_ui_win, EFL_UI_WIN, Efl_Ui_Win_Data, Elm_Part_Data)
ELM_PART_IMPLEMENT_CONTENT_SET(efl_ui_win, EFL_UI_WIN, Efl_Ui_Win_Data, Elm_Part_Data)
ELM_PART_IMPLEMENT_CONTENT_GET(efl_ui_win, EFL_UI_WIN, Efl_Ui_Win_Data, Elm_Part_Data)
ELM_PART_IMPLEMENT_CONTENT_UNSET(efl_ui_win, EFL_UI_WIN, Efl_Ui_Win_Data, Elm_Part_Data)
#include "efl_ui_win_internal_part.eo.c"

/* Efl.Part end */

EOLIAN static Eina_Bool
_efl_ui_win_move_resize_start(Eo *obj EINA_UNUSED, Efl_Ui_Win_Data *sd, Efl_Ui_Win_Move_Resize_Mode mode)
{
   return _win_move_resize_start(sd, mode);
}

/* windowing specific calls - shall we do this differently? */

EAPI Ecore_X_Window
elm_win_xwindow_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) 0;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, 0);

#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin) return sd->x.xwin;
   if (sd->parent) return elm_win_xwindow_get(sd->parent);
#endif
   return 0;
}

EAPI Ecore_Wl2_Window *
elm_win_wl_window_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);
   const char *engine_name = ecore_evas_engine_name_get(sd->ee);

   if (!(engine_name &&
         ((!strcmp(engine_name, ELM_WAYLAND_SHM)) ||
          (!strcmp(engine_name, ELM_WAYLAND_EGL)))))
     return NULL;

   if (!evas_object_smart_type_check_ptr(obj, MY_CLASS_NAME_LEGACY))
     {
        Ecore_Evas *ee = ecore_evas_ecore_evas_get(evas_object_evas_get(obj));
        return _elm_ee_wlwin_get(ee);
     }

#if HAVE_ELEMENTARY_WL2
   if (sd->wl.win) return sd->wl.win;
   if (sd->parent) return elm_win_wl_window_get(sd->parent);
#endif

   return NULL;
}

EAPI Ecore_Cocoa_Window *
elm_win_cocoa_window_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

   Ecore_Evas *ee = ecore_evas_ecore_evas_get(evas_object_evas_get(obj));
   return _elm_ee_cocoa_win_get(ee);
}

EAPI Ecore_Win32_Window *
elm_win_win32_window_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

   const char *engine_name = ecore_evas_engine_name_get(sd->ee);

   if (!(engine_name &&
         ((!strcmp(engine_name, ELM_SOFTWARE_WIN32)) ||
          (!strcmp(engine_name, ELM_SOFTWARE_DDRAW)))))
     return NULL;

   if (!evas_object_smart_type_check_ptr(obj, MY_CLASS_NAME_LEGACY))
     {
        Ecore_Evas *ee = ecore_evas_ecore_evas_get(evas_object_evas_get(obj));
        return _elm_ee_win32win_get(ee);
     }

   Ecore_Win32_Window *ret = NULL;

#if HAVE_ELEMENTARY_WIN32
   if (sd->win32.win) ret = sd->win32.win;
   if (sd->parent) ret = elm_win_win32_window_get(sd->parent);
#endif

   return ret;
}

EAPI void *
elm_win_trap_data_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

   return sd->trap_data;
}

EAPI void
elm_win_override_set(Evas_Object *obj, Eina_Bool override)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   TRAP(sd, override_set, override);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI Eina_Bool
elm_win_override_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return ecore_evas_override_get(sd->ee);
}

EAPI void
elm_win_lower(Evas_Object *obj)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   TRAP(sd, lower);
}

EAPI void
elm_win_quickpanel_set(Evas_Object *obj, Eina_Bool quickpanel)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     {
        ecore_x_e_illume_quickpanel_set(sd->x.xwin, quickpanel);
        if (quickpanel)
          {
             Ecore_X_Window_State states[2];

             states[0] = ECORE_X_WINDOW_STATE_SKIP_TASKBAR;
             states[1] = ECORE_X_WINDOW_STATE_SKIP_PAGER;
             ecore_x_netwm_window_state_set(sd->x.xwin, states, 2);
             ecore_x_icccm_hints_set(sd->x.xwin, 0, 0, 0, 0, 0, 0, 0);
          }
     }
#else
   (void)sd;
   (void)quickpanel;
#endif
}

EAPI Eina_Bool
elm_win_quickpanel_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     return ecore_x_e_illume_quickpanel_get(sd->x.xwin);
#else
   (void)sd;
#endif

   return EINA_FALSE;
}

EAPI void
elm_win_quickpanel_priority_major_set(Evas_Object *obj, int priority)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     ecore_x_e_illume_quickpanel_priority_major_set(sd->x.xwin, priority);
#else
   (void)sd;
   (void)priority;
#endif
}

EAPI int
elm_win_quickpanel_priority_major_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, -1);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     return ecore_x_e_illume_quickpanel_priority_major_get(sd->x.xwin);
#else
   (void)sd;
#endif

   return -1;
}

EAPI void
elm_win_quickpanel_priority_minor_set(Evas_Object *obj, int priority)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     ecore_x_e_illume_quickpanel_priority_minor_set(sd->x.xwin, priority);
#else
   (void)sd;
   (void)priority;
#endif
}

EAPI int
elm_win_quickpanel_priority_minor_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, -1);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     return ecore_x_e_illume_quickpanel_priority_minor_get(sd->x.xwin);
#else
   (void)sd;
#endif

   return -1;
}

EAPI void
elm_win_quickpanel_zone_set(Evas_Object *obj, int zone)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     ecore_x_e_illume_quickpanel_zone_set(sd->x.xwin, zone);
#else
   (void)sd;
   (void)zone;
#endif
}

EAPI int
elm_win_quickpanel_zone_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, 0);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     return ecore_x_e_illume_quickpanel_zone_get(sd->x.xwin);
#else
   (void)sd;
#endif

   return 0;
}

EAPI void
elm_win_indicator_mode_set(Evas_Object *obj, Elm_Win_Indicator_Mode mode)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (mode == sd->indmode) return;
#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
#endif
   sd->indmode = mode;
#ifdef HAVE_ELEMENTARY_X
   if (sd->x.xwin)
     {
        if (sd->indmode == ELM_WIN_INDICATOR_SHOW)
          ecore_x_e_illume_indicator_state_set
            (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_STATE_ON);
        else if (sd->indmode == ELM_WIN_INDICATOR_HIDE)
          ecore_x_e_illume_indicator_state_set
            (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_STATE_OFF);
     }
#endif
   efl_event_callback_legacy_call
     (obj, EFL_UI_WIN_EVENT_INDICATOR_PROP_CHANGED, NULL);
}

EAPI Elm_Win_Indicator_Mode
elm_win_indicator_mode_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, ELM_WIN_INDICATOR_UNKNOWN);

   return sd->indmode;
}

EAPI void
elm_win_indicator_opacity_set(Evas_Object *obj, Elm_Win_Indicator_Opacity_Mode mode)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (mode == sd->ind_o_mode) return;
   sd->ind_o_mode = mode;
#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     {
        if (sd->ind_o_mode == ELM_WIN_INDICATOR_OPAQUE)
          ecore_x_e_illume_indicator_opacity_set
            (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_OPAQUE);
        else if (sd->ind_o_mode == ELM_WIN_INDICATOR_TRANSLUCENT)
          ecore_x_e_illume_indicator_opacity_set
            (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_TRANSLUCENT);
        else if (sd->ind_o_mode == ELM_WIN_INDICATOR_TRANSPARENT)
          ecore_x_e_illume_indicator_opacity_set
            (sd->x.xwin, ECORE_X_ILLUME_INDICATOR_TRANSPARENT);
     }
#endif
   efl_event_callback_legacy_call
     (obj, EFL_UI_WIN_EVENT_INDICATOR_PROP_CHANGED, NULL);
}

EAPI Elm_Win_Indicator_Opacity_Mode
elm_win_indicator_opacity_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, ELM_WIN_INDICATOR_OPACITY_UNKNOWN);

   return sd->ind_o_mode;
}

EAPI void
elm_win_keyboard_win_set(Evas_Object *obj, Eina_Bool is_keyboard)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     ecore_x_e_virtual_keyboard_set(sd->x.xwin, is_keyboard);
#else
   (void)sd;
   (void)is_keyboard;
#endif
}

EAPI Eina_Bool
elm_win_keyboard_win_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin) return ecore_x_e_virtual_keyboard_get(sd->x.xwin);
#else
   (void)sd;
#endif
   return EINA_FALSE;
}

EAPI void
elm_win_conformant_set(Evas_Object *obj, Eina_Bool conformant)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     ecore_x_e_illume_conformant_set(sd->x.xwin, conformant);
#else
   (void)sd;
   (void)conformant;
#endif
}

EAPI Eina_Bool
elm_win_conformant_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     return ecore_x_e_illume_conformant_get(sd->x.xwin);
#else
   (void)sd;
#endif

   return EINA_FALSE;
}

EAPI void
elm_win_wm_rotation_manual_rotation_done_set(Evas_Object *obj, Eina_Bool set)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (!sd->wm_rot.use) return;
   ecore_evas_wm_rotation_manual_rotation_done_set(sd->ee, set);
}

EAPI Eina_Bool
elm_win_wm_rotation_manual_rotation_done_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   if (!sd->wm_rot.use) return EINA_FALSE;
   return ecore_evas_wm_rotation_manual_rotation_done_get(sd->ee);
}

EAPI void
elm_win_wm_rotation_manual_rotation_done(Evas_Object *obj)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (!sd->wm_rot.use) return;
   ecore_evas_wm_rotation_manual_rotation_done(sd->ee);
}

EAPI void
elm_win_rotation_set(Evas_Object *obj, int rotation)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   _win_rotate(obj, sd, rotation, EINA_FALSE);
}

/*
 * This API does not resize the internal window (ex: X window).
 * But this resizes evas_output, elm window, and its contents.
 */
EAPI void
elm_win_rotation_with_resize_set(Evas_Object *obj, int rotation)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   _win_rotate(obj, sd, rotation, EINA_TRUE);
}

EAPI int
elm_win_rotation_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) -1;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, -1);

   return sd->rot;
}

EAPI void
elm_win_wm_preferred_rotation_set(Evas_Object *obj, int rotation)
{
   int rot;

   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (!sd->wm_rot.use)
     sd->wm_rot.use = EINA_TRUE;

   // '-1' means that elm_win doesn't use preferred rotation.
   if (rotation == -1)
     rot = -1;
   else
     rot = _win_rotation_degree_check(rotation);

   if (sd->wm_rot.preferred_rot == rot) return;
   sd->wm_rot.preferred_rot = rot;

   ecore_evas_wm_rotation_preferred_rotation_set(sd->ee, rot);
}

EAPI int
elm_win_wm_rotation_preferred_rotation_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) -1;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, -1);

   return sd->wm_rot.preferred_rot;
}

EAPI Eina_Bool
elm_win_wm_rotation_supported_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return sd->wm_rot.wm_supported;
}

/* This will unset a preferred rotation, if given preferred rotation is '-1'.
 */
EAPI void
elm_win_wm_rotation_preferred_rotation_set(Evas_Object *obj, int rotation)
{
   ELM_WIN_CHECK(obj);
   elm_win_wm_preferred_rotation_set(obj, rotation);
}

EAPI void
elm_win_screen_size_get(const Evas_Object *obj, int *x, int *y, int *w, int *h)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   ecore_evas_screen_geometry_get(sd->ee, x, y, w, h);
}

EAPI void
elm_win_screen_position_get(const Evas_Object *obj, int *x, int *y)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (x) *x = sd->screen.x;
   if (y) *y = sd->screen.y;
}

EAPI void
elm_win_screen_dpi_get(const Evas_Object *obj, int *xdpi, int *ydpi)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   ecore_evas_screen_dpi_get(sd->ee, xdpi, ydpi);
}

EAPI void
elm_win_icon_name_set(Evas_Object *obj, const char *icon_name)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (!icon_name) return;
   eina_stringshare_replace(&(sd->icon_name), icon_name);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI const char*
elm_win_icon_name_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

   return sd->icon_name;
}

EAPI void
elm_win_withdrawn_set(Evas_Object *obj, Eina_Bool withdrawn)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

//   sd->withdrawn = withdrawn;
   TRAP(sd, withdrawn_set, withdrawn);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI Eina_Bool
elm_win_withdrawn_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return sd->withdrawn;
}

EAPI void
elm_win_urgent_set(Evas_Object *obj, Eina_Bool urgent)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (sd->urgent == urgent)
     return;
   sd->urgent = urgent;
   TRAP(sd, urgent_set, urgent);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI Eina_Bool
elm_win_urgent_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return sd->urgent;
}

EAPI void
elm_win_demand_attention_set(Evas_Object *obj, Eina_Bool demand_attention)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   sd->demand_attention = demand_attention;
   TRAP(sd, demand_attention_set, demand_attention);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI Eina_Bool
elm_win_demand_attention_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return sd->demand_attention;
}

EAPI void
elm_win_modal_set(Evas_Object *obj, Eina_Bool modal)
{
   Efl_Ui_Win_Modal_Mode modality;

   modality = modal ? EFL_UI_WIN_MODAL_MODAL : EFL_UI_WIN_MODAL_NONE;
   efl_ui_win_modal_set(obj, modality);
}

EAPI Eina_Bool
elm_win_modal_get(const Evas_Object *obj)
{
   Efl_Ui_Win_Modal_Mode modality;

   modality = efl_ui_win_modal_get(obj);
   return (modality != EFL_UI_WIN_MODAL_NONE);
}

EAPI void
elm_win_shaped_set(Evas_Object *obj, Eina_Bool shaped)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   TRAP(sd, shaped_set, shaped);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI Eina_Bool
elm_win_shaped_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) EINA_FALSE;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, EINA_FALSE);

   return ecore_evas_shaped_get(sd->ee);
}

EAPI void
elm_win_title_set(Evas_Object *obj, const char *title)
{
   efl_text_set(obj, title);
}

EAPI const char*
elm_win_title_get(const Evas_Object *obj)
{
   return efl_text_get(obj);
}

EAPI void
elm_win_size_base_set(Evas_Object *obj, int w, int h)
{
   efl_gfx_size_hint_base_set(obj, w, h);
}

EAPI void
elm_win_size_base_get(const Evas_Object *obj, int *w, int *h)
{
   efl_gfx_size_hint_base_get(obj, w, h);
}

EAPI void
elm_win_size_step_set(Evas_Object *obj, int w, int h)
{
   efl_gfx_size_hint_step_set(obj, w, h);
}

EAPI void
elm_win_size_step_get(const Evas_Object *obj, int *w, int *h)
{
   efl_gfx_size_hint_step_get(obj, w, h);
}

EAPI void
elm_win_illume_command_send(Evas_Object *obj, Elm_Illume_Command command, void *params)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   (void) params;

#ifdef HAVE_ELEMENTARY_X
   _internal_elm_win_xwindow_get(sd);
   if (sd->x.xwin)
     {
        switch (command)
          {
           case ELM_ILLUME_COMMAND_FOCUS_BACK:
             ecore_x_e_illume_focus_back_send(sd->x.xwin);
             break;

           case ELM_ILLUME_COMMAND_FOCUS_FORWARD:
             ecore_x_e_illume_focus_forward_send(sd->x.xwin);
             break;

           case ELM_ILLUME_COMMAND_FOCUS_HOME:
             ecore_x_e_illume_focus_home_send(sd->x.xwin);
             break;

           case ELM_ILLUME_COMMAND_CLOSE:
             ecore_x_e_illume_close_send(sd->x.xwin);
             break;

           default:
             break;
          }
     }
#else
   (void)sd;
   (void)command;
#endif
}

EAPI void
elm_win_profile_set(Evas_Object *obj, const char *profile)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   /* check to see if a given profile is present in an available profiles */
   if ((profile) && (sd->profile.available_list))
     {
        Eina_Bool found = EINA_FALSE;
        unsigned int i;
        for (i = 0; i < sd->profile.count; i++)
          {
             if (!strcmp(profile,
                         sd->profile.available_list[i]))
               {
                  found = EINA_TRUE;
                  break;
               }
          }
        if (!found) return;
     }

   if (ecore_evas_window_profile_supported_get(sd->ee))
     {
        if (!profile) _elm_win_profile_del(sd);
        ecore_evas_window_profile_set(sd->ee, profile);
     }
   else
     {
        if (_internal_elm_win_profile_set(sd, profile))
          _elm_win_profile_update(sd);
     }
}

EAPI const char*
elm_win_profile_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

   return sd->profile.name;
}

EAPI void
elm_win_layer_set(Evas_Object *obj, int layer)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   TRAP(sd, layer_set, layer);
#ifdef HAVE_ELEMENTARY_X
   _elm_win_xwin_update(sd);
#endif
}

EAPI int
elm_win_layer_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) 0;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, 0);

   return ecore_evas_layer_get(sd->ee);
}

EAPI Evas_Object*
elm_win_inlined_image_object_get(const Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

   return sd->img_obj;
}

static Ecore_Window
_elm_win_window_id_get(Efl_Ui_Win_Data *sd)
{
   const char *engine_name = ecore_evas_engine_name_get(sd->ee);

   if ((engine_name &&
        ((!strcmp(engine_name, ELM_WAYLAND_SHM)) ||
         (!strcmp(engine_name, ELM_WAYLAND_EGL)))))
     {
#if HAVE_ELEMENTARY_WL2
        if (sd->wl.win)
          return (Ecore_Window)ecore_wl2_window_id_get(sd->wl.win);
        if (sd->parent)
          {
             Ecore_Wl2_Window *parent;

             parent = elm_win_wl_window_get(sd->parent);
             if (parent)
               return (Ecore_Window)ecore_wl2_window_id_get(parent);
             return 0;
          }
#endif
     }
   else if ((engine_name &&
             ((!strcmp(engine_name, ELM_SOFTWARE_X11)) ||
              (!strcmp(engine_name, ELM_OPENGL_X11)))))
     {
#ifdef HAVE_ELEMENTARY_X
        _internal_elm_win_xwindow_get(sd);
        if (sd->x.xwin) return (Ecore_Window)sd->x.xwin;
        if (sd->parent) return (Ecore_Window)elm_win_xwindow_get(sd->parent);
#endif
     }
   else if (engine_name &&
            ((!strcmp(engine_name, ELM_OPENGL_COCOA)) ||
             (!strcmp(engine_name, "gl_cocoa"))))
     {
#ifdef HAVE_ELEMENTARY_COCOA
        if (sd->cocoa.win) return (Ecore_Window)(sd->cocoa.win);
        if (sd->parent)
          {
             Ecore_Cocoa_Window *pwin;
             pwin = elm_win_cocoa_window_get(sd->parent);
             return (Ecore_Window)pwin;
          }
#endif
     }
   else if ((engine_name &&
             ((!strcmp(engine_name, ELM_SOFTWARE_WIN32)) ||
              (!strcmp(engine_name, ELM_SOFTWARE_DDRAW)))))
     {
#ifdef HAVE_ELEMENTARY_WIN32
        _internal_elm_win_win32window_get(sd);
        if (sd->win32.win) return (Ecore_Window)sd->win32.win;
        if (sd->parent) return (Ecore_Window)elm_win_win32_window_get(sd->parent);
#endif
     }

   return 0;
}

EAPI Ecore_Window
elm_win_window_id_get(const Evas_Object *obj)
{
   if (!obj) return 0;

   if (!evas_object_smart_type_check_ptr(obj, MY_CLASS_NAME_LEGACY))
     {
        Ecore_Evas *ee = ecore_evas_ecore_evas_get(evas_object_evas_get(obj));
        return ecore_evas_window_get(ee);
     }

   ELM_WIN_CHECK(obj) 0;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, 0);
   return _elm_win_window_id_get(sd);
}

EAPI void
elm_win_fake_canvas_set(Evas_Object *obj, Ecore_Evas *oee)
{
   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   sd->ee = oee;
   _elm_win_need_frame_adjust(sd, ecore_evas_engine_name_get(oee));
}

EAPI Evas_Object *
elm_win_main_menu_get(Evas_Object *obj)
{
   ELM_WIN_CHECK(obj) NULL;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, NULL);

#ifdef HAVE_ELEMENTARY_X
   Eina_Bool use_dbus = EINA_FALSE;
#endif

   if (sd->main_menu) goto end;

   sd->main_menu = elm_menu_add(obj);
   _elm_menu_menu_bar_set(sd->main_menu, EINA_TRUE);

#ifdef HAVE_ELEMENTARY_X
   if (!_elm_config->disable_external_menu && sd->x.xwin) use_dbus = EINA_TRUE;
#endif

#ifdef HAVE_ELEMENTARY_X
   if (use_dbus && _elm_dbus_menu_register(sd->main_menu))
     {
        _elm_dbus_menu_app_menu_register(sd->x.xwin, sd->main_menu,
                                         _dbus_menu_set, obj);
     }
   else
#endif
     _dbus_menu_set(EINA_FALSE, obj);

end:
   return sd->main_menu;
}

EAPI void
elm_win_aspect_set(Eo *obj, double aspect)
{
   if (aspect > DBL_EPSILON)
     efl_gfx_size_hint_aspect_set(obj, EFL_GFX_SIZE_HINT_ASPECT_NONE, 1000 * aspect, 1000);
   else
     efl_gfx_size_hint_aspect_set(obj, EFL_GFX_SIZE_HINT_ASPECT_NONE, 0, 0);
}

EAPI double
elm_win_aspect_get(const Eo *obj)
{
   ELM_WIN_CHECK(obj) 0.0;
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd, 0.0);
   return _win_aspect_get(sd);
}

/* legacy APIs */

EAPI Evas_Object *
elm_win_util_standard_add(const char *name, const char *title)
{
   return efl_add(EFL_UI_WIN_STANDARD_CLASS, NULL,
                  efl_text_set(efl_added, title),
                  efl_ui_win_name_set(efl_added, name));
}

EAPI Evas_Object *
elm_win_util_dialog_add(Evas_Object *parent, const char *name, const char *title)
{
   Evas_Object *win;

   win = efl_add(EFL_UI_WIN_CLASS, parent,
                 efl_text_set(efl_added, title),
                 efl_ui_win_name_set(efl_added, name),
                 efl_ui_win_type_set(efl_added, EFL_UI_WIN_DIALOG_BASIC));
   if (!win) return NULL;

   _elm_win_standard_init(win);
   return win;
}

/**
  * @internal
  *
  * Recalculate the size of window considering its resize objects' weight and
  * min size. If any of its resize objects' weight equals to 0.0, window
  * layout's weight will be set to 0.0.
  *
  * @param o box object
  * @param p box's private data
  * @param data window object
  */
static void
_window_layout_stack(Evas_Object *o, Evas_Object_Box_Data *p, void *data)
{
   const Eina_List *l;
   Evas_Object *child;
   Evas_Object_Box_Option *opt;
   Evas_Coord x, y, w, h;
   double wx, wy;
   Evas_Coord minw = -1, minh = -1;
   double weight_x = EVAS_HINT_EXPAND;
   double weight_y = EVAS_HINT_EXPAND;

   EINA_LIST_FOREACH(p->children, l, opt)
     {
        child = opt->obj;
        efl_gfx_size_hint_weight_get(child, &wx, &wy);
        if (wx == 0.0) weight_x = 0;
        if (wy == 0.0) weight_y = 0;

        efl_gfx_size_hint_combined_min_get(child, &w, &h);
        if (w > minw) minw = w;
        if (h > minh) minh = h;
     }

   efl_gfx_size_hint_restricted_min_set(o, minw, minh);
   evas_object_geometry_get(o, &x, &y, &w, &h);
   if (w < minw) w = minw;
   if (h < minh) h = minh;
   evas_object_resize(o, w, h);

   EINA_LIST_FOREACH(p->children, l, opt)
     {
        child = opt->obj;
        evas_object_move(child, x, y);
        evas_object_resize(child, w, h);
     }

   ELM_WIN_DATA_GET(data, sd);
   efl_gfx_size_hint_weight_set(sd->legacy.edje, weight_x, weight_y);
   evas_object_smart_changed(sd->legacy.edje);
}

static void
_elm_win_legacy_init(Efl_Ui_Win_Data *sd)
{
   sd->legacy.edje = edje_object_add(sd->evas);
   _elm_win_theme_internal(sd->obj, sd);

   sd->legacy.box = evas_object_box_add(sd->evas);
   evas_object_box_layout_set(sd->legacy.box, _window_layout_stack, sd->obj, NULL);
   edje_object_part_swallow(sd->legacy.edje, "elm.swallow.contents", sd->legacy.box);
   evas_object_move(sd->legacy.edje, 0, 0);
   evas_object_resize(sd->legacy.edje, 1, 1);
   if (sd->type != ELM_WIN_FAKE)
     {
        edje_object_update_hints_set(sd->legacy.edje, EINA_TRUE);
        evas_object_event_callback_add(sd->legacy.edje, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                       _elm_win_on_resize_obj_changed_size_hints, sd->obj);
     }
}

EAPI void
elm_win_resize_object_add(Eo *obj, Evas_Object *subobj)
{
   Eina_Bool ret;

   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (sd->legacy.forbidden)
     {
        CRI("Use of this API is forbidden after calling an EO API on this "
            "window. Fix your code!");
        return;
     }

   // Little hack for E
   if (evas_obj_box_count(sd->legacy.box) > 0)
     sd->single_edje_content = 0;
   else if (efl_isa(subobj, EDJE_OBJECT_CLASS))
     sd->single_edje_content = 1;

   ret  = elm_widget_sub_object_add(obj, subobj);
   ret &= (evas_object_box_append(sd->legacy.box, subobj) != NULL);

   if (!ret)
     ERR("could not add sub object %p to window %p", subobj, obj);
}

EAPI void
elm_win_resize_object_del(Eo *obj, Evas_Object *subobj)
{
   Eina_Bool ret;

   ELM_WIN_CHECK(obj);
   ELM_WIN_DATA_GET_OR_RETURN(obj, sd);

   if (sd->legacy.forbidden)
     {
        CRI("Use of this API is forbidden after calling an EO API on this "
            "window. Fix your code!");
        return;
     }

   ret  = elm_widget_sub_object_del(obj, subobj);
   ret &= evas_object_box_remove(sd->legacy.box, subobj);

   if (!ret)
     ERR("could not remove sub object %p from window %p", subobj, obj);
}

// deprecated
EAPI void
elm_win_name_set(Evas_Object *obj, const char *name)
{
   ERR("Calling deprecrated function '%s'", __FUNCTION__);
   efl_ui_win_name_set(obj, name);
}

EAPI void
elm_win_type_set(Evas_Object *obj, Elm_Win_Type type)
{
   ERR("Calling deprecrated function '%s'", __FUNCTION__);
   efl_ui_win_type_set(obj, type);
}

EAPI void
elm_win_teamwork_uri_preload(Efl_Ui_Win *obj EINA_UNUSED, const char *uri EINA_UNUSED)
{
   ERR("Calling deprecrated function '%s'", __FUNCTION__);
}

EAPI void
elm_win_teamwork_uri_show(Efl_Ui_Win *obj EINA_UNUSED, const char *uri EINA_UNUSED)
{
   ERR("Calling deprecrated function '%s'", __FUNCTION__);
}

EAPI void
elm_win_teamwork_uri_hide(Efl_Ui_Win *obj EINA_UNUSED)
{
   ERR("Calling deprecrated function '%s'", __FUNCTION__);
}

EAPI void
elm_win_teamwork_uri_open(Efl_Ui_Win *obj EINA_UNUSED, const char *uri EINA_UNUSED)
{
   ERR("Calling deprecrated function '%s'", __FUNCTION__);
}

#include "efl_ui_win.eo.c"
