/**
 * @brief Instantiates a new Edje object.
 *
 * @param evas A valid Evas handle, the canvas to place the new object
 * in
 * @return A handle to the new object created, or @c NULL on errors.
 *
 * This function creates a new Edje smart object, returning its @c
 * Evas_Object handle. An Edje object is useless without a (source)
 * file set to it, so you'd most probably call edje_object_file_set()
 * afterwards, like in:
 * @code
 * Evas_Object *edje;
 *
 * edje = edje_object_add(canvas);
 * if (!edje)
 *   {
 *      fprintf(stderr, "could not create edje object!\n");
 *      return NULL;
 *   }
 *
 * if (!edje_object_file_set(edje, "theme.edj", "group_name"))
 *   {
 *      int err = edje_object_load_error_get(edje);
 *      const char *errmsg = edje_load_error_str(err);
 *      fprintf(stderr, "could not load 'group_name' from theme.edj: %s",
 *      	errmsg);
 *
 *      evas_object_del(edje);
 *      return NULL;
 *   }
 *
 * @endcode
 *
 * @note You can get a callback every time edje re-calculates the object
 * (either due to animation or some kind of signal or input). This is called
 * in-line just after the recalculation has occurred. It is a good idea not
 * to go and delete or alter the object inside this callbacks, simply make
 * a note that the recalculation has taken place and then do something about
 * it outside the callback. To register a callback use code like:
 *
 * @code
 *    evas_object_smart_callback_add(edje_obj, "recalc", my_cb, my_cb_data);
 * @endcode
 *
 * @see evas_object_smart_callback_add()
 *
 * @note Before creating the first Edje object in your code, remember
 * to initialize the library, with edje_init(), or unexpected behavior
 * might occur.
 */
EAPI Evas_Object *edje_object_add                 (Evas *evas);

/**
 * @brief Removes a signal-triggered callback from an object.
 *
 * @param obj A valid Evas_Object handle.
 * @param emission The emission string.
 * @param source The source string.
 * @param func The callback function.
 * @return The data pointer
 *
 * This function removes a callback, previously attached to the
 * emission of a signal, from the object @a obj. The parameters @a
 * emission, @a source and @a func must match exactly those passed to
 * a previous call to edje_object_signal_callback_add(). The data
 * pointer that was passed to this call will be returned.
 *
 * @see edje_object_signal_callback_add().
 * @see edje_object_signal_callback_del_full().
 *
 */
EAPI void        *edje_object_signal_callback_del (Evas_Object *obj, const char *emission, const char *source, Edje_Signal_Cb func);

/**
 * @brief Unregisters/deletes a callback set for an arriving Edje
 * signal, emitted by a given Edje object.
 *
 * @param obj A handle to an Edje object
 * @param emission The signal's "emission" string
 * @param source The signal's "source" string
 * @param func The callback function passed on the callback's
 * registration
 * @param data The pointer given to be passed as data to @p func
 * @return @p data on success, or @c NULL on errors (or if @p data
 * had this value)
 *
 * This function removes a callback, previously attached to the
 * emission of a signal, from the object @a obj. The parameters
 * @a emission, @a source, @a func and @a data must match exactly those
 * passed to a previous call to edje_object_signal_callback_add(). The
 * data pointer that was passed to this call will be returned.
 *
 * @see edje_object_signal_callback_add().
 * @see edje_object_signal_callback_del().
 *
 */
EAPI void        *edje_object_signal_callback_del_full(Evas_Object *obj, const char *emission, const char *source, Edje_Signal_Cb func, void *data);

 /**
 * @deprecated use evas_object_size_hint_min_set() instead.
 * @brief Sets the object minimum size.
 *
 * @param obj A valid Evas_Object handle
 * @param minw The minimum width
 * @param minh The minimum height
 *
 * This sets the minimum size restriction for the object.
 *
 */
EINA_DEPRECATED EAPI void         edje_extern_object_min_size_set (Evas_Object *obj, Evas_Coord minw, Evas_Coord minh);

/**
 * @deprecated use evas_object_size_hint_max_set() instead.
 * @brief Sets the object maximum size.
 *
 * @param obj A valid Evas_Object handle
 * @param maxw The maximum width
 * @param maxh The maximum height
 *
 * This sets the maximum size restriction for the object.
 *
 */
