// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QObject>
#include <albert/asyncgeneratorqueryhandler.h>
#include <albert/ratelimiter.h>
#include <mutex>
class Plugin;
class QJsonArray;
class QNetworkReply;
namespace albert { class Item; }
namespace github { class RestApi; }

class GithubSearchHandler : public QObject, public albert::AsyncGeneratorQueryHandler
{
    Q_OBJECT

public:

    GithubSearchHandler(const QString &id,
                        const QString &name,
                        const QString &description,
                        const QString &defaultTrigger,
                        const github::RestApi&);

    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString defaultTrigger() const override;
    void setTrigger(const QString &t) override;
    albert::AsyncItemGenerator items(albert::QueryContext &) override;

    QString trigger();  // thread-safe

    std::vector<std::pair<QString, QString>> savedSearches() const;
    void setSavedSearches(const std::vector<std::pair<QString, QString>>&);

    virtual std::vector<std::pair<QString, QString>> defaultSearches() const = 0;
    virtual QNetworkReply *requestSearch(const QString &query, uint page) const = 0;
    virtual std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const = 0;

protected:
    const QString id_;
    const QString name_;
    const QString description_;
    const QString default_trigger_;
    const github::RestApi &api_;
    albert::detail::RateLimiter rate_limiter_;

    // Things accessesd by main and query threads
    mutable std::mutex mtx;
    QString trigger_;
    std::vector<std::pair<QString, QString>> saved_searches_;

signals:

    void savedSearchesChanged();

    friend class GithubQueryExecution;

};


class UserSearchHandler : public GithubSearchHandler
{
public:
    UserSearchHandler(const github::RestApi&);
    QNetworkReply *requestSearch(const QString &query, uint page) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};


class RepoSearchHandler : public GithubSearchHandler
{
public:
    RepoSearchHandler(const github::RestApi&);
    QNetworkReply *requestSearch(const QString &query, uint page) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};

class StarredRepoHandler : public GithubSearchHandler
{
public:
    StarredRepoHandler(const github::RestApi&);
    albert::AsyncItemGenerator items(albert::QueryContext &) override; // override because /user/starred returns array
    QNetworkReply *requestSearch(const QString &query, uint page) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};

class IssueSearchHandler : public GithubSearchHandler
{
public:
    IssueSearchHandler(const github::RestApi&);
    QNetworkReply *requestSearch(const QString &query, uint page) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};
