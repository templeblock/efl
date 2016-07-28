#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <Eina.h>

#include "Ecore_Win32.h"
#include "ecore_win32_private.h"

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
//static void *sel_data = NULL;
//static size_t sel_length = 0;
//static Ecore_Win32_Selection_Format sel_format = ECORE_WIN32_SELECTION_FORMAT_TEXT;

static int init_count = 0;
static Ecore_Win32_Clipboard_Converter_Cb  sel_converter_cb = NULL;
static void *sel_converter_udata = NULL;
static UINT REGISTERED_MARKUP = 0;
static UINT REGISTERED_VCARD = 0;
static UINT REGISTERED_URL_LIST = 0;
static UINT REGISTERED_HTML = 0;

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

/**
 * @cond LOCAL
 */

 static void
_clipboard_init(void)
{
   init_count++;
   if (init_count > 1) return;
   REGISTERED_MARKUP = RegisterClipboardFormat("application/x-elementary-markup");
   REGISTERED_VCARD = RegisterClipboardFormat("text/x-vcard");
   REGISTERED_URL_LIST = RegisterClipboardFormat("text/uri-list");
   REGISTERED_HTML = RegisterClipboardFormat("text/html");
}

//FIXME: Group formats into category, since we have system converstion
//Ex: TEXT, UNICODETEXT, OEMTEXT -> TEXT
static UINT
_convert_to_windows_format(Ecore_Win32_Selection_Format format)
{
   switch (format)
     {
      case ECORE_WIN32_SELECTION_FORMAT_TEXT:
         ERR("CF: TEXT");
         return CF_TEXT;
      case ECORE_WIN32_SELECTION_FORMAT_UNICODE_TEXT:
         ERR("CF: UNICODETEXT");
         return CF_UNICODETEXT;
      case ECORE_WIN32_SELECTION_FORMAT_OEM_TEXT:
         ERR("CF: OEMTEXT");
         return CF_OEMTEXT;
      case ECORE_WIN32_SELECTION_FORMAT_DIB:
         ERR("CF: DIB");
         return CF_DIB;
      case ECORE_WIN32_SELECTION_FORMAT_BITMAP:
         ERR("CF: BITMAP");
         return CF_BITMAP;
      case ECORE_WIN32_SELECTION_FORMAT_MARKUP:
         ERR("CF: MARKUP");
         return REGISTERED_MARKUP;
      case ECORE_WIN32_SELECTION_FORMAT_VCARD:
         ERR("CF: VCARD");
         return REGISTERED_VCARD;
      case ECORE_WIN32_SELECTION_FORMAT_URL_LIST:
         ERR("CF: URL_LIST");
         return REGISTERED_URL_LIST;
      case ECORE_WIN32_SELECTION_FORMAT_HTML:
         ERR("CF: HTML");
         return REGISTERED_HTML;
     }

   ERR("CF: TEXT");
   return CF_TEXT;
}

static Ecore_Win32_Selection_Format
_convert_to_ecore_sel_format(UINT window_format)
{
   switch (window_format)
     {
      case CF_TEXT:
         ERR("->FORMAT: TEXT");
         return ECORE_WIN32_SELECTION_FORMAT_TEXT;
      case CF_UNICODETEXT:
         ERR("->FORMAT: UNICODETEXT");
         return ECORE_WIN32_SELECTION_FORMAT_UNICODE_TEXT;
      case CF_OEMTEXT:
         ERR("->FORMAT: OEMTEXT");
         return ECORE_WIN32_SELECTION_FORMAT_OEM_TEXT;
      case CF_DIB:
         ERR("->FORMAT: DIB");
         return ECORE_WIN32_SELECTION_FORMAT_DIB;
      case CF_BITMAP:
         ERR("->FORMAT: BITMAP");
         return ECORE_WIN32_SELECTION_FORMAT_BITMAP;
      case REGISTERED_MARKUP:
         ERR("->FORMAT: MARKUP");
         return ECORE_WIN32_SELECTION_FORMAT_MARKUP;
      case REGISTERED_VCARD:
         ERR("->FORMAT: VCARD");
         return ECORE_WIN32_SELECTION_FORMAT_VCARD;
      case REGISTERED_URL_LIST:
         ERR("->FORMAT: URL_LIST");
         return ECORE_WIN32_SELECTION_FORMAT_URL_LIST;
      case REGISTERED_HTML:
         ERR("->FORMAT: HTML");
         return ECORE_WIN32_SELECTION_FORMAT_HTML;
     }
   ERR("->FORMAT: TEXT");
   return ECORE_WIN32_SELECTION_FORMAT_TEXT;
}

