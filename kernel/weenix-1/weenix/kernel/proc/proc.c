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
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/kmalloc.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
        list_init(&_proc_list);
        proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
        KASSERT(proc_allocator != NULL);
}

proc_t *
proc_lookup(int pid)
{
        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid == pid) {
                        return p;
                }
        } list_iterate_end();
        return NULL;
}

list_t *
proc_list()
{
        return &_proc_list;
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
        const proc_t *p = (proc_t *) arg;
        size_t size = osize;
        proc_t *child;

        KASSERT(NULL != p);
        KASSERT(NULL != buf);

        iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
        iprintf(&buf, &size, "name:         %s\n", p->p_comm);
        if (NULL != p->p_pproc) {
                iprintf(&buf, &size, "parent:       %i (%s)\n",
                        p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
                iprintf(&buf, &size, "parent:       -\n");
        }

#ifdef __MTP__
        int count = 0;
        kthread_t *kthr;
        list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
                ++count;
        } list_iterate_end();
        iprintf(&buf, &size, "thread count: %i\n", count);
#endif

        if (list_empty(&p->p_children)) {
                iprintf(&buf, &size, "children:     -\n");
        } else {
                iprintf(&buf, &size, "children:\n");
        }
        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
                iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
        } list_iterate_end();

        iprintf(&buf, &size, "status:       %i\n", p->p_status);
        iprintf(&buf, &size, "state:        %i\n", p->p_state);

#ifdef __VFS__
#ifdef __GETCWD__
        if (NULL != p->p_cwd) {
                char cwd[256];
                lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                iprintf(&buf, &size, "cwd:          %-s\n", cwd);
        } else {
                iprintf(&buf, &size, "cwd:          -\n");
        }
#endif /* __GETCWD__ */
#endif

#ifdef __VM__
        iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
        iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif

        return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
        size_t size = osize;
        proc_t *p;

        KASSERT(NULL == arg);
        KASSERT(NULL != buf);

#if defined(__VFS__) && defined(__GETCWD__)
        iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
        iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif

        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                char parent[64];
                if (NULL != p->p_pproc) {
                        snprintf(parent, sizeof(parent),
                                 "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
                } else {
                        snprintf(parent, sizeof(parent), "  -");
                }

#if defined(__VFS__) && defined(__GETCWD__)
                if (NULL != p->p_cwd) {
                        char cwd[256];
                        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                        iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                                p->p_pid, p->p_comm, parent, cwd);
                } else {
                        iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                                p->p_pid, p->p_comm, parent);
                }
#else
                iprintf(&buf, &size, " %3i  %-13s %-s\n",
                        p->p_pid, p->p_comm, parent);
#endif
        } list_iterate_end();
        return size;
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
        proc_t *p;
        pid_t pid = next_pid;
        while (1) {
failed:
                list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                        // dbg_print("iter proc %d, cur pid %d\n", p->p_pid, pid);
                        if (p->p_pid == pid) {
                                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                                        return -1;
                                } else {
                                        goto failed;
                                }
                        }
                } list_iterate_end();
                next_pid = (pid + 1) % PROC_MAX_COUNT;
                return pid;
        }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
