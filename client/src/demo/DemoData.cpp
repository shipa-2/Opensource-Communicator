#include "DemoData.h"

#include "chat/ChatManager.h"

#include <QDateTime>

namespace itl::DemoData {

namespace {
QString normalizedLogin(QString login)
{
  login = login.trimmed();
  const int at = login.indexOf(QLatin1Char('@'));
  if (at > 0) {
    login = login.left(at);
  }
  return login.toLower();
}
} // namespace

bool isDemoCredentials(const QString &login, const QString &password)
{
  return normalizedLogin(login) == QStringLiteral("demo") && password == QStringLiteral("demo");
}

QString demoDomain()
{
  return QStringLiteral("demo.local");
}

QList<DemoContact> contacts()
{
  const QString domain = demoDomain();
  return {
      {QStringLiteral("demo@") + domain, QStringLiteral("Демо-пользователь"), QStringLiteral("100"), {}, QStringLiteral("online"), true},
      {QStringLiteral("ivan@") + domain, QStringLiteral("Иван Петров"), QStringLiteral("702"), {}, QStringLiteral("online"), false},
      {QStringLiteral("maria@") + domain, QStringLiteral("Мария Сидорова"), QStringLiteral("705"), {}, QStringLiteral("away"), false},
      {QStringLiteral("admin@") + domain, QStringLiteral("Администратор"), QStringLiteral("701"), {}, QStringLiteral("online"), false},
      {QStringLiteral("+79991234567"), QStringLiteral("Клиент VIP"), {}, QStringLiteral("+79991234567"), {}, false},
  };
}

QList<CallHistoryEntry> callHistory()
{
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const QString domain = demoDomain();
  return {
      {QStringLiteral("ivan@") + domain, QStringLiteral("Иван Петров"), QStringLiteral("outgoing"), now - 3600000,
       now - 3580000, now - 3400000, 180, true, QStringLiteral("connected"), {}, {}},
      {QStringLiteral("maria@") + domain, QStringLiteral("Мария Сидорова"), QStringLiteral("incoming"), now - 86400000,
       now - 86380000, now - 86350000, 30, true, QStringLiteral("connected"), {}, {}},
      {QStringLiteral("+79991234567"), QStringLiteral("Клиент VIP"), QStringLiteral("incoming"), now - 172800000, 0,
       now - 172790000, 0, false, QStringLiteral("no-answer"), {}, QStringLiteral("Иван Петров <ivan@") + domain + QLatin1Char('>')},
      {QStringLiteral("705"), QStringLiteral("Мария Сидорова"), QStringLiteral("outgoing"), now - 7200000, now - 7180000,
       now - 7000000, 120, true, QStringLiteral("connected"), {}, QStringLiteral("Демо-пользователь <100>"),
       QStringLiteral("100@") + domain, QStringLiteral("705@") + domain, true},
  };
}

void seedChatMessages(ChatManager *chat)
{
  if (!chat) {
    return;
  }

  chat->clearDemoMessages();
  const QString domain = demoDomain();
  const QString ivanPeer = QStringLiteral("ivan@") + domain;
  chat->addDemoMessage(ivanPeer, QStringLiteral("Привет! Это демо-чат."), true, false);
  chat->addDemoMessage(ivanPeer, QStringLiteral("Можно проверить интерфейс без подключения к ВАТС."), true, false);
  chat->addDemoMessage(ivanPeer, QStringLiteral("Понял, спасибо!"), false, false);
}

} // namespace itl::DemoData