static void
_print_type(Ecore_Win32_Callback_Data *msg)
{
   switch (window_format)
     {
      case CF_TEXT:
         ERR("CF_TEXT");
         break;
      case CF_UNICODETEXT:
         ERR("CF_UNICODETEXT");
         break;
      case CF_OEMTEXT:
         ERR("CF_OEMTEXT");
         break;
      case CF_BITMAP:
         ERR("CF_BITMAP");
         break;
      case CF_DIB:
         ERR("CF_DIB");
         break;
      case CF_DIBV5:
         ERR("CF_DIBV5");
         break;
      case REGISTERED_MARKUP:
         ERR("MARKUP");
         break;
      case REGISTERED_VCARD:
         ERR("VCARD");
         break;
      case REGISTERED_URL_LIST:
         ERR("URL_LIST");
         break;
      case REGISTERED_HTML:
         ERR("HTML");
         break;
      default:
         ERR("Other");
         break;
     }
}

void
_ecore_win32_set_clipboard(Ecore_Win32_Callback_Data *msg)
{
   ERR("In");
   _print_type(msg);

   void *data = NULL;
   int length = 0;

   if (sel_converter_cb)
     {
        ERR("Call to sel_converter_cb");
        sel_converter_cb(sel_converter_udata,
                         _convert_to_ecore_sel_format(msg->window_param),
                         &data,
                         &length);
     }
   //if (!sel_data)
   if (!data)
     {
        ERR("Sel_data does not exist!!!");
        return;
     }
   HGLOBAL hglb;
   LPTSTR lptstr;

   hglb = GlobalAlloc(GMEM_MOVEABLE, length + 1);
   if (hglb == NULL)
     {
        ERR("Cannot allocate memory");
        return;
     }

   ERR("set data to clipboard: %s; %d", (char *)data, length);
   lptstr = GlobalLock(hglb);
   memcpy(lptstr, data, length);
   lptstr[length] = (TCHAR) 0;
   GlobalUnlock(hglb);

   //need to convert back to correct format
   //SetClipboardData(_convert_to_windows_format(sel_format), hglb);
   //UINT win_format = _convert_to_windows_format(msg->window_param);
   //ERR("set format: %d", win_format);
   SetClipboardData(msg->window_param, hglb);
   //SetClipboardData(CF_TEXT, hglb);

   free(data);
}
/**
 * @endcond
 */



/*============================================================================*
 *                                   API                                      *
 *============================================================================*/


 
//EAPI Ecore_Win32_Handler*
EAPI void
ecore_win32_clipboard_converter_add(const Ecore_Win32_Window *window,
                                    Ecore_Win32_Clipboard_Converter_Cb func,
                                    void *udata)
{
   sel_converter_cb = func;
   sel_converter_udata = udata;
}

EAPI void
ecore_win32_clipboard_converter_del()
{
   sel_converter_cb = NULL;
   sel_converter_udata = NULL;
}

EAPI Eina_Bool
ecore_win32_clipboard_set(const Ecore_Win32_Window *window,
                          Ecore_Win32_Selection_Format format,
                          const void *data,
                          int size)
{
   HGLOBAL global;
   char *d;

   /*
    * 1. Open the clipboard
    * 2. Empty the clipboard
    * 3. Set the data
    * 4. Close the clipboard
    */

   //sel_format = format;
   INF("setting data to the clipboard");

   if (!window || !data || (size <= 0))
     {
        ERR("No window or data");
        return EINA_FALSE;
     }

   _clipboard_init();
   /*if (sel_data)
     {
     free(sel_data);
     }
     sel_data = calloc(1, size + 1);
     memcpy(sel_data, data, size);
     sel_length = size;*/

   if (!OpenClipboard(window->window))
     {
        ERR("Cannot open clipboard");
        return EINA_FALSE;
     }

   if (!EmptyClipboard())
     {
        ERR("Cannot empty clipboard");
        goto close_clipboard;
     }

   /*global = GlobalAlloc(GMEM_MOVEABLE, size + 1);
   if (!global)
   {
     ERR("Failed to alloc global");
     goto close_clipboard;
   }

   d = (char *)GlobalLock(global);
   if (!d)
   {
     ERR("cannot lock global");
     goto unlock_global;
   }

   memcpy(d, data, size);
   d[size] = '\0';
   GlobalUnlock(global);*/
   //SetClipboardData(CF_TEXT, global);
   UINT win_format = _convert_to_windows_format(format);
   ERR("set format: %d, value: NULL (delayed rendering)", win_format);
   SetClipboardData(win_format, NULL); //Enable delayed rendering
   //for working with other Windows apps (like notepad)
   if ((win_format == REGISTERED_MARKUP) ||
        (win_format == REGISTERED_HTML) ||
        (win_format == REGISTERED_URL_LIST))
     {
        ERR("Also set format as CF_TEXT");
        SetClipboardData(CF_TEXT, NULL);
     }
   //SetClipboardData(CF_TEXT, NULL); //Enable delayed rendering
   CloseClipboard();

   ERR("End");
   return EINA_TRUE;

 /*unlock_global:
   GlobalUnlock(global);*/
 close_clipboard:
   CloseClipboard();

   return EINA_FALSE;
}

