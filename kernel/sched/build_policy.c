// SPDX-License-Identifier: GPL-2.0-only
/*
 * These are the scheduling policy related scheduler files, built
 * in a single compilation unit for build efficiency reasons.
 *
 * ( Incidentally, the size of the compilation unit is roughly
 *   comparable to core.c and fair.c, the other two big
 *   compilation units. This helps balance build time, while
 *   coalescing source files to amortize header inclusion
 *   cost. )
 *
 * core.c and fair.c are built separately.
 */

/* Headers: */
#include <linux/sched/clock.h>
#include <linux/sched/cputime.h>
#include <linux/sched/hotplug.h>
#include <linux/sched/posix-timers.h>
#include <linux/sched/rt.h>

#include <linux/cpuidle.h>
#include <linux/jiffies.h>
#include <linux/livepatch.h>
#include <linux/psi.h>
#include <linux/seqlock_api.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/tsacct_kern.h>
#include <linux/vtime.h>

#include <uapi/linux/sched/types.h>

#include <linux/sched/new.h>

#include "sched.h"
#include "smp.h"

#include "autogroup.h"
#include "stats.h"
#include "pelt.h"

/* Source code modules: */

#include "idle.c"

#include "rt.c"

static inline int on_new_rq(struct sched_new_entity *new_se)
{
	return new_se->on_rq;
}

static inline struct task_struct * new_task_of(struct sched_new_entity *new_se)
{
	return container_of(new_se, struct task_struct, new_se);
}

static void
enqueue_new_entity(struct rq *rq, struct sched_new_entity *new_se, bool head)
{
	struct list_head * queue = & rq->new_runqueue.task_list;

	if (head)
		list_add(&new_se->task_list, queue);
	else
		list_add_tail(&new_se->task_list, queue);
	
	new_se->on_rq = 1;
	rq->new_runqueue.new_nr_running += 1;
}

static void
dequeue_new_entity(struct rq *rq, struct sched_new_entity *new_se)
{
	list_del_init(&new_se->task_list);
	new_se->on_rq = 0;
	rq->new_runqueue.new_nr_running -= 1;
}

static void enqueue_task_new(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_new_entity *new_se = & p->new_se;

	if (flags & ENQUEUE_WAKEUP)
		new_se->time_out = 0;
	
	enqueue_new_entity(rq, new_se, flags & ENQUEUE_HEAD);
	add_nr_running(rq, 1);
}

static void update_curr_new(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	delta_exec = rq_clock_task(rq) - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;
	
	schedstat_set(curr->stats.exec_max,
		      max(curr->stats.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = rq_clock_task(rq);
	cgroup_account_cputime(curr, delta_exec);
}

static void dequeue_task_new(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_new_entity *new_se = & p->new_se;

	update_curr_new(rq);
	dequeue_new_entity(rq, new_se);
	sub_nr_running(rq, 1);
}

static void requeue_task_new(struct rq *rq, struct task_struct *p)
{
	list_move_tail(&p->new_se.task_list, &rq->new_runqueue.task_list);
}

static void yield_task_new(struct rq *rq)
{
	requeue_task_new(rq, rq->curr);
}

// No Preemption
static void check_preempt_curr_new(struct rq *rq, struct task_struct *p, int flags)
{
}

static inline void set_next_task_new(struct rq *rq, struct task_struct *p, bool first)
{
	p->se.exec_start = rq_clock_task(rq);
}

static struct task_struct *pick_next_task_new(struct rq *rq)
{
    // printk(KERN_DEBUG "[NEW %s] try new_sched_class", __func__);
	// return NULL;

	struct task_struct *next;
	struct sched_new_entity *next_se;

	if (!rq->new_runqueue.new_nr_running)
		return NULL;
	
	next_se = list_first_entry(&rq->new_runqueue.task_list, struct sched_new_entity, task_list);
	next = new_task_of(next_se);
	if (!next)
		return NULL;
	
	next->se.exec_start = rq_clock_task(rq);
	set_next_task_new(rq, next, true);
	return next;
}

static void task_tick_new(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_new_entity *new_se = &p->new_se;

	update_curr_new(rq);

	if (!task_has_new_policy(p))
		return;

	if (--new_se->time_slice)
		return;  // time slices are not used up yet
	
	new_se->time_slice = NEW_RR_TIMESLICE;

	if (new_se->task_list.prev != new_se->task_list.next) {
		requeue_task_new(rq, p);
		resched_curr(rq);
	}
}

static void
prio_changed_new(struct rq *rq, struct task_struct *p, int oldprio)
{
}

static void switched_to_new(struct rq *rq, struct task_struct *p)
{
}

static unsigned int get_rr_interval_new(struct rq *rq, struct task_struct *p)
{
	return NEW_RR_TIMESLICE;
}

static void put_prev_task_new(struct rq *rq, struct task_struct *p)
{
	if (on_new_rq(&p->new_se))
		update_curr_new(rq);
}

DEFINE_SCHED_CLASS(new) = {
	.enqueue_task		= enqueue_task_new,
	.dequeue_task		= dequeue_task_new,
	.yield_task			= yield_task_new,
	.check_preempt_curr	= check_preempt_curr_new,
	.pick_next_task		= pick_next_task_new,
	.put_prev_task		= put_prev_task_new,
	.set_next_task		= set_next_task_new,
	.task_tick			= task_tick_new,
	.switched_to		= switched_to_new,
	.prio_changed		= prio_changed_new,
	.get_rr_interval	= get_rr_interval_new,
	.update_curr		= update_curr_new,
};

#ifdef CONFIG_SMP
# include "cpudeadline.c"
# include "pelt.c"
#endif

#include "cputime.c"
#include "deadline.c"

