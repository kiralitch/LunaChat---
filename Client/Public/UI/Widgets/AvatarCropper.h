#pragma once

#include <QDialog>
#include <QImage>
#include <QSize>

class QLabel;
class QSlider;

// 通用裁剪器：支持自定义输出比例（头像 1:1、朋友圈背景 10:3）
class AvatarCropper : public QDialog
{
public:
    explicit AvatarCropper(const QImage &image, const QSize &target = QSize(256, 256),
                           QWidget *parent = nullptr);

    QImage result() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setScale(double scale);
    void clampOffset();
    void render();

    QImage m_image;
    QLabel *m_view = nullptr;
    QSlider *m_zoom = nullptr;
    QSize m_target;
    int m_viewW = 300;
    int m_viewH = 300;
    double m_scale = 1.0;
    double m_fitScale = 1.0;
    QPointF m_offset;
    QPoint m_lastPos;
    bool m_dragging = false;
};