EAPI void         edje_extern_object_max_size_set (Evas_Object *obj, Evas_Coord maxw, Evas_Coord maxh);

/**
 * @deprecated use evas_object_size_hint_aspect_set() instead.
 * @brief Sets the object aspect size.
 *
 * @param obj A valid Evas_Object handle
 * @param aspect The aspect control axes
 * @param aw The aspect radio width
 * @param ah The aspect ratio height
 *
 * This sets the desired aspect ratio to keep an object that will be
 * swallowed by Edje. The width and height define a preferred size
 * ASPECT and the object may be scaled to be larger or smaller, but
 * retaining the relative scale of both aspect width and height.
 *
 */
EAPI void         edje_extern_object_aspect_set   (Evas_Object *obj, Edje_Aspect_Control aspect, Evas_Coord aw, Evas_Coord ah);

/**
 * @brief Sets the @b EDJ file (and group within it) to load an Edje
 * object's contents from
 *
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on errors (check
 * edje_object_load_error_get() after this call to get errors causes)
 *
 * Edje expects EDJ files, which are theming objects' descriptions and
 * resources packed together in an EET file, to read Edje object
 * definitions from. They usually are created with the @c .edj
 * extension. EDJ files, in turn, are assembled from @b textual object
 * description files, where one describes Edje objects declaratively
 * -- the EDC files (see @ref edcref "the syntax" for those files).
 *
 * Those description files were designed so that many Edje object
 * definitions -- also called @b groups (or collections) -- could be
 * packed together <b>in the same EDJ file</b>, so that a whole
 * application's theme could be packed in one file only. This is the
 * reason for the @p group argument.
 *
 * Use this function after you instantiate a new Edje object, so that
 * you can "give him life", telling where to get its contents from.
 *
 * @see edje_object_add()
 * @see edje_object_file_get()
 * @see edje_object_mmap_set()
 *
 * @param[in] file The path to the EDJ file to load @p from
 * @param[in] group The name of the group, in @p file, which implements an
Edje object
 */
EAPI Eina_Bool edje_object_file_set(Edje_Object *obj, const char *file, const char *group);

/**
 * @brief Gets the file and group name that a given Edje object is bound to.
 *
 * This gets the EDJ file's path, with the respective group set for
 * the given Edje object. If @a obj is either not an Edje file, or has
 * not had its file/group set previously, by edje_object_file_set(),
 * then both @p file and @p group will be set to @c NULL, indicating
 * an error.
 *
 * @see edje_object_file_set()
 *
 * @note Use @c NULL pointers on the file/group components you're not
 * interested in: they'll be ignored by the function.
 *
 * @param[out] file The path to the EDJ file to load @p from
 * @param[out] group The name of the group, in @p file, which implements an
Edje object
 */
EAPI void edje_object_file_get(const Edje_Object *obj, const char **file, const char **group);


/**
 * @brief Sets the @b EDJ file (and group within it) to load an Edje
 * object's contents from.
 *
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on errors (check
 * edje_object_load_error_get() after this call to get errors causes)
 *
 * Edje expects EDJ files, which are theming objects' descriptions and
 * resources packed together in an EET file, to read Edje object
 * definitions from. They usually are created with the @c .edj
 * extension. EDJ files, in turn, are assembled from @b textual object
 * description files, where one describes Edje objects declaratively
 * -- the EDC files (see @ref edcref "the syntax" for those files).
 *
 * Those description files were designed so that many Edje object
 * definitions -- also called @b groups (or collections) -- could be
 * packed together <b>in the same EDJ file</b>, so that a whole
 * application's theme could be packed in one file only. This is the
 * reason for the @p group argument.
 *
 * Use this function after you instantiate a new Edje object, so that
 * you can "give him life", telling where to get its contents from.
 *
 * @see edje_object_add()
 * @see edje_object_file_get()
 * @see edje_object_mmap_set()
 * @since 1.8
 *
 * @param[in] file The Eina.File pointing to the EDJ file to load @p from
 * @param[in] group The name of the group, in @p file, which implements an
Edje object
 */
EAPI Eina_Bool edje_object_mmap_set(Edje_Object *obj, const Eina_File *file, const char *group);

