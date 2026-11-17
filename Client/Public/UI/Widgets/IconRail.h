#pragma once

#include <QHash>
#include <QPropertyAnimation>
#include <QWidget>

class QToolButton;
class QVBoxLayout;

// 最左侧图标竖栏（Discord 风格 server rail）：只放"切主界面"的图标
class IconRail : public QWidget
{
    Q_OBJECT

public:
    explicit IconRail(QWidget *parent = nullptr);

    // 添加一个图标项：id 用于回传，glyph 是显示的字符，tooltip 是悬停提示
    void addItem(const QString &id, const QString &glyph, const QString &tooltip);
    void addIconItem(const QString &id, const QString &iconPath, const QString &tooltip);
    void setCurrent(const QString &id);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

signals:
    void currentChanged(const QString &id);

private:
    void moveHighlight(QWidget *button, bool animate);

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_highlight = nullptr;   // 会跟着选中项滑动的背景块
    QPropertyAnimation *m_anim = nullptr;
    QHash<QString, QToolButton *> m_buttons;
    QString m_current;
};