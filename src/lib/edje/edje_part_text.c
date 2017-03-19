/* Implementation of "TEXT" part as an Efl Object. */

#include "edje_private.h"
#include "edje_part_helper.h"

#define EFL_CANVAS_LAYOUT_INTERNAL_TEXT_PROTECTED
#include "efl_canvas_layout_internal_text.eo.h"
#define MY_CLASS EFL_CANVAS_LAYOUT_INTERNAL_TEXT_CLASS

#include "../evas/canvas/evas_text.eo.h"

PROXY_IMPLEMENTATION(text, TEXT, Edje_Text_Data)
#undef PROXY_IMPLEMENTATION

EOLIAN static void
_efl_canvas_layout_internal_text_efl_text_text_set(Eo *obj,
      Edje_Text_Data *pd, const char *text)
{
   _edje_efl_text_set(obj, pd->ed, pd->part, text);
}

EOLIAN static const char *
_efl_canvas_layout_internal_text_efl_text_text_get(Eo *obj,
      Edje_Text_Data *pd)
{
   RETURN_VAL(_edje_efl_text_get(obj, pd->ed, pd->part));
}

#include "efl_canvas_layout_internal_text.eo.c"
