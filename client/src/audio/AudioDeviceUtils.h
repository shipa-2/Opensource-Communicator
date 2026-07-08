#pragma once

#include <QAudioDevice>
#include <QList>
#include <QString>

namespace itl::AudioDeviceUtils {

QList<QAudioDevice> inputDevices();
QList<QAudioDevice> audioOutputDevices();
QAudioDevice findInputDevice(const QString &id);
QAudioDevice findOutputDevice(const QString &id);
QString deviceId(const QAudioDevice &device);

} // namespace itl::AudioDeviceUtils
