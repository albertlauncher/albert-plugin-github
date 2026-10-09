// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QObject>
#include <albert/asyncgeneratorqueryhandler.h>
#include <mutex>
class API;

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

protected:
    const QString id_;
    const QString name_;
    const QString description_;
    const QString default_trigger_;
    API &api_;

    // Things accessesd by main and query threads
    mutable std::mutex mtx;
    QString trigger_;
};


class UserSearchHandler : public GithubSearchHandler
{
public:
    UserSearchHandler(API&);
    albert::AsyncItemGenerator items(albert::QueryContext&) override;
};


class RepoSearchHandler : public GithubSearchHandler
{
public:
    RepoSearchHandler(API&);
    albert::AsyncItemGenerator items(albert::QueryContext&) override;
};


class IssueSearchHandler : public GithubSearchHandler
{
public:
    IssueSearchHandler(API&);
    albert::AsyncItemGenerator items(albert::QueryContext&) override;
};
