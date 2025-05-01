// // Copyright (c) 2025-2025 Manuel Schneider

#include "items.h"
#include <albert/albert.h>
#include <albert/logging.h>
#include "plugin.h"
using namespace albert::util;
using namespace albert;
using namespace std;


GitHubItem::GitHubItem(Plugin &plugin,
                       const QJsonObject &json,
                       const QString &id,
                       const QString &title,
                       const QString &description,
                       const QString &account,
                       const QString &icon_url):
    plugin_(plugin),
    json_(json),
    id_(id),
    title_(title),
    description_(description),
    account_(account),
    icon_url_(icon_url) {}

QString GitHubItem::id() const { return id_; }

QString GitHubItem::text() const { return title_; }

QString GitHubItem::subtext() const { return description_; }

QStringList GitHubItem::iconUrls() const
{
    if (icon_urls_.isEmpty() && !icon_url_.isEmpty())  // lazy icon fetch, abuse icon_url_ as flag
    {
        if (const auto v = plugin_.downloadAvatar(account_, icon_url_);
            holds_alternative<QString>(v))
            icon_urls_ = {plugin_.makeMaskedIconUrl(get<QString>(v))};
        else
        {
            connect(get<FileDownloader*>(v), &FileDownloader::finished,
                    this, [this](bool success, const QString &path_or_error)
                    {
                        if (success)
                        {
                            icon_urls_ = {plugin_.makeMaskedIconUrl(path_or_error)};
                            for (auto observer : observers)
                                observer->notify(this);
                        }
                    });

        }
        icon_url_.clear(); // indicates that a download is awaited
    }
    return icon_urls_;
}

vector<Action> GitHubItem::actions() const
{
    const auto url = json_["html_url"].toString();
    return {{"open", tr("Show on GitHub"), [url] { openUrl(url); }}};
}

void GitHubItem::addObserver(Observer *observer) { observers.insert(observer); }

void GitHubItem::removeObserver(Observer *observer) { observers.erase(observer); }

shared_ptr<IssueItem> IssueItem::make(Plugin &plugin, const QJsonObject &obj)
{
    const auto repo_id  = obj["repository_url"].toString().section('/', -2);
    const auto number   = obj["number"].toInt();
    const auto issue_id = QStringLiteral("%1#%2").arg(repo_id).arg(number);

    const auto title    = obj["title"].toString();
    auto description    = obj["state"].toString().toUpper();

    if (const auto reactions = obj["reactions"];
        reactions["total_count"].toInt())
    {
        description.append(QStringLiteral(" ·"));

        static const std::array<pair<QString, QString>, 8> reactions_map{{{"+1", "👍"},
                                                                          {"-1", "👎"},
                                                                          {"laugh", "😄"},
                                                                          {"hooray", "🎉"},
                                                                          {"confused", "😕"},
                                                                          {"heart", "❤️"},
                                                                          {"rocket", "🚀"},
                                                                          {"eyes", "👀"}}};
        for (const auto &[key, emoji] : reactions_map)
            if (const auto v = reactions[key].toInt(); v)
                description.append(QStringLiteral(" %1%2").arg(emoji).arg(v));
    }

    description.append(QStringLiteral(" · %2 #%3").arg(repo_id).arg(number));

    // todo composed icon
    const auto author_account    = obj["user"]["login"].toString();
    const auto author_avatar_url = obj["user"]["avatar_url"].toString();
    // const auto repo_account      = repo_id.section('/', 0, 0);
    // const auto repo_name         = repo_id.section('/', 1, 1);
    // const auto url               = obj["html_url"].toString();

    return make_shared<IssueItem>(plugin, obj, issue_id, title, description,
                                  author_account, author_avatar_url);
}

shared_ptr<RepoItem> RepoItem::make(Plugin &plugin, const QJsonObject &obj)
{
    const auto account = obj["owner"]["login"].toString();
    const auto icon = obj["owner"]["avatar_url"].toString();

    const auto id = obj["full_name"].toString();

    const auto title = obj["name"].toString();

    QString description(account);
    if (const auto stars = obj["stargazers_count"].toInt(); stars)
        description.append(QStringLiteral(" ✨%1").arg(stars));
    if (const auto forks = obj["forks_count"].toInt(); forks)
        description.append(QStringLiteral(" 🍴%1").arg(forks));
    if (const auto issues = obj["open_issues_count"].toInt(); issues)
        description.append(QStringLiteral(" ⚠️%1").arg(issues));
    if (const auto desc = obj["description"].toString(); !desc.isEmpty())
        description.append(" · " + desc);

    return make_shared<RepoItem>(plugin, obj, id, title, description, account, icon);
}

vector<Action> RepoItem::actions() const
{
    auto actions = GitHubItem::actions();
    const auto url = json_["html_url"].toString();
    if (json_["has_issues"].toBool())
    {
        actions.emplace_back("oi", "Open issues", [=]{ openUrl(url + "/issues"); });
        actions.emplace_back("op", "Open pull requests", [=]{ openUrl(url + "/pulls"); });
    }
    if (json_["has_discussions"].toBool())
        actions.emplace_back("od", "Open discussions", [=]{ openUrl(url + "/discussions"); });
    if (json_["has_wiki"].toBool())
        actions.emplace_back("ow", "Open wiki", [=]{ openUrl(url + "/wiki"); });
    return actions;
}

shared_ptr<AccountItem> AccountItem::make(Plugin &plugin, const QJsonObject &obj)
{
    const auto login = obj["login"].toString();
    const auto avatar_url = obj["avatar_url"].toString();
    const auto type = obj["type"].toString();

    return make_shared<AccountItem>(plugin, obj, login, login, type, login, avatar_url);
}
