#include "UI/Widgets/ConfirmDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ConfirmDialog::ConfirmDialog(QWidget *parent, const QString &title, const QString &message,
                             const QString &confirmText)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("ConfirmDialog"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("ConfirmCard"));
    card->setFixedWidth(380);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("ChatTitle"));
    auto *messageLabel = new QLabel(message, card);
    messageLabel->setObjectName(QStringLiteral("ChatSubtitle"));
    messageLabel->setWordWrap(true);
    messageLabel->setMinimumHeight(40);

    auto *cancel = new QPushButton(QStringLiteral("取消"), card);
    cancel->setFixedWidth(84);
    auto *confirm = new QPushButton(confirmText, card);
    confirm->setObjectName(QStringLiteral("DangerButton"));
    confirm->setFixedWidth(96);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(confirm);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 18);
    cardLayout->setSpacing(10);
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(messageLabel);
    cardLayout->addSpacing(4);
    cardLayout->addLayout(buttons);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(card);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, this, &QDialog::accept);
}

bool ConfirmDialog::ask(QWidget *parent, const QString &title, const QString &message,
                        const QString &confirmText)
{
    ConfirmDialog dialog(parent, title, message, confirmText);
    return dialog.exec() == QDialog::Accepted;
}
