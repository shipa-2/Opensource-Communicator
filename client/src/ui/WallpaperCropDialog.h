#pragma once

#include <QDialog>
#include <QSize>

class QPixmap;
class QWidget;

class WallpaperCropDialog : public QDialog {
    Q_OBJECT

public:
    explicit WallpaperCropDialog(const QPixmap &source, const QSize &targetSize, QWidget *parent = nullptr,
                                 const QString &dialogTitle = {});

    QPixmap croppedPixmap() const;

    static QPixmap cropImage(const QPixmap &source, const QSize &targetSize, QWidget *parent = nullptr,
                             const QString &dialogTitle = {});

private:
    QWidget *m_canvas = nullptr;
    QSize m_targetSize;
};
