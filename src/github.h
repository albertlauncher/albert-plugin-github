// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <albert/oauth.h>
class QJsonDocument;
class QNetworkReply;
class QNetworkRequest;
class QString;
class QUrlQuery;

namespace github
{

class RestApi
{
public:

    RestApi();

    uint rateLimit() const;

    /// Requiress ``user`` scope
    [[nodiscard]] QNetworkReply *user() const;

    /// Requires the ``notifications`` or ``repo`` scopes.
    [[nodiscard]] QNetworkReply *notifications() const;

    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply* searchUsers(const QString &query,
                                             int per_page = 100,
                                             int page = 1) const;

    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply* searchRepositories(const QString &query,
                                                    int per_page = 100,
                                                    int page = 1) const;

    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply* searchIssues(const QString &query,
                                              int per_page = 100,
                                              int page = 1) const;

    [[nodiscard]] QNetworkReply *getLinkData(const QString & url) const;

    static std::variant<QJsonDocument, QString> parseJson(QNetworkReply *reply);

    albert::OAuth2 oauth;

private:

    QNetworkRequest request(const QString &, const QUrlQuery &) const;

};








}
