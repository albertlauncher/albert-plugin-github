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
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace github;
using namespace std;

namespace
{
static const auto kerrors = "errors"_L1;
static const auto kresource = "resource"_L1;
static const auto kfield = "field"_L1;
static const auto kcode = "code"_L1;
}
// -------------------------------------------------------------------------------------------------

void RestApi::setBearerToken(const QString &bearer_token)
{ auth_header_ = "Bearer " + bearer_token.toUtf8(); }

variant<QJsonDocument, QString> RestApi::parseJson(QNetworkReply *reply)
{
    reply->deleteLater();
    const QByteArray data = reply->readAll();

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);

    if (reply->error() == QNetworkReply::NoError)
    {
        if (parseError.error == QJsonParseError::NoError)
            return doc;
        return u"JSON parse error: %1"_s.arg(parseError.errorString());
    }

    if (parseError.error == QJsonParseError::NoError && doc.isObject())
    {
        const QJsonObject obj = doc.object();

        QString message = obj["message"_L1].toString();
        if (obj.contains(kerrors) && obj[kerrors].isArray())
            for (const QJsonValue &val : obj[kerrors].toArray())
                if (val.isObject())
                    if (const QJsonObject err = val.toObject();
                        err.contains(kresource) && err.contains(kfield) && err.contains(kcode))
                        message += u" [%1:%2 - %3]"_s
                                       .arg(err[kresource].toString(),
                                            err[kfield].toString(),
                                            err[kcode].toString());

        return message.isEmpty() ? QString::fromUtf8(data) : message;
    }

    return u"%1: %2"_s.arg(reply->errorString(), QString::fromUtf8(data));
}

static QNetworkRequest request(const QString &p, const QUrlQuery &q, const QByteArray &h)
{ return makeRestRequest(u"https://api.github.com"_s, p, q, h); }

// -------------------------------------------------------------------------------------------------

QNetworkReply *RestApi::user() const
{
    return network().get(request(u"/user"_s,
                                 {},
                                 auth_header_));
}

QNetworkReply *RestApi::notifications() const
{
    return network().get(request(u"/notifications"_s,
                                 {
                                  {u"all"_s, u"true"_s}
                                 },
                                 auth_header_));
}

QNetworkReply *RestApi::searchUsers(const QString &query) const
{
    return network().get(request(u"/search/users"_s,
                                 {
                                  {u"q"_s, QString::fromUtf8(QUrl::toPercentEncoding(query))}
                                 },
                                 auth_header_));
}

QNetworkReply *RestApi::searchIssues(const QString &query) const
{
    return network().get(request(u"/search/issues"_s,
                                 {
                                  {u"q"_s, QString::fromUtf8(QUrl::toPercentEncoding(query))},
                                  {u"advanced_search"_s, u"true"_s}
                                 },
                                 auth_header_));
}

QNetworkReply *RestApi::searchRepositories(const QString &query) const
{
    return network().get(request(u"/search/repositories"_s,
                                 {
                                  {u"q"_s, QString::fromUtf8(QUrl::toPercentEncoding(query))}
                                 },
                                 auth_header_));
}

QNetworkReply * RestApi::getLinkData(const QString &url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    if (!auth_header_.isNull())
        request.setRawHeader("Authorization", auth_header_);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json"_L1);
    return network().get(request);
}
