// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QByteArray>
class QNetworkReply;
class QString;

namespace github
{

class RestApi
{
    QByteArray auth_header_;

public:

    void setBearerToken(const QString&);

    /// https://docs.github.com/en/rest/users/users#get-the-authenticated-user
    /// Requiress ``user`` scope
    [[nodiscard]] QNetworkReply *user() const;

    /// https://docs.github.com/en/rest/activity/notifications#list-notifications-for-the-authenticated-user
    /// Requires the ``notifications`` or ``repo`` scopes.
    [[nodiscard]] QNetworkReply *notifications() const;

    /// https://docs.github.com/en/rest/search/search#search-users
    /// Requires no scopes (if public data is sufficient)
    [[nodiscard]] QNetworkReply *searchUsers(const QString &query) const;

    /// https://docs.github.com/en/rest/search/search#search-issues-and-pull-requests
    [[nodiscard]] QNetworkReply *searchIssues(const QString &query) const;

    /// https://docs.github.com/en/rest/search/search#search-repositories
    [[nodiscard]] QNetworkReply *searchRepositories(const QString &query) const;

    [[nodiscard]] QNetworkReply *getLinkData(const QString & url) const;

};

}
