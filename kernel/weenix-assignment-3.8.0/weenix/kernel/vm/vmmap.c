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

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        list_link_init(&newvma->vma_olink);
        list_link_init(&newvma->vma_plink);
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        dbg_print("[*] vmarea_free vma_mmobj=%#08x\n", (uint32_t)vma->vma_obj);
        mmobj_t *vma_mmobj = vma->vma_obj;
        // list_remove(&vma->vma_plink);
        if(list_link_is_linked(&vma->vma_olink)){
                list_remove(&vma->vma_olink);
        }
        vma->vma_obj->mmo_ops->put(vma_mmobj);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        dbg_print("[*] create vmmap\n");
        vmmap_t * new_vmmap = (vmmap_t *) slab_obj_alloc(vmmap_allocator);
        if(new_vmmap){
                list_init(&(new_vmmap)->vmm_list);
                new_vmmap->vmm_proc = NULL;
        }
        return new_vmmap;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{

        KASSERT(NULL != map); /* function argument must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.a)\n");

        while(!list_empty(&map->vmm_list)){
                vmarea_t * headvma = list_head(&map->vmm_list, vmarea_t, vma_plink);
                // dbg_print("[*] try to destroy vmarea [%#08x, %#08x), refcount in mmobj = %d, nrespage in mmobj = %d, prot=%d, flag=%d\n", headvma->vma_start, headvma->vma_end, headvma->vma_obj->mmo_refcount, headvma->vma_obj->mmo_nrespages, headvma->vma_prot, headvma->vma_flags);
                list_remove(&headvma->vma_plink);
                if(list_link_is_linked(&headvma->vma_olink)){
                        list_remove(&headvma->vma_olink);
                }
                
                vmarea_free(headvma);
        }
        slab_obj_free(vmmap_allocator, map);
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
        // dbg_print("[*] insert/ a new vmarea to vmmap\n");
        KASSERT(newvma);
        KASSERT(NULL != map && NULL != newvma); /* both function arguments must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(NULL == newvma->vma_vmmap); /* newvma must be newly create and must not be part of any existing vmmap */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(newvma->vma_start < newvma->vma_end); /* newvma must not be empty */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");

        KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
                                  /* addresses in this memory segment must lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");

        vmarea_t *curvma;
        // add to vma_plink list
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                // dbg_print("%#08x, %#08x\n", curvma->vma_start, curvma->vma_end);
                if(curvma->vma_start >= newvma->vma_end){
                        list_insert_before(&curvma->vma_plink, &newvma->vma_plink);
                        newvma->vma_vmmap = map;
                        // dbg_print("finish vmmap_insert\n");
                        return;
                }
                // dbg_print("next\n");
        } list_iterate_end();
        list_insert_tail(&map->vmm_list, &newvma->vma_plink);
        newvma->vma_vmmap = map;
        //("finish vmmap_insert\n");
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        if(dir == VMMAP_DIR_LOHI){
                uint32_t lastend = ADDR_TO_PN(USER_MEM_LOW);
                vmarea_t *curvma;

                list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                        uint32_t diff = (curvma->vma_start) - lastend;
                        if(diff >= npages){
                                return lastend;
                        }
                        lastend = curvma->vma_end;
                } list_iterate_end();
        }
        else if(dir == VMMAP_DIR_HILO){
                uint32_t nextstart = ADDR_TO_PN(USER_MEM_HIGH);
                vmarea_t *curvma;

                list_iterate_reverse(&map->vmm_list, curvma, vmarea_t, vma_plink){
                        uint32_t diff = nextstart - (curvma->vma_end);
                        if(diff >= npages){
                                return nextstart - npages;
                        }
                        nextstart = curvma->vma_start;
                } list_iterate_end();
        }
        return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        KASSERT(NULL != map); /* the first function argument must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.c)\n");

        vmarea_t *curvma;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                if(curvma -> vma_start <= vfn && curvma->vma_end > vfn){
                        return curvma;
                }
        } list_iterate_end();
        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        // dbg_print("[*] enter vmmap_clone\n");
        vmmap_t * newvmmap = vmmap_create();
        if(newvmmap == NULL){
                // dbg_print("[*] fail to create vmmap in vmmap_clone\n");
                return NULL;
        }

        vmarea_t *curvma;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                vmarea_t *newvma = vmarea_alloc();
                if(newvma == NULL){
                        // dbg_print("[*] fail vmmap_clone, try creating vmarea [%#08x, %#08x)\n", curvma->vma_start, curvma->vma_end);
                        return NULL;
                }
                newvma->vma_start = curvma->vma_start;
                newvma->vma_end = curvma->vma_end;
                newvma->vma_off = curvma->vma_off;
                newvma->vma_prot = curvma->vma_prot;
                newvma->vma_flags = curvma->vma_flags;
                newvma->vma_vmmap = newvmmap; 
                // warning: when vma_obj is initialized, add vma_olink to mmobj's mmo_vmas
                newvma->vma_obj = NULL;
                list_link_init(&newvma->vma_olink);
                list_link_init(&newvma->vma_plink);
                list_insert_tail(&newvmmap->vmm_list, &newvma->vma_plink);
        } list_iterate_end();

        return newvmmap;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        KASSERT(map);
        KASSERT(NULL != map); /* must not add a memory segment into a non-existing vmmap */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        KASSERT(0 < npages); /* number of pages of this memory segment cannot be 0 */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags)); /* must specify whether the memory segment is shared or private */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage)); /* if lopage is not zero, it must be a user space vpn */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
                                    /* if lopage is not zero, the specified page range must lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        
        KASSERT(PAGE_ALIGNED(off)); /* the off argument must be page aligned */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        KASSERT(0 < npages); /* number of pages of this memory segment cannot be 0 */

        if(lopage == 0){
                int ret = vmmap_find_range(map, npages, dir);
                if(ret < 0){
                        return -1;
                }
                lopage = ret;
        }
        else{
                if(!vmmap_is_range_empty(map, lopage, npages)){
                        if(vmmap_remove(map, lopage, npages) < 0){
                                return -1;
                        }
                }
        }

        vmarea_t *newvma = vmarea_alloc();
        if(newvma == NULL){
                return -1;
        }
        newvma->vma_start = lopage;
        newvma->vma_end = lopage + npages;
        newvma->vma_off = off;
        newvma->vma_prot = prot;
        newvma->vma_flags = flags;
        list_link_init(&newvma->vma_olink);

        if(file == NULL){


                if((flags & MAP_TYPE) == MAP_PRIVATE && (prot & PROT_WRITE) == PROT_WRITE){
                        mmobj_t *bottom_vnode_mmobj = anon_create();
                        if(bottom_vnode_mmobj == NULL){
                                return -1;
                        }
                        mmobj_t *shadow_mmobj;
                        shadow_mmobj = shadow_create();
                        shadow_mmobj->mmo_shadowed = bottom_vnode_mmobj;
                        shadow_mmobj->mmo_un.mmo_bottom_obj = bottom_vnode_mmobj;
                        list_insert_tail(&bottom_vnode_mmobj->mmo_un.mmo_vmas, &newvma->vma_olink);
                        newvma->vma_obj = shadow_mmobj;
                } else{
                // else if((flags & MAP_TYPE) == MAP_SHARED){
                        mmobj_t *anon_obj = anon_create();
                        if(anon_obj == NULL){
                                return -1;
                        }
                        newvma->vma_obj = anon_obj;
                        list_insert_tail(&anon_obj->mmo_un.mmo_vmas, &newvma->vma_olink);
                }
               
        }
        else{
                // if the vmarea is shared mapping
                if((flags & MAP_TYPE) == MAP_PRIVATE && (prot & PROT_WRITE) == PROT_WRITE){
                        mmobj_t *bottom_vnode_mmobj;
                        if(file->vn_ops->mmap(file, newvma, &bottom_vnode_mmobj) < 0){
                                return -1;
                        }
                        mmobj_t *shadow_mmobj;
                        shadow_mmobj = shadow_create();
                        shadow_mmobj->mmo_shadowed = bottom_vnode_mmobj;
                        shadow_mmobj->mmo_un.mmo_bottom_obj = bottom_vnode_mmobj;
                        list_insert_tail(&bottom_vnode_mmobj->mmo_un.mmo_vmas, &newvma->vma_olink);
                        newvma->vma_obj = shadow_mmobj;
                }
                else {
                // if((flags & MAP_TYPE) == MAP_SHARED){
                        mmobj_t *ret_mmobj;
                        if(file->vn_ops->mmap(file, newvma, &ret_mmobj) < 0){
                                return -1;
                        }
                        // dbg_print("mmap from file, use file vn_ops mmap, get mmobj refcount = %d, nrespage = %d\n", ret_mmobj->mmo_refcount, ret_mmobj->mmo_nrespages);
                        newvma->vma_obj = ret_mmobj;
                        list_insert_tail(&ret_mmobj->mmo_un.mmo_vmas, &newvma->vma_olink);
                }
                // if the vmarea is private mapping
                // else{
                //         panic("[*] vmmap_map doesn't have legal flag=%d\n", flags);
                //         return -1;
                // }
        }
        vmmap_insert(map, newvma);
        newvma->vma_vmmap = map;

        if(new){
                *new = newvma;
        }
        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        vmarea_t *curvma;

        list_iterate_reverse(&map->vmm_list, curvma, vmarea_t, vma_plink){
                if(curvma->vma_start < lopage && curvma->vma_end > lopage + npages){
                        
                        vmarea_t *nextvma = vmarea_alloc();
                        nextvma->vma_start = curvma->vma_end - npages;
                        nextvma->vma_end = curvma->vma_end;
                        nextvma->vma_off = curvma->vma_off + (nextvma->vma_start - curvma->vma_start);
                        nextvma->vma_flags = curvma->vma_flags;
                        nextvma->vma_prot = curvma->vma_prot;
                        nextvma->vma_obj = curvma->vma_obj;

                        curvma->vma_end = lopage;

                        vmmap_insert(map, nextvma);
                        nextvma->vma_vmmap=curvma->vma_vmmap;
                        curvma->vma_obj->mmo_ops->ref(curvma->vma_obj);
                }
                else if(lopage > curvma->vma_start && lopage < curvma->vma_end && lopage + npages >= curvma->vma_end){
                        curvma->vma_end = lopage;
                }
                else if(lopage <= curvma->vma_start && lopage + npages < curvma->vma_end){
                        curvma->vma_start = lopage + npages;
                }
                else if(lopage <= curvma->vma_start && lopage + npages >= curvma->vma_end){
                        list_remove(&curvma->vma_plink);
                        curvma->vma_obj->mmo_ops->put(curvma->vma_obj);
                        if(list_link_is_linked(&curvma->vma_olink)){
                                list_remove(&curvma->vma_olink);
                        }
                }
        } list_iterate_end();
        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        uint32_t endvfn = startvfn+npages;
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
                                  /* the specified page range must not be empty and lie completely within the user space */

        dbg(DBG_PRINT, "(GRADING3A 3.e)\n");

        vmarea_t *curvma;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                if(startvfn < curvma->vma_end && endvfn > curvma->vma_start){
                        return 0;
                }
        } list_iterate_end();
        return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        // vaddr is user address, buf is kernel address
        // dbg_print("[*] Enter vmmap_read %#08x, %d\n", (uint32_t) vaddr, count);
        KASSERT(map);
        uint32_t vpn = ADDR_TO_PN(vaddr);
        size_t voffset = PAGE_OFFSET(vaddr);
        void *end_vaddr = (uint32_t*)vaddr + count;

        // check all the areas you are accessing exist
        uint32_t start_vpn = vpn;
        uint32_t end_vpn = ADDR_TO_PN(end_vaddr) + 1;
        vmarea_t *curvma;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                if(start_vpn < end_vpn){
                        if(curvma->vma_start >= end_vpn){
                                return -EFAULT;
                        }
                        else if(curvma->vma_start > start_vpn){
                                return -EFAULT;
                        }
                        else if(start_vpn < curvma->vma_end){
                                start_vpn = curvma->vma_end;
                        }
                }
        } list_iterate_end();

        size_t read_count = 0;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
