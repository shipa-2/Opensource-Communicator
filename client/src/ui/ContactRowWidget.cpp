#include "ContactRowWidget.h"

#include "ui/StyleHelper.h"

#include <QApplication>
#include <QDateTime>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QWidgetAction>

#include <QtMath>

namespace {

constexpr int kOscWaveDurationMs = 1500;
constexpr int kOscWaveFrameMs = 16;

class ContactAvatarLabel : public QWidget {
public:
  explicit ContactAvatarLabel(QWidget *parent = nullptr)
      : QWidget(parent)
  {
    setFixedSize(36, 36);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
  }

  void setLetter(const QString &letter)
  {
    if (m_letter == letter) {
      return;
    }
    m_letter = letter.isEmpty() ? QStringLiteral("?") : letter;
    update();
  }

  void setBackgroundColor(const QColor &color)
  {
    m_background = color;
    update();
  }

  void setPhoto(const QPixmap &photo)
  {
    m_photo = photo;
    update();
  }

  void setPresenceRingEnabled(bool enabled)
  {
    if (m_presenceRingEnabled == enabled) {
      return;
    }
    m_presenceRingEnabled = enabled;
    update();
  }

  void setPresenceRingColor(const QColor &color)
  {
    if (m_presenceRingColor == color) {
      return;
    }
    m_presenceRingColor = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QColor background =
        m_background.isValid() ? m_background : palette().color(QPalette::Midlight);
    const QColor text = palette().color(QPalette::ButtonText);
    const QColor border = palette().color(QPalette::WindowText);
    const QRectF outer = QRectF(1.0, 1.0, width() - 2.0, height() - 2.0);
    constexpr qreal kRingWidth = 2.5;
    const bool ring = m_presenceRingEnabled && m_presenceRingColor.isValid();
    const QRectF face = ring ? outer.adjusted(kRingWidth, kRingWidth, -kRingWidth, -kRingWidth) : outer;

    if (!m_photo.isNull()) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(background);
      painter.drawEllipse(face);

      QPainterPath clip;
      clip.addEllipse(face);
      painter.setClipPath(clip);
      const QPixmap scaled =
          m_photo.scaled(face.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      const QPointF topLeft(face.center().x() - scaled.width() / 2.0,
                            face.center().y() - scaled.height() / 2.0);
      painter.drawPixmap(topLeft, scaled);
      painter.setClipping(false);

      if (ring) {
        QPen ringPen(m_presenceRingColor, kRingWidth);
        ringPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(ringPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(outer);
      } else {
        painter.setPen(QPen(border, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(face);
      }
      return;
    }

    painter.setPen(ring ? Qt::NoPen : QPen(border, 2));
    painter.setBrush(background);
    painter.drawEllipse(face);

    painter.setPen(text);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(14);
    painter.setFont(font);
    painter.drawText(face, Qt::AlignCenter, m_letter);

    if (ring) {
      QPen ringPen(m_presenceRingColor, kRingWidth);
      ringPen.setJoinStyle(Qt::RoundJoin);
      painter.setPen(ringPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(outer);
    }
  }

private:
  QString m_letter = QStringLiteral("?");
  QColor m_background;
  QPixmap m_photo;
  bool m_presenceRingEnabled = false;
  QColor m_presenceRingColor;
};

QString presenceColor(const QString &presence)
{
  const QString p = presence.toLower();
  if (p == QStringLiteral("in-call")) {
    return QStringLiteral("#2880d4");
  }
  if (p == QStringLiteral("online")) {
    return QStringLiteral("#5a9e2f");
  }
  if (p == QStringLiteral("away")) {
    return QStringLiteral("#d4a017");
  }
  return QStringLiteral("#aaaaaa");
}

QString avatarLetter(const QString &name)
{
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    return QStringLiteral("?");
  }
  return trimmed.left(1).toUpper();
}

void configureEmojiButton(QPushButton *button)
{
  if (!button) {
    return;
  }
  const QFontMetrics metrics(button->font());
  const int side = qMax(metrics.height() + 8,
                        qMax(metrics.horizontalAdvance(QStringLiteral("📞")) + 14,
                             qMax(metrics.horizontalAdvance(QStringLiteral("💬")) + 14,
                                      metrics.horizontalAdvance(QStringLiteral("📹")) + 14)));
  button->setFixedSize(side, side);
  button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  button->setFlat(true);
  button->setAttribute(Qt::WA_Hover, true);
  // Contour hover for flat emoji buttons (Breeze draws almost nothing when flat+transparent).
  button->setStyleSheet(QStringLiteral(
      "QPushButton {"
      "  background-color: transparent;"
      "  border: 1px solid transparent;"
      "  border-radius: 4px;"
      "  padding: 0px;"
      "}"
      "QPushButton:hover {"
      "  background-color: transparent;"
      "  border: 1px solid palette(mid);"
      "}"
      "QPushButton:pressed {"
      "  background-color: palette(midlight);"
      "  border: 1px solid palette(mid);"
      "}"));
}
} // namespace

ContactRowWidget::ContactRowWidget(const QString &peer, const QString &name, const QString &ext,
                                   const QString &phone, const QString &presence, bool isSelf,
                                   bool canDelete, QWidget *parent)
    : QWidget(parent)
    , m_peer(peer)
    , m_presence(presence)
    , m_isSelf(isSelf)
    , m_canDelete(canDelete)
{
  setObjectName(isSelf ? QStringLiteral("contactRowSelf") : QStringLiteral("contactRow"));
  setMinimumHeight(48);
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);
  setAutoFillBackground(false);
  if (!isSelf) {
    setToolTip(tr("Двойной щелчок — заметка\nПКМ — действия с контактом"));
  }

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(8);

  auto *avatarWrap = new QWidget;
  avatarWrap->setFixedSize(40, 40);
  auto *avatarLayout = new QVBoxLayout(avatarWrap);
  avatarLayout->setContentsMargins(0, 0, 0, 0);

  auto *avatar = new ContactAvatarLabel;
  avatar->setObjectName(QStringLiteral("contactRowAvatar"));
  avatar->setLetter(avatarLetter(name));
  m_avatar = avatar;
  avatarLayout->addWidget(m_avatar, 0, Qt::AlignCenter);
  refreshAvatarStyle();

  m_statusDot = new QLabel(avatarWrap);
  m_statusDot->setFixedSize(10, 10);
  m_statusDot->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_statusDot->move(26, 26);
  refreshStatusDot();

  layout->addWidget(avatarWrap);

  QString displayName = name;
  if (isSelf) {
    displayName += tr(" (Я)");
  }

  auto *textCol = new QVBoxLayout;
  textCol->setSpacing(0);
  m_nameLabel = new QLabel(displayName);
  m_nameLabel->setObjectName(QStringLiteral("contactName"));
  textCol->addWidget(m_nameLabel);

  const QString numberText = !ext.isEmpty() ? ext : phone;
  if (!numberText.isEmpty()) {
    m_numberLabel = new QLabel(numberText);
    m_numberLabel->setObjectName(QStringLiteral("contactNumber"));
    QFont numberFont = m_numberLabel->font();
    numberFont.setPixelSize(12);
    m_numberLabel->setFont(numberFont);
    textCol->addWidget(m_numberLabel);
  }

  layout->addLayout(textCol, 1);
  refreshTextLabels();

  m_chatBtn = new QPushButton(QStringLiteral("💬"));
  m_chatBtn->setObjectName(QStringLiteral("rowChatBtn"));
  m_chatBtn->setToolTip(tr("Сообщение"));
  configureEmojiButton(m_chatBtn);
  layout->addWidget(m_chatBtn);

  m_callBtn = new QPushButton(QStringLiteral("📞"));
  m_callBtn->setObjectName(QStringLiteral("rowCallBtn"));
  m_callBtn->setToolTip(tr("Звонок (ПКМ — выбор номера)"));
  configureEmojiButton(m_callBtn);
  m_callBtn->installEventFilter(this);
  layout->addWidget(m_callBtn);

  m_videoBtn = new QPushButton(QStringLiteral("📹"));
  m_videoBtn->setObjectName(QStringLiteral("rowVideoBtn"));
  m_videoBtn->setToolTip(tr("Видеозвонок"));
  configureEmojiButton(m_videoBtn);
  m_videoBtn->setVisible(false);
  layout->addWidget(m_videoBtn);

  connect(m_callBtn, &QPushButton::clicked, this, [this]() { emit callRequested(m_peer); });
  connect(m_videoBtn, &QPushButton::clicked, this, [this]() { emit videoCallRequested(m_peer); });
  connect(m_chatBtn, &QPushButton::clicked, this, [this]() { emit chatRequested(m_peer); });
}

void ContactRowWidget::setChatButtonVisible(bool visible)
{
  if (m_chatBtn) {
    m_chatBtn->setVisible(visible);
    if (!visible) {
      setUnreadBlink(false);
    } else {
      refreshChatButtonStyle();
    }
  }
}

void ContactRowWidget::setCallButtonVisible(bool visible)
{
  if (m_callBtn) {
    m_callBtn->setVisible(visible);
  }
}

void ContactRowWidget::setVideoCallSupported(bool supported)
{
  m_videoCallSupported = supported && !m_isSelf;
}

void ContactRowWidget::setVideoButtonVisible(bool visible)
{
  if (m_videoBtn) {
    m_videoBtn->setVisible(visible && m_videoCallSupported);
  }
}

void ContactRowWidget::setPeerColor(const QString &color)
{
  m_peerColor = color;
  refreshAvatarStyle();
}

void ContactRowWidget::setPeerAvatar(const QPixmap &avatar)
{
  m_peerAvatar = avatar;
  refreshAvatarStyle();
}

void ContactRowWidget::setUnreadBlink(bool enabled)
{
  if (m_unreadBlink == enabled) {
    return;
  }
  m_unreadBlink = enabled;
  if (!enabled) {
    if (m_blinkTimer) {
      m_blinkTimer->stop();
    }
    m_blinkAccentOn = false;
    refreshChatButtonStyle();
    return;
  }

  if (!m_blinkTimer) {
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
      if (!m_unreadBlink || !m_chatBtn || !m_chatBtn->isVisible()) {
        return;
      }
      m_blinkAccentOn = !m_blinkAccentOn;
      refreshChatButtonStyle();
    });
  }
  m_blinkAccentOn = true;
  refreshChatButtonStyle();
  m_blinkTimer->start();
}

