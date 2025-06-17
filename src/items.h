// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include <QJsonObject>
#include <albert/item.h>
#include <memory>
#include <vector>
namespace albert::util { class Download; }

class GitHubItem : public QObject, public albert::detail::DynamicItem
{
    Q_OBJECT

public:

    GitHubItem(const QString &id,
               const QString &title,
               const QString &description,
               const QString &html_url,
               const QString &remote_icon_url);

    QString id() const override;
    QString text() const override;
    QString subtext() const override;
    QStringList iconUrls() const override;
    std::vector<albert::Action> actions() const override;

protected:

    const QString id_;
    const QString title_;
    const QString description_;
    const QString html_url_;
    const QString remote_icon_url_;
    mutable QString local_icon_url_;
    mutable std::shared_ptr<albert::util::Download> download_;
};


class UserItem : public GitHubItem
{
public:
    using GitHubItem::GitHubItem;
    static std::shared_ptr<UserItem> fromJson(const QJsonObject &);
};


class RepositoryItem : public GitHubItem
{
public:
    using GitHubItem::GitHubItem;
    static std::shared_ptr<RepositoryItem> fromJson(const QJsonObject &);
    std::vector<albert::Action> actions() const override;
private:
    bool has_issues;
    bool has_discussions;
    bool has_wiki;
};


class IssueItem : public GitHubItem
{
public:
    using GitHubItem::GitHubItem;
    static std::shared_ptr<IssueItem> fromJson(const QJsonObject &);
};
