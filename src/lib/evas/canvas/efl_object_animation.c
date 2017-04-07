#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_OBJECT_ANIMATION_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_OBJECT_ANIMATION_CHECK_OR_RETURN(anim, ...) \
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

struct _Evas_Object_Object_Animation_Data
{
   Evas_Object *target;
};

EOLIAN static void
_efl_object_animation_target_object_set(Eo *eo_obj, Evas_Object_Object_Animation_Data *pd, Evas_Object *target)
{
   EFL_OBJECT_ANIMATION_CHECK_OR_RETURN(eo_obj);

   pd->target = target;
}

EOLIAN static Evas_Object *
_efl_object_animation_target_object_get(Eo *eo_obj, Evas_Object_Object_Animation_Data *pd)
{
   EFL_OBJECT_ANIMATION_CHECK_OR_RETURN(eo_obj, NULL);

   return pd->target;
}

#include "efl_object_animation.eo.c"
