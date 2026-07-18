#pragma once

#include <QJsonObject>
#include <QString>

namespace itl {

inline constexpr const char *kEmptyKey = "";

struct Payload {
    QJsonObject body;

    static Payload fromJson(const QJsonObject &obj) { return {obj}; }
    QJsonObject toJson() const { return body; }

    QString what() const { return body.value(QStringLiteral("What")).toString(); }
    void setWhat(const QString &v) { body.insert(QStringLiteral("What"), v); }

    int id() const { return body.value(QStringLiteral("id")).toInt(-1); }
    void setId(int v) { body.insert(QStringLiteral("id"), v); }

    QJsonValue command() const { return body.value(QString::fromUtf8(kEmptyKey)); }
    void setCommand(const QJsonValue &v) { body.insert(QString::fromUtf8(kEmptyKey), v); }
};

} // namespace itl
