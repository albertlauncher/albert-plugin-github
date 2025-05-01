// Copyright (c) 2025-2025 Manuel Schneider

#include "github.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <albert/networkutil.h>
using namespace albert::util;
using namespace albert;
using namespace github;
using namespace std;

const auto api_base_url = "https://api.github.com";

// -------------------------------------------------------------------------------------------------

void RestApi::setBearerToken(const QString &bearer_token)
{ auth_header_ = "Bearer " + bearer_token.toUtf8(); }

variant<QJsonDocument, QString> RestApi::parseJson(QNetworkReply *reply)
{
    reply->deleteLater();
    const QByteArray data = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (reply->error() == QNetworkReply::NoError) {
        if (parseError.error == QJsonParseError::NoError)
            return doc;
        return QString("JSON parse error: %1").arg(parseError.errorString());
    }

    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();
        QString message = obj.value("message").toString();
        if (obj.contains("errors") && obj["errors"].isArray()) {
            const QJsonArray errors = obj["errors"].toArray();
            for (const QJsonValue &val : errors) {
                if (val.isObject()) {
                    const QJsonObject err = val.toObject();
                    QString detail;
                    if (err.contains("resource") && err.contains("field") && err.contains("code")) {
                        detail = QString(" [%1:%2 - %3]")
                        .arg(err["resource"].toString(),
                             err["field"].toString(),
                             err["code"].toString());
                    }
                    message += detail;
                }
            }
        }
        return message.isEmpty() ? QString::fromUtf8(data) : message;
    }

    return QString("%1: %2").arg(reply->errorString(), QString::fromUtf8(data));
}

static QNetworkRequest request(const QString &p, const QUrlQuery &q, const QByteArray &h)
{ return makeRestRequest(api_base_url, p, q, h); }

// -------------------------------------------------------------------------------------------------

QNetworkReply *RestApi::user() const
{ return network().get(request("/user", {}, auth_header_)); }

QNetworkReply *RestApi::notifications() const
{ return network().get(request("/notifications", {{"all", "true"}}, auth_header_)); }

QNetworkReply *RestApi::searchUsers(const QString &query) const
{
    return network().get(request("/search/users",
                                 {{"q", QUrl::toPercentEncoding(query)}},
                                 auth_header_));
}

QNetworkReply *RestApi::searchIssues(const QString &query) const
{
    return network().get(request("/search/issues",
                                 {{"q", QUrl::toPercentEncoding(query)},
                                  {"advanced_search", "true"}},
                                 auth_header_));
}

QNetworkReply *RestApi::searchRepositories(const QString &query) const
{
    return network().get(request("/search/repositories",
                                 {{"q", QUrl::toPercentEncoding(query)}},
                                 auth_header_));
}

QNetworkReply * RestApi::getLinkData(const QString &url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    if (!auth_header_.isNull())
        request.setRawHeader("Authorization", auth_header_);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return network().get(request);
}
