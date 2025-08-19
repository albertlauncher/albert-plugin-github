// Copyright (c) 2025-2025 Manuel Schneider

#include "github.h"
#include "handlers.h"
#include "items.h"
#include "plugin.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <albert/albert.h>
#include <albert/logging.h>
#include <albert/matcher.h>
#include <albert/networkutil.h>
#include <albert/standarditem.h>
#include <albert/systemutil.h>
#include <ranges>
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace github;
using namespace std;

GithubSearchHandler::GithubSearchHandler(const RestApi &api,
                                         const QString &id,
                                         const QString &name,
                                         const QString &description,
                                         const QString &defaultTrigger) :
    api_(api),
    id_(id),
    name_(name),
    description_(description),
    default_trigger_(defaultTrigger)
{}

QString GithubSearchHandler::id() const { return id_; }

QString GithubSearchHandler::name() const { return name_; }

QString GithubSearchHandler::description() const { return description_; }

QString GithubSearchHandler::defaultTrigger() const { return default_trigger_ + QChar::Space; }

void GithubSearchHandler::setTrigger(const QString &t)
{
    lock_guard lock(mtx);
    trigger_ = t;
}

static auto makeErrorItem(const QString &error)
{
    auto icon = QStringList{u"comp:?src1=%3Agithub&src2=qsp%3ASP_MessageBoxWarning"_s};
    WARN << error;
    return StandardItem::make(u"notify"_s, u"GitHub"_s, error, ::move(icon));
}

void GithubSearchHandler::handleTriggerQuery(albert::Query &q)
{
    if (q.string().isEmpty())
        GlobalQueryHandler::handleTriggerQuery(q);

    else if (static auto limiter = albert::detail::RateLimiter(api_.rateLimit());
             !limiter.debounce(q.isValid()))
        return;

    else if (const auto var = RestApi::parseJson(await(requestSearch(q)));
             holds_alternative<QString>(var))
        q.add(makeErrorItem(get<QString>(var)));

    else
    {
        const auto v = get<QJsonDocument>(var)["items"_L1].toArray()
                       | views::transform([this](const auto& val) { return parseItem(val.toObject()); });
        vector<shared_ptr<Item>> items{v.begin(), v.end()};
        q.add(items);  // Todo ranges add
    }
}

vector<pair<QString, QString>> GithubSearchHandler::savedSearches() const
{
    lock_guard lock(mtx);
    return saved_searches_;
}

void GithubSearchHandler::setSavedSearches(const vector<pair<QString, QString>> &saved_searches)
{
    bool notify = false;  // avoid holding lock on emit

    if (lock_guard lock(mtx);
        saved_searches != saved_searches_)
    {
        saved_searches_ = saved_searches;
        notify = true;
    }
    if (notify)
        emit savedSearchesChanged();
}

vector<RankItem> GithubSearchHandler::handleGlobalQuery(const Query &query)
{
    std::lock_guard lock(mtx);
    vector<RankItem> r;
    Matcher matcher(query);
    for (const auto &[t, q] : saved_searches_)
        if (auto m = matcher.match(t); m)
        {
            auto _q = trigger_ + q;

            vector<Action> actions;

            actions.emplace_back(
                u"show"_s, Plugin::tr("Show"),
                [=]{
                    show(_q + QChar::Space);
                },
                false
                );

            actions.emplace_back(
                u"github"_s, Plugin::tr("Show on GitHub"),
                [=]{
                    auto percEncQuery = QString::fromUtf8(QUrl::toPercentEncoding(q));
                    openUrl(u"https://github.com/issues?q="_s + percEncQuery);
                });

            auto icon = QStringList{u":github"_s};
            r.emplace_back(StandardItem::make(t, t, ::move(_q), ::move(icon), ::move(actions)), m);
        }
    return r;
}

//--------------------------------------------------------------------------------------------------

UserSearchHandler::UserSearchHandler(const github::RestApi &api):
    GithubSearchHandler(api,
                        u"github.users"_s,
                        Plugin::tr("GitHub users"),
                        Plugin::tr("Search GitHub users"),
                        u"ghu"_s)
{}

QNetworkReply *UserSearchHandler::requestSearch(const QString &q) const
{ return api_.searchUsers(q); }

shared_ptr<Item> UserSearchHandler::parseItem(const QJsonObject &o) const
{ return UserItem::fromJson(o); }

