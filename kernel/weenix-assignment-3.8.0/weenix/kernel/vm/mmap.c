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
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"


#define LEN_TO_PAGES(len) len / PAGE_SIZE + ((len % PAGE_SIZE == 0) ? 0 : 1)

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
        dbg_print("in do_mmap addr: %p, len: %d, prot: %d, flags: %d, fd: %d, off: %d\n", addr, len, prot, flags, fd, off);
        uint32_t vpn = ADDR_TO_PN(addr);
        size_t voffset = PAGE_OFFSET(addr);
        void *end_vaddr = (uint32_t*)addr + len;
        uint32_t start_vpn = vpn;
        uint32_t end_vpn = ADDR_TO_PN(end_vaddr) + 1;
        vmarea_t *map_vma;

        if((len == 0) || (len > 0xb0000000)){
            dbg_print("do_mmap invalid len %d\n", len);
            return -EINVAL;
        }
        if(((uint32_t)addr < USER_MEM_LOW) || ((uint32_t)addr >= USER_MEM_HIGH) || (len > (USER_MEM_HIGH-USER_MEM_LOW))){
            if(addr){
                dbg_print("do_mmap invalid addr\n");
                return -EINVAL;
            }
        }
        if(fd < 0){
                if(!(MAP_ANON & flags)){
                        dbg_print("do_mmap invalid fd %d\n", fd);
                        return -EINVAL;
                }
        }
        if(!((MAP_SHARED & flags) || (MAP_PRIVATE & flags))){
                dbg_print("do_mmap invalid flags neigher %d\n", fd);
                return -EINVAL;
        }
        if (addr == NULL && (flags & MAP_FIXED)) {
            dbg_print("do_mmap invalid flags fixmap %d\n", fd);
            return -EINVAL;
        }
        if((uint32_t)off > PAGE_SIZE){
            dbg_print("do_mmap invalid off %d\n", off);
            return -EINVAL;
        }
        



        if ((flags & MAP_ANON) != 0)  {
                // dbg_print("mmap anon obj\n");
                int result = vmmap_map(curproc->p_vmmap, NULL, start_vpn, LEN_TO_PAGES(len), prot, flags, off, VMMAP_DIR_HILO, &map_vma);
                if(!result){
                        uintptr_t mapped_addr = (uintptr_t)PN_TO_ADDR(map_vma->vma_start);
                        tlb_flush_range(mapped_addr, LEN_TO_PAGES(len));
                        pt_unmap_range(curproc->p_pagedir, mapped_addr, mapped_addr + (uintptr_t)PN_TO_ADDR(LEN_TO_PAGES(len)));
                        if(ret) *ret = (void *)mapped_addr;
                }
                //else{
                //         tlb_flush_range((uintptr_t)addr, LEN_TO_PAGES(len));
                //         pt_unmap_range(curproc->p_pagedir, (uintptr_t)addr, (uintptr_t)addr + (uintptr_t)PN_TO_ADDR(LEN_TO_PAGES(len)));
                //         *ret = addr;
                // }

        }else{
                // dbg_print("file obj\n");
                file_t* my_file = fget(fd);
                if(my_file == NULL){
                    dbg_print("do_mmap invalid filer\n");
                    return -EINVAL;
                }
                if(!(my_file->f_mode & FMODE_WRITE) && (prot & FMODE_WRITE)){
                    dbg_print("do_mmap write to readonly\n");
                    fput(my_file);
                    return -EINVAL;
                }

                vnode_t* file_vnode = my_file->f_vnode;
                int result = vmmap_map(curproc->p_vmmap, file_vnode, start_vpn, LEN_TO_PAGES(len), prot, flags, off, VMMAP_DIR_HILO, &map_vma);
                fput(my_file);
                if(!result){
                        uintptr_t mapped_addr = (uintptr_t)PN_TO_ADDR(map_vma->vma_start);
                        tlb_flush_range(mapped_addr, LEN_TO_PAGES(len));
                        pt_unmap_range(curproc->p_pagedir, mapped_addr, mapped_addr + (uintptr_t)PN_TO_ADDR(LEN_TO_PAGES(len)));
                        if(*ret) *ret = (void *)mapped_addr;
                        // dbg_print("tlb pt_unmap newaddr: %p, pages: %d, pd: %p, newadd+page: %p\n", mapped_addr, LEN_TO_PAGES(len), curproc->p_pagedir, mapped_addr + (uintptr_t)PN_TO_ADDR(LEN_TO_PAGES(len)));
                }
                // else{
                //         tlb_flush_range((uintptr_t)addr, LEN_TO_PAGES(len));

                //         pt_unmap_range(curproc->p_pagedir, (uintptr_t)addr, (uintptr_t)addr + (uintptr_t)PN_TO_ADDR(LEN_TO_PAGES(len)));
                //         *ret = addr;
                // }

        }
        
        KASSERT(NULL != curproc->p_pagedir); /* page table must be valid after a memory segment is mapped into the address space */
        dbg(DBG_PRINT, "(GRADING3A 2.a)\n");

        return 1;

}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        // NOT_YET_IMPLEMENTED("VM: do_munmap");
        
        dbg_print("in do munmap addr %p, len %d\n", addr, len);

        void *end_vaddr = (uint32_t*)addr + len;
        if((len == 0) || (len > 0xb0000000)){
            dbg_print("do_mmap invalid len %d\n", len);
            return -EINVAL;
        }

        if(((uint32_t)addr < USER_MEM_LOW) || ((uint32_t)addr > USER_MEM_HIGH) || (len > (USER_MEM_HIGH-USER_MEM_LOW))){
            dbg_print("do_munmap invalid addr\n");
            return -EINVAL;
        }

        uint32_t start_vpn = ADDR_TO_PN(addr);
        int result = vmmap_remove(curproc->p_vmmap, start_vpn, LEN_TO_PAGES(len));
        tlb_flush_range((uintptr_t)addr, LEN_TO_PAGES(len));
        pt_unmap_range(curproc->p_pagedir, (uintptr_t)addr, (uintptr_t)PN_TO_ADDR(start_vpn + LEN_TO_PAGES(len)));
        return result;

}