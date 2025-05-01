// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QJsonObject>
#include <albert/item.h>
#include <memory>
#include <vector>
#include <set>
class Plugin;

class GitHubItem : public QObject, public albert::Item
{
    Q_OBJECT
public:
    GitHubItem(Plugin &plugin,
               const QJsonObject &json,
               const QString &id,
               const QString &title,
               const QString &description,
               const QString &account,
               const QString &icon_url);

    QString id() const override;
    QString text() const override;
    QString subtext() const override;
    QStringList iconUrls() const override;
    std::vector<albert::Action> actions() const override;

protected:

    void addObserver(Observer *observer) override;
    void removeObserver(Observer *observer) override;
    std::set<Item::Observer*> observers;

    Plugin &plugin_;
    QJsonObject json_;
    QString id_;
    QString title_;
    QString description_;
    QString account_;
    mutable QString icon_url_;
    mutable QStringList icon_urls_;
};


class IssueItem : public GitHubItem
{
public:
    using GitHubItem::GitHubItem;
    static std::shared_ptr<IssueItem> make(Plugin&, const QJsonObject&);
};


class RepoItem : public GitHubItem
{
public:
    using GitHubItem::GitHubItem;
    static std::shared_ptr<RepoItem> make(Plugin&, const QJsonObject&);
    std::vector<albert::Action> actions() const override;
};


class AccountItem : public GitHubItem
{
public:
    using GitHubItem::GitHubItem;
    static std::shared_ptr<AccountItem> make(Plugin&, const QJsonObject&);
};

