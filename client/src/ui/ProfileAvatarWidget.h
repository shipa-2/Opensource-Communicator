#pragma once

#include <QPixmap>
#include <QWidget>

namespace itl {
class AppSettings;
}

class ProfileAvatarWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProfileAvatarWidget(itl::AppSettings *settings, QWidget *parent = nullptr);

    void setLetter(const QString &letter);
    void setMenuEnabled(bool enabled);
    void refreshFromSettings();
    void refreshAppearance();

signals:
    void settingsChanged();

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void reloadFromSettings();
    void showAvatarMenu();
    void pickPhoto();
    void pickColor();
    QRectF circleRect() const;

    itl::AppSettings *m_settings = nullptr;
    QString m_letter;
    QPixmap m_photo;
    QColor m_bgColor = QColor(QStringLiteral("#5a9e2f"));
    bool m_menuEnabled = true;
};