void ContactRowWidget::refreshChatButtonStyle()
{
  if (!m_chatBtn || !m_chatBtn->isVisible()) {
    return;
  }

  if (m_unreadBlink && m_blinkAccentOn) {
    QPalette pal = QApplication::palette(m_chatBtn);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor accentText = pal.color(QPalette::HighlightedText);
    m_chatBtn->setStyleSheet(QStringLiteral(
                                 "QPushButton {"
                                 "  background-color: %1;"
                                 "  color: %2;"
                                 "  border: 1px solid %1;"
                                 "  border-radius: 4px;"
                                 "  padding: 0px;"
                                 "}"
                                 "QPushButton:hover {"
                                 "  background-color: %1;"
                                 "  border: 1px solid palette(light);"
                                 "}")
                                 .arg(accent.name(), accentText.name()));
  } else {
    configureEmojiButton(m_chatBtn);
  }
  m_chatBtn->update();
}

void ContactRowWidget::setCallNumbers(const QVector<CallNumber> &numbers)
{
  m_numbers = numbers;
}

void ContactRowWidget::setPhones(const QString &phone, const QString &personalPhone)
{
  m_phone = phone;
  m_personalPhone = personalPhone;
}

void ContactRowWidget::showCallMenu(const QPoint &globalPos)
{
  if (m_numbers.isEmpty()) {
    emit callRequested(m_peer);
    return;
  }

  QMenu menu(this);
  itl::applyPopupMenuStyle(&menu);
  auto *header = new QLabel(tr("Позвонить по:"), &menu);
  header->setObjectName(QStringLiteral("callMenuHeader"));
  header->setContentsMargins(12, 6, 12, 4);
  QFont headerFont = header->font();
  headerFont.setBold(true);
  header->setFont(headerFont);
  auto *headerAction = new QWidgetAction(&menu);
  headerAction->setDefaultWidget(header);
  headerAction->setEnabled(false);
  menu.addAction(headerAction);

  for (const CallNumber &number : m_numbers) {
    const QString label = number.second.isEmpty()
                              ? number.first
                              : QStringLiteral("%1  —  %2").arg(number.second, number.first);
    QAction *action = menu.addAction(label);
    const QString dial = number.second;
    connect(action, &QAction::triggered, this, [this, dial]() {
      emit callNumberRequested(dial);
    });
  }

  menu.exec(globalPos);
}