/**
 * @brief "Swallows" an object into one of the Edje object @c SWALLOW parts.
 *
 * Swallowing an object into an Edje object is, for a given part of type
 * @c SWALLOW in the EDC group which gave life to  obj, to set an external
 * object to be controlled by  obj, being displayed exactly over that part's
 * region inside the whole Edje object's viewport.
 *
 * From this point on,  obj will have total control over obj_swallow's geometry
 * and visibility. For instance, if  obj is visible, as in @ref
 * evas_object_show(), the swallowed object will be visible too -- if the given
 * @c SWALLOW part it's in is also visible. Other actions on  obj will also
 * reflect on the swallowed object as well (e.g. resizing, moving,
 * raising/lowering, etc.).
 *
 * Finally, all internal changes to  part, specifically, will reflect on the
 * displaying of  obj_swallow, for example state changes leading to different
 * visibility states, geometries, positions, etc.
 *
 * If an object has already been swallowed into this part, then it will first
 * be unswallowed (as in edje_object_part_unswallow()) before the new object is
 * swallowed.
 *
 * @note  obj  won't delete the swallowed object once it is deleted --
 *  obj_swallow will get to an unparented state again.
 *
 * For more details on EDC @c SWALLOW parts, see @ref edcref "syntax
 * reference".
 *
 * @param[in] obj_swallow The object to occupy that part
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_swallow(Edje_Object *obj, const char *part, Evas_Object *obj_swallow);

/**
 * @brief Gets the object currently swallowed by a part.
 *
 * @param[in] part The part name
 *
 * @return The swallowed object, or @c null if there is none.
 *
 * @ingroup Edje_Object
 */
EAPI Evas_Object *edje_object_part_swallow_get(const Edje_Object *obj, const char *part);

/**
 * @brief Unswallows an object.
 *
 * Causes the edje to regurgitate a previously swallowed object. :)
 *
 * @note obj_swallow will  not be deleted or hidden. Note: obj_swallow may
 * appear shown on the evas depending on its state when it got unswallowed.
 * Make sure you delete it or hide it if you do not want it to.
 *
 * @param[in] obj_swallow The swallowed object
 *
 * @ingroup Edje_Object
 */
EAPI void edje_object_part_unswallow(Edje_Object *obj, Evas_Object *obj_swallow);

/**
 * @brief Retrieves a list all accessibility part names
 *
 * @return A list all accessibility part names on obj
 *
 * @since 1.7.0
 *
 * @ingroup Edje_Object
 */
EAPI Eina_List *edje_object_access_part_list_get(const Edje_Object *obj);

