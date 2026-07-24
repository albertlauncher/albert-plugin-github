// Copyright (c) 2025-2026 Manuel Schneider

#pragma once
#include <albert/oauth.h>
#include <albert/ratelimiter.h>
#include <expected>
class QJsonDocument;
class QNetworkReply;
class QNetworkRequest;
class QString;
class QUrlQuery;

class API
{
public:
    API();

    /// Requiress `user` scope
    [[nodiscard]] QNetworkReply *user();

    /// Requires the `notifications` or `repo` scopes.
    [[nodiscard]] QNetworkReply *notifications();

    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply *searchUsers(const QString &query, int per_page, int page);

    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply *searchRepositories(const QString &query, int per_page, int page);

    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply *searchIssues(const QString &query, int per_page, int page);

    [[nodiscard]] QNetworkReply *getLinkData(const QString & url);

    static std::expected<QJsonDocument, QString> parseJson(QNetworkReply &reply);

    albert::OAuth2 oauth;
    albert::detail::RateLimiter rate_limiter;

private:

    QNetworkRequest request(const QString &, const QUrlQuery &);

};