bool ContactRowWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_callBtn && event->type() == QEvent::ContextMenu) {
    if (m_numbers.size() > 1) {
      const auto *ctx = static_cast<QContextMenuEvent *>(event);
      showCallMenu(ctx->globalPos());
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void ContactRowWidget::contextMenuEvent(QContextMenuEvent *event)
{
  if (m_isSelf) {
    QMenu menu(this);
    itl::applyPopupMenuStyle(&menu);
    bool hasAny = false;
    if (!m_personalPhone.isEmpty()) {
      QAction *action = menu.addAction(tr("Скопировать личный номер"));
      connect(action, &QAction::triggered, this, [this]() { emit copyNumberRequested(m_personalPhone); });
      hasAny = true;
    }
    if (!m_phone.isEmpty()) {
      QAction *action = menu.addAction(tr("Скопировать рабочий номер"));
      connect(action, &QAction::triggered, this, [this]() { emit copyNumberRequested(m_phone); });
      hasAny = true;
    }
    if (hasAny) {
      menu.exec(event->globalPos());
      event->accept();
    }
    return;
  }

  if (isInteractiveChild(childAt(event->pos()))) {
    QWidget::contextMenuEvent(event);
    return;
  }

  QMenu menu(this);
  itl::applyPopupMenuStyle(&menu);

  if (m_numbers.size() > 1) {
    auto *callMenu = menu.addMenu(tr("Звонок"));
    for (const CallNumber &number : m_numbers) {
      const QString label = number.second.isEmpty()
                                ? number.first
                                : QStringLiteral("%1  —  %2").arg(number.second, number.first);
      QAction *action = callMenu->addAction(label);
      const QString dial = number.second;
      connect(action, &QAction::triggered, this, [this, dial]() {
        emit callNumberRequested(dial);
      });
    }
  } else {
    menu.addAction(tr("Звонок"), this, [this]() { emit callRequested(m_peer); });
  }

  if (m_videoCallSupported) {
    menu.addAction(tr("Видеозвонок"), this, [this]() { emit videoCallRequested(m_peer); });
  }

  menu.addAction(tr("Сообщение"), this, [this]() { emit chatRequested(m_peer); });
  menu.addAction(tr("Заметка"), this, [this]() { emit notesRequested(m_peer); });

  menu.addSeparator();

  if (m_canDelete) {
    menu.addAction(tr("Удалить"), this, [this]() { emit deleteRequested(m_peer); });
  }
  menu.addAction(tr("Экспортировать..."), this, [this]() { emit exportRequested(m_peer); });

  menu.exec(event->globalPos());
  event->accept();
}

void ContactRowWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (m_isSelf) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }

  if (isInteractiveChild(childAt(event->pos()))) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }

  emit notesRequested(m_peer);
  event->accept();
}