proc_t *
proc_create(char *name)
{
        // dbg_print("in proc_create\n");
        // proc_t *process = (proc_t *) kmalloc(sizeof(proc_t));
        proc_t *process = (proc_t *) slab_obj_alloc(proc_allocator);
        process->p_pagedir = pt_create_pagedir();
        strncpy(process->p_comm, name, sizeof(char)*PROC_NAME_LEN);
        process->p_state = PROC_RUNNING;
        process->p_pproc = curproc;
        list_init(&process->p_threads);
        list_init(&process->p_children);
        list_init(&process->p_list_link);
        list_init(&process->p_child_link);
        sched_queue_init(&process->p_wait);
        // add to parent proc children
        if(curproc != NULL){
                list_insert_tail(&curproc->p_children, &process->p_child_link);
        }
        // dbg_print("next_pid before %d\n", next_pid);
        process->p_pid = _proc_getid();
        list_insert_tail(proc_list(), &process->p_list_link);
        // dbg_print("pid return %d\n", process->p_pid);
        // dbg_print("next_pid after %d\n", next_pid);
        if(process->p_pid == 1){
                proc_initproc = process;
        }
        dbg_print("create_proc %d\n", process->p_pid);
        return process;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{
        // NOT_YET_IMPLEMENTED("PROCS: proc_cleanup");

        // not fully complete need reparenting ...
        sched_broadcast_on(&curproc->p_pproc->p_wait);
        curproc->p_status = status;
        curproc->p_state = PROC_DEAD;
        proc_t * child_proc;
        list_iterate_begin(&curproc->p_children, child_proc, proc_t, p_child_link) {
                list_remove(&child_proc->p_child_link);
                list_insert_tail(&(proc_initproc->p_children), &child_proc->p_child_link);
                child_proc->p_pproc = proc_initproc;
                // dbg_print("reparent %d\n", child_proc->p_pid);

        } list_iterate_end();
        
}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
void
proc_kill(proc_t *p, int status)
{
        if(p == curproc){
                do_exit(status);
        }
        else{
                p->p_status = status;
                kthread_t* child_thread;
                list_iterate_begin(&p->p_threads, child_thread, kthread_t, kt_plink) {
                        if(child_thread->kt_state !=  KT_EXITED){
                                kthread_cancel(child_thread, (void *)status);
                                dbg_print("kill proc %d\n", child_thread->kt_proc->p_pid);
                        }
                } list_iterate_end();
        }
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */
void
proc_kill_all()
{               
        // NOT_YET_IMPLEMENTED("PROCS: proc_kill_all");

        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid != 0 && p -> p_pid != 1 && p -> p_pid != 2 && p != curproc) {
                        proc_kill(p, -1);
                }

        } list_iterate_end();
        proc_kill(curproc, -1);
        
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
void
proc_thread_exited(void *retval)
{
        // NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");
        proc_cleanup((int)retval);
        dbg_print("exit proc_thread_exited by proc %d, try to return to proc %d\n", curproc->p_pid, curproc->p_pproc->p_pid);
        sched_switch();
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
        // NOT_YET_IMPLEMENTED("PROCS: do_waitpid");
        dbg_print("in do_waitpid\n");
        if(list_empty(&curproc->p_children)){
                return -ECHILD;
        }

        if(pid == -1){
                // give away the cpu to child 
                //TODO before return pid, set next_pid
                while(1){
                        proc_t * cur = curproc;
                        // When return 

                        proc_t * child_proc;
                        pid_t ret = -1;
                        list_iterate_begin(&cur->p_children, child_proc, proc_t, p_child_link) {
                                dbg_print("check child proc %d\n", child_proc->p_pid);
                                if(child_proc->p_state == PROC_DEAD){
                                        // TODO: how to dispose an exited child process
                                        list_remove(&child_proc->p_child_link);
                                        pid_t id = child_proc->p_pid;
                                        kthread_t * child_proc_child_thread;
                                        *status = child_proc->p_status;
                                        list_iterate_begin(&child_proc->p_threads, child_proc_child_thread, kthread_t, kt_plink) {
                                                kthread_destroy(child_proc_child_thread);
                                        } list_iterate_end();
                                        list_remove(&child_proc->p_list_link);

                                        dbg_print("remove proc %d\n", child_proc->p_pid);
                                        slab_obj_free(proc_allocator, (void *)child_proc);
                                        // proc_t * p;
                                        // list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                                        //         dbg_print("%d\n ", p->p_pid);
                                        // } list_iterate_end();
                                        // dbg_print("Remove pid %d \n", id);
                                        next_pid = MIN(next_pid, id);
                                        // dbg_print("next pid = %d\n", next_pid);
                                        ret = id;
                                }
                        } list_iterate_end();

                        if(ret != -1) return ret;
                        sched_sleep_on(&(cur->p_wait));
                        dbg_print("proc %d back to do_wait\n", curproc->p_pid);

                }
        }
        else{
                KASSERT(pid > 0);
                dbg_print("wait for pid %d\n", pid);
                proc_t *lookup = proc_lookup(pid);
                if(lookup == NULL){
                        dbg_print("cannot find pid %d\n", pid);

                        // proc_t * child_proc;
                        // list_iterate_begin(&curproc->p_children, child_proc, proc_t, p_child_link) {
                        //         dbg_print("check child proc %d\n", child_proc->p_pid);
                        //         if(child_proc->p_state == PROC_DEAD){
                        //                 // TODO: how to dispose an exited child process
                        //                 list_remove(&child_proc->p_child_link);
                        //                 pid_t id = child_proc->p_pid;
                        //                 kthread_t * child_proc_child_thread;
                        //                 *status = child_proc->p_status;
                        //                 list_iterate_begin(&child_proc->p_threads, child_proc_child_thread, kthread_t, kt_plink) {
                        //                         kthread_destroy(child_proc_child_thread);
                        //                 } list_iterate_end();
                        //                 list_remove(&child_proc->p_list_link);
                        //                 slab_obj_free(proc_allocator, (void *)child_proc);
                        //                 dbg_print("remove proc pid %d \n", id);
                        //                 next_pid = MIN(next_pid, id);
                                        
                        //         }
                        // } list_iterate_end();

                        return -ECHILD;
                }
                if(lookup->p_pproc->p_pid != curproc->p_pid){

                        // proc_t * child_proc;
                        // list_iterate_begin(&curproc->p_children, child_proc, proc_t, p_child_link) {
                        //         dbg_print("check child proc %d\n", child_proc->p_pid);
                        //         if(child_proc->p_state == PROC_DEAD){
                        //                 // TODO: how to dispose an exited child process
                        //                 list_remove(&child_proc->p_child_link);
                        //                 pid_t id = child_proc->p_pid;
                        //                 kthread_t * child_proc_child_thread;
                        //                 *status = child_proc->p_status;
                        //                 list_iterate_begin(&child_proc->p_threads, child_proc_child_thread, kthread_t, kt_plink) {
                        //                         kthread_destroy(child_proc_child_thread);
                        //                 } list_iterate_end();
                        //                 list_remove(&child_proc->p_list_link);
                        //                 slab_obj_free(proc_allocator, (void *)child_proc);
                        //                 dbg_print("remove proc pid %d \n", id);
                        //                 next_pid = MIN(next_pid, id);
                                        
                        //         }
                        // } list_iterate_end();
                        return -ECHILD;
                }
                dbg_print("lookup status %d\n", lookup->p_state - PROC_RUNNING);
                while(lookup->p_state == PROC_RUNNING){

                        proc_t * child_proc;
                        list_iterate_begin(&curproc->p_children, child_proc, proc_t, p_child_link) {
                                dbg_print("check child proc %d\n", child_proc->p_pid);
                                if(child_proc->p_state == PROC_DEAD){
                                        // TODO: how to dispose an exited child process
                                        list_remove(&child_proc->p_child_link);
                                        pid_t id = child_proc->p_pid;
                                        kthread_t * child_proc_child_thread;
                                        *status = child_proc->p_status;
                                        list_iterate_begin(&child_proc->p_threads, child_proc_child_thread, kthread_t, kt_plink) {
                                                kthread_destroy(child_proc_child_thread);
                                        } list_iterate_end();
                                        list_remove(&child_proc->p_list_link);
                                        slab_obj_free(proc_allocator, (void *)child_proc);
                                        dbg_print("remove proc pid %d \n", id);
                                        next_pid = MIN(next_pid, id);
                                        
                                }
                        } list_iterate_end();

                        sched_sleep_on(&(curproc->p_wait));
                        dbg_print("proc %d back to do_wait\n", curproc->p_pid);

                        dbg_print("lookup status %d\n", lookup->p_state - PROC_RUNNING);
                }

                proc_t * child_proc;
                list_iterate_begin(&curproc->p_children, child_proc, proc_t, p_child_link) {
                        dbg_print("check child proc %d\n", child_proc->p_pid);
                        if(child_proc->p_state == PROC_DEAD){
                                // TODO: how to dispose an exited child process
                                list_remove(&child_proc->p_child_link);
                                pid_t id = child_proc->p_pid;
                                kthread_t * child_proc_child_thread;
                                *status = child_proc->p_status;
                                list_iterate_begin(&child_proc->p_threads, child_proc_child_thread, kthread_t, kt_plink) {
                                        kthread_destroy(child_proc_child_thread);
                                } list_iterate_end();
                                slab_obj_free(proc_allocator, (void *)child_proc);
                                list_remove(&child_proc->p_list_link);
                                dbg_print("remove proc pid %d \n", id);
                                next_pid = MIN(next_pid, id);
                                
                        }
                } list_iterate_end();
                return pid;
        }
        return 0;
}

/*
 * Cancel all threads and join with them (if supporting MTP), and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
        // NOT_YET_IMPLEMENTED("PROCS: do_exit");
        //cancel threads
        curproc->p_status = status;
        kthread_t* child_thread;
        list_iterate_begin(&curproc->p_threads, child_thread, kthread_t, kt_plink) {
                if(child_thread->kt_state !=  KT_EXITED){
                        kthread_cancel(child_thread, (void *)status);
                }
        } list_iterate_end();
        dbg_print("cancel thread of proc %d\n", curproc->p_pid);

}
