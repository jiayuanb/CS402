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

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int  anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  anon_fillpage(mmobj_t *o, pframe_t *pf);
static int  anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int  anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
        .ref = anon_ref,
        .put = anon_put,
        .lookuppage = anon_lookuppage,
        .fillpage  = anon_fillpage,
        .dirtypage = anon_dirtypage,
        .cleanpage = anon_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * anonymous page sub system. Currently it only initializes the
 * anon_allocator object.
 */
void
anon_init()
{
        anon_allocator = slab_allocator_create("anon", sizeof(mmobj_t));
        KASSERT(anon_allocator != NULL);
        dbg(DBG_PRINT, "(GRADING3A 4.a)\n");

}

/*
 * You'll want to use the anon_allocator to allocate the mmobj to
 * return, then initialize it. Take a look in mm/mmobj.h for
 * definitions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
anon_create()
{
        mmobj_t *anon_mmobj = slab_obj_alloc(anon_allocator);
        mmobj_init(anon_mmobj, &anon_mmobj_ops);
        anon_mmobj->mmo_refcount = 1;
        return anon_mmobj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
anon_ref(mmobj_t *o)
{

        KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
                                  /* the o function argument must be non-NULL, has a positive refcount, and is an anonymous object */
        dbg(DBG_PRINT, "(GRADING3A 4.b)\n");

        o->mmo_refcount++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is an anonymous object, it will
 * never be used again. You should unpin and uncache all of the
 * object's pages and then free the object itself.
 */
static void
anon_put(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
                                  /* the o function argument must be non-NULL, has a positive refcount, and is an anonymous object */
        dbg(DBG_PRINT, "(GRADING3A 4.c)\n");

        if(o->mmo_refcount == 0){
                return;
        }
        o->mmo_refcount--;
        if(o->mmo_nrespages == o->mmo_refcount){
                pframe_t *curpf;

                while(!list_empty(&o->mmo_respages)){
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
                if(o->mmo_nrespages){
                        slab_obj_free(anon_allocator, o);
                }
        }
        // NOT_YET_IMPLEMENTED("VM: anon_put");
}

/* Get the corresponding page from the mmobj. No special handling is
 * required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        return pframe_get(o, pagenum, pf);
}

/* The following three functions should not be difficult. */

        /* Fill the page frame starting at address pf->pf_addr with the
         * contents of the page identified by pf->pf_obj and pf->pf_pagenum.
         * This may block.
         * Return 0 on success and -errno otherwise.
         */
static int
anon_fillpage(mmobj_t *o, pframe_t *pf)
{

        KASSERT(pframe_is_busy(pf)); /* can only "fill" a page frame when the page frame is in the "busy" state */
        dbg(DBG_PRINT, "(GRADING3A 4.d)\n");

        KASSERT(!pframe_is_pinned(pf)); /* must not fill a page frame that's already pinned */
        dbg(DBG_PRINT, "(GRADING3A 4.d)\n");

        KASSERT(pf);
        KASSERT(o);
        memset(pf->pf_addr, 0, PAGE_SIZE);
        anon_dirtypage(o, pf);
        return 0;

        // KASSERT(NULL != pf);
        // KASSERT(NULL != o);

        // vnode_t *v = mmobj_to_vnode(o);
        // return v->vn_ops->fillpage(v, (int)PN_TO_ADDR(pf->pf_pagenum), pf->pf_addr);
}

        /* A hook; called when a request is made to dirty a non-dirty page.
         * Perform any necessary actions that must take place in order for it
         * to be possible to dirty (write to) the provided page. (For example,
         * if this page corresponds to a sparse block of a file that belongs to
         * an S5 filesystem, it would be necessary/desirable to allocate a
         * block in the fs before allowing a write to the block to proceed).
         * This may block.
         * Return 0 on success and -errno otherwise.
         */
static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
        dbg_print("[*] dirty anon object page\n");
        KASSERT(pf);
        KASSERT(o);
        pframe_set_dirty(pf);
        return 0;
}

        /*
         * Write the contents of the page frame starting at address
         * pf->pf_paddr to the page identified by pf->pf_obj and
         * pf->pf_pagenum.
         * This may block.
         * Return 0 on success and -errno otherwise.
         */
static int
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
        dbg_print("[*] clean anon object page\n");
        KASSERT(pf);
        KASSERT(o);
        
        pframe_clear_dirty(pf);
        // KASSERT(pf);
        // KASSERT(o);
        // mmobj_t *pf_mmobj = pf->pf_obj;
        // uint32_t pf_pagenum = pf->pf_pagenum;
        // pframe_t *write_to;
        // int err = o->mmo_ops->lookuppage(o, pf_pagenum, 1, &write_to);
        // if(err < 0){
        //         return err;
        // }
        // memcpy(write_to->pf_addr, pf->pf_addr, PAGE_SIZE);
        
        return 0;
}
