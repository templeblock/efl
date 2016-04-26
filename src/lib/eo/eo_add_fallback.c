#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined HAVE_DLADDR && ! defined _WIN32
# include <dlfcn.h>
#endif

#include <Eina.h>

#include "eo_ptr_indirection.h"
#include "eo_private.h"

#include "eo_add_fallback.h"

// 1024 entries == 16k or 32k (32 or 64bit) for eo call stack. that's 1023
// imbricated/recursive calls it can handle before barfing. i'd say that's ok
#define EO_CALL_STACK_DEPTH_MIN 1024

typedef struct _Eo_Call_Stack {
   Eo_Stack_Frame *frames;
   Eo_Stack_Frame *frame_ptr;
} Eo_Call_Stack;

#define EO_CALL_STACK_SIZE (EO_CALL_STACK_DEPTH_MIN * sizeof(Eo_Stack_Frame))

static Eina_TLS _eo_call_stack_key = 0;

#define MEM_PAGE_SIZE 4096

static void *
_eo_call_stack_mem_alloc(size_t size)
{
#ifdef HAVE_MMAP
   // allocate eo call stack via mmped anon segment if on linux - more
   // secure and safe. also gives page aligned memory allowing madvise
   void *ptr;
   size_t newsize;
   newsize = MEM_PAGE_SIZE * ((size + MEM_PAGE_SIZE - 1) /
                              MEM_PAGE_SIZE);
   ptr = mmap(NULL, newsize, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANON, -1, 0);
   if (ptr == MAP_FAILED)
     {
        ERR("eo call stack mmap failed.");
        return NULL;
     }
   return ptr;
#else
   //in regular cases just use malloc
   return calloc(1, size);
#endif
}

static void
_eo_call_stack_mem_free(void *ptr, size_t size)
{
#ifdef HAVE_MMAP
   munmap(ptr, size);
#else
   (void) size;
   free(ptr);
#endif
}

static Eo_Call_Stack *
_eo_call_stack_create()
{
   Eo_Call_Stack *stack;

   stack = calloc(1, sizeof(Eo_Call_Stack));
   if (!stack)
     return NULL;

   stack->frames = _eo_call_stack_mem_alloc(EO_CALL_STACK_SIZE);
   if (!stack->frames)
     {
        free(stack);
        return NULL;
     }

   // first frame is never used
   stack->frame_ptr = stack->frames;

   return stack;
}

static void
_eo_call_stack_free(void *ptr)
{
   Eo_Call_Stack *stack = (Eo_Call_Stack *) ptr;

   if (!stack) return;

   if (stack->frames)
     _eo_call_stack_mem_free(stack->frames, EO_CALL_STACK_SIZE);

   free(stack);
}

static Eo_Call_Stack *main_loop_stack = NULL;

#define _EO_CALL_STACK_GET() ((EINA_LIKELY(eina_main_loop_is())) ? main_loop_stack : _eo_call_stack_get_thread())

static inline Eo_Call_Stack *
_eo_call_stack_get_thread(void)
{
   Eo_Call_Stack *stack = eina_tls_get(_eo_call_stack_key);

   if (stack) return stack;

   stack = _eo_call_stack_create();
   eina_tls_set(_eo_call_stack_key, stack);

   return stack;
}

EAPI Eo *
_eo_self_get(void)
{
   return _EO_CALL_STACK_GET()->frame_ptr->obj;
}

Eo_Stack_Frame *
_eo_add_fallback_stack_push(Eo *obj)
{
   Eo_Call_Stack *stack = _EO_CALL_STACK_GET();
   if (stack->frame_ptr == (stack->frames + EO_CALL_STACK_DEPTH_MIN))
     {
        CRI("eo_add fallback stack overflow.");
     }

   stack->frame_ptr++;
   stack->frame_ptr->obj = obj;

   return stack->frame_ptr;
}

Eo_Stack_Frame *
_eo_add_fallback_stack_pop(void)
{
   Eo_Call_Stack *stack = _EO_CALL_STACK_GET();
   if (stack->frame_ptr == stack->frames)
     {
        CRI("eo_add fallback stack underflow.");
     }

   stack->frame_ptr--;

   return stack->frame_ptr;
}

Eina_Bool
_eo_add_fallback_init(void)
{
   if (_eo_call_stack_key != 0)
     WRN("_eo_call_stack_key already set, this should not happen.");
   else
     {
        if (!eina_tls_cb_new(&_eo_call_stack_key, _eo_call_stack_free))
          {
             EINA_LOG_ERR("Could not create TLS key for call stack.");
             return EINA_FALSE;

          }
     }

   main_loop_stack = _eo_call_stack_create();
   if (!main_loop_stack)
     {
        EINA_LOG_ERR("Could not alloc eo call stack.");
        return EINA_FALSE;
     }

   return EINA_TRUE;
}

Eina_Bool
_eo_add_fallback_shutdown(void)
{
   if (_eo_call_stack_key != 0)
     {
        eina_tls_free(_eo_call_stack_key);
        _eo_call_stack_key = 0;
     }

   return EINA_TRUE;
}