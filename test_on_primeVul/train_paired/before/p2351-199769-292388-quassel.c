QByteArray Cipher::blowfishECB(QByteArray cipherText, bool direction)
{
    QCA::Initializer init;
    QByteArray temp = cipherText;

    //do padding ourselves
    if (direction)
    {
        while ((temp.length() % 8) != 0) temp.append('\0');
    }
    else
    {
        temp = b64ToByte(temp);
        while ((temp.length() % 8) != 0) temp.append('\0');
    }

    QCA::Direction dir = (direction) ? QCA::Encode : QCA::Decode;
    QCA::Cipher cipher(m_type, QCA::Cipher::ECB, QCA::Cipher::NoPadding, dir, m_key);
    QByteArray temp2 = cipher.update(QCA::MemoryRegion(temp)).toByteArray();
    temp2 += cipher.final().toByteArray();

    if (!cipher.ok())
        return cipherText;

    if (direction)
        temp2 = byteToB64(temp2);

    return temp2;
}