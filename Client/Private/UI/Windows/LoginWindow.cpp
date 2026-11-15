#include "UI/Windows/LoginWindow.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"
#include "UI/Windows/RegisterDialog.h"

LoginWindow::LoginWindow(LunaClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setWindowTitle(QStringLiteral("LunaChat 月之音 - 登录"));
    setFixedSize(380, 330);

    auto *title = new QLabel(QStringLiteral("月之音 LunaChat"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:20px; font-weight:bold; padding:6px;"));

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("用户名"));
    m_usernameEdit->setText(QStringLiteral("luna"));      // 演示方便：预填测试账号

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setText(QStringLiteral("123456"));

    m_nicknameEdit = new QLineEdit(this);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("昵称（注册请用独立窗口）"));
    m_nicknameEdit->setVisible(false);   // 注册改用独立窗口后，登录页不再显示昵称

    m_loginButton = new QPushButton(QStringLiteral("登录"), this);
    m_registerButton = new QPushButton(QStringLiteral("注册新账号"), this);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(m_loginButton);
    buttonRow->addWidget(m_registerButton);

    m_statusLabel = new QLabel(QStringLiteral("正在连接服务器..."), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_usernameEdit);
    layout->addWidget(m_passwordEdit);
    layout->addWidget(m_nicknameEdit);
    layout->addLayout(buttonRow);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    // 调试/自动化演示用：LUNA_AUTOLOGIN=username:password 时自动登录，无需点击
    const QString autoLogin = qEnvironmentVariable("LUNA_AUTOLOGIN");
    if (!autoLogin.isEmpty()) {
        const QStringList parts = autoLogin.split(QLatin1Char(':'));
        if (parts.size() == 2) {
            m_usernameEdit->setText(parts.at(0));
            m_passwordEdit->setText(parts.at(1));
            m_autoLogin = true;
        }
    }

    connect(m_loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);

    // 网络层状态直接反映到界面文案，用户能分清"没连上"和"密码错"
    connect(m_client, &LunaClient::connected, this, &LoginWindow::onConnected);
    connect(m_client, &LunaClient::disconnected, this, &LoginWindow::onDisconnected);
    connect(m_client, &LunaClient::frameReceived, this, &LoginWindow::onFrameReceived);
}

void LoginWindow::onConnected()
{
    m_statusLabel->setText(QStringLiteral("已连接服务器，可以登录"));
    m_loginButton->setEnabled(true);
    m_registerButton->setEnabled(true);

    // 自动化/演示：首次连上就直接登录
    if (m_autoLogin && !m_loggedIn) {
        m_autoLogin = false;
        onLoginClicked();
        return;
    }

    // 曾经登录成功过：重连成功后自动重登，用户不用重新输密码
    if (m_loggedIn) {
        QJsonObject data;
        data.insert(QStringLiteral("username"), m_savedUsername);
        data.insert(QStringLiteral("password"), m_savedPassword);
        data.insert(QStringLiteral("nickname"), QString());

        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kLogin);
        frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        frame.insert(QStringLiteral("data"), data);
        m_client->sendFrame(frame);
        m_pendingLogin = false;
        m_statusLabel->setText(QStringLiteral("连接已恢复，正在自动重登..."));
    }
}

void LoginWindow::onDisconnected()
{
    m_statusLabel->setText(QStringLiteral("连接已断开，正在自动重连..."));
    m_loginButton->setEnabled(false);
    m_registerButton->setEnabled(false);
}

void LoginWindow::sendAuth(const QString &type)
{
    QJsonObject data;
    data.insert(QStringLiteral("username"), m_usernameEdit->text().trimmed());
    data.insert(QStringLiteral("password"), m_passwordEdit->text());
    data.insert(QStringLiteral("nickname"), m_nicknameEdit->text().trimmed());

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), type);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
}

void LoginWindow::onLoginClicked()
{
    if (m_client->state() != LunaClient::Connected) {
        m_statusLabel->setText(QStringLiteral("尚未连接服务器，请稍候"));
        return;
    }
    m_pendingLogin = true;
    sendAuth(MsgType::kLogin);
    m_statusLabel->setText(QStringLiteral("登录中..."));
}

void LoginWindow::onRegisterClicked()
{
    if (m_client->state() != LunaClient::Connected) {
        m_statusLabel->setText(QStringLiteral("尚未连接服务器，请稍候"));
        return;
    }
    // 打开独立的注册窗口
    RegisterDialog dialog(m_client, this);
    if (dialog.exec() == QDialog::Accepted)
        m_statusLabel->setText(QStringLiteral("注册成功，请登录"));
}

void LoginWindow::onFrameReceived(const QJsonObject &frame)
{
    if (frame.value(QStringLiteral("type")).toString() != MsgType::kLoginResp)
        return;

    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const bool ok = data.value(QStringLiteral("ok")).toBool();
    const QString msg = data.value(QStringLiteral("msg")).toString();

    if (!ok) {
        m_statusLabel->setText(msg);
        m_pendingLogin = false;
        return;
    }

    if (m_pendingLogin) {
        const quint64 userId = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());
        m_statusLabel->setText(QStringLiteral("登录成功"));

        // 记住本次凭据（仅内存），供断线重连后自动重登
        m_loggedIn = true;
        m_savedUsername = m_usernameEdit->text().trimmed();
        m_savedPassword = m_passwordEdit->text();

        emit loginSucceeded(userId, data.value(QStringLiteral("nickname")).toString());
    } else if (m_loggedIn) {
        m_statusLabel->setText(QStringLiteral("已自动重登"));
    } else {
        m_statusLabel->setText(QStringLiteral("注册成功，请点击登录"));
    }
}