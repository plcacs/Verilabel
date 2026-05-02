static int selinux_ptrace_traceme(struct task_struct *parent)
{
	return avc_has_perm(&selinux_state,
			    task_sid_subj(parent), task_sid_obj(current),
			    SECCLASS_PROCESS, PROCESS__PTRACE, NULL);
}