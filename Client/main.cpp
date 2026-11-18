#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QTranslator>
#include <QtDebug>

#include "LunaProtocol/LunaConstants.h"
#include "Core/AppSettings.h"
#include "Core/AudioProbe.h"
#include "Network/LunaClient.h"
#include "UI/Windows/LoginWindow.h"
#include "UI/Windows/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 中文标准按钮（QMessageBox / QFileDialog 等）
    auto *translator = new QTranslator(&app);
    if (translator->load(QStringLiteral("qt_zh_CN"),
                         QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(translator);
    }
    QCoreApplication::setOrganizationName(QStringLiteral("LunaChat"));
    QCoreApplication::setApplicationName(QStringLiteral("LunaChat"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));

    // 阶段10 临时入口：设置 LUNA_AUDIO_PROBE=1 时只打印音频能力并退出
    if (qEnvironmentVariableIsSet("LUNA_AUDIO_PROBE")) {
        const QStringList lines = LunaAudio::describeCapabilities();
        for (const QString &line : lines)
            qInfo().noquote() << line;
        return 0;
    }

    // 启动即套用上次保存的主题（资源缺失时是空串，界面退回默认样式也能跑）
    qApp->setStyleSheet(AppSettings::loadStyleSheet(AppSettings::theme()));

    LunaClient client;
    LoginWindow login(&client);

    MainWindow *mainWindow = nullptr;

    QObject::connect(&login, &LoginWindow::loginSucceeded, [&](quint64 userId, const QString &nickname) {
        login.hide();
        if (!mainWindow) {
            mainWindow = new MainWindow(&client, userId, nickname);
            mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        }
        mainWindow->show();
    });

    login.show();
    client.connectToServer(QStringLiteral("127.0.0.1"), LunaConst::kServerPort);

    return app.exec();
}