#pragma once

#include <QDialog>

class QLabel;

// 统一风格的危险操作确认框（删除好友 / 删除评论等）
class ConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    static bool ask(QWidget *parent, const QString &title, const QString &message,
                    const QString &confirmText = QStringLiteral("确定"));

private:
    explicit ConfirmDialog(QWidget *parent, const QString &title, const QString &message,
                           const QString &confirmText);
};
