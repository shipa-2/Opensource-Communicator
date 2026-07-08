#include "AudioDeviceUtils.h"

#include <QMediaDevices>

namespace itl::AudioDeviceUtils {

QList<QAudioDevice> inputDevices()
{
  return QMediaDevices::audioInputs();
}

QList<QAudioDevice> audioOutputDevices()
{
  return QMediaDevices::audioOutputs();
}

QString deviceId(const QAudioDevice &device)
{
  return QString::fromUtf8(device.id());
}

QAudioDevice findInputDevice(const QString &id)
{
  if (id.isEmpty()) {
    return QMediaDevices::defaultAudioInput();
  }
  for (const QAudioDevice &device : inputDevices()) {
    if (deviceId(device) == id) {
      return device;
    }
  }
  return QMediaDevices::defaultAudioInput();
}

QAudioDevice findOutputDevice(const QString &id)
{
  if (id.isEmpty()) {
    return QMediaDevices::defaultAudioOutput();
  }
  for (const QAudioDevice &device : audioOutputDevices()) {
    if (deviceId(device) == id) {
      return device;
    }
  }
  return QMediaDevices::defaultAudioOutput();
}

} // namespace itl::AudioDeviceUtils
