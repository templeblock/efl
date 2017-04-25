#include "evas_common_private.h"
#include "evas_private.h"
#include <Ecore.h>

#define MY_CLASS EFL_ANIMATION_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_CHECK_OR_RETURN(anim, ...) \
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

#define EFL_ANIMATION_DATA_GET(o, pd) \
   Evas_Object_Animation_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_CLASS)

struct _Evas_Object_Animation_Data
{
   /*
    * FIXME: The following properties should be added.
    * Current Interpolation Function
    * Current State
    * State (State Change) Callbacks
    * Eina_Bool auto_delete;
    * int repeat_time;
    * int current_loop;               //Default value is 1
    * Eina_Bool is_reverse;
    */
   Ecore_Animator *animator;

   struct
     {
        double begin;
        double current;
     } time;

   double    progress;
   double    duration;

   Eina_Bool deleted : 1;
   Eina_Bool finished : 1;
};

EOLIAN static void
_efl_animation_duration_set(Eo *eo_obj, Evas_Object_Animation_Data *pd, double duration)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj);

   if (duration <= 0.0) duration = 0.0;

   pd->duration = duration;
}

EOLIAN static double
_efl_animation_duration_get(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj, 0.0);

   return pd->duration;
}

EOLIAN static Eina_Bool
_efl_animation_is_deleted(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(eo_obj, EINA_TRUE);

   return pd->deleted;
}

static Eina_Bool
_animate_op(void *data)
{
   Eo *eo_obj = data;
   EFL_ANIMATION_DATA_GET(eo_obj, pd);
   double elapsed_time, duration;

   pd->time.current = ecore_loop_time_get();
   elapsed_time = pd->time.current - pd->time.begin;
   duration = pd->duration;
   if (elapsed_time > duration)
     elapsed_time = duration;

   if (duration <= 0.0) goto end;

   pd->progress = elapsed_time / duration;

   Efl_Animation_Animate_Event_Info event_info;
   event_info.progress = pd->progress;

   efl_event_callback_call(eo_obj, EFL_ANIMATION_EVENT_ANIMATE, &event_info);

  /* Not end. Keep going. */
   if (elapsed_time < duration) return ECORE_CALLBACK_RENEW;

end:
   pd->finished = EINA_TRUE;
   pd->animator = NULL;
   efl_event_callback_call(eo_obj, EFL_ANIMATION_EVENT_END, NULL);
   //FIXME: Delete animation here
   return ECORE_CALLBACK_CANCEL;
}

EOLIAN static Eina_Bool
_efl_animation_start(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj, EINA_FALSE);

   if (pd->duration <= 0.0)
     return EINA_FALSE;

   ecore_animator_del(pd->animator);
   pd->animator = NULL;

   pd->time.begin = ecore_loop_time_get();
   pd->animator = ecore_animator_add(_animate_op, eo_obj);

   _animate_op(eo_obj);

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_animation_cancel(Eo *eo_obj, Evas_Object_Animation_Data *pd EINA_UNUSED)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj, EINA_FALSE);

   return EINA_TRUE;
}

EOLIAN static Efl_Object *
_efl_animation_efl_object_constructor(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->time.begin = 0.0;
   pd->time.current = 0.0;

   pd->progress = 0.0;
   pd->duration = 0.0;

   pd->deleted = EINA_FALSE;
   pd->finished = EINA_FALSE;

   return eo_obj;
}

EOLIAN static void
_efl_animation_efl_object_destructor(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   ecore_animator_del(pd->animator);
   pd->animator = NULL;

   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

#include "efl_animation.eo.c"