bool ContactRowWidget::isInteractiveChild(QWidget *target) const
{
  while (target && target != this) {
    if (target == m_callBtn || target == m_videoBtn || target == m_chatBtn) {
      return true;
    }
    target = target->parentWidget();
  }
  return false;
}

void ContactRowWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange) {
    refreshAppearance();
  }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ContactRowWidget::enterEvent(QEnterEvent *event)
#else
void ContactRowWidget::enterEvent(QEvent *event)
#endif
{
  m_hovered = true;
  update();
  QWidget::enterEvent(event);
}

void ContactRowWidget::leaveEvent(QEvent *event)
{
  QWidget::leaveEvent(event);

  // Clear immediately so fast cursor movement does not leave a trail of highlighted rows.
  if (m_hovered) {
    m_hovered = false;
    update();
  }

  // Moving onto 💬/📞 children also fires leave on the row — restore hover if still inside.
  QTimer::singleShot(0, this, [this]() {
    const bool inside = underMouse();
    if (m_hovered != inside) {
      m_hovered = inside;
      update();
    }
  });
}

void ContactRowWidget::startOscDiscoveryWave()
{
  m_waveStartedMs = QDateTime::currentMSecsSinceEpoch();
  m_waveProgress = 0.0;
  if (!m_waveTimer) {
    m_waveTimer = new QTimer(this);
    m_waveTimer->setInterval(kOscWaveFrameMs);
    connect(m_waveTimer, &QTimer::timeout, this, &ContactRowWidget::onOscWaveTick);
  }
  if (!m_waveTimer->isActive()) {
    m_waveTimer->start();
  }
  update();
}

