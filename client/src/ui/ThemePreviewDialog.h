#pragma once

#include <QDialog>
#include <QPixmap>

class QLabel;

class ThemePreviewDialog : public QDialog {
    Q_OBJECT

public:
    ThemePreviewDialog(const QPixmap &wallpaper, int uiOpacity, int listOpacity, QWidget *parent = nullptr);

private:
    static QPixmap buildPreviewPixmap(const QPixmap &wallpaper, int uiOpacity, int listOpacity);

    QLabel *m_previewLabel = nullptr;
};
