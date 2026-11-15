#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QtDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "Admin/AdminWindow.h"
#include "LunaDb/DbManager.h"
#include "LunaProtocol/LunaConstants.h"
#include "Network/LunaServer.h"

namespace {

AdminWindow *g_adminWindow = nullptr;

// 统一日志出口：管理界面日志区 + VS 输出窗口（UTF-16 走 OutputDebugStringW，中文不乱码）
void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const char *level = "INFO";
    if (type == QtWarningMsg)
        level = "WARN";
    else if (type == QtCriticalMsg)
        level = "ERROR";
    else if (type == QtFatalMsg)
        level = "FATAL";

    const QString line = QStringLiteral("[%1] %2").arg(QString::fromLatin1(level), msg);

    if (g_adminWindow)
        QMetaObject::invokeMethod(g_adminWindow, "appendLog", Qt::QueuedConnection, Q_ARG(QString, line));

#ifdef Q_OS_WIN
    OutputDebugStringW(reinterpret_cast<const wchar_t *>(line.utf16()));
    OutputDebugStringW(L"\n");
#endif
    // 同时写 stderr：控制台直接看，重定向时也能落文件
    fprintf(stderr, "%s\n", line.toUtf8().constData());
    fflush(stderr);
}

// 数据库配置来源：Server\config.ini（允许环境变量覆盖）
bool loadDbConfig(QString *host, quint16 *port, QString *user, QString *password, QString *dbName)
{
    QStringList candidates;
    candidates << QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini")
               << QCoreApplication::applicationDirPath() + QStringLiteral("/../Server/config.ini")
               << QCoreApplication::applicationDirPath() + QStringLiteral("/../../Server/config.ini");

    QString path;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            path = candidate;
            break;
        }
    }
    if (path.isEmpty()) {
        qCritical() << "[server] config.ini not found, expected next to exe or in Server/";
        return false;
    }

    QSettings ini(path, QSettings::IniFormat);
    *host = qEnvironmentVariable("LUNA_DB_HOST", ini.value(QStringLiteral("database/host"), QStringLiteral("127.0.0.1")).toString());
    *port = static_cast<quint16>(ini.value(QStringLiteral("database/port"), 3306).toUInt());
    *user = qEnvironmentVariable("LUNA_DB_USER", ini.value(QStringLiteral("database/user"), QStringLiteral("root")).toString());
    *password = qEnvironmentVariable("LUNA_DB_PASSWORD", ini.value(QStringLiteral("database/password")).toString());
    *dbName = qEnvironmentVariable("LUNA_DB_NAME", ini.value(QStringLiteral("database/name"), QStringLiteral("luna_chat")).toString());

    qInfo().noquote() << "[server] config:" << path;
    return true;
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LunaChat"));
    QCoreApplication::setApplicationName(QStringLiteral("LunaChatServer"));

    LunaServer server;
    AdminWindow admin(&server);
    g_adminWindow = &admin;
    qInstallMessageHandler(messageHandler);   // 之后所有 qInfo/qWarning 都进管理界面日志区

    QString host, user, password, dbName;
    quint16 port = 0;
    if (!loadDbConfig(&host, &port, &user, &password, &dbName))
        return 1;

    DbManager &db = DbManager::instance();
    if (!db.initialize(host, port, user, password, dbName)) {
        qCritical().noquote() << "[server] database connect failed:" << db.lastError();
        return 1;
    }

    const QStringList missing = db.missingTables();
    if (!missing.isEmpty()) {
        qCritical().noquote() << "[server] missing tables:" << missing.join(QStringLiteral(", "))
                              << "-> run sql/schema.sql first";
        return 1;
    }
    qInfo().noquote() << QStringLiteral("[server] database ready, %1 tables checked")
                             .arg(DbManager::expectedTables().size());

    if (!server.start(LunaConst::kServerPort))
        return 1;

    QObject::connect(&server, &LunaServer::onlineChanged, &admin, &AdminWindow::refresh);
    admin.show();

    const int code = app.exec();
    db.shutdown();
    return code;
}