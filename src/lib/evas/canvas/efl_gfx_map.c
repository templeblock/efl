#include "evas_map.h"

// FIXME: cur vs. prev is not handled (may be an issue?)
// FIXME: some render artifacts appear when this API is used (green pixels)

#define EINA_INLIST_REMOVE(l,i) do { l = (__typeof__(l)) eina_inlist_remove(EINA_INLIST_GET(l), EINA_INLIST_GET(i)); } while (0)
#define EINA_INLIST_APPEND(l,i) do { l = (__typeof__(l)) eina_inlist_append(EINA_INLIST_GET(l), EINA_INLIST_GET(i)); } while (0)
#define EINA_INLIST_PREPEND(l,i) do { l = (__typeof__(l)) eina_inlist_prepend(EINA_INLIST_GET(l), EINA_INLIST_GET(i)); } while (0)
#define EINA_INLIST_NEXT(l) (typeof(l)) EINA_INLIST_CONTAINER_GET(EINA_INLIST_GET(l)->next, typeof(*l))

#define MY_CLASS EFL_GFX_MAP_MIXIN

typedef struct _Gfx_Map               Gfx_Map;
typedef struct _Gfx_Map_Op            Gfx_Map_Op;
typedef struct _Gfx_Map_Pivot         Gfx_Map_Pivot;
typedef struct _Efl_Gfx_Map_Data      Efl_Gfx_Map_Data;
typedef enum _Gfx_Map_Op_Type         Gfx_Map_Op_Type;

enum _Gfx_Map_Op_Type {
   GFX_MAP_RAW_COORD,
   GFX_MAP_COLOR,
   GFX_MAP_ROTATE_2D,
   GFX_MAP_ROTATE_3D,
   GFX_MAP_ROTATE_QUAT,
   GFX_MAP_ZOOM,
   GFX_MAP_TRANSLATE,
   GFX_MAP_LIGHTNING_3D,
   GFX_MAP_PERSPECTIVE_3D,
};

struct _Gfx_Map_Op {
   EINA_INLIST;

   Gfx_Map_Op_Type op;
   union {
      struct {
         int idx;
         double x, y, z;
      } raw_coord;
      struct {
         int idx;
         uint8_t r, g, b, a;
      } color;
      struct {
         double degrees;
      } rotate_2d;
      struct {
         double dx, dy, dz;
      } rotate_3d;
      struct {
         double qx, qy, qz, qw;
      } rotate_quat;
      struct {
         double zx, zy;
      } zoom;
      struct {
         double dx, dy, dz;
      } translate;
      struct {
         uint8_t lr, lg, lb, ar, ag, ab;
      } lightning_3d;
      struct {
         double z0, foc;
      } perspective_3d;
   };
   struct {
      Gfx_Map_Pivot  *pivot;
      double          cx, cy, cz;
      Eina_Bool       is_absolute;
      Eina_Bool       is_self;
   } pivot;
};

struct _Gfx_Map_Pivot
{
   EINA_INLIST;

   Evas_Object_Protected_Data *map_obj;
   Eo             *eo_obj; // strong or weak ref?
   Eina_Rectangle  geometry;
   Eina_Bool       event_cbs;
   Eina_Bool       is_canvas;
   Eina_Bool       changed;
};

struct _Gfx_Map {
   Gfx_Map_Op *ops;
   struct {
      double u, v;
   } point[4];

   Gfx_Map_Pivot *pivots;
   Evas_Map      *map;
   Gfx_Map_Op    *last_calc_op;
   int            imw, imh;

   // FIXME: Those need a quality vs. performance setting instead
   Eina_Bool alpha;
   Eina_Bool smooth;
   Eina_Bool event_cbs;
};

struct _Efl_Gfx_Map_Data {
   const Gfx_Map *cow;
};

// ----------------------------------------------------------------------------

static Eina_Cow *gfx_map_cow = NULL;
static const Gfx_Map gfx_map_cow_default = {
   NULL,
   { { 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 }, { 0.0, 1.0 } },
   NULL,
   NULL,
   0, 0,
   EINA_TRUE,
   EINA_TRUE,
   EINA_FALSE
};

