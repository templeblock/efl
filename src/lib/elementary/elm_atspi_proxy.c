#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include <Elementary.h>
#include <assert.h>
#include "elm_priv.h"

typedef struct _Elm_Atspi_Proxy_Data {
     const char *bus;
     const char *path;
     Elm_Atspi_Bridge *bridge;
} Elm_Atspi_Proxy_Data;

EOLIAN void
_elm_atspi_proxy_efl_object_destructor(Eo *obj EINA_UNUSED, Elm_Atspi_Proxy_Data *pd)
{
   eina_stringshare_del(pd->bus);
   eina_stringshare_del(pd->path);
}

EOLIAN void
_elm_atspi_proxy_fixed_address_get(Eo *obj EINA_UNUSED, Elm_Atspi_Proxy_Data *pd, const char **bus, const char **path)
{
   if (bus) *bus = pd->bus;
   if (path) *path = pd->path;
}

EOLIAN void
_elm_atspi_proxy_fixed_address_set(Eo *obj EINA_UNUSED, Elm_Atspi_Proxy_Data *pd, const char *bus, const char *path)
{
   eina_stringshare_replace(&pd->bus, bus);
   eina_stringshare_replace(&pd->path, path);
}

EOLIAN void
_elm_atspi_proxy_deserialize_constructor(Eo *obj, Elm_Atspi_Proxy_Data *pd, const char *id)
{
   return;
}

EOLIAN const char*
_elm_atspi_proxy_serialize(Eo *obj, Elm_Atspi_Proxy_Data *pd)
{
   char buf[512];

   int written = snprintf("%s:%s", pd->bus, pd->path);
   if (written >= sizeof(buf))
     {
        DBG("Invalid bus & path strings. Unable to serialize");
        return NULL;
     }
   return strdup(buf);
}

#include "elm_atspi_proxy.eo.c"
