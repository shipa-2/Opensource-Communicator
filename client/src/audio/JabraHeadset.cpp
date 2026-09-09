#include "audio/JabraHeadset.h"

#include <QLoggingCategory>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSocketNotifier>
#include <QTimer>

#include <fcntl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcJabra, "itl.jabra")

namespace itl {
namespace {

constexpr auto kJabraVendorFragment = "b0e/";
constexpr int kLongPressMs = 3000;

bool sendBits(int fd, unsigned char bits)
{
  const unsigned char report[3] = {0x02, bits, 0x00};
  const ssize_t written = ::write(fd, report, sizeof(report));
  return written == sizeof(report);
}

} // namespace

JabraHeadset &JabraHeadset::instance()
{
  static JabraHeadset s_instance;
  return s_instance;
}

JabraHeadset::JabraHeadset(QObject *parent)
    : QObject(parent)
{
}

bool JabraHeadset::ensureDevice()
{
  if (m_fd >= 0) {
    return true;
  }

  QDir hidrawDir(QStringLiteral("/sys/class/hidraw"));
  const auto entries = hidrawDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString &entry : entries) {
    QFile uevent(QStringLiteral("/sys/class/hidraw/%1/device/../../uevent").arg(entry));
    if (!uevent.open(QIODevice::ReadOnly)) {
      continue;
    }
    QByteArray product;
    for (const QByteArray &line : uevent.readAll().split('\n')) {
      if (line.startsWith("PRODUCT=")) {
        product = line.mid(8);
        break;
      }
    }
    uevent.close();
    if (!product.toLower().contains(QByteArrayLiteral(kJabraVendorFragment))) {
      continue;
    }

    const QString node = QStringLiteral("/dev/%1").arg(entry);
    int fd = ::open(node.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
      fd = ::open(node.toLocal8Bit().constData(), O_WRONLY | O_NONBLOCK);
    }
    if (fd < 0) {
      qCWarning(lcJabra) << "Found Jabra headset at" << node
                         << "but cannot open it; install the udev uaccess rule"
                            " (packaging/linux/udev) or add the device to your group";
      continue;
    }
    m_fd = fd;
    qCInfo(lcJabra) << "Jabra headset LED control via" << node;

    if (m_notifier) {
      m_notifier->setEnabled(false);
      m_notifier->deleteLater();
      m_notifier = nullptr;
    }
    if (::fcntl(fd, F_GETFL) != -1) {
      int flags = ::fcntl(fd, F_GETFL);
      if (flags != -1) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      }
      m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
      connect(m_notifier, &QSocketNotifier::activated, this, [this]() {
        m_notifier->setEnabled(false);
        pollInput();
        m_notifier->setEnabled(true);
      });
    }
    return true;
  }
  qCInfo(lcJabra) << "No Jabra hidraw device found";
  return false;
}

bool JabraHeadset::writeBits(unsigned char bits)
{
  if (!m_ledEnabled) {
    return false;
  }
  if (!ensureDevice()) {
    return false;
  }
  if (!sendBits(m_fd, bits)) {
    ::close(m_fd);
    m_fd = -1;
    if (ensureDevice()) {
      return sendBits(m_fd, bits);
    }
    return false;
  }
  return true;
}

void JabraHeadset::pollInput()
{
  unsigned char buf[64];
  ssize_t n;
  while ((n = ::read(m_fd, buf, sizeof(buf))) > 0) {
    if (n < 2 || buf[0] != 0x02) {
      continue;
    }
    qCDebug(lcJabra) << "input report" << Qt::hex << Qt::showbase << buf[1];
    const bool hook = buf[1] & 0x01;
    if (hook && !m_hookPressed) {
      m_hookPressed = true;
      if (!m_longPressTimer) {
        m_longPressTimer = new QTimer(this);
        m_longPressTimer->setSingleShot(true);
        connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
          m_hookPressed = false;
          emit hookLongPress();
        });
      }
      m_longPressTimer->start(kLongPressMs);
    } else if (!hook && m_hookPressed) {
      m_hookPressed = false;
      if (m_longPressTimer && m_longPressTimer->isActive()) {
        m_longPressTimer->stop();
        emit hookShortPress();
      }
    }
  }
}

void JabraHeadset::setLedEnabled(bool enabled)
{
  if (m_ledEnabled == enabled) {
    return;
  }
  m_ledEnabled = enabled;
  if (!enabled) {
    clear();
  }
}

void JabraHeadset::stopPulse()
{
  if (m_pulseTimer) {
    m_pulseTimer->stop();
  }
  m_pulseOn = false;
}

void JabraHeadset::showRinging()
{
  stopPulse();
  if (!m_pulseTimer) {
    m_pulseTimer = new QTimer(this);
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
      m_pulseOn = !m_pulseOn;
      writeBits(m_pulseOn ? 0x10 : 0x00);
    });
  }
  m_pulseOn = true;
  writeBits(0x10);
  m_pulseTimer->start(700);
}

void JabraHeadset::showInCall()
{
  stopPulse();
  writeBits(0x01);
}

void JabraHeadset::showHold()
{
  stopPulse();
  writeBits(0x05);
}

void JabraHeadset::clear()
{
  stopPulse();
  writeBits(0x00);
}

} // namespace itl

#else

namespace itl {

JabraHeadset &JabraHeadset::instance()
{
  static JabraHeadset s_instance;
  return s_instance;
}

JabraHeadset::JabraHeadset(QObject *parent)
    : QObject(parent)
{
}

void JabraHeadset::setLedEnabled(bool enabled)
{
  m_ledEnabled = enabled;
}

void JabraHeadset::showRinging() {}
void JabraHeadset::showInCall() {}
void JabraHeadset::showHold() {}
void JabraHeadset::clear() {}

} // namespace itl

#endif
