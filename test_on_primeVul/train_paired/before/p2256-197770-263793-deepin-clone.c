QString Helper::temporaryMountDevice(const QString &device, const QString &name, bool readonly)
{
    QString mount_point = mountPoint(device);

    if (!mount_point.isEmpty())
        return mount_point;

    mount_point = "%1/.%2/mount/%3";
    const QStringList &tmp_paths = QStandardPaths::standardLocations(QStandardPaths::TempLocation);

    mount_point = mount_point.arg(tmp_paths.isEmpty() ? "/tmp" : tmp_paths.first()).arg(qApp->applicationName()).arg(name);

    if (!QDir::current().mkpath(mount_point)) {
        dCError("mkpath \"%s\" failed", qPrintable(mount_point));

        return QString();
    }

    if (!mountDevice(device, mount_point, readonly)) {
        dCError("Mount the device \"%s\" to \"%s\" failed", qPrintable(device), qPrintable(mount_point));

        return QString();
    }

    return mount_point;
}