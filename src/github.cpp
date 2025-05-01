// Copyright (c) 2025-2025 Manuel Schneider

#include "github.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <albert/networkutil.h>
using namespace albert;
using namespace github;
using namespace util;

const auto api_base_url = "https://api.github.com";

// -------------------------------------------------------------------------------------------------

void RestApi::setBearerToken(const QString &bearer_token)
{ auth_header_ = "Bearer " + bearer_token.toUtf8(); }

static QNetworkRequest request(const QString &p, const QUrlQuery &q, const QByteArray &h)
{ return makeRestRequest(api_base_url, p, q, h); }

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
