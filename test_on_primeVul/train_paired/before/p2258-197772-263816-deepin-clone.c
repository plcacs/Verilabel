bool Helper::getPartitionSizeInfo(const QString &partDevice, qint64 *used, qint64 *free, int *blockSize)
{
    QProcess process;
    QStringList env_list = QProcess::systemEnvironment();

    env_list.append("LANG=C");
    process.setEnvironment(env_list);

    if (Helper::isMounted(partDevice)) {
        process.start(QString("df -B1 -P %1").arg(partDevice));
        process.waitForFinished();

        if (process.exitCode() != 0) {
            dCError("Call df failed: %s", qPrintable(process.readAllStandardError()));

            return false;
        }

        QByteArray output = process.readAll();
        const QByteArrayList &lines = output.trimmed().split('\n');

        if (lines.count() != 2)
            return false;

        output = lines.last().simplified();

        const QByteArrayList &values = output.split(' ');

        if (values.count() != 6)
            return false;

        bool ok = false;

        if (used)
            *used = values.at(2).toLongLong(&ok);

        if (!ok)
            return false;

        if (free)
            *free = values.at(3).toLongLong(&ok);

        if (!ok)
            return false;

        return true;
    } else {
        process.start(QString("%1 -s %2 -c -q -C -L /tmp/partclone.log").arg(getPartcloneExecuter(DDevicePartInfo(partDevice))).arg(partDevice));
        process.setStandardOutputFile("/dev/null");
        process.setReadChannel(QProcess::StandardError);
        process.waitForStarted();

        qint64 used_block = -1;
        qint64 free_block = -1;

        while (process.waitForReadyRead(5000)) {
            const QByteArray &data = process.readAll();

            for (QByteArray line : data.split('\n')) {
                line = line.simplified();

                if (QString::fromLatin1(line).contains(QRegularExpression("\\berror\\b"))) {
                    dCError("Call \"%s %s\" failed: \"%s\"", qPrintable(process.program()), qPrintable(process.arguments().join(' ')), line.constData());
                }

                if (line.startsWith("Space in use:")) {
                    bool ok = false;
                    const QByteArray &value = line.split(' ').value(6, "-1");

                    used_block = value.toLongLong(&ok);

                    if (!ok) {
                        dCError("String to LongLong failed, String: %s", value.constData());

                        return false;
                    }
                } else if (line.startsWith("Free Space:")) {
                    bool ok = false;
                    const QByteArray &value = line.split(' ').value(5, "-1");

                    free_block = value.toLongLong(&ok);

                    if (!ok) {
                        dCError("String to LongLong failed, String: %s", value.constData());

                        return false;
                    }
                } else if (line.startsWith("Block size:")) {
                    bool ok = false;
                    const QByteArray &value = line.split(' ').value(2, "-1");

                    int block_size = value.toInt(&ok);

                    if (!ok) {
                        dCError("String to Int failed, String: %s", value.constData());

                        return false;
                    }

                    if (used_block < 0 || free_block < 0 || block_size < 0)
                        return false;

                    if (used)
                        *used = used_block * block_size;

                    if (free)
                        *free = free_block * block_size;

                    if (blockSize)
                        *blockSize = block_size;

                    process.terminate();
                    process.waitForFinished();

                    return true;
                }
            }
        }
    }

    return false;
}