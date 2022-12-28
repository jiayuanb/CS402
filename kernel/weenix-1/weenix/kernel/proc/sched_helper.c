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

#include "main/interrupt.h"

#include "proc/sched.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/debug.h"

void ktqueue_enqueue(ktqueue_t *q, kthread_t *thr);
kthread_t * ktqueue_dequeue(ktqueue_t *q);

/*
 * Updates the thread's state and enqueues it on the given
 * queue. Returns when the thread has been woken up with wakeup_on or
 * broadcast_on.
 *
 * Use the private queue manipulation functions above.
 */
void
sched_sleep_on(ktqueue_t *q)
{
        //TODO FINISH sleep on 
        proc_t * cur = curproc;
        // dbg_print(" sched sleep current process: %d\n", cur->p_pid);
        kthread_t * child_thread;
        list_iterate_begin(&curproc->p_threads, child_thread, kthread_t, kt_plink) {
                // dbg_print("sched thread parent pid %d\n", child_thread->kt_proc->p_pid);
                child_thread->kt_state = KT_SLEEP;
                // remove from run queue
                // if(child_thread->kt_wchan != NULL){
                //         list_remove(&child_thread->kt_qlink);
                //         child_thread->kt_wchan->tq_size--;
                //         child_thread->kt_wchan = NULL;
                // }
                // Note deque
                child_thread->kt_wchan = NULL;
                ktqueue_enqueue(q, child_thread);
        } list_iterate_end();
        dbg_print("fall asleep\n");
        sched_switch();
        // NOT_YET_IMPLEMENTED("PROCS: sched_sleep_on");

}

kthread_t *
sched_wakeup_on(ktqueue_t *q)
{
        kthread_t * thr = ktqueue_dequeue(q);
        if(thr->kt_state != KT_RUN){
                dbg_print("wakeup on %d\n", thr->kt_proc->p_pid);
                thr->kt_state = KT_RUN;
                sched_make_runnable(thr);
        }
        return thr;
}

void
sched_broadcast_on(ktqueue_t *q)
{
        // NOT_YET_IMPLEMENTED("PROCS: sched_broadcast_on");

        // kthread_t* par_proc_thread;
        // list_iterate_begin(q, par_proc_thread, kthread_t, kt_plink) {
        //         // dbg_print("sched thread parent pid %d\n", child_thread->kt_proc->p_pid);
        //         if(par_proc_thread->kt_state != KT_RUN){
        //                 dbg_print("broad cast %d \n", curproc->p_pproc->p_pid);
        //                 par_proc_thread->kt_state = KT_RUN;
        //                 sched_make_runnable(par_proc_thread);
        //         }
        // } list_iterate_end();
        while(q->tq_size != 0){
                kthread_t * thr = ktqueue_dequeue(q);
                if(thr->kt_state != KT_RUN){
                        dbg_print("broad cast %d\n", thr->kt_proc->p_pid);
                        thr->kt_state = KT_RUN;
                }
                sched_make_runnable(thr);
        }
        // dbg_print("return from broad cast\n");
}

