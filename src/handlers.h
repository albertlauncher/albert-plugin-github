// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QObject>
#include <albert/asyncgeneratorqueryhandler.h>
#include <mutex>
class Plugin;
class QJsonArray;
class QNetworkReply;
class API;
namespace albert { class Item; }

class GithubSearchHandler : public QObject, public albert::AsyncGeneratorQueryHandler
{
    Q_OBJECT

public:
    GithubSearchHandler(const QString &id,
                        const QString &name,
                        const QString &description,
                        const QString &defaultTrigger,
                        API &);

    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString defaultTrigger() const override;
    void setTrigger(const QString &t) override;

    QString trigger();  // thread-safe

    std::vector<std::pair<QString, QString>> savedSearches() const;
    void setSavedSearches(const std::vector<std::pair<QString, QString>>&);

    virtual std::vector<std::pair<QString, QString>> defaultSearches() const = 0;

protected:
    const QString id_;
    const QString name_;
    const QString description_;
    const QString default_trigger_;
    API &api_;

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
    UserSearchHandler(API&);
    albert::AsyncItemGenerator items(albert::QueryContext&) override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};


class RepoSearchHandler : public GithubSearchHandler
{
public:
    RepoSearchHandler(API&);
    albert::AsyncItemGenerator items(albert::QueryContext&) override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};


class IssueSearchHandler : public GithubSearchHandler
{
public:
    IssueSearchHandler(API&);
    albert::AsyncItemGenerator items(albert::QueryContext&) override;
    std::vector<std::pair<QString, QString>> defaultSearches() const override;
};
