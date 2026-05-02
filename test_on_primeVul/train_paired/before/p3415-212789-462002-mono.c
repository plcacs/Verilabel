void ves_icall_System_Threading_InternalThread_Thread_free_internal (MonoInternalThread *this, HANDLE thread)
{
	MONO_ARCH_SAVE_REGS;

	THREAD_DEBUG (g_message ("%s: Closing thread %p, handle %p", __func__, this, thread));

	if (thread)
		CloseHandle (thread);

	if (this->synch_cs) {
		DeleteCriticalSection (this->synch_cs);
		g_free (this->synch_cs);
		this->synch_cs = NULL;
	}

	g_free (this->name);
}