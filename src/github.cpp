// Copyright (c) 2025-2025 Manuel Schneider

#include "github.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <albert/logging.h>
#include <albert/networkutil.h>
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace github;
using namespace std;

namespace
{
static const auto kerrors         = "errors"_L1;
static const auto kresource       = "resource"_L1;
static const auto kfield          = "field"_L1;
static const auto kcode           = "code"_L1;
static const auto oauth_auth_url  = u"https://github.com/login/oauth/authorize"_s;
static const auto oauth_scope     = u"notifications,read:org,read:user"_s;
static const auto oauth_token_url = u"https://github.com/login/oauth/access_token"_s;
}
// -------------------------------------------------------------------------------------------------


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

QNetworkRequest RestApi::request(const QString &path, const QUrlQuery &query) const
{
    QUrl url(u"https://api.github.com"_s);
    url.setPath(path);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    if (oauth.state() == OAuth2::State::Granted)
        request.setRawHeader("Authorization", "Bearer " + oauth.accessToken().toUtf8());

    return request;
}

// -------------------------------------------------------------------------------------------------

RestApi::RestApi()
{
    oauth.setAuthUrl(oauth_auth_url);
    oauth.setScope(oauth_scope);
    oauth.setTokenUrl(oauth_token_url);
    oauth.setRedirectUri("%1://github/"_L1.arg(qApp->applicationName()));
    oauth.setPkceEnabled(false);

    QObject::connect(&oauth, &OAuth2::tokensChanged, &oauth, [this] {
        if (oauth.error().isEmpty())
            DEBG << "Tokens updated.";
        else
            WARN << oauth.error();
    });
}

QNetworkReply *RestApi::user() const
{
    // https://docs.github.com/en/rest/users/users#get-the-authenticated-user
    return network().get(request(u"/user"_s, {}));
}

QNetworkReply *RestApi::notifications() const
{
    // https://docs.github.com/en/rest/activity/notifications#list-notifications-for-the-authenticated-user
    return network().get(request(u"/notifications"_s,
                                 {{u"all"_s, u"true"_s}}));
}

QNetworkReply *RestApi::searchUsers(const QString &query, int per_page, int page) const
{
    // https://docs.github.com/en/rest/search/search#search-users
    return network().get(request(u"/search/users"_s,
                                 {{u"q"_s, percentEncoded(query)},
                                  {u"per_page"_s, QString::number(per_page)},
                                  {u"page"_s, QString::number(page)}}));
}

QNetworkReply *RestApi::searchIssues(const QString &query, int per_page, int page) const
{
    // https://docs.github.com/en/rest/search/search#search-repositories
    return network().get(request(u"/search/issues"_s,
                                 {{u"q"_s, percentEncoded(query)},
                                  {u"per_page"_s, QString::number(per_page)},
                                  {u"page"_s, QString::number(page)},
                                  {u"advanced_search"_s, u"true"_s}}));
}

QNetworkReply *RestApi::searchRepositories(const QString &query, int per_page, int page) const
{
    // https://docs.github.com/en/rest/search/search#search-issues-and-pull-requests
    return network().get(request(u"/search/repositories"_s,
                                 {{u"q"_s, percentEncoded(query)},
                                  {u"per_page"_s, QString::number(per_page)},
                                  {u"page"_s, QString::number(page)}}));
}

QNetworkReply * RestApi::getLinkData(const QString &url) const
{ return network().get(request(url, {})); }

uint RestApi::rateLimit() const { return oauth.state() == OAuth2::State::Granted ? 2000 : 6000; }