#define MAPCOW_BEGIN(_pd) eina_cow_write(gfx_map_cow, (const Eina_Cow_Data**)&(_pd->cow))
#define MAPCOW_END(_mapcow, _pd) eina_cow_done(gfx_map_cow, (const Eina_Cow_Data**)&(_pd->cow), _mapcow, EINA_FALSE)
#define MAPCOW_WRITE(pd, name, value) do { \
   if (pd->cow->name != (value)) { \
     Gfx_Map *_cow = MAPCOW_BEGIN(pd); \
     _cow->name = (value); \
     MAPCOW_END(_cow, pd); \
   }} while (0)

#define PIVOT_REF(_pivot) (_pivot ? efl_xref(_pivot, eo_obj) : NULL)
#define PIVOT_UNREF(_pivot) (_pivot ? efl_xunref(_pivot, eo_obj) : NULL)

static inline void _map_ops_clean(Eo *eo_obj, Efl_Gfx_Map_Data *pd);

// ----------------------------------------------------------------------------

void
_efl_gfx_map_init(void)
{
   gfx_map_cow = eina_cow_add("Efl.Gfx.Map", sizeof(Gfx_Map), 8,
                              &gfx_map_cow_default, EINA_FALSE);
}

void
_efl_gfx_map_shutdown(void)
{
   eina_cow_del(gfx_map_cow);
   gfx_map_cow = NULL;
}

// ----------------------------------------------------------------------------

EOLIAN static Efl_Object *
_efl_gfx_map_efl_object_constructor(Eo *eo_obj, Efl_Gfx_Map_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));
   pd->cow = eina_cow_alloc(gfx_map_cow);
   return eo_obj;
}

