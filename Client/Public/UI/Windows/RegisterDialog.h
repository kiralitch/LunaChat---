#pragma once

#include <QDialog>
#include <QJsonObject>

class LunaClient;
class QLabel;
class QLineEdit;

// 独立的注册窗口（不再和登录共用一屏）
class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(LunaClient *client, QWidget *parent = nullptr);

private slots:
    void onRegister();
    void onFrameReceived(const QJsonObject &frame);

private:
    LunaClient *m_client = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_confirmEdit = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
};