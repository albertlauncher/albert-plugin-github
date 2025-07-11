// // Copyright (c) 2025-2025 Manuel Schneider

#include "items.h"
#include <albert/albert.h>
#include <albert/logging.h>
#include <albert/download.h>
#include <albert/systemutil.h>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace std;


static QString makeMaskedIconUrl(const QString path)
{ return u"mask:?src=%1&radius=2"_s.arg(QString::fromUtf8(QUrl::toPercentEncoding(path))); }

GitHubItem::GitHubItem(const QString &id,
                       const QString &title,
                       const QString &description,
                       const QString &html_url,
                       const QString &remote_icon_url) :
    id_(id),
    title_(title),
    description_(description),
    html_url_(html_url),
    remote_icon_url_(remote_icon_url)
{
    moveToThread(qApp->thread());  // Signals wont work with affinity to a thread w/o loop
}

QString GitHubItem::id() const { return id_; }

QString GitHubItem::text() const { return title_; }

QString GitHubItem::subtext() const { return description_; }

QStringList GitHubItem::iconUrls() const
{
    if (local_icon_url_.isNull())  // lazy, first request
    {
        const auto icons_location = cacheLocation() / "github" / "icons";

        if (const auto icon_path = QDir(icons_location).filePath(id() + u".jpeg"_s);
            QFile::exists(icon_path))
            local_icon_url_ = makeMaskedIconUrl(icon_path);

        else if (!download_)
        {
            download_ = Download::unique(remote_icon_url_, icon_path);

            connect(download_.get(), &Download::finished, this, [=, this]{
                if (const auto error = download_->error();
                    error.isNull())
                    local_icon_url_ = makeMaskedIconUrl(download_->path());
                else
                {
                    WARN << "Failed to download icon:" << error;
                    local_icon_url_ = u":github"_s;
                }

                dataChanged();
            });
        }
    }
    return {local_icon_url_};  // awaiting if null
}

vector<Action> GitHubItem::actions() const
{
    return {{u"open"_s, tr("Show on GitHub"), [this] { openUrl(html_url_); }}};
}

// -------------------------------------------------------------------------------------------------

shared_ptr<UserItem> UserItem::fromJson(const QJsonObject &o)
{
    const auto id = o["login"_L1].toString();

    return make_shared<UserItem>(
        id,
        id,
        o["type"_L1].toString(),
        o["html_url"_L1].toString(),
        o["avatar_url"_L1].toString());
}

// -------------------------------------------------------------------------------------------------

static QString makeRepositoryDescription(const QJsonObject &o)
{
    QStringList tokens;
    if (const auto v = o["stargazers_count"_L1].toInt(); v)
        tokens << u"✨"_s + QString::number(v);
    if (const auto v = o["forks_count"_L1].toInt(); v)
        tokens << u"🍴"_s + QString::number(v);
    if (const auto v = o["open_issues_count"_L1].toInt(); v)
        tokens << u"⚠️"_s + QString::number(v);

    if (!tokens.isEmpty())
        tokens = {tokens.join(QChar::Space)};

    if (const auto d = o["description"_L1].toString();
        !d.isEmpty())
        tokens << d;

    return tokens.join(u" · "_s);
}

shared_ptr<RepositoryItem> RepositoryItem::fromJson(const QJsonObject &o)
{
    const auto id = o["full_name"_L1].toString();

    auto item = make_shared<RepositoryItem>(
        id,
        id,
        makeRepositoryDescription(o),
        o["html_url"_L1].toString(),
        o["owner"_L1]["avatar_url"_L1].toString());

    item->has_issues = o["has_issues"_L1].toBool();
    item->has_discussions = o["has_discussions"_L1].toBool();
    item->has_wiki = o["has_wiki"_L1].toBool();

    return item;
}

vector<Action> RepositoryItem::actions() const
{
    auto actions = GitHubItem::actions();

    if (has_issues)
    {
        actions.emplace_back(u"oi"_s, GitHubItem::tr("Open issues"),
                             [this]{ openUrl(html_url_ + u"/issues"_s); });

        actions.emplace_back(u"op"_s, GitHubItem::tr("Open pull requests"),
                             [this]{ openUrl(html_url_ + u"/pulls"_s); });
    }

    if (has_discussions)
        actions.emplace_back(u"od"_s, GitHubItem::tr("Open discussions"),
                             [this]{ openUrl(html_url_ + u"/discussions"_s); });

    if (has_wiki)
        actions.emplace_back(u"ow"_s, GitHubItem::tr("Open wiki"),
                             [this]{ openUrl(html_url_ + u"/wiki"_s); });

    return actions;
}

// -------------------------------------------------------------------------------------------------

static QString makeIssueDescription(const QJsonObject &o, const QString id)
{
    if (const auto reactions = o["reactions"_L1];
        reactions["total_count"_L1].toInt())
    {
        static const std::array<pair<QLatin1String, QString>, 8>
            reactions_map{{{"+1"_L1, u"👍"_s},
                           {"-1"_L1, u"👎"_s},
                           {"laugh"_L1, u"😄"_s},
                           {"hooray"_L1, u"🎉"_s},
                           {"confused"_L1, u"😕"_s},
                           {"heart"_L1, u"❤️"_s},
                           {"rocket"_L1, u"🚀"_s},
                           {"eyes"_L1, u"👀"_s}}};

        QStringList reaction_tokens;
        for (const auto &[key, emoji] : reactions_map)
            if (const auto c = reactions[key].toInt(); c)
                reaction_tokens << u"%1%2"_s.arg(emoji).arg(c);

        return u"%1 · %2 · %3"_s.arg(o["state"_L1].toString().toUpper(),
                                     reaction_tokens.join(QChar::Space),
                                     id);
    }
    else
        return u"%1 · %2"_s.arg(o["state"_L1].toString().toUpper(), id);
}

shared_ptr<IssueItem> IssueItem::fromJson(const QJsonObject &o)
{
    const auto id = u"%1#%2"_s
                        .arg(o["repository_url"_L1].toString().section(u'/', -2))
                        .arg(o["number"_L1].toInteger());

    return make_shared<IssueItem>(
        id,
        o["title"_L1].toString(),
        makeIssueDescription(o, id),
        o["html_url"_L1].toString(),
        o["user"_L1]["avatar_url"_L1].toString());
}

