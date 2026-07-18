#include "PresenceSelector.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

PresenceSelector::PresenceSelector(QWidget *parent)
    : QWidget(parent)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  m_dot = new QLabel;
  m_dot->setFixedSize(10, 10);
  layout->addWidget(m_dot);

  m_combo = new QComboBox;
  m_combo->setObjectName(QStringLiteral("presenceCombo"));
  m_combo->addItem(labelForStatus(QStringLiteral("online")), QStringLiteral("online"));
  m_combo->addItem(labelForStatus(QStringLiteral("away")), QStringLiteral("away"));
  m_combo->addItem(labelForStatus(QStringLiteral("busy")), QStringLiteral("busy"));
  m_combo->addItem(labelForStatus(QStringLiteral("invisible")), QStringLiteral("invisible"));

  layout->addWidget(m_combo, 1);

  connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
    refreshDot();
    emit currentIndexChanged(index);
  });

  refreshDot();
}

void PresenceSelector::setManualInCallAllowed(bool allowed)
{
  if (allowed == m_inCallPersistent) {
    return;
  }

  m_inCallPersistent = allowed;
  const int index = m_combo->findData(QStringLiteral("in-call"));
  if (allowed) {
    if (index < 0) {
      m_combo->addItem(labelForStatus(QStringLiteral("in-call")), QStringLiteral("in-call"));
    }
    return;
  }

  if (index >= 0 && !m_inCall) {
    m_combo->removeItem(index);
  }
}

void PresenceSelector::setEnabled(bool enabled)
{
  QWidget::setEnabled(enabled);
  m_combo->setEnabled(enabled && !m_inCall);
}

int PresenceSelector::currentIndex() const
{
  return m_combo->currentIndex();
}

QString PresenceSelector::currentStatus() const
{
  return m_combo->currentData().toString();
}

void PresenceSelector::setCurrentStatus(const QString &status)
{
  if (m_inCall) {
    return;
  }
  QString normalized = status.toLower();
  if (normalized == QStringLiteral("offline")) {
    normalized = QStringLiteral("invisible");
  }
  const int index = m_combo->findData(normalized);
  if (index >= 0) {
    m_combo->setCurrentIndex(index);
  }
  refreshDot();
}

void PresenceSelector::setInCall(bool inCall)
{
  if (inCall == m_inCall) {
    return;
  }

  m_inCall = inCall;
  if (inCall) {
    m_savedIndex = m_combo->currentIndex();
    if (m_combo->findData(QStringLiteral("in-call")) < 0) {
      m_combo->insertItem(0, labelForStatus(QStringLiteral("in-call")), QStringLiteral("in-call"));
    }
    const int index = m_combo->findData(QStringLiteral("in-call"));
    if (index >= 0) {
      m_combo->setCurrentIndex(index);
    }
    m_combo->setEnabled(false);
  } else {
    if (!m_inCallPersistent) {
      const int index = m_combo->findData(QStringLiteral("in-call"));
      if (index >= 0) {
        m_combo->removeItem(index);
      }
    }
    if (m_savedIndex >= 0 && m_savedIndex < m_combo->count()) {
      const QString savedData = m_combo->itemData(m_savedIndex).toString();
      if (savedData == QStringLiteral("in-call") && m_inCallPersistent) {
        m_combo->setCurrentIndex(0);
      } else {
        m_combo->setCurrentIndex(m_savedIndex);
      }
    }
    m_savedIndex = -1;
    m_combo->setEnabled(QWidget::isEnabled());
  }
  refreshDot();
}

void PresenceSelector::refreshAppearance()
{
  refreshDot();
  if (m_combo) {
    m_combo->update();
  }
}

void PresenceSelector::setOpaquePopup(bool enabled, int alpha)
{
  Q_UNUSED(alpha);
  if (!m_combo) {
    return;
  }
  // Always restore native combo chrome. Custom QSS was breaking the system status look.
  Q_UNUSED(enabled);
  m_combo->setStyleSheet(QString());
  if (QStyle *appStyle = QApplication::style()) {
    m_combo->setStyle(appStyle);
  }
  m_combo->setPalette(QApplication::palette());
  if (QAbstractItemView *view = m_combo->view()) {
    view->setStyleSheet(QString());
    if (QStyle *appStyle = QApplication::style()) {
      view->setStyle(appStyle);
    }
    view->setAutoFillBackground(true);
    view->setAttribute(Qt::WA_TranslucentBackground, false);
    view->setAttribute(Qt::WA_StyledBackground, false);
    view->setPalette(QApplication::palette());
    if (QWidget *viewport = view->viewport()) {
      viewport->setStyleSheet(QString());
      viewport->setAutoFillBackground(true);
      viewport->setAttribute(Qt::WA_TranslucentBackground, false);
      viewport->setPalette(QApplication::palette());
    }
  }
  m_combo->style()->unpolish(m_combo);
  m_combo->style()->polish(m_combo);
  m_combo->update();
}

void PresenceSelector::refreshDot()
{
  const QString color = colorForStatus(currentStatus());
  const QString borderColor = QApplication::palette().color(QPalette::Window).name();
  m_dot->setStyleSheet(
      QStringLiteral("background:%1; border:1px solid %2; border-radius:5px;").arg(color, borderColor));
}

QString PresenceSelector::colorForStatus(const QString &status)
{
  const QString value = status.toLower();
  if (value == QStringLiteral("in-call")) {
    return QStringLiteral("#2880d4");
  }
  if (value == QStringLiteral("online")) {
    return QStringLiteral("#5a9e2f");
  }
  if (value == QStringLiteral("away")) {
    return QStringLiteral("#d4a017");
  }
  if (value == QStringLiteral("busy")) {
    return QStringLiteral("#c03030");
  }
  return QStringLiteral("#aaaaaa");
}

QString PresenceSelector::labelForStatus(const QString &status)
{
  if (status == QStringLiteral("in-call")) {
    return QObject::tr("Говорит по телефону");
  }
  if (status == QStringLiteral("online")) {
    return QObject::tr("На месте");
  }
  if (status == QStringLiteral("away")) {
    return QObject::tr("Нет на месте");
  }
  if (status == QStringLiteral("busy")) {
    return QObject::tr("Занят");
  }
  return QObject::tr("Не в сети");
}
