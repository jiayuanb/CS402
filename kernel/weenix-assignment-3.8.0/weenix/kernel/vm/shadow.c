/******************************************************************************/
/* Important Fall 2022 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "globals.h"
#include "errno.h"

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        shadow_allocator = slab_allocator_create("shadow", sizeof(mmobj_t));
        KASSERT(shadow_allocator != NULL);
        dbg(DBG_PRINT, "(GRADING3A 6.a)\n");

}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros or functions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
        // NOT_YET_IMPLEMENTED("VM: shadow_create");
        mmobj_t *shadow_mmobj = (mmobj_t *)slab_obj_alloc(shadow_allocator);
        mmobj_init(shadow_mmobj, &shadow_mmobj_ops);
        shadow_mmobj->mmo_refcount = 1;
        return shadow_mmobj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_ref");
        // dbg_print("[*] ref shadow obj %#08x to refcount=%d\n", (uint32_t)o, o->mmo_refcount+1);
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
                                  /* the o function argument must be non-NULL, has a positive refcount, and is a shadow object */
        dbg(DBG_PRINT, "(GRADING3A 6.b)\n");

        o->mmo_refcount++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
                                  /* the o function argument must be non-NULL, has a positive refcount, and is a shadow object */
        dbg(DBG_PRINT, "(GRADING3A 6.c)\n");


        if(o->mmo_refcount <= 0){
                o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
                slab_obj_free(shadow_allocator, o);
                return;
        }
        o->mmo_refcount--;
        pframe_t *pf;
        mmobj_t * shadow_mmobj = o->mmo_shadowed;
        if (o->mmo_refcount == o->mmo_nrespages)
        {
                if(o->mmo_nrespages == 0){
                        o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
                        slab_obj_free(shadow_allocator, o);
                        return;
                }
                else{
                        pframe_t *pf = list_head(&o->mmo_respages, pframe_t, pf_olink);
                        if(pframe_is_pinned(pf)){
                                pframe_unpin(pf);
                        }
                        if(pf->pf_pincount == 0){
                                if(pframe_is_dirty(pf)){
                                        pframe_clean(pf);
                                }
                                pframe_free(pf);
                        }
                }

        }
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. It is important to
 * use iteration rather than recursion here as a recursive implementation
 * can overflow the kernel stack when looking down a long shadow chain */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_lookuppage");
        // dbg_print("[*] lookup shadow obj page, mmobj=%#08x, pagenum=%d, forwrite=%d\n", (uint32_t)o, pagenum, forwrite);
        pframe_t *page = NULL;
        // when forwrite is false find the first shadow object in the chain which has the given page resident
        if (forwrite == 0)
        {
                mmobj_t *shadow_object = o;
                while (shadow_object->mmo_shadowed != NULL)
                {
                        page = pframe_get_resident(shadow_object, pagenum);
                        if (page != NULL)
                        {
                                *pf = page;
                                KASSERT((*pf)->pf_pagenum == pagenum && !pframe_is_busy(*pf));
                                
                                KASSERT(NULL != (*pf)); /* on return, (*pf) must be non-NULL */
                                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");

                                KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
                                                    /* on return, the page frame must have the right pagenum and it must not be in the "busy" state */
                                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");

                                return 0;
                        }
                        shadow_object = shadow_object->mmo_shadowed;
                }
                // dbg_print("[*] lookup bottom mmobj in shadow\n");
                // int ret = pframe_lookup(shadow_object, pagenum, 0, pf);
                int ret = shadow_object->mmo_ops->lookuppage(shadow_object, pagenum, forwrite, pf);
                // dbg_print("[*] exit lookup shadow %d\n", ret);
                return ret;
        }else if (forwrite == 1)
        {
    
                int ret = pframe_get(o, pagenum, pf);
                return ret;
        }
        else{
                panic("shadow lookuppage has wrong forwrite=%d\n", forwrite);
                return -1;
        }
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain).
 * It is important to use iteration rather than recursion here as a
 * recursive implementation can overflow the kernel stack when
 * looking down a long shadow chain */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_fillpage");
        // dbg_print("[*] Enter fill shadow page, pf=%#08x\n", (uint32_t)pf);
        // find mmojb it is shadowing to fill page
        KASSERT(pframe_is_busy(pf)); /* can only "fill" a page frame when the page frame is in the "busy" state */
        dbg(DBG_PRINT, "(GRADING3A 6.e)\n");

        KASSERT(!pframe_is_pinned(pf)); /* must not fill a page frame that's already pinned */
        dbg(DBG_PRINT, "(GRADING3A 6.e)\n");

        o = o->mmo_shadowed;
        while(o->mmo_shadowed){
                pframe_t *pf_curlevel = pframe_get_resident(o, pf->pf_pagenum);
                if(pf_curlevel){
                        dbg_print("[*] find in shadow obj\n");
                        memcpy(pf->pf_addr, pf_curlevel->pf_addr, PAGE_SIZE);
                        o->mmo_ops->dirtypage(o, pf);
                        return 0;
                }
                o = o->mmo_shadowed;
        }
        pframe_t *bottom_obj_pframe;
        dbg_print("search bottom obj\n");
        int ret = pframe_get(o, pf->pf_pagenum, &bottom_obj_pframe);
        if(ret < 0){
                return ret;
        }
        memcpy(pf->pf_addr, bottom_obj_pframe->pf_addr, PAGE_SIZE);
        o->mmo_ops->dirtypage(o, pf);
        return 0;
}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_dirtypage");
        KASSERT(pf);
        KASSERT(o);
        dbg_print("dirty pframe %#08x in shadow obj\n", (uint32_t)pf);
        pframe_set_dirty(pf);
        return 0;
        // return -1;
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
        KASSERT(pf);
        KASSERT(o);
        dbg_print("clean pframe in shadow obj\n");
        // return pframe_clean(pf);
        dbg_print("clear dirty pframe %#08x in shadow obj\n", (uint32_t)pf);
        pframe_clear_dirty(pf);
        return 0;
        // NOT_YET_IMPLEMENTED("VM: shadow_cleanpage");
        // return -1;
}
