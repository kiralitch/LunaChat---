#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class LunaClient;

// 登录/注册窗口：成功后发 loginSucceeded，由 main 切到主窗口
class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(LunaClient *client, QWidget *parent = nullptr);

signals:
    void loginSucceeded(quint64 userId, const QString &nickname);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onDisconnected();
    void onFrameReceived(const QJsonObject &frame);

private:
    void sendAuth(const QString &type);

    LunaClient *m_client = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_registerButton = nullptr;
    bool m_pendingLogin = false;   // true=本次等的是登录结果，false=注册结果

    // 断线重连后自动重登用：账号密码只存在内存，绝不落盘
    bool m_autoLogin = false;   // 调试用：环境变量 LUNA_AUTOLOGIN=user:pass 时自动登录
    bool m_loggedIn = false;
    QString m_savedUsername;
    QString m_savedPassword;
};