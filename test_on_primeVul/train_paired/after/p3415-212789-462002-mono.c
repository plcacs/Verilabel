void ves_icall_System_Threading_InternalThread_Thread_free_internal (MonoInternalThread *this, HANDLE thread)
{
	MONO_ARCH_SAVE_REGS;

	THREAD_DEBUG (g_message ("%s: Closing thread %p, handle %p", __func__, this, thread));

	if (thread)
		CloseHandle (thread);

	if (this->synch_cs) {
		CRITICAL_SECTION *synch_cs = this->synch_cs;
		this->synch_cs = NULL;
		DeleteCriticalSection (synch_cs);
		g_free (synch_cs);
	}

	if (this->name) {
		void *name = this->name;
		this->name = NULL;
		g_free (name);
	}
}