void ContactRowWidget::onOscWaveTick()
{
  const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_waveStartedMs;
  if (elapsed >= kOscWaveDurationMs) {
    m_waveTimer->stop();
    m_waveProgress = 0.0;
    update();
    return;
  }
  m_waveProgress = qBound(0.0, elapsed / static_cast<qreal>(kOscWaveDurationMs), 1.0);
  update();
}

void ContactRowWidget::paintOscWave(QPainter &painter, const QRectF &frame) const
{
  if (m_waveProgress <= 0.0) {
    return;
  }

  const QColor base = QApplication::palette().color(QPalette::Highlight);
  const qreal fade = qSin(m_waveProgress * M_PI);
  const qreal bandWidth = frame.width() * 0.42;

  auto drawBand = [&](qreal progress, int alphaPeak) {
    if (alphaPeak <= 0) {
      return;
    }
    const qreal centerX = frame.left() + progress * (frame.width() + bandWidth) - bandWidth * 0.5;
    QLinearGradient gradient(centerX - bandWidth * 0.5, frame.top(), centerX + bandWidth * 0.5, frame.top());
    QColor transparent = base;
    transparent.setAlpha(0);
    gradient.setColorAt(0.0, transparent);
    QColor peak = base;
    peak.setAlpha(alphaPeak);
    gradient.setColorAt(0.5, peak);
    gradient.setColorAt(1.0, transparent);
    painter.fillRect(frame, gradient);
  };

  drawBand(m_waveProgress, int(85 * fade));
  drawBand(qMax<qreal>(0.0, m_waveProgress - 0.14), int(55 * fade));
  drawBand(qMin<qreal>(1.0, m_waveProgress + 0.08), int(35 * fade));
}