/*EAPI Eina_Bool
ecore_win32_clipboard_set(const Ecore_Win32_Window *window,
                          const void *data,
                          int size)
{
   HGLOBAL global;
   char *d;

   /*
    * See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms649016%28v=vs.85%29.aspx#_win32_Copying_Information_to_the_Clipboard
    * 1. Open the clipboard
    * 2. Empty the clipboard
    * 3. Set the data
    * 4. Close the clipboard
    *

   INF("setting data to the clipboard");

   if (!window || !data || (size <= 0))
   {
     ERR("No window or data");
     return EINA_FALSE;
   }

   if (!OpenClipboard(window->window))
   {
      ERR("Cannot open clipboard");
     return EINA_FALSE;
   }

   if (!EmptyClipboard())
   {
     ERR("Cannot empty clipboard");
     goto close_clipboard;
   }

   /*global = GlobalAlloc(GMEM_MOVEABLE, size + 1);
   if (!global)
   {
     ERR("Failed to alloc global");
     goto close_clipboard;
   }

   d = (char *)GlobalLock(global);
   if (!d)
   {
     ERR("cannot lock global");
     goto unlock_global;
   }

   memcpy(d, data, size);
   d[size] = '\0';
   GlobalUnlock(global);*
   //SetClipboardData(CF_TEXT, global);
   SetClipboardData(CF_TEXT, NULL); //Enable delayed rendering
   CloseClipboard();

   ERR("End");
   return EINA_TRUE;

 /*unlock_global:
   GlobalUnlock(global);*
 close_clipboard:
   CloseClipboard();

   return EINA_FALSE;
}*/

EAPI Eina_Bool
ecore_win32_clipboard_get(const Ecore_Win32_Window *window,
                          Ecore_Win32_Selection_Format format,
                          void **data,
                          int *size)
{
   HGLOBAL global;
   void *d;
   void *p;

   /*
    * See https://msdn.microsoft.com/en-us/library/windows/desktop/ms649016%28v=vs.85%29.aspx#_win32_Pasting_Information_from_the_Clipboard
    * 1. Open Clipboard
    * 2. Determine format
    * 3. Retrieve data
    * 4. Manage data
    * 5. Close clipboard
    */

   INF("getting data from the clipboard");

   ERR("In");
   if (!window)
     return EINA_FALSE;

   _clipboard_init();
   UINT win_format = _convert_to_windows_format(format);
   ERR("get format: %d", win_format);
   if (!IsClipboardFormatAvailable(win_format))
     {
        ERR("Format: %d is NOT available", win_format);
        return EINA_FALSE;
     }

   if (!OpenClipboard(window->window))
     goto set_val;

   /* { */
   /*   UINT fmt = 0; */

   /*   while (1) */
   /*     { */
   /*       fmt = EnumClipboardFormats(fmt); */
   /*       printf(" $ Format : %x\n", fmt); */
   /*       if (!fmt) */
   /*         break; */
   /*     } */
   /* } */

   ERR("Get data from clipboard");
   global = GetClipboardData(win_format);
   //global = GetClipboardData(CF_TEXT);
   if (!global)
     goto close_clipboard;

   d = GlobalLock(global);
   if (!d)
     goto unlock_global;

   *size = strlen(d);
   p = malloc(*size);
   if (!p)
     goto unlock_global;

   memcpy(p, d, *size);
   *data = p;
   GlobalUnlock(global);
   CloseClipboard();
   ERR("End");

   return EINA_TRUE;

 unlock_global:
   GlobalUnlock(global);
 close_clipboard:
   CloseClipboard();
 set_val:
   *data = NULL;
   *size = 0;

   return EINA_FALSE;
}

EAPI Eina_Bool
ecore_win32_clipboard_clear(const Ecore_Win32_Window *window)
{
   INF("clearing the clipboard");

   if (!window)
     return EINA_FALSE;

   if (!OpenClipboard(window->window))
     return EINA_FALSE;

   if (!EmptyClipboard())
     goto close_clipboard;

   CloseClipboard();

   return EINA_TRUE;

 close_clipboard:
   CloseClipboard();

   return EINA_FALSE;
}

/**
 * @}
 */
