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
#include "util/debug.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/mman.h"

#include "vm/mmap.h"
#include "vm/vmmap.h"

#include "proc/proc.h"

/*
 * This function implements the brk(2) system call.
 *
 * This routine manages the calling process's "break" -- the ending address
 * of the process's "dynamic" region (often also referred to as the "heap").
 * The current value of a process's break is maintained in the 'p_brk' member
 * of the proc_t structure that represents the process in question.
 *
 * The 'p_brk' and 'p_start_brk' members of a proc_t struct are initialized
 * by the loader. 'p_start_brk' is subsequently never modified; it always
 * holds the initial value of the break. Note that the starting break is
 * not necessarily page aligned!
 *
 * 'p_start_brk' is the lower limit of 'p_brk' (that is, setting the break
 * to any value less than 'p_start_brk' should be disallowed).
 *
 * The upper limit of 'p_brk' is defined by the minimum of (1) the
 * starting address of the next occuring mapping or (2) USER_MEM_HIGH.
 * That is, growth of the process break is limited only in that it cannot
 * overlap with/expand into an existing mapping or beyond the region of
 * the address space allocated for use by userland. (note the presence of
 * the 'vmmap_is_range_empty' function).
 *
 * The dynamic region should always be represented by at most ONE vmarea.
 * Note that vmareas only have page granularity, you will need to take this
 * into account when deciding how to set the mappings if p_brk or p_start_brk
 * is not page aligned.
 *
 * You are guaranteed that the process data/bss region is non-empty.
 * That is, if the starting brk is not page-aligned, its page has
 * read/write permissions.
 *
 * If addr is NULL, you should "return" the current break. We use this to
 * implement sbrk(0) without writing a separate syscall. Look in
 * user/libc/syscall.c if you're curious.
 *
 * You should support combined use of brk and mmap in the same process.
 *
 * Note that this function "returns" the new break through the "ret" argument.
 * Return 0 on success, -errno on failure.
 */
int
do_brk(void *addr, void **ret)
{
        dbg_print("[*] in do_brk addr %p, start %p, end %p\n", addr, curproc->p_start_brk, curproc->p_brk);
        if(addr == NULL){
                // dbg_print("addr = null\n");
                *ret = curproc->p_brk;
                return 0;
        }
        uintptr_t start_page = ADDR_TO_PN(curproc->p_start_brk);
        uintptr_t end_page = ADDR_TO_PN(curproc->p_brk);
        uintptr_t target_page = ADDR_TO_PN(addr);
        vmarea_t* myheap_area = vmmap_lookup(curproc->p_vmmap, start_page);

        uintptr_t area_end = myheap_area->vma_end;   
        if(addr < curproc->p_start_brk){
                dbg_print("do_brk target_page < start_page\n");
                return -ENOMEM;
        }

        // make new mapp
        if((end_page  < area_end) && (target_page >= area_end)){ // create a new vmarea
                vmmap_map(curproc->p_vmmap, NULL, area_end, target_page - area_end +1,PROT_WRITE | PROT_READ, MAP_PRIVATE, 0, VMMAP_DIR_HILO, &myheap_area);

        }else if((area_end < end_page) && (target_page < area_end)){ // reduce a page
                vmmap_remove(curproc->p_vmmap, area_end, end_page - area_end +1);

        }else if((end_page >= area_end) && (target_page >=  area_end)){
                myheap_area = vmmap_lookup(curproc->p_vmmap, end_page);
                // list_iterate_begin(&curproc->p_vmmap->vmm_list,curvma, vmarea_t, vma_plink){
                //         if(&curvma->vma_plink == myheap_area->vma_plink.l_next){
                //                 myheap_area = curvma;
                //         }
                // } list_iterate_end();
        }


        if(target_page > end_page){
                if(end_page >= area_end){
                        if(!vmmap_is_range_empty(curproc->p_vmmap, end_page + 1, target_page - myheap_area->vma_end + 1)){
                                return -ENOMEM;
                        }
                        myheap_area->vma_end = target_page+1;
                }
                curproc->p_brk = addr;
                *ret = curproc->p_brk;
        }else if(target_page < end_page){
                if(target_page >= area_end){
                        myheap_area->vma_end = target_page+1;
                }
                curproc->p_brk = addr;
                *ret = curproc->p_brk;
        }else{
                curproc->p_brk = addr;
                *ret = curproc->p_brk;
        }
        // dbg_print("area end: %p, ret: %p, p_brk: %p\n", myheap_area->vma_end, *ret, curproc->p_brk);
        return 0;


}
