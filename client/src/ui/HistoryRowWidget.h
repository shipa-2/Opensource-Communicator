#pragma once

#include <QWidget>

class QLabel;
class QEvent;
class QContextMenuEvent;
class QEnterEvent;
class QMouseEvent;
class QPaintEvent;

class HistoryRowWidget : public QWidget {
    Q_OBJECT

public:
    HistoryRowWidget(const QString &peer, const QString &displayName, const QString &firstLine,
                     const QString &secondLine, const QString &whenText, const QString &arrow,
                     const QString &arrowColor, bool missed, QWidget *parent = nullptr);

    QString peer() const { return m_peer; }
    QString displayName() const { return m_displayName; }
    void setSelected(bool selected);
    void setChromeAlpha(int alpha);
    void refreshAppearance();

signals:
    void callRequested(const QString &peer);
    void chatRequested(const QString &peer);
    void notesRequested(const QString &peer);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void refreshBackground();
    void refreshTextLabels();

    QString m_peer;
    QString m_displayName;
    bool m_missed = false;
    bool m_selected = false;
    bool m_hovered = false;
    int m_chromeAlpha = 255;

    QLabel *m_arrowLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QLabel *m_dateLabel = nullptr;
    QString m_arrowColor;
};
