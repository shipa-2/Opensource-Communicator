#include "PresenceSelector.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

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
  m_combo->addItem(labelForStatus(QStringLiteral("offline")), QStringLiteral("offline"));

  layout->addWidget(m_combo, 1);

  connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
    refreshDot();
    emit currentIndexChanged(index);
  });

  refreshDot();
}

void PresenceSelector::setEnabled(bool enabled)
{
  QWidget::setEnabled(enabled);
  m_combo->setEnabled(enabled);
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
  const int index = m_combo->findData(status.toLower());
  if (index >= 0) {
    m_combo->setCurrentIndex(index);
  }
  refreshDot();
}

void PresenceSelector::refreshAppearance()
{
  refreshDot();
}

void PresenceSelector::refreshDot()
{
  const QString color = colorForStatus(currentStatus());
  const QString borderColor = palette().color(QPalette::Window).name();
  m_dot->setStyleSheet(
      QStringLiteral("background:%1; border:1px solid %2; border-radius:5px;").arg(color, borderColor));
}

QString PresenceSelector::colorForStatus(const QString &status)
{
  const QString value = status.toLower();
  if (value == QStringLiteral("online") || value == QStringLiteral("in-call")) {
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
