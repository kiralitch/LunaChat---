#include "LunaUtil/Sha256.h"

#include <QCryptographicHash>

QString LunaCrypto::sha256Hex(const QString &text)
{
    // 用 Qt 自带实现，不引入任何第三方加密库
    return QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}