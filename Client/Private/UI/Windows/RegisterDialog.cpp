#include "UI/Windows/RegisterDialog.h"

#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"

RegisterDialog::RegisterDialog(LunaClient *client, QWidget *parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle(QStringLiteral("注册新账号"));
    setFixedSize(380, 320);

    auto *title = new QLabel(QStringLiteral("创建 LunaChat 账号"), this);
    title->setObjectName(QStringLiteral("ChatTitle"));

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("用户名（登录用，唯一）"));
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit = new QLineEdit(this);
    m_confirmEdit->setPlaceholderText(QStringLiteral("确认密码"));
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_nicknameEdit = new QLineEdit(this);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("昵称（别人看到的名字）"));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("用户名"), m_usernameEdit);
    form->addRow(QStringLiteral("密码"), m_passwordEdit);
    form->addRow(QStringLiteral("确认密码"), m_confirmEdit);
    form->addRow(QStringLiteral("昵称"), m_nicknameEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    auto *registerButton = new QPushButton(QStringLiteral("注册"), this);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(registerButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    connect(registerButton, &QPushButton::clicked, this, &RegisterDialog::onRegister);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_client, &LunaClient::frameReceived, this, &RegisterDialog::onFrameReceived);
}

void RegisterDialog::onRegister()
{
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    const QString nickname = m_nicknameEdit->text().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("用户名和密码不能为空"));
        return;
    }
    if (password != m_confirmEdit->text()) {
        m_statusLabel->setText(QStringLiteral("两次输入的密码不一致"));
        return;
    }
    if (m_client->state() != LunaClient::Connected) {
        m_statusLabel->setText(QStringLiteral("尚未连接服务器"));
        return;
    }

    QJsonObject data;
    data.insert(QStringLiteral("username"), username);
    data.insert(QStringLiteral("password"), password);
    data.insert(QStringLiteral("nickname"), nickname.isEmpty() ? username : nickname);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kRegister);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
    m_statusLabel->setText(QStringLiteral("注册中..."));
}

void RegisterDialog::onFrameReceived(const QJsonObject &frame)
{
    if (frame.value(QStringLiteral("type")).toString() != MsgType::kLoginResp)
        return;

    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const bool ok = data.value(QStringLiteral("ok")).toBool();
    m_statusLabel->setText(data.value(QStringLiteral("msg")).toString());
    if (ok)
        accept();          // 注册成功直接关窗，回到登录界面
}