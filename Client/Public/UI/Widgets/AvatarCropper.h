#pragma once

#include <QDialog>
#include <QImage>

class QLabel;
class QSlider;

// 通用头像裁剪器：滑杆缩放 + 拖动平移，输出正方形；个人头像与群头像共用
class AvatarCropper : public QDialog
{
public:
    explicit AvatarCropper(const QImage &image, QWidget *parent = nullptr);

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
    double m_scale = 1.0;
    double m_fitScale = 1.0;
    QPointF m_offset;
    QPoint m_lastPos;
    bool m_dragging = false;
};