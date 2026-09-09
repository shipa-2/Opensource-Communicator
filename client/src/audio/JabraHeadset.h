#pragma once

#include <QObject>

class QTimer;
class QSocketNotifier;

namespace itl {

// Jabra headset bridge over raw hidraw (Telephony HID collection, report 0x02).
//
// Output (LEDs): bit0 Off-Hook ("in call"), bit4 Hold, bit5 Microphone, etc. —
// the firmware renders the actual patterns from these state bits.
// Input: bit0 Hook Switch — the big multi-function button asserts it.
// A short press answers (or ends) a call; holding it for 3 s rejects.
//
// LED writes require access to /dev/hidraw (udev uaccess rule); without it the
// indication silently stays off, button monitoring keeps working.
class JabraHeadset : public QObject
{
    Q_OBJECT

public:
    static JabraHeadset &instance();

    void setLedEnabled(bool enabled);
    bool ledEnabled() const { return m_ledEnabled; }

    void showRinging(); // silent red flash pulses (incoming/outgoing setup)
    void showInCall();  // steady red
    void showHold();    // hold: red + headset volume + mic off
    void clear();

signals:
    void hookShortPress(); // big button released quickly
    void hookLongPress();  // big button held for 3 s

private:
    explicit JabraHeadset(QObject *parent = nullptr);
    bool ensureDevice();
    bool writeBits(unsigned char bits);
    void stopPulse();
    void pollInput();

    bool m_ledEnabled = true;
    int m_fd = -1;
    class QTimer *m_pulseTimer = nullptr;
    bool m_pulseOn = false;
    class QSocketNotifier *m_notifier = nullptr;
    bool m_hookPressed = false;
    class QTimer *m_longPressTimer = nullptr;
};

} // namespace itl
