// Copyright (c) 2025-2026 Manuel Schneider

#pragma once
#include <QCoroAsyncGenerator>
#include <QCoroTask>
#include <albert/oauth.h>
#include <albert/ratelimiter.h>
#include <expected>
class QJsonArray;
class QJsonObject;
class QNetworkReply;
class QNetworkRequest;
class QString;
class QUrlQuery;

class API
{
public:
    API();

    QCoro::Task<std::expected<QJsonObject, QString>> user();

    QCoro::AsyncGenerator<std::expected<QJsonArray, QString>>
    searchIssues(const QString &query, int per_page);

    QCoro::AsyncGenerator<std::expected<QJsonArray, QString>>
    searchUsers(const QString &query, int per_page);

    QCoro::AsyncGenerator<std::expected<QJsonArray, QString>>
    searchRepositories(const QString &query, int per_page);

    albert::OAuth2 oauth;

private:
    QNetworkRequest makeRequest(const QString &path, const QUrlQuery &query) const;

    using RequestFn = QNetworkRequest (API::*)(const QString &, int, int) const;
    QCoro::AsyncGenerator<std::expected<QJsonArray, QString>>
    searchGenerator(RequestFn req_fn, QString query, int per_page);

    QNetworkRequest userRequest() const;
    QNetworkRequest searchUsersRequest(const QString &query, int per_page, int page) const;
    QNetworkRequest searchIssuesRequest(const QString &query, int per_page, int page) const;
    QNetworkRequest searchRepositoriesRequest(const QString &query, int per_page, int page) const;

    QNetworkReply *get(QNetworkRequest request);

    albert::detail::RateLimiter rate_limiter;


};
