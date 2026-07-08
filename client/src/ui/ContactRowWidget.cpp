#include "ContactRowWidget.h"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>

namespace {
QString presenceColor(const QString &presence)
{
  const QString p = presence.toLower();
  if (p == QStringLiteral("online") || p == QStringLiteral("in-call")) {
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
                                   QWidget *parent)
    : QWidget(parent)
    , m_peer(peer)
    , m_presence(presence)
    , m_isSelf(isSelf)
{
  setObjectName(isSelf ? QStringLiteral("contactRowSelf") : QStringLiteral("contactRow"));
  setMinimumHeight(48);
  setAutoFillBackground(false);
  if (!isSelf) {
    setToolTip(tr("Двойной щелчок — заметки"));
  }

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(8);

  auto *avatarWrap = new QWidget;
  avatarWrap->setFixedSize(40, 40);
  auto *avatarLayout = new QVBoxLayout(avatarWrap);
  avatarLayout->setContentsMargins(0, 0, 0, 0);

  m_avatar = new QLabel(avatarLetter(name));
  m_avatar->setObjectName(QStringLiteral("contactRowAvatar"));
  m_avatar->setAlignment(Qt::AlignCenter);
  m_avatar->setFixedSize(36, 36);
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
  m_callBtn->setToolTip(tr("Позвонить"));
  m_callBtn->setFlat(true);
  configureEmojiButton(m_callBtn);
  layout->addWidget(m_callBtn);

  connect(m_callBtn, &QPushButton::clicked, this, [this]() { emit callRequested(m_peer); });
  connect(m_chatBtn, &QPushButton::clicked, this, [this]() { emit chatRequested(m_peer); });
}

void ContactRowWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
    refreshAppearance();
  }
}

void ContactRowWidget::refreshAppearance()
{
  refreshAvatarStyle();
  refreshStatusDot();
  refreshTextLabels();
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
  if (!m_avatar) {
    return;
  }

  const QColor background = palette().color(QPalette::Midlight);
  const QColor text = palette().color(QPalette::ButtonText);
  const QColor border = palette().color(QPalette::WindowText);
  m_avatar->setStyleSheet(
      QStringLiteral("QLabel {"
                     "  background-color: %1;"
                     "  color: %2;"
                     "  border: 2px solid %3;"
                     "  border-radius: 18px;"
                     "  font-weight: bold;"
                     "  font-size: 14px;"
                     "}")
          .arg(background.name(), text.name(), border.name()));
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
  m_avatar->setText(avatarLetter(name));
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

void ContactRowWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (m_isSelf) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }

  QWidget *target = childAt(event->pos());
  while (target && target != this) {
    if (target == m_callBtn || target == m_chatBtn) {
      QWidget::mouseDoubleClickEvent(event);
      return;
    }
    target = target->parentWidget();
  }

  emit notesRequested(m_peer);
  event->accept();
}

void ContactRowWidget::refreshStatusDot()
{
  const QString color = presenceColor(m_presence);
  const QString borderColor = palette().color(QPalette::Window).name();
  m_statusDot->setStyleSheet(
      QStringLiteral("background:%1; border:1px solid %2; border-radius:5px;").arg(color, borderColor));
}
