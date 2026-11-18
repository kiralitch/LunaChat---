#pragma once

#include <QPoint>
#include <QWidget>

class QLabel;
class QPushButton;

// 无边框主窗口的自绘标题栏：图标 + 标题 + 最小化/最大化/关闭，支持拖动与双击最大化
class TitleBar : public QWidget
{
public:
    explicit TitleBar(QWidget *window, const QString &title, QWidget *parent = nullptr);

    // 面包屑：显示 "LunaChat 月之音 / 页面名"
    void setSection(const QString &section);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void toggleMaximize();
    void updateMaximizeGlyph();

    QWidget *m_window = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_section = nullptr;
    QPushButton *m_minButton = nullptr;
    QPushButton *m_maxButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
};
