// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QObject>
#include <albert/globalqueryhandler.h>
#include <mutex>
class Plugin;
class QNetworkReply;
class QJsonArray;
namespace github { class RestApi; }


class GithubSearchHandler : public QObject,
                            public albert::GlobalQueryHandler
{
    Q_OBJECT

public:

    GithubSearchHandler(const github::RestApi&,
                        const QString &id,
                        const QString &name,
                        const QString &description,
                        const QString &defaultTrigger);
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString defaultTrigger() const override;
    void setTrigger(const QString &t) override;
    void handleThreadedQuery(ThreadedQuery &) override;
    std::vector<albert::RankItem> handleGlobalQuery(const albert::Query &) override;

    const QString &trigger();

    std::vector<std::pair<QString, QString>> savedSearches() const;
    void setSavedSearches(const std::vector<std::pair<QString, QString>>&);

    virtual std::vector<std::pair<QString, QString>> defaultSearches() const = 0;
    virtual QNetworkReply *requestSearch(const QString &) const = 0;
    virtual std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const = 0;

protected:
    const github::RestApi &api_;
    const QString id_;
    const QString name_;
    const QString description_;
    const QString default_trigger_;

    // Things accessesd by main and query threads
    mutable std::mutex mtx;
    QString trigger_;
    std::vector<std::pair<QString, QString>> saved_searches_;

signals:

    void savedSearchesChanged();

};


class UserSearchHandler : public GithubSearchHandler
{
public:
    UserSearchHandler(const github::RestApi&);
    QNetworkReply *requestSearch(const QString &) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};


class RepoSearchHandler : public GithubSearchHandler
{
public:
    RepoSearchHandler(const github::RestApi&);
    QNetworkReply *requestSearch(const QString &) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};


class IssueSearchHandler : public GithubSearchHandler
{
public:
    IssueSearchHandler(const github::RestApi&);
    QNetworkReply *requestSearch(const QString &) const override;
    std::shared_ptr<albert::Item> parseItem(const QJsonObject &) const override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};
