#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ALPHA_ANIMATION_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ALPHA_ANIMATION_CHECK_OR_RETURN(anim, ...) \
   do { \
      if (!anim) { \
         CRI("Efl_Animation " # anim " is NULL!"); \
         return __VA_ARGS__; \
      } \
      if (efl_animation_is_deleted(anim)) { \
         ERR("Efl_Animation " # anim " has already been deleted!"); \
         return __VA_ARGS__; \
      } \
   } while (0)

typedef struct _Evas_Object_Alpha_Animation_Property
{
   double alpha;
} Evas_Object_Alpha_Animation_Property;

struct _Evas_Object_Alpha_Animation_Data
{
   Evas_Object_Alpha_Animation_Property from;
   Evas_Object_Alpha_Animation_Property to;
};

EOLIAN static void
_efl_alpha_animation_alpha_set(Eo *eo_obj, Evas_Object_Alpha_Animation_Data *pd, double from_alpha, double to_alpha)
{
   EFL_ALPHA_ANIMATION_CHECK_OR_RETURN(eo_obj);

   pd->to.alpha = from_alpha;
   pd->from.alpha = to_alpha;
}

EOLIAN static void
_efl_alpha_animation_alpha_get(Eo *eo_obj EINA_UNUSED, Evas_Object_Alpha_Animation_Data *pd, double *from_alpha, double *to_alpha)
{
   EFL_ALPHA_ANIMATION_CHECK_OR_RETURN(eo_obj);

   if (from_alpha)
     *from_alpha = pd->from.alpha;
   if (to_alpha)
     *to_alpha = pd->to.alpha;
}

static void
_animate_cb(void *data, const Efl_Event *event)
{
   Efl_Animation_Animate_Event_Info *event_info = event->info;
}

EOLIAN static Efl_Object *
_efl_alpha_animation_efl_object_constructor(Eo *eo_obj, Evas_Object_Alpha_Animation_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->from.alpha = 1.0;
   pd->to.alpha = 1.0;

   efl_object_animation_target_object_set(efl_super(eo_obj, MY_CLASS), NULL);
   efl_animation_duration_set(efl_super(eo_obj, MY_CLASS), 0.0);

   efl_event_callback_add(eo_obj, EFL_ANIMATION_EVENT_ANIMATE, _animate_cb, NULL);

   return eo_obj;
}

EOLIAN static void
_efl_alpha_animation_efl_object_destructor(Eo *eo_obj, Evas_Object_Alpha_Animation_Data *pd EINA_UNUSED)
{
   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

#include "efl_alpha_animation.eo.c"