EOLIAN static void
_efl_gfx_map_efl_object_destructor(Eo *eo_obj, Efl_Gfx_Map_Data *pd)
{
   _map_ops_clean(eo_obj, pd);
   eina_cow_free(gfx_map_cow, (const Eina_Cow_Data **) &pd->cow);
   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

// ----------------------------------------------------------------------------

static void
_geometry_changed_cb(void *data, const Efl_Event *ev EINA_UNUSED)
{
   Evas_Object_Protected_Data *obj = data;
   Efl_Gfx_Map_Data *pd = efl_data_scope_get(obj->object, MY_CLASS);

   MAPCOW_WRITE(pd, last_calc_op, NULL);
   obj->gfx_map_update = EINA_TRUE;
}

EFL_CALLBACKS_ARRAY_DEFINE(_geometry_changes,
                           { EFL_GFX_EVENT_MOVE, _geometry_changed_cb },
                           { EFL_GFX_EVENT_RESIZE, _geometry_changed_cb });

static void
_pivot_changed_cb(void *data, const Efl_Event *ev EINA_UNUSED)
{
   Gfx_Map_Pivot *pivot = data;
   Evas_Object_Protected_Data *obj = pivot->map_obj;

   obj->gfx_map_update = EINA_TRUE;
   pivot->changed = EINA_TRUE;
}

EFL_CALLBACKS_ARRAY_DEFINE(_pivot_changes,
                           { EFL_GFX_EVENT_MOVE, _pivot_changed_cb },
                           { EFL_GFX_EVENT_RESIZE, _pivot_changed_cb });

static inline void
_map_dirty(Eo *eo_obj, Efl_Gfx_Map_Data *pd, Eina_Bool reset)
{
   Evas_Object_Protected_Data *obj = EVAS_OBJ_GET_OR_RETURN(eo_obj);
   Gfx_Map_Pivot *pivot;

   obj->gfx_map_has = EINA_TRUE;
   obj->gfx_map_update |= !reset;
   obj->changed_map = EINA_TRUE;
   evas_object_change(eo_obj, obj);

   if (reset)
     {
        _evas_map_reset(pd->cow->map);
        return;
     }

   // FIXME: Only add if there is a self-pivot (relative coordinates)
   if (!pd->cow->event_cbs)
     {
        MAPCOW_WRITE(pd, event_cbs, EINA_TRUE);
        efl_event_callback_array_add(eo_obj, _geometry_changes(), obj);
     }

   EINA_INLIST_FOREACH(pd->cow->pivots, pivot)
     {
        if (pivot->event_cbs) continue;
        pivot->event_cbs = EINA_TRUE;
        efl_event_callback_array_add(pivot->eo_obj, _pivot_changes(), pivot);
     }
}

static Evas_Map *
_map_calc(Eo *eo_obj, Evas_Object_Protected_Data *obj, Efl_Gfx_Map_Data *pd)
{
   Gfx_Map_Op *op, *first_op = pd->cow->ops, *last_op;
   Gfx_Map_Pivot *pivot;
   Gfx_Map *mcow;
   Evas_Map *m;
   int imw, imh;

   if (pd->cow == &gfx_map_cow_default)
     return NULL;

   m = pd->cow->map;
   if (!obj->gfx_map_update) return m;

   last_op = pd->cow->last_calc_op;

   EINA_INLIST_FOREACH(pd->cow->pivots, pivot)
     {
        if (!pivot->changed) continue;
        last_op = NULL;
        pivot->changed = EINA_FALSE;
        if (!pivot->is_canvas)
          {
             efl_gfx_geometry_get(pivot->eo_obj,
                                  &pivot->geometry.x, &pivot->geometry.y,
                                  &pivot->geometry.w, &pivot->geometry.h);
          }
        else
          {
             // Note: pivot can not be an Evas when using pure EO API
             // FIXME: efl_isa is also very expensive here!
             if (efl_isa(pivot->eo_obj, EVAS_CANVAS_CLASS))
               evas_output_size_get(pivot->eo_obj, &pivot->geometry.w, &pivot->geometry.h);
             else
               efl_gfx_size_get(pivot->eo_obj, &pivot->geometry.w, &pivot->geometry.h);
             pivot->geometry.x = 0;
             pivot->geometry.y = 0;
          }
     }

   if (m && last_op)
     {
        first_op = EINA_INLIST_NEXT(last_op);
        imw = pd->cow->imw;
        imh = pd->cow->imh;
     }
   else
     {
        if (!m) m = evas_map_new(4);
        else _evas_map_reset(m);
        m->alpha = pd->cow->alpha;
        m->smooth = pd->cow->smooth;
        m->move_sync.enabled = EINA_FALSE;

        _evas_map_util_points_populate(m,
                                       obj->cur->geometry.x, obj->cur->geometry.y,
                                       obj->cur->geometry.w, obj->cur->geometry.h,
                                       0);

        if (obj->is_image_object)
          {
             // Image is a special case in terms of geometry
             efl_gfx_view_size_get(eo_obj, &imw, &imh);
          }
        else
          {
             imw = obj->cur->geometry.w;
             imh = obj->cur->geometry.h;
          }

        last_op = NULL;
        first_op = pd->cow->ops;
     }

   for (int k = 0; k < 4; k++)
     {
        Evas_Map_Point *p = &(m->points[k]);
        p->u = pd->cow->point[k].u * imw;
        p->v = pd->cow->point[k].v * imh;
     }

   EINA_INLIST_FOREACH(first_op, op)
     {
        int k, kmin = 0, kmax = 3;
        double cx, cy, cz;
        Evas_Map_Point *p;

        if (!op->pivot.is_absolute)
          {
             int px = 0, py = 0, pw = 1, ph = 1;

             pivot = op->pivot.pivot;
             if (op->pivot.is_self)
               {
                  px = obj->cur->geometry.x;
                  py = obj->cur->geometry.y;
                  pw = obj->cur->geometry.w;
                  ph = obj->cur->geometry.h;
               }
             else
               {
                  EINA_SAFETY_ON_NULL_RETURN_VAL(pivot, NULL);
                  px = pivot->geometry.x;
                  py = pivot->geometry.y;
                  pw = pivot->geometry.w;
                  ph = pivot->geometry.h;
               }

             cx = (double) px + (double) pw * op->pivot.cx;
             cy = (double) py + (double) ph * op->pivot.cy;
             cz = op->pivot.cz;
          }
        else
          {
             cx = op->pivot.cx;
             cy = op->pivot.cy;
             cz = op->pivot.cz;
          }

        switch (op->op)
          {
           case GFX_MAP_RAW_COORD:
             if (op->raw_coord.idx != -1)
               kmin = kmax = op->raw_coord.idx;
             for (k = kmin; k <= kmax; k++)
               {
                  p = &(m->points[k]);
                  p->px = p->x = op->raw_coord.x;
                  p->py = p->y = op->raw_coord.y;
                  p->z = op->raw_coord.z;
               }
             break;
           case GFX_MAP_COLOR:
             if (op->raw_coord.idx != -1)
               kmin = kmax = op->raw_coord.idx;
             for (k = kmin; k <= kmax; k++)
               {
                  p = &(m->points[k]);
                  p->r = op->color.r;
                  p->g = op->color.g;
                  p->b = op->color.b;
                  p->a = op->color.a;
               }
             break;
           case GFX_MAP_ROTATE_2D:
             _map_util_rotate(m, op->rotate_2d.degrees, cx, cy);
             break;
           case GFX_MAP_ROTATE_3D:
             _map_util_3d_rotate(m, op->rotate_3d.dx, op->rotate_3d.dy,
                                 op->rotate_3d.dz, cx, cy, cz);
             break;
           case GFX_MAP_ROTATE_QUAT:
             _map_util_quat_rotate(m, op->rotate_quat.qx, op->rotate_quat.qy,
                                   op->rotate_quat.qz, op->rotate_quat.qw,
                                   cx, cy, cz);
             break;
           case GFX_MAP_ZOOM:
             _map_util_zoom(m, op->zoom.zx, op->zoom.zy, cx, cy);
             break;
           case GFX_MAP_TRANSLATE:
             _map_util_translate(m, op->translate.dx, op->translate.dy,
                                 op->translate.dz);
             break;
           case GFX_MAP_LIGHTNING_3D:
             _map_util_3d_lighting(m, cx, cy, cz, op->lightning_3d.lr,
                                   op->lightning_3d.lg, op->lightning_3d.lb,
                                   op->lightning_3d.ar, op->lightning_3d.ag,
                                   op->lightning_3d.ab);
             break;
           case GFX_MAP_PERSPECTIVE_3D:
             _map_util_3d_perspective(m, cx, cy, op->perspective_3d.z0,
                                      op->perspective_3d.foc);
             break;
          }

        last_op = op;
     }

   mcow = MAPCOW_BEGIN(pd);
   mcow->map = m;
   mcow->last_calc_op = last_op;
   mcow->imw = imw;
   mcow->imh = imh;
   MAPCOW_END(mcow, pd);
   obj->gfx_map_update = EINA_FALSE;

   return m;
}

void
_efl_gfx_map_update(Eo *eo_obj)
{
   Evas_Object_Protected_Data *obj = EVAS_OBJ_GET_OR_RETURN(eo_obj);
   Efl_Gfx_Map_Data *pd = efl_data_scope_get(eo_obj, MY_CLASS);
   Evas_Map *m;

   m = _map_calc(eo_obj, obj, pd);
   evas_object_map_set(eo_obj, m);
   _evas_object_map_enable_set(eo_obj, obj, m != NULL);
}

static inline void
_map_ops_clean(Eo *eo_obj, Efl_Gfx_Map_Data *pd)
{
   if (pd->cow->ops)
     {
        Gfx_Map_Pivot *pivot;
        Gfx_Map_Op *op;
        Gfx_Map *mcow;

        mcow = MAPCOW_BEGIN(pd);
        EINA_INLIST_FREE(mcow->ops, op)
          {
             EINA_INLIST_REMOVE(mcow->ops, op);
             free(op);
          }
        EINA_INLIST_FREE(mcow->pivots, pivot)
          {
             EINA_INLIST_REMOVE(mcow->pivots, pivot);
             if (pivot->event_cbs)
               efl_event_callback_array_del(pivot->eo_obj, _pivot_changes(), pivot);
             PIVOT_UNREF(pivot->eo_obj);
             free(pivot);
          }
        MAPCOW_END(mcow, pd);
     }
}

EOLIAN Eina_Bool
_efl_gfx_map_map_has(Eo *eo_obj EINA_UNUSED, Efl_Gfx_Map_Data *pd EINA_UNUSED)
{
   if (pd->cow == &gfx_map_cow_default) return EINA_FALSE;
   if (pd->cow->ops) return EINA_TRUE;
   if (pd->cow->map) return EINA_TRUE;
   return EINA_FALSE;
}

EOLIAN static void
_efl_gfx_map_map_reset(Eo *eo_obj, Efl_Gfx_Map_Data *pd)
{
   Evas_Object_Protected_Data *obj = EVAS_OBJ_GET_OR_RETURN(eo_obj);
   Eina_Bool alpha, smooth;

   alpha = pd->cow->alpha;
   smooth = pd->cow->smooth;
   _map_ops_clean(eo_obj, pd);
   if (pd->cow->event_cbs)
     efl_event_callback_array_del(eo_obj, _geometry_changes(), obj);

   eina_cow_memcpy(gfx_map_cow, (const Eina_Cow_Data * const *) &pd->cow,
                   (const Eina_Cow_Data *) &gfx_map_cow_default);
   _map_dirty(eo_obj, pd, EINA_TRUE);
   MAPCOW_WRITE(pd, alpha, alpha);
   MAPCOW_WRITE(pd, smooth, smooth);
   obj->gfx_map_has = EINA_FALSE;
}

EOLIAN static Eina_Bool
_efl_gfx_map_map_clockwise_get(Eo *eo_obj, Efl_Gfx_Map_Data *pd)
{
   Evas_Object_Protected_Data *obj = EVAS_OBJ_GET_OR_RETURN(eo_obj, EINA_TRUE);
   Evas_Map *m;

   m = _map_calc(eo_obj, obj, pd);
   if (!m) return EINA_TRUE;
   return evas_map_util_clockwise_get(m);
}

EOLIAN static void
_efl_gfx_map_map_smooth_set(Eo *eo_obj, Efl_Gfx_Map_Data *pd, Eina_Bool smooth)
{
   if (pd->cow->smooth == smooth) return;

   MAPCOW_WRITE(pd, smooth, smooth);

   _map_dirty(eo_obj, pd, EINA_FALSE);
}

EOLIAN static Eina_Bool
_efl_gfx_map_map_smooth_get(Eo *eo_obj EINA_UNUSED, Efl_Gfx_Map_Data *pd)
{
   return pd->cow->smooth;
}

EOLIAN static void
_efl_gfx_map_map_alpha_set(Eo *eo_obj, Efl_Gfx_Map_Data *pd, Eina_Bool alpha)
{
   if (pd->cow->alpha == alpha) return;

   MAPCOW_WRITE(pd, alpha, alpha);

   _map_dirty(eo_obj, pd, EINA_FALSE);
}

EOLIAN static Eina_Bool
_efl_gfx_map_map_alpha_get(Eo *eo_obj EINA_UNUSED, Efl_Gfx_Map_Data *pd)
{
   return pd->cow->alpha;
}

EOLIAN static void
_efl_gfx_map_map_raw_coord_get(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                               int idx, double *x, double *y, double *z)
{
   Evas_Object_Protected_Data *obj = EVAS_OBJ_GET_OR_RETURN(eo_obj);
   Evas_Map *m;

   EINA_SAFETY_ON_FALSE_RETURN((idx >= 0) && (idx < 4));

   m = _map_calc(eo_obj, obj, pd);
   if (!m)
     {
        int X, Y, W, H;

        X = obj->cur->geometry.x;
        Y = obj->cur->geometry.y;
        W = obj->cur->geometry.w;
        H = obj->cur->geometry.h;

        if (x)
          {
             if ((idx == 0) || (idx == 3)) *x = X;
             else *x = X + W;
          }
        if (y)
          {
             if ((idx == 0) || (idx == 1)) *y = Y;
             else *y = Y + H;
          }
        if (z) *z = 0;
        return;
     }

   _map_point_coord_get(m, idx, x, y, z);
}

EOLIAN static void
_efl_gfx_map_map_uv_set(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                        int idx, double u, double v)
{
   Gfx_Map *mcow;

   EINA_SAFETY_ON_FALSE_RETURN((idx >= 0) && (idx < 4));

   if (EINA_DBL_EQ(pd->cow->point[idx].u, u) &&
       EINA_DBL_EQ(pd->cow->point[idx].v, v))
     return;

   mcow = MAPCOW_BEGIN(pd);
   mcow->point[idx].u = CLAMP(0.0, u, 1.0);
   mcow->point[idx].v = CLAMP(0.0, v, 1.0);
   MAPCOW_END(mcow, pd);

   _map_dirty(eo_obj, pd, EINA_FALSE);
}

EOLIAN static void
_efl_gfx_map_map_uv_get(Eo *eo_obj EINA_UNUSED, Efl_Gfx_Map_Data *pd,
                        int idx, double *u, double *v)
{
   EINA_SAFETY_ON_FALSE_RETURN((idx >= 0) && (idx < 4));

   if (u) *u = pd->cow->point[idx].u;
   if (v) *v = pd->cow->point[idx].v;
}

EOLIAN static void
_efl_gfx_map_map_color_get(Eo *eo_obj EINA_UNUSED, Efl_Gfx_Map_Data *pd,
                           int idx, int *r, int *g, int *b, int *a)
{
   Evas_Object_Protected_Data *obj = EVAS_OBJ_GET_OR_RETURN(eo_obj);
   Evas_Map_Point *p;
   Evas_Map *m;

   EINA_SAFETY_ON_FALSE_RETURN((idx >= 0) && (idx < 4));

   if (!r && !g && !b && !a) return;

   m = _map_calc(eo_obj, obj, pd);
   if (!m)
     {
        if (r) *r = 255;
        if (g) *g = 255;
        if (b) *b = 255;
        if (a) *a = 255;
        return;
     }

   p = &(m->points[idx]);
   if (r) *r = p->r;
   if (g) *g = p->g;
   if (b) *b = p->b;
   if (a) *a = p->a;
}

static Gfx_Map_Op *
_gfx_map_op_add(Eo *eo_obj, Efl_Gfx_Map_Data *pd, Gfx_Map_Op_Type type,
                Efl_Gfx *eo_pivot, double cx, double cy, double cz)
{
   Eina_Bool is_absolute = EINA_FALSE, is_self = EINA_TRUE;
   Gfx_Map_Pivot *pivot;
   Gfx_Map_Op *op;
   Gfx_Map *mcow;

   op = calloc(1, sizeof(*op));
   if (!op) return NULL;

   mcow = MAPCOW_BEGIN(pd);

   if (eo_pivot == eo_obj)
     eo_pivot = NULL;

   if (eo_pivot)
     {
        Evas_Object_Protected_Data *obj = efl_data_scope_get(eo_obj, MY_CLASS);

        is_self = EINA_FALSE;

        /* TODO:
        if (eo_pivot == absolute_pivot)
          is_absolute = 1;
        else */
          {
             EINA_INLIST_FOREACH(mcow->pivots, pivot)
               if (pivot->eo_obj == eo_pivot) break;
             if (!pivot)
               {
                  pivot = calloc(1, sizeof(*pivot));
                  pivot->eo_obj = PIVOT_REF(eo_pivot);
                  pivot->changed = EINA_TRUE;
                  if (efl_isa(eo_pivot, EFL_CANVAS_INTERFACE))
                    pivot->is_canvas = EINA_TRUE;
                  pivot->map_obj = obj;
                  EINA_INLIST_APPEND(mcow->pivots, pivot);
               }
          }
     }

   op->op = type;
   op->pivot.is_absolute = is_absolute;
   op->pivot.is_self = is_self;
   op->pivot.cx = cx;
   op->pivot.cy = cy;
   op->pivot.cz = cz;

   EINA_INLIST_APPEND(mcow->ops, op);
   MAPCOW_END(mcow, pd);

   _map_dirty(eo_obj, pd, EINA_FALSE);

   return op;
}

EOLIAN static void
_efl_gfx_map_map_raw_coord_set(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                               int idx, double x, double y, double z)
{
   Gfx_Map_Op *op;

   EINA_SAFETY_ON_FALSE_RETURN((idx >= 0) && (idx < 4));

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_RAW_COORD, NULL, 0, 0, 0);
   if (!op) return;

   op->raw_coord.idx = idx;
   op->raw_coord.x = x;
   op->raw_coord.y = y;
   op->raw_coord.z = z;
}

