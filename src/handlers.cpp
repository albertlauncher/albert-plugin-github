// Copyright (c) 2025-2025 Manuel Schneider

#include "github.h"
#include "handlers.h"
#include "items.h"
#include "plugin.h"
#include <QCoroAsyncGenerator>
#include <QCoroNetworkReply>
#include <QCoroSignal>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <albert/icon.h>
#include <albert/logging.h>
#include <albert/matcher.h>
#include <albert/networkutil.h>
#include <albert/queryexecution.h>
#include <albert/queryresults.h>
#include <albert/standarditem.h>
#include <albert/systemutil.h>
#include <memory>
#include <ranges>
using namespace Qt::StringLiterals;
using namespace albert::detail;
using namespace albert;
using namespace github;
using namespace std;

static unique_ptr<Icon> makeGithubIcon() { return Icon::image(u":github"_s); }

static shared_ptr<Item> makeErrorItem(const QString &error)
{
    WARN << error;
    return StandardItem::make(u"notify"_s, u"GitHub"_s, error, [] {
        return Icon::composed(makeGithubIcon(), Icon::standard(Icon::MessageBoxWarning));
    });
}

GithubSearchHandler::GithubSearchHandler(const QString &id,
                                         const QString &name,
                                         const QString &description,
                                         const QString &defaultTrigger,
                                         const RestApi &api)
    : id_(id)
    , name_(name)
    , description_(description)
    , default_trigger_(defaultTrigger)
    , api_(api)
    , rate_limiter_(api_.rateLimit())
{
    connect(&api_.oauth, &OAuth2::stateChanged,
            this, [this]{ rate_limiter_.setDelay(api_.rateLimit()); });
}

QString GithubSearchHandler::id() const { return id_; }

QString GithubSearchHandler::name() const { return name_; }

QString GithubSearchHandler::description() const { return description_; }

QString GithubSearchHandler::defaultTrigger() const { return default_trigger_ + QChar::Space; }

QString GithubSearchHandler::trigger()
{
    lock_guard lock(mtx);
    return trigger_;
}

void GithubSearchHandler::setTrigger(const QString &t)
{
    lock_guard lock(mtx);
    trigger_ = t;
}

AsyncItemGenerator GithubSearchHandler::items(QueryContext &ctx)
{
    try {
        for (auto page = 1;; ++page)
        {
            co_await qCoro(rate_limiter_.acquire().get(), &Acquire::granted);

            if (!ctx.isValid())
                co_return;

            unique_ptr<QNetworkReply> reply(requestSearch(ctx, page));
            DEBG << "Fetch" << reply->request().url();
            co_await qCoro(reply.get()).waitForFinished();

            if (const auto var = RestApi::parseJson(*reply);
                holds_alternative<QJsonDocument>(var))
            {
                auto v = get<QJsonDocument>(var)["items"_L1].toArray()
                         | views::transform([this](const auto &val)
                                            { return parseItem(val.toObject()); });
                // TODO: GCC>13 yieling temporaries is fine
                vector<std::shared_ptr<albert::Item>> items(begin(v), end(v));
                co_yield ::move(items);
            }
            else
            {
                // TODO: GCC>13 yieling temporaries is fine
                vector<std::shared_ptr<albert::Item>> items;
                items.push_back(makeErrorItem(get<QString>(var)));
                co_yield ::move(items);
                co_return;
            }
        }
    } catch (...) {
        CRIT << "EXCEP";
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

//--------------------------------------------------------------------------------------------------

UserSearchHandler::UserSearchHandler(const github::RestApi &api):
    GithubSearchHandler(u"github.users"_s,
                        Plugin::tr("GitHub users"),
                        Plugin::tr("Search GitHub users"),
                        u"ghu"_s,
                        api)
{}

QNetworkReply *UserSearchHandler::requestSearch(const QString &query, uint page) const
{ return api_.searchUsers(query, 10, page); }

shared_ptr<Item> UserSearchHandler::parseItem(const QJsonObject &o) const
{ return UserItem::fromJson(o); }

vector<pair<QString, QString>> UserSearchHandler::defaultSearches() const { return {}; }

//--------------------------------------------------------------------------------------------------

RepoSearchHandler::RepoSearchHandler(const github::RestApi &api):
    GithubSearchHandler(u"github.repositories"_s,
                        Plugin::tr("GitHub repositories"),
                        Plugin::tr("Search GitHub repositories"),
                        u"ghr"_s,
                        api)
{}

QNetworkReply *RepoSearchHandler::requestSearch(const QString &query, uint page) const
{ return api_.searchRepositories(query, 10, page); }

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
    GithubSearchHandler(u"github.issues"_s,
                        Plugin::tr("GitHub issues"),
                        Plugin::tr("Search GitHub issues"),
                        u"ghi"_s,
                        api)
{}

QNetworkReply *IssueSearchHandler::requestSearch(const QString &query, uint page) const
{ return api_.searchIssues(query, 10, page); }

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
