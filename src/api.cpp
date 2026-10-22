// Copyright (c) 2025-2025 Manuel Schneider

#include "api.h"
#include <QCoreApplication>
#include <QCoro>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <albert/logging.h>
#include <albert/networkutil.h>
using namespace QCoro;
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

namespace
{
static const auto oauth_auth_url  = u"https://github.com/login/oauth/authorize"_s;
static const auto oauth_scope     = u"notifications,read:org,read:user"_s;
static const auto oauth_token_url = u"https://github.com/login/oauth/access_token"_s;

static expected<QJsonObject, QString> handleRestReply(QNetworkReply &reply)
{
    const auto data = reply.readAll();

    if (reply.error() != QNetworkReply::NoError)
        return unexpected(reply.errorString() + QString::fromUtf8(data));

    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(data, &parse_error);

    if (parse_error.error != QJsonParseError::NoError)
        return unexpected(u"JSON parse error: %1"_s.arg(parse_error.errorString()));

    else if (!doc.isObject())
        return unexpected(u"Unexpected JSON format: %1"_s.arg(QString::fromUtf8(doc.toJson())));

    else
        return doc.object();
}


}

// -------------------------------------------------------------------------------------------------

API::API()
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

QNetworkRequest API::makeRequest(const QString &path, const QUrlQuery &query) const
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

QNetworkReply *API::get(QNetworkRequest request)
{
    rate_limiter.limit(oauth.state() == OAuth2::State::Granted ? 2000ms : 6000ms);
    DEBG << "Fetch" << request.url();
    return network().get(request);
}

// -------------------------------------------------------------------------------------------------

QNetworkRequest API::userRequest() const
{
    // https://docs.github.com/en/rest/users/users#get-the-authenticated-user
    return makeRequest(u"/user"_s, {});
}

QNetworkRequest API::searchUsersRequest(const QString &query, int per_page, int page) const
{
    // https://docs.github.com/en/rest/search/search#search-users
    return makeRequest(u"/search/users"_s,
                       {{u"q"_s, percentEncoded(query)},
                        {u"per_page"_s, QString::number(per_page)},
                        {u"page"_s, QString::number(page)}});
}

QNetworkRequest API::searchIssuesRequest(const QString &query, int per_page, int page) const
{
    // https://docs.github.com/en/rest/search/search#search-issues-and-pull-requests
    return makeRequest(u"/search/issues"_s,
                       {{u"q"_s, percentEncoded(query)},
                        {u"per_page"_s, QString::number(per_page)},
                        {u"page"_s, QString::number(page)},
                        {u"advanced_search"_s, u"true"_s}});
}

QNetworkRequest API::searchRepositoriesRequest(const QString &query, int per_page, int page) const
{
    // https://docs.github.com/en/rest/search/search#search-repositories
    return makeRequest(u"/search/repositories"_s,
                       {{u"q"_s, percentEncoded(query)},
                        {u"per_page"_s, QString::number(per_page)},
                        {u"page"_s, QString::number(page)}});
}

// -------------------------------------------------------------------------------------------------

QCoro::Task<std::expected<QJsonObject, QString>> API::user()
{
    co_await rate_limiter.acquire();
    unique_ptr<QNetworkReply> reply(get(userRequest()));
    co_await qCoro(reply.get()).waitForFinished();  // TODO: QCoro>13 QCoroNetworkReply
    co_return handleRestReply(*reply);
}

AsyncGenerator<expected<QJsonArray, QString>>
API::searchGenerator(RequestFn req_fn, QString query, int per_page)
{
    for (auto page = 1;; ++page)
    {
        co_await rate_limiter.acquire();

        unique_ptr<QNetworkReply> reply(get(invoke(req_fn, this, query, per_page, page)));

        co_await qCoro(reply.get()).waitForFinished();  // TODO: QCoro>13 QCoroNetworkReply

        const auto obj = handleRestReply(*reply);
        if (!obj)
        {
            auto lval = unexpected(obj.error());
            co_yield lval;  // TODO: GCC>13 yieling temporaries is fine
            co_return;
        }

        auto lval = (*obj)["items"_L1].toArray();
        co_yield lval;  // TODO: GCC>13 yieling temporaries is fine

        if (page * per_page >= (*obj)["total_count"_L1].toInt())
            co_return;
    }
}

AsyncGenerator<expected<QJsonArray, QString>>
API::searchIssues(const QString &query, int per_page)
{ return searchGenerator(&API::searchIssuesRequest, query, per_page); }

AsyncGenerator<expected<QJsonArray, QString>>
API::searchRepositories(const QString &query, int per_page)
{ return searchGenerator(&API::searchRepositoriesRequest, query, per_page); }

AsyncGenerator<expected<QJsonArray, QString>>
API::searchUsers(const QString &query, int per_page)
{ return searchGenerator(&API::searchUsersRequest, query, per_page); }
