typedef enum _Elm_Photocam_Zoom_Mode
{
   ELM_PHOTOCAM_ZOOM_MODE_MANUAL = 0,
   ELM_PHOTOCAM_ZOOM_MODE_AUTO_FIT,
   ELM_PHOTOCAM_ZOOM_MODE_AUTO_FILL,
   ELM_PHOTOCAM_ZOOM_MODE_AUTO_FIT_IN,
   ELM_PHOTOCAM_ZOOM_MODE_LAST,
} Elm_Photocam_Zoom_Mode;

/**
 * @brief Add a new Photocam object
 *
 * @param parent The parent object
 * @return The new object or NULL if it cannot be created
 *
 * @ingroup Elm_Photocam
 */
EAPI Evas_Object           *elm_photocam_add(Evas_Object *parent);

/**
 * @brief Bring in the viewed portion of the image
 *
 * @param obj The photocam object
 * @param x X-coordinate of region in image original pixels
 * @param y Y-coordinate of region in image original pixels
 * @param w Width of region in image original pixels
 * @param h Height of region in image original pixels
 *
 * This shows the region of the image using animation.
 *
 * @ingroup Elm_Photocam
 */
EAPI void                   elm_photocam_image_region_bring_in(Evas_Object *obj, int x, int y, int w, int h);

/**
 *
 * @brief Set the photo file to be shown
 *
 * @return The return error (see EVAS_LOAD_ERROR_NONE, EVAS_LOAD_ERROR_GENERIC etc.)
 *
 * This sets (and shows) the specified file (with a relative or absolute
 * path) and will return a load error (same error that
 * evas_object_image_load_error_get() will return). The image will change and
 * adjust its size at this point and begin a background load process for this
 * photo that at some time in the future will be displayed at the full
 * quality needed.
 *
 * @ingroup Elm_Photocam
 *
 * @param[in] file The photo file
 */
EAPI Evas_Load_Error elm_photocam_file_set(Evas_Object *obj, const char *file);

/**
 *
 * @brief Returns the path of the current image file
 *
 * @return Returns the path
 *
 * @see elm_photocam_file_set()
 *
 * @ingroup Elm_Photocam
 *
 */
EAPI const char *elm_photocam_file_get(const Evas_Object *obj);


EAPI void elm_photocam_zoom_mode_set(Evas_Object *obj, Elm_Photocam_Zoom_Mode mode);


EAPI Elm_Photocam_Zoom_Mode elm_photocam_zoom_mode_get(Evas_Object *obj);

EAPI void elm_photocam_zoom_set(Evas_Object *obj, double zoom);


EAPI double elm_photocam_zoom_get(Evas_Object *obj);

EAPI void elm_photocam_zoom_paused_set(Evas_Object *obj, Eina_Bool paused);

EAPI Eina_Bool elm_photocam_paused_get(Evas_Object *obj);

EAPI Eina_Bool elm_photocam_gesture_enabled_get(Evas_Object *obj);

EAPI void elm_photocam_gesture_enabled_set(Evas_Object *obj, Eina_Bool gesture);

EAPI void elm_photocam_image_region_show(Evas_Object *obj, int x, int y, int w, int h);

EAPI void elm_photocam_image_region_get(Evas_Object *obj, int *x, int *y, int *w, int *h);

EAPI Evas_Object * elm_photocam_internal_image_get(Evas_Object *obj);






















#include "efl_ui_image_zoom.eo.legacy.h"
