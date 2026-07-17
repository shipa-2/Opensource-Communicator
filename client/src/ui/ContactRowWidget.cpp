#include "ContactRowWidget.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QApplication>
#include <QStyle>
#include <QTimer>
#include <QWidgetAction>

namespace {

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

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor background =
        m_background.isValid() ? m_background : palette().color(QPalette::Midlight);
    const QColor text = palette().color(QPalette::ButtonText);
    const QColor border = palette().color(QPalette::WindowText);

    painter.setPen(QPen(border, 2));
    painter.setBrush(background);
    painter.drawEllipse(rect().adjusted(1, 1, -1, -1));

    painter.setPen(text);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(14);
    painter.setFont(font);
    painter.drawText(rect(), Qt::AlignCenter, m_letter);
  }

private:
  QString m_letter = QStringLiteral("?");
  QColor m_background;
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
  const QSize naturalSize = button->sizeHint();
  const QFontMetrics metrics(button->font());
  const int width = metrics.horizontalAdvance(button->text()) + 14;
  button->setFixedSize(width, naturalSize.height());
  button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
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
  if (!numberText.isEmpty() && !isSelf) {
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
  m_chatBtn->setFlat(true);
  configureEmojiButton(m_chatBtn);
  layout->addWidget(m_chatBtn);

  m_callBtn = new QPushButton(QStringLiteral("📞"));
  m_callBtn->setObjectName(QStringLiteral("rowCallBtn"));
  m_callBtn->setToolTip(tr("Позвонить (ПКМ — выбор номера)"));
  m_callBtn->setFlat(true);
  configureEmojiButton(m_callBtn);
  m_callBtn->installEventFilter(this);
  layout->addWidget(m_callBtn);

  connect(m_callBtn, &QPushButton::clicked, this, [this]() { emit callRequested(m_peer); });
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

void ContactRowWidget::setPeerColor(const QString &color)
{
  m_peerColor = color;
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
    QPalette pal = palette();
    pal.setColor(QPalette::Button, pal.color(QPalette::Highlight));
    pal.setColor(QPalette::ButtonText, pal.color(QPalette::HighlightedText));
    m_chatBtn->setAutoFillBackground(true);
    m_chatBtn->setPalette(pal);
  } else {
    m_chatBtn->setAutoFillBackground(false);
    m_chatBtn->setPalette(QApplication::palette(m_chatBtn));
    m_chatBtn->setStyleSheet({});
  }
  m_chatBtn->style()->unpolish(m_chatBtn);
  m_chatBtn->style()->polish(m_chatBtn);
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

  if (m_numbers.size() > 1) {
    auto *callMenu = menu.addMenu(tr("Позвонить"));
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
    menu.addAction(tr("Позвонить"), this, [this]() { emit callRequested(m_peer); });
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
    if (target == m_callBtn || target == m_chatBtn) {
      return true;
    }
    target = target->parentWidget();
  }
  return false;
}

void ContactRowWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
}

void ContactRowWidget::refreshAppearance()
{
  refreshAvatarStyle();
  refreshStatusDot();
  refreshTextLabels();
  refreshChatButtonStyle();
}

void ContactRowWidget::refreshTextLabels()
{
  const QPalette appPalette = palette();

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
  if (m_isSelf) {
    return;
  }
  setProperty("selected", selected);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void ContactRowWidget::refreshStatusDot()
{
  const QString color = presenceColor(m_presence);
  const QString borderColor = palette().color(QPalette::Window).name();
  m_statusDot->setStyleSheet(
      QStringLiteral("background:%1; border:1px solid %2; border-radius:5px;").arg(color, borderColor));
}
