#pragma once

#include <QString>

// 密码哈希：统一走 SHA-256 十六进制小写，数据库里绝不出现明文
namespace LunaCrypto {

QString sha256Hex(const QString &text);

}