#pragma once

#include <QStringList>

namespace itl {

// Pauses other apps' music/video during phone calls (MPRIS on Linux).
class ExternalMediaPauser {
public:
  void pause();
  void resume();

private:
  int m_pauseDepth = 0;
  QStringList m_playingServices;

  static QStringList mprisServiceNames();
};

} // namespace itl
