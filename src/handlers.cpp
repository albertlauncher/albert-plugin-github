// Copyright (c) 2025-2025 Manuel Schneider

#include "api.h"
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
                                         API &api)
    : id_(id)
    , name_(name)
    , description_(description)
    , default_trigger_(defaultTrigger)
    , api_(api)
{}

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

UserSearchHandler::UserSearchHandler(API &api):
    GithubSearchHandler(u"github.users"_s,
                        Plugin::tr("GitHub users"),
                        Plugin::tr("Search GitHub users"),
                        u"ghu"_s,
                        api)
{}

vector<pair<QString, QString>> UserSearchHandler::defaultSearches() const { return {}; }

AsyncItemGenerator UserSearchHandler::items(QueryContext &ctx)
{
    if (ctx.query().isEmpty() && api_.oauth.state() == OAuth2::State::Granted)
    {
        if (const auto obj = co_await api_.user())
        {
            vector<shared_ptr<Item>> items{UserItem::fromJson(*obj)};
            co_yield ::move(items);  // TODO: GCC>13 yieling temporaries is fine
        }
        else
        {
            vector<shared_ptr<Item>> items{makeErrorItem(obj.error())};
            co_yield ::move(items);  // TODO: GCC>13 yieling temporaries is fine
        }
    }
    else
    {
        auto gen = api_.searchUsers(ctx, 10);
        for (auto it = co_await gen.begin(); it != gen.end(); co_await ++it)
        {
            if (!ctx.isValid())
                co_return;

            if (const auto arr = *it)
            {
                auto v = *arr
                         | views::transform([](const auto &val)
                                            { return UserItem::fromJson(val.toObject()); });
                vector<shared_ptr<Item>> items(begin(v), end(v));
                co_yield ::move(items);  // TODO: GCC>13 yieling temporaries is fine
            }
            else
            {
                vector<shared_ptr<Item>> items{makeErrorItem(arr.error())};
                co_yield ::move(items); // TODO: GCC>13 yieling temporaries is fine
                co_return;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------

RepoSearchHandler::RepoSearchHandler(API &api):
    GithubSearchHandler(u"github.repositories"_s,
                        Plugin::tr("GitHub repositories"),
                        Plugin::tr("Search GitHub repositories"),
                        u"ghr"_s,
                        api)
{}

AsyncItemGenerator RepoSearchHandler::items(albert::QueryContext &ctx)
{
    auto gen = api_.searchRepositories(ctx, 10);
    for (auto it = co_await gen.begin(); it != gen.end(); co_await ++it)
    {
        if (!ctx.isValid())
            co_return;

        if (const auto arr = *it)
        {
            auto v = *arr
                     | views::transform([](const auto &val)
                                        { return RepositoryItem::fromJson(val.toObject()); });
            vector<shared_ptr<Item>> items(begin(v), end(v));
            co_yield ::move(items);  // TODO: GCC>13 yieling temporaries is fine
        }
        else
        {
            vector<shared_ptr<Item>> items{makeErrorItem(arr.error())};
            co_yield ::move(items); // TODO: GCC>13 yieling temporaries is fine
            co_return;
        }
    }
}

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

IssueSearchHandler::IssueSearchHandler(API &api):
    GithubSearchHandler(u"github.issues"_s,
                        Plugin::tr("GitHub issues"),
                        Plugin::tr("Search GitHub issues"),
                        u"ghi"_s,
                        api)
{}

AsyncItemGenerator IssueSearchHandler::items(albert::QueryContext &ctx)
{
    auto gen = api_.searchIssues(ctx, 10);
    for (auto it = co_await gen.begin(); it != gen.end(); co_await ++it)
    {
        if (!ctx.isValid())
            co_return;

        if (const auto arr = *it)
        {
            auto v = *arr
                     | views::transform([](const auto &val)
                                        { return IssueItem::fromJson(val.toObject()); });
            vector<shared_ptr<Item>> items(begin(v), end(v));
            co_yield ::move(items);  // TODO: GCC>13 yieling temporaries is fine
        }
        else
        {
            vector<shared_ptr<Item>> items{makeErrorItem(arr.error())};
            co_yield ::move(items); // TODO: GCC>13 yieling temporaries is fine
            co_return;
        }
    }
}

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
