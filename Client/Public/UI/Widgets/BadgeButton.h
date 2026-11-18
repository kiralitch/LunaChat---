#pragma once

#include <QToolButton>

// 带未处理数量红点的工具按钮：数量为 0 时只显示图标，>99 显示 "99+"
class BadgeButton : public QToolButton
{
    Q_OBJECT

public:
    explicit BadgeButton(QWidget *parent = nullptr);

    void setCount(int count);
    int count() const { return m_count; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_count = 0;
};