/**
 * @brief Appends an object to the box.
 *
 * Appends child to the box indicated by part.
 *
 * See also @ref edje_object_part_box_prepend(),
 * @ref edje_object_part_box_insert_before(),
 * @ref edje_object_part_box_insert_after() and
 * @ref edje_object_part_box_insert_at()
 *
 * @param[in] child The object to append
 *
 * @return @c true: Successfully added. @c false: An error occurred.
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_box_append(Edje_Object *obj, const char *part, Evas_Object *child);

/**
 * @brief Prepends an object to the box.
 *
 * Prepends child to the box indicated by part.
 *
 * See also @ref edje_object_part_box_append(),
 * @ref edje_object_part_box_insert_before(),
 * @ref edje_object_part_box_insert_after and
 * @ref edje_object_part_box_insert_at()
 *
 * @param[in] child The object to prepend
 *
 * @return @c true: Successfully added. @c false: An error occurred.
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_box_prepend(Edje_Object *obj, const char *part, Evas_Object *child);

/**
 * @brief Adds an object to the box.
 *
 * Inserts child in the box given by part, in the position marked by reference.
 *
 * See also @ref edje_object_part_box_append(),
 * @ref edje_object_part_box_prepend(),
 * @ref edje_object_part_box_insert_after() and
 * @ref edje_object_part_box_insert_at()
 *
 * @param[in] child The object to insert
 * @param[in] reference The object to be used as reference
 *
 * @return @c true: Successfully added. @c false: An error occurred.
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_box_insert_before(Edje_Object *obj, const char *part, Evas_Object *child, const Evas_Object *reference);

/**
 * @brief Adds an object to the box.
 *
 * Inserts child in the box given by part, in the position marked by reference.
 *
 * See also @ref edje_object_part_box_append(),
 * @ref edje_object_part_box_prepend(),
 * @ref edje_object_part_box_insert_before() and
 * @ref edje_object_part_box_insert_at()
 *
 * @param[in] child The object to insert
 * @param[in] reference The object to be used as reference
 *
 * @return @c true: Successfully added. @c false: An error occurred.
 *
 * @since 1.18
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_box_insert_after(Edje_Object *obj, const char *part, Evas_Object *child, const Evas_Object *reference);

/**
 * @brief Inserts an object to the box.
 *
 * Adds child to the box indicated by part, in the position given by pos.
 *
 * See also @ref edje_object_part_box_append(),
 * @ref edje_object_part_box_prepend(),
 * @ref edje_object_part_box_insert_before() and
 * @ref edje_object_part_box_insert_after()
 *
 * @param[in] child The object to insert
 * @param[in] pos The position where to insert child
 *
 * @return @c true: Successfully added. @c false: An error occurred.
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_box_insert_at(Edje_Object *obj, const char *part, Evas_Object *child, unsigned int pos);

/**
 * @brief Removes an object from the box.
 *
 * Removes from the box indicated by part, the object in the position pos.
 *
 * See also @ref edje_object_part_box_remove() and
 * @ref edje_object_part_box_remove_all()
 *
 * @param[in] pos The position index of the object (starts counting from 0)
 *
 * @return Pointer to the object removed, or @c null.
 *
 * @ingroup Edje_Object
 */
EAPI Evas_Object *edje_object_part_box_remove_at(Edje_Object *obj, const char *part, unsigned int pos);

/**
 * @brief Removes an object from the box.
 *
 * Removes child from the box indicated by part.
 *
 * See also @ref edje_object_part_box_remove_at() and
 * @ref edje_object_part_box_remove_all()
 *
 * @param[in] child The object to remove
 *
 * @return Pointer to the object removed, or @c null.
 *
 * @ingroup Edje_Object
 */
EAPI Evas_Object *edje_object_part_box_remove(Edje_Object *obj, const char *part, Evas_Object *child);

/**
 * @brief Removes all elements from the box.
 *
 * Removes all the external objects from the box indicated by part. Elements
 * created from the theme will not be removed.
 *
 * See also @ref edje_object_part_box_remove() and
 * @ref edje_object_part_box_remove_at()
 *
 * @param[in] clear Delete objects on removal
 *
 * @return 1: Successfully cleared. 0: An error occurred.
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_box_remove_all(Edje_Object *obj, const char *part, Eina_Bool clear);

/**
 * @brief Packs an object into the table.
 *
 * Packs an object into the table indicated by part.
 *
 * @param[in] child_obj The object to pack in
 * @param[in] col The column to place it in
 * @param[in] row The row to place it in
 * @param[in] colspan Columns the child will take
 * @param[in] rowspan Rows the child will take
 *
 * @return @c true object was added, @c false on failure
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_table_pack(Edje_Object *obj, const char *part, Evas_Object *child_obj, unsigned short col, unsigned short row, unsigned short colspan, unsigned short rowspan);

/**
 * @brief Removes an object from the table.
 *
 * Removes an object from the table indicated by part.
 *
 * @param[in] child_obj The object to pack in
 *
 * @return @c true object removed, @c false on failure
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_table_unpack(Edje_Object *obj, const char *part, Evas_Object *child_obj);

/**
 * @brief Gets the number of columns and rows the table has.
 *
 * Retrieves the size of the table in number of columns and rows.
 *
 * @param[out] cols Pointer where to store number of columns (can be @c null)
 * @param[out] rows Pointer where to store number of rows (can be @c null)
 *
 * @return @c true get some data, @c false on failure
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_table_col_row_size_get(const Edje_Object *obj, const char *part, int *cols, int *rows);

/**
 * @brief Retrieves a child from a table
 *
 * @param[in] col The column of the child to get
 * @param[in] row The row of the child to get
 *
 * @return The child Efl.Canvas.Object
 *
 * @ingroup Edje_Object
 */
