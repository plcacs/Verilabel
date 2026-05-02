void update_process_times(int user_tick)
{
	struct task_struct *p = current;

	/* Note: this timer irq context must be accounted for as well. */
	account_process_tick(p, user_tick);
	run_local_timers();
	rcu_sched_clock_irq(user_tick);
#ifdef CONFIG_IRQ_WORK
	if (in_irq())
		irq_work_tick();
#endif
	scheduler_tick();
	if (IS_ENABLED(CONFIG_POSIX_TIMERS))
		run_posix_cpu_timers();

	/* The current CPU might make use of net randoms without receiving IRQs
	 * to renew them often enough. Let's update the net_rand_state from a
	 * non-constant value that's not affine to the number of calls to make
	 * sure it's updated when there's some activity (we don't care in idle).
	 */
	this_cpu_add(net_rand_state.s1, rol32(jiffies, 24) + user_tick);
}