EOLIAN static void
_efl_gfx_map_map_color_set(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                           int idx, int r, int g, int b, int a)
{
   Gfx_Map_Op *op;

   EINA_SAFETY_ON_FALSE_RETURN((idx >= -1) && (idx < 4));

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_COLOR, NULL, 0, 0, 0);
   if (!op) return;

   op->color.idx = idx;
   op->color.r = r;
   op->color.g = g;
   op->color.b = b;
   op->color.a = a;
}

EOLIAN static void
_efl_gfx_map_rotate(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                    double degrees, Efl_Gfx *pivot, double cx, double cy)
{
   Gfx_Map_Op *op;

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_ROTATE_2D, pivot, cx, cy, 0);
   if (!op) return;

   op->rotate_2d.degrees = degrees;
}

EOLIAN static void
_efl_gfx_map_rotate_3d(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                       double dx, double dy, double dz,
                       Efl_Gfx *pivot, double cx, double cy, double cz)
{
   Gfx_Map_Op *op;

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_ROTATE_3D, pivot, cx, cy, cz);
   if (!op) return;

   op->rotate_3d.dx = dx;
   op->rotate_3d.dy = dy;
   op->rotate_3d.dz = dz;
}

EOLIAN static void
_efl_gfx_map_rotate_quat(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                         double qx, double qy, double qz, double qw,
                         Efl_Gfx *pivot, double cx, double cy, double cz)
{
   Gfx_Map_Op *op;

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_ROTATE_QUAT, pivot, cx, cy, cz);
   if (!op) return;

   op->rotate_quat.qx = qx;
   op->rotate_quat.qy = qy;
   op->rotate_quat.qz = qz;
   op->rotate_quat.qw = qw;
}

