// // Copyright (c) 2025-2025 Manuel Schneider

#include "items.h"
#include "plugin.h"
#include <albert/logging.h>
#include <albert/systemutil.h>
using namespace Qt::StringLiterals;
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
    const auto url = json_["html_url"_L1].toString();
    return {{u"open"_s, tr("Show on GitHub"), [url] { openUrl(url); }}};
}

void GitHubItem::addObserver(Observer *observer) { observers.insert(observer); }

void GitHubItem::removeObserver(Observer *observer) { observers.erase(observer); }

shared_ptr<IssueItem> IssueItem::make(Plugin &plugin, const QJsonObject &obj)
{
    const auto repo_id  = obj["repository_url"_L1].toString().section(u'/', -2);
    const auto number   = obj["number"_L1].toInt();
    const auto issue_id = u"%1#%2"_s.arg(repo_id).arg(number);

    const auto title    = obj["title"_L1].toString();
    auto description    = obj["state"_L1].toString().toUpper();

    if (const auto reactions = obj["reactions"_L1];
        reactions["total_count"_L1].toInt())
    {
        description.append(u" ·"_s);

        static const std::array<pair<QLatin1String, QString>, 8> reactions_map{
            {{"+1"_L1, u"👍"_s},
             {"-1"_L1, u"👎"_s},
             {"laugh"_L1, u"😄"_s},
             {"hooray"_L1, u"🎉"_s},
             {"confused"_L1, u"😕"_s},
             {"heart"_L1, u"❤️"_s},
             {"rocket"_L1, u"🚀"_s},
             {"eyes"_L1, u"👀"_s}}};

        for (const auto &[key, emoji] : reactions_map)
            if (const auto v = reactions[key].toInt(); v)
                description.append(u" %1%2"_s.arg(emoji).arg(v));
    }

    description.append(u" · %2 #%3"_s.arg(repo_id).arg(number));

    // todo composed icon
    const auto author_account    = obj["user"_L1]["login"_L1].toString();
    const auto author_avatar_url = obj["user"_L1]["avatar_url"_L1].toString();
    // const auto repo_account      = repo_id.section('/', 0, 0);
    // const auto repo_name         = repo_id.section('/', 1, 1);
    // const auto url               = obj["html_url"].toString();

    return make_shared<IssueItem>(plugin, obj, issue_id, title, description,
                                  author_account, author_avatar_url);
}

shared_ptr<RepoItem> RepoItem::make(Plugin &plugin, const QJsonObject &obj)
{
    const auto account = obj["owner"_L1]["login"_L1].toString();
    const auto icon = obj["owner"_L1]["avatar_url"_L1].toString();

    const auto id = obj["full_name"_L1].toString();

    const auto title = obj["name"_L1].toString();

    QString description(account);
    if (const auto stars = obj["stargazers_count"_L1].toInt(); stars)
        description.append(u" ✨%1"_s.arg(stars));
    if (const auto forks = obj["forks_count"_L1].toInt(); forks)
        description.append(u" 🍴%1"_s.arg(forks));
    if (const auto issues = obj["open_issues_count"_L1].toInt(); issues)
        description.append(u" ⚠️%1"_s.arg(issues));
    if (const auto desc = obj["description"_L1].toString(); !desc.isEmpty())
        description.append(u" · "_s + desc);

    return make_shared<RepoItem>(plugin, obj, id, title, description, account, icon);
}

vector<Action> RepoItem::actions() const
{
    auto actions = GitHubItem::actions();
    const auto url = json_["html_url"_L1].toString();
    if (json_["has_issues"_L1].toBool())
    {
        actions.emplace_back(u"oi"_s, u"Open issues"_s, [=]{ openUrl(url + u"/issues"_s); });
        actions.emplace_back(u"op"_s, u"Open pull requests"_s, [=]{ openUrl(url + u"/pulls"_s); });
    }
    if (json_["has_discussions"_L1].toBool())
        actions.emplace_back(u"od"_s, u"Open discussions"_s, [=]{ openUrl(url + u"/discussions"_s); });
    if (json_["has_wiki"_L1].toBool())
        actions.emplace_back(u"ow"_s, u"Open wiki"_s, [=]{ openUrl(url + u"/wiki"_s); });
    return actions;
}

shared_ptr<AccountItem> AccountItem::make(Plugin &plugin, const QJsonObject &obj)
{
    const auto login = obj["login"_L1].toString();
    const auto avatar_url = obj["avatar_url"_L1].toString();
    const auto type = obj["type"_L1].toString();

    return make_shared<AccountItem>(plugin, obj, login, login, type, login, avatar_url);
}
