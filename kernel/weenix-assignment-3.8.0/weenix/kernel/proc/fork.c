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

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
        KASSERT(regs != NULL); /* the function argument must be non-NULL */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        KASSERT(curproc != NULL); /* the parent process, which is curproc, must be non-NULL */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        KASSERT(curproc->p_state == PROC_RUNNING); /* the parent process must be in the running state and not in the zombie state */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        vmarea_t *vma, *clone_vma;
        pframe_t *pf;
        mmobj_t *to_delete, *new_shadowed;


        vmarea_t *curvma;

        KASSERT(regs);
        // dbg_print("[*] Enter do_fork\n");
        proc_t *child = proc_create(curproc->p_comm);
        KASSERT(child->p_state == PROC_RUNNING); /* new child process starts in the running state */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        KASSERT(child->p_pagedir != NULL); /* new child process must have a valid page table */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        // dbg_print("[*] do fork create proc pid=%d\n", child->p_pid);
        vmmap_t *child_vmmap = child->p_vmmap;

        child->p_vmmap = vmmap_clone(curproc->p_vmmap);
        if(child_vmmap){
                vmmap_destroy(child_vmmap);
        }
        child->p_vmmap->vmm_proc = child;
        // dbg_print("[*] do fork finish copying vmmap\n");

        list_iterate_begin(&child->p_vmmap->vmm_list, curvma, vmarea_t, vma_plink){
                // dbg_print("curvma [%#08x, %#08x), prot=%d, flags=%d\n", curvma->vma_start, curvma->vma_end, curvma->vma_prot, curvma->vma_flags);
                // dbg_print("%d, %d\n", ((curvma->vma_prot & PROT_WRITE) == PROT_WRITE), ((curvma->vma_flags & MAP_TYPE) == MAP_PRIVATE));
                if((curvma->vma_prot & PROT_WRITE) == PROT_WRITE && (curvma->vma_flags & MAP_TYPE) == MAP_PRIVATE){
                        // create child process new shadow object
                        // dbg_print("create child process new shadow object\n");
                        vmarea_t *parent_proc_vmarea = vmmap_lookup(curproc->p_vmmap, curvma->vma_start);
                        mmobj_t *parent_proc_vmarea_mmobj = parent_proc_vmarea->vma_obj;
                        curvma->vma_obj = shadow_create();
                        curvma->vma_obj->mmo_shadowed = parent_proc_vmarea_mmobj;
                        if(parent_proc_vmarea_mmobj->mmo_shadowed == NULL){ // is bottom
                                curvma->vma_obj->mmo_un.mmo_bottom_obj = parent_proc_vmarea_mmobj;
                        }
                        else{
                                curvma->vma_obj->mmo_un.mmo_bottom_obj = parent_proc_vmarea_mmobj->mmo_un.mmo_bottom_obj;
                        }
                        mmobj_t *last = parent_proc_vmarea_mmobj;
                        while(last->mmo_shadowed){
                                last->mmo_ops->ref(last);
                                last = last->mmo_shadowed;
                        }
                        // last->mmo_ops->ref(last);

                        // create parent process new shadow object
                        // dbg_print("create parent process new shadow object\n");
                        parent_proc_vmarea->vma_obj = shadow_create();
                        parent_proc_vmarea->vma_obj->mmo_shadowed = parent_proc_vmarea_mmobj;
                        if(parent_proc_vmarea_mmobj->mmo_shadowed == NULL){
                                parent_proc_vmarea->vma_obj->mmo_un.mmo_bottom_obj = parent_proc_vmarea_mmobj;
                        }
                        else{
                                parent_proc_vmarea->vma_obj->mmo_un.mmo_bottom_obj = parent_proc_vmarea_mmobj->mmo_un.mmo_bottom_obj;
                        }
                        last = parent_proc_vmarea_mmobj;
                        // while(last->mmo_shadowed){
                        //         last->mmo_ops->ref(last);
                        //         last = last->mmo_shadowed;
                        // }
                        // last->mmo_ops->ref(last);

                        // add new vmarea to bottom mmobj mmo_vmas
                        list_insert_tail(&curvma->vma_obj->mmo_un.mmo_bottom_obj->mmo_un.mmo_vmas, &curvma->vma_olink);
                }
                else{
                        // shared mapping
                        // dbg_print("share mapping\n");
                        // dbg_print("curvma [%#08x, %#08x)\n", curvma->vma_start, curvma->vma_end);
                        vmarea_t *parent_proc_vmarea = vmmap_lookup(curproc->p_vmmap, curvma->vma_start);
                        mmobj_t *parent_proc_vmarea_mmobj = parent_proc_vmarea->vma_obj;
                        curvma->vma_obj = parent_proc_vmarea_mmobj;
                        if(parent_proc_vmarea_mmobj->mmo_shadowed == NULL){
                                list_insert_tail(&parent_proc_vmarea_mmobj->mmo_un.mmo_vmas, &curvma->vma_olink);
                        }
                        else{
                                mmobj_t *bottom = parent_proc_vmarea_mmobj->mmo_un.mmo_bottom_obj;
                                list_insert_tail(&bottom->mmo_un.mmo_vmas, &curvma->vma_olink);
                        }

                        mmobj_t *last = parent_proc_vmarea_mmobj;
                        while(last->mmo_shadowed){
                                last->mmo_ops->ref(last);
                                // dbg_print("shared mapping ref mmobj=%#08x, refcount=%d, nrespages=%d\n", (uint32_t)last, last->mmo_refcount, last->mmo_nrespages);
                                last = last->mmo_shadowed;
                        }
                        last->mmo_ops->ref(last);
                        // dbg_print("shared mapping ref mmobj=%#08x, refcount=%d, nrespages=%d\n", (uint32_t)last, last->mmo_refcount, last->mmo_nrespages);
                }
        } list_iterate_end();
        // dbg_print("[*] do fork finish private mapping\n");


        pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();


        for(int i=0; i<NFILES; i++){
                child->p_files[i] = curproc->p_files[i];
                if(child->p_files[i]){
                        fref(child->p_files[i]);
                }
        }

        child->p_cwd = curproc->p_cwd;
        vref(child->p_cwd);
        dbg_print("check all file descripts\n");

        kthread_t *curthread;
        list_iterate_begin(&curproc->p_threads, curthread, kthread_t, kt_plink){
                kthread_t *child_thread_clone = kthread_clone(curthread);
                child_thread_clone->kt_proc = child;
                list_remove(&child_thread_clone->kt_plink);
                list_insert_tail(&child->p_threads, &child_thread_clone->kt_plink);

                if(curthread == curthr){
                        child_thread_clone->kt_ctx.c_pdptr = child->p_pagedir;
                        child_thread_clone->kt_ctx.c_eip = (uint32_t)userland_entry;
                        uint32_t origin_eax = regs->r_eax;
                        regs->r_eax = 0;
                        child_thread_clone->kt_ctx.c_esp = fork_setup_stack(regs, child_thread_clone->kt_kstack);
                        child_thread_clone->kt_ctx.c_kstack = (uintptr_t)child_thread_clone->kt_kstack;
                        child_thread_clone->kt_ctx.c_kstacksz = (uintptr_t)child_thread_clone->kt_ctx.c_esp - (uintptr_t)child_thread_clone->kt_ctx.c_kstack;
                        // child_thread_clone->kt_retval = 0;
                        regs->r_eax = origin_eax;
                }
                
                KASSERT(child_thread_clone->kt_kstack != NULL); /* thread in the new child process must have a valid kernel stack */
                dbg(DBG_PRINT, "(GRADING3A 7.a)\n");


        } list_iterate_end();

        // dbg_print("[*] create all thread\n");
        child->p_start_brk = curproc->p_start_brk;
        child->p_brk = curproc->p_brk;
        list_iterate_begin(&child->p_threads, curthread, kthread_t, kt_plink){
                sched_make_runnable(curthread);
        } list_iterate_end();
        return child->p_pid;


}