vector<pair<QString, QString>> UserSearchHandler::defaultSearches() const { return {}; }

//--------------------------------------------------------------------------------------------------

RepoSearchHandler::RepoSearchHandler(const github::RestApi &api):
    GithubSearchHandler(api,
                        u"github.repositories"_s,
                        Plugin::tr("GitHub repositories"),
                        Plugin::tr("Search GitHub repositories"),
                        u"ghr"_s)
{}

QNetworkReply *RepoSearchHandler::requestSearch(const QString &q) const
{ return api_.searchRepositories(q); }

shared_ptr<Item> RepoSearchHandler::parseItem(const QJsonObject &o) const
{ return RepositoryItem::fromJson(o); }

vector<pair<QString, QString>> RepoSearchHandler::defaultSearches() const
{
    return {
        {
            Plugin::tr("My repositories"),
            u"sort:updated-desc fork:true user:@me"_s
        },
        {
            Plugin::tr("Albert repositories"),
            u"sort:updated-desc fork:true archived:false org:albertlauncher"_s
        },
        {
            Plugin::tr("Archived Albert repositories"),
            u"sort:updated-desc fork:true archived:true org:albertlauncher"_s
        }
    };
}

//--------------------------------------------------------------------------------------------------

IssueSearchHandler::IssueSearchHandler(const github::RestApi &api):
    GithubSearchHandler(api,
                        u"github.issues"_s,
                        Plugin::tr("GitHub issues"),
                        Plugin::tr("Search GitHub issues"),
                        u"ghi"_s)
{}

QNetworkReply *IssueSearchHandler::requestSearch(const QString &q) const
{ return api_.searchIssues(q); }

shared_ptr<Item> IssueSearchHandler::parseItem(const QJsonObject &o) const
{ return IssueItem::fromJson(o); }

vector<pair<QString, QString>> IssueSearchHandler::defaultSearches() const
{
    return {
        {Plugin::tr("Assigned issues"),        u"is:open is:issue assignee:@me"_s},
        {Plugin::tr("Created issues"),         u"is:open is:issue author:@me"_s},
        {Plugin::tr("Albert issues"),          u"is:open is:issue org:albertlauncher"_s},
        {Plugin::tr("Assigned pull requests"), u"is:open is:pr assignee:@me"_s},
        {Plugin::tr("Created pull requests"),  u"is:open is:pr author:@me"_s},
        {Plugin::tr("Albert pull requests"),   u"is:open is:pr org:albertlauncher"_s},
        {Plugin::tr("Review requests"),        u"is:open is:pr review-requested:@me"_s},
        {Plugin::tr("Mentions"),               u"mentions:@me"_s},
        {Plugin::tr("Recent activity"),        u"involves:@me"_s}
    };
}




























// else if (prefix == u"n"_s)
// {
//     if (!debounce(query.isValid()))
//         return;

//     if (const auto var = parseJson(await(api.notifications()));
//         holds_alternative<QString>(var))
//         query.add(createErrorItem(get<QString>(var)));
//     else
//     {
//         vector<shared_ptr<Item>> items;
//         Matcher matcher(query);

//         for (const QJsonValue &val : get<QJsonDocument>(var).array())
//         {
//             const auto obj = val.toObject();
//             const auto title = obj["subject"]["title"].toString();

//             if (auto m = matcher.match(title); m)
//             {
//                 const auto slug = obj["repository"]["full_name"].toString();
//                 const auto type = obj["subject"]["type"].toString();
//                 const auto unread = obj["unread"].toBool();
//                 const auto url = obj["subject"]["latest_comment_url"].isNull()
//                                      ? obj["subject"]["latest_comment_url"].toString()
//                                      : obj["subject"]["url"].toString();

//                 items.emplace_back(StandardItem::make(
//                     title,
//                     title,
//                     unread ? u"[UNREAD] %1 · %2"_s.arg(type, slug)
//                            : u"%1 · %2"_s.arg(type, slug),
//                     default_icon_urls,
//                     {{
//                         "open",
//                         "Open in browser",
//                         [url, this] {
//                             if (const auto v = parseJson(await(api.getLinkData(url)));
//                                 holds_alternative<QString>(v))
//                                 WARN << get<QString>(v);
//                             else
//                                 openUrl(get<QJsonDocument>(v).object()["html_url"].toString());
//                         }
//                     }}
//                 ));
//             }
//         }

//         query.add(::move(items));
//     }
// }