find:
                 if(curvma->vma_start <= vpn && curvma->vma_end > vpn){
                        pframe_t *pf;
                        uint32_t pagenum = vpn - curvma->vma_start + curvma->vma_off;
                        int err;
                         if((err = curvma->vma_obj->mmo_ops->lookuppage(curvma->vma_obj, pagenum, 0, &pf)) < 0){
                                 return err;
                        }
                         
                        if(PAGE_SIZE - voffset >= count){
                                memcpy((char *)buf+read_count, (char *)pf->pf_addr + voffset, count);
                                 return 0;
                        } else {
                                memcpy((char *)buf+read_count, (char *)pf->pf_addr + voffset, PAGE_SIZE - voffset);
                                count = count - (PAGE_SIZE - voffset);
                                read_count += PAGE_SIZE - voffset;
                                voffset = 0;
                                ++vpn;
                                goto find;
                        }
                }
        } list_iterate_end();
         return -EFAULT;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        // vaddr is user address, buf is kernel address
         KASSERT(map);
        uint32_t vpn = ADDR_TO_PN(vaddr);
        size_t voffset = PAGE_OFFSET(vaddr);
        void *end_vaddr = (uint32_t*)vaddr + count;

        // check all the areas you are accessing exist
        uint32_t start_vpn = vpn;
        uint32_t end_vpn = ADDR_TO_PN(end_vaddr) + 1;
        vmarea_t *curvma;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
                if(start_vpn < end_vpn){
                        if(curvma->vma_start >= end_vpn){
                                 return -EFAULT;
                        }
                        else if(curvma->vma_start > start_vpn){
                                 return -EFAULT;
                        }
                        else{
                                start_vpn = curvma->vma_end;
                        }
                }
        } list_iterate_end();


        size_t write_count = 0;
        list_iterate_begin(&map->vmm_list, curvma, vmarea_t, vma_plink){
find:
                if(curvma->vma_start <= vpn && curvma->vma_end > vpn){
                        pframe_t *pf;
                        uint32_t pagenum = vpn - curvma->vma_start + curvma->vma_off;
                        int err;
                        if((err = curvma->vma_obj->mmo_ops->lookuppage(curvma->vma_obj, pagenum, 1, &pf)) < 0){
                                return err;
                        }
                        if((err = curvma->vma_obj->mmo_ops->dirtypage(curvma->vma_obj, pf)) < 0){
                                return err;
                        }
                        
                        if(PAGE_SIZE - voffset >= count){
                                memcpy((char *)pf->pf_addr + voffset, (char *)buf + write_count, count);
                                return 0;
                        } else {
                                memcpy((char *)pf->pf_addr + voffset, (char *)buf + write_count, PAGE_SIZE - voffset);
                                count = count - (PAGE_SIZE - voffset);
                                write_count += PAGE_SIZE - voffset;
                                voffset = 0;
                                ++vpn;
                                goto find;
                        }
                }
        } list_iterate_end();
        return -EFAULT;
}