EAPI Evas_Object *edje_object_part_table_child_get(const Edje_Object *obj, const char *part, unsigned int col, unsigned int row);

/**
 * @brief Removes all object from the table.
 *
 * Removes all object from the table indicated by part, except the internal
 * ones set from the theme.
 *
 * @param[in] clear If set, will delete subobjs on remove
 *
 * @return @c true clear the table, @c false on failure
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_table_clear(Edje_Object *obj, const char *part, Eina_Bool clear);

/**
 * @brief Sets the object color class.
 *
 * This function sets the color values for an object level color class. This
 * will cause all edje parts in the specified object that have the specified
 * color class to have their colors multiplied by these values.
 *
 * The first color is the object, the second is the text outline, and the third
 * is the text shadow. (Note that the second two only apply to text parts).
 *
 * Setting color emits a signal "color_class,set" with source being the given
 * color.
 *
 * @note unlike Evas, Edje colors are not pre-multiplied. That is,
 * half-transparent white is 255 255 255 128.
 *
 * @param[in] color_class The name of color class
 * @param[in] r Object Red value
 * @param[in] g Object Green value
 * @param[in] b Object Blue value
 * @param[in] a Object Alpha value
 * @param[in] r2 Outline Red value
 * @param[in] g2 Outline Green value
 * @param[in] b2 Outline Blue value
 * @param[in] a2 Outline Alpha value
 * @param[in] r3 Shadow Red value
 * @param[in] g3 Shadow Green value
 * @param[in] b3 Shadow Blue value
 * @param[in] a3 Shadow Alpha value
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_color_class_set(Evas_Object *obj, const char * color_class, int r, int g, int b, int a, int r2, int g2, int b2, int a2, int r3, int g3, int b3, int a3);

/**
 * @brief Gets the object color class.
 *
 * This function gets the color values for an object level color class. If no
 * explicit object color is set, then global values will be used.
 *
 * The first color is the object, the second is the text outline, and the third
 * is the text shadow. (Note that the second two only apply to text parts).
 *
 * @note unlike Evas, Edje colors are not pre-multiplied. That is,
 * half-transparent white is 255 255 255 128.
 *
 * @param[in] color_class The name of color class
 * @param[out] r Object Red value
 * @param[out] g Object Green value
 * @param[out] b Object Blue value
 * @param[out] a Object Alpha value
 * @param[out] r2 Outline Red value
 * @param[out] g2 Outline Green value
 * @param[out] b2 Outline Blue value
 * @param[out] a2 Outline Alpha value
 * @param[out] r3 Shadow Red value
 * @param[out] g3 Shadow Green value
 * @param[out] b3 Shadow Blue value
 * @param[out] a3 Shadow Alpha value
 *
 * @return true if found or false if not found and all values are zeroed.
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_color_class_get(const Evas_Object *obj, const char * color_class, int *r, int *g, int *b, int *a, int *r2, int *g2, int *b2, int *a2, int *r3, int *g3, int *b3, int *a3);

/**
 * @brief Enables selection if the entry is an EXPLICIT selection mode type.
 *
 * The default is to  not allow selection. This function only affects user
 * selection, functions such as edje_object_part_text_select_all() and
 * edje_object_part_text_select_none() are not affected.
 *
 * @param[in] part The part name
 * @param[in] allow true to enable, false otherwise
 *
 * @ingroup Edje_Object
 */
EAPI void edje_object_part_text_select_allow_set(const Edje_Object *obj, const char *part, Eina_Bool allow);

/**
 * @brief Sets a given text to an Edje object @c TEXT or TEXTBLOCK
 * parts.
 *
 * @param[in] part The part name
 * @param[in] text The text to set on that part
 *
 * @ingroup Edje_Object
 */
EAPI Eina_Bool edje_object_part_text_set(const Edje_Object *obj, const char *part, const char *text);

/**
 * @brief Gets the text currntly set to the given part
 *
 * @param[in] part The part name
 *
 * @return The text set on the part, @c null otherwise.
 *
 * @ingroup Edje_Object
 */
EAPI const char * edje_object_part_text_get(const Edje_Object *obj, const char *part);

#include "edje_object.eo.legacy.h"
#include "edje_edit.eo.legacy.h"
