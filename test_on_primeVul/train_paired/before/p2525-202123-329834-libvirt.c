qemuProcessHandleMonitorEOF(qemuMonitorPtr mon,
                            virDomainObjPtr vm,
                            void *opaque)
{
    virQEMUDriverPtr driver = opaque;
    qemuDomainObjPrivatePtr priv;
    struct qemuProcessEvent *processEvent;

    virObjectLock(vm);

    VIR_DEBUG("Received EOF on %p '%s'", vm, vm->def->name);

    priv = vm->privateData;
    if (priv->beingDestroyed) {
        VIR_DEBUG("Domain is being destroyed, EOF is expected");
        goto cleanup;
    }

    processEvent = g_new0(struct qemuProcessEvent, 1);

    processEvent->eventType = QEMU_PROCESS_EVENT_MONITOR_EOF;
    processEvent->vm = virObjectRef(vm);

    if (virThreadPoolSendJob(driver->workerPool, 0, processEvent) < 0) {
        virObjectUnref(vm);
        qemuProcessEventFree(processEvent);
        goto cleanup;
    }

    /* We don't want this EOF handler to be called over and over while the
     * thread is waiting for a job.
     */
    qemuMonitorUnregister(mon);

    /* We don't want any cleanup from EOF handler (or any other
     * thread) to enter qemu namespace. */
    qemuDomainDestroyNamespace(driver, vm);

 cleanup:
    virObjectUnlock(vm);
}