EOLIAN static void
_efl_gfx_map_zoom(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                  double zoomx, double zoomy,
                  Efl_Gfx *pivot, double cx, double cy)
{
   Gfx_Map_Op *op;

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_ZOOM, pivot, cx, cy, 0);
   if (!op) return;

   op->zoom.zx = zoomx;
   op->zoom.zy = zoomy;
}

EOLIAN static void
_efl_gfx_map_translate(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                       double dx, double dy, double dz)
{
   Gfx_Map_Op *op;

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_TRANSLATE, NULL, 0, 0, 0);
   if (!op) return;

   op->translate.dx = dx;
   op->translate.dy = dy;
   op->translate.dz = dz;
}

EOLIAN static void
_efl_gfx_map_lightning_3d(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                          Efl_Gfx *pivot, double lx, double ly, double lz,
                          int lr, int lg, int lb, int ar, int ag, int ab)
{
   Gfx_Map_Op *op;

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_LIGHTNING_3D, pivot, lx, ly, lz);
   if (!op) return;

   op->lightning_3d.lr = lr;
   op->lightning_3d.lg = lg;
   op->lightning_3d.lb = lb;
   op->lightning_3d.ar = ar;
   op->lightning_3d.ag = ag;
   op->lightning_3d.ab = ab;
}

EOLIAN static void
_efl_gfx_map_perspective_3d(Eo *eo_obj, Efl_Gfx_Map_Data *pd,
                            Efl_Gfx *pivot, double px, double py,
                            double z0, double foc)
{
   Gfx_Map_Op *op;

   if (foc <= 0.0)
     {
        ERR("Focal length must be greater than 0!");
        return;
     }

   op = _gfx_map_op_add(eo_obj, pd, GFX_MAP_PERSPECTIVE_3D, pivot, px, py, 0);
   if (!op) return;

   op->perspective_3d.z0 = z0;
   op->perspective_3d.foc = foc;
}

#include "canvas/efl_gfx_map.eo.c"