void ContactRowWidget::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);
  const bool drawChrome = m_hovered || m_selected;
  const bool drawWave = m_waveProgress > 0.0;
  if (!drawChrome && !drawWave) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setClipRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

  const QColor accent = QApplication::palette().color(QPalette::Highlight);
  const qreal margin = 2.0;
  const qreal radius = 6.0;
  const QRectF frame = QRectF(rect()).adjusted(margin, margin, -margin, -margin);

  if (drawChrome) {
    QColor fill = accent;
    fill.setAlpha(m_selected ? 55 : 35);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(frame, radius, radius);

    if (m_hovered && !m_selected) {
      QPen pen(accent, 1.0);
      pen.setJoinStyle(Qt::RoundJoin);
      pen.setCapStyle(Qt::RoundCap);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(pen);
      painter.drawRoundedRect(frame, radius, radius);
    }
  }

  if (drawWave) {
    painter.setPen(Qt::NoPen);
    paintOscWave(painter, frame);
  }
}

void ContactRowWidget::refreshAppearance()
{
  refreshAvatarStyle();
  refreshStatusDot();
  refreshTextLabels();
  refreshChatButtonStyle();
  update();
}

void ContactRowWidget::refreshBackground()
{
  // Contour/hover is drawn in paintEvent — keep the row transparent for wallpaper.
  setStyleSheet({});
  setAutoFillBackground(false);
  setPalette(QApplication::palette());
  update();
}

void ContactRowWidget::refreshTextLabels()
{
  const QPalette appPalette = QApplication::palette();

  if (m_nameLabel) {
    m_nameLabel->setStyleSheet({});
    m_nameLabel->setPalette(appPalette);
    m_nameLabel->update();
  }

  if (m_numberLabel) {
    QPalette numberPalette = appPalette;
    numberPalette.setColor(QPalette::WindowText, appPalette.color(QPalette::Link));
    m_numberLabel->setStyleSheet({});
    m_numberLabel->setPalette(numberPalette);
    m_numberLabel->update();
  }
}

void ContactRowWidget::setOscPeerStyle(bool enabled)
{
  if (m_oscPeerStyle == enabled) {
    return;
  }
  m_oscPeerStyle = enabled;
  refreshStatusDot();
  refreshAvatarStyle();
}

bool ContactRowWidget::showsOscPresenceRing() const
{
  return m_isSelf || m_oscPeerStyle;
}

void ContactRowWidget::refreshAvatarStyle()
{
  auto *avatar = dynamic_cast<ContactAvatarLabel *>(m_avatar);
  if (!avatar) {
    return;
  }

  if (m_peerColor.isEmpty()) {
    avatar->setBackgroundColor({});
  } else {
    avatar->setBackgroundColor(QColor(m_peerColor));
  }
  avatar->setPhoto(m_peerAvatar);
  if (showsOscPresenceRing()) {
    avatar->setPresenceRingEnabled(true);
    avatar->setPresenceRingColor(QColor(presenceColor(m_presence)));
  } else {
    avatar->setPresenceRingEnabled(false);
  }
  avatar->update();
}

void ContactRowWidget::updatePresence(const QString &presence)
{
  m_presence = presence;
  refreshStatusDot();
}

void ContactRowWidget::updateName(const QString &name)
{
  QString displayName = name;
  if (m_isSelf) {
    displayName += tr(" (Я)");
  }
  m_nameLabel->setText(displayName);
  if (auto *avatar = dynamic_cast<ContactAvatarLabel *>(m_avatar)) {
    avatar->setLetter(avatarLetter(name));
  }
}

void ContactRowWidget::setSelected(bool selected)
{
  if (m_selected == selected) {
    return;
  }
  m_selected = selected;
  setProperty("selected", selected);
  update();
}

void ContactRowWidget::refreshStatusDot()
{
  if (showsOscPresenceRing()) {
    if (m_statusDot) {
      m_statusDot->hide();
    }
    refreshAvatarStyle();
    return;
  }

  if (m_statusDot) {
    m_statusDot->show();
  }
  const QString color = presenceColor(m_presence);
  const QString borderColor = palette().color(QPalette::Window).name();
  m_statusDot->setStyleSheet(
      QStringLiteral("background:%1; border:1px solid %2; border-radius:5px;").arg(color, borderColor));
}
