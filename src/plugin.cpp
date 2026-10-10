// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
#include "handlers.h"
#include "items.h"
#include "plugin.h"
#include <QCoreApplication>
#include <QCoroAsyncGenerator>
#include <QCoroTask>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QtConcurrentRun>
#include <albert/app.h>
#include <albert/icon.h>
#include <albert/logging.h>
#include <albert/matcher.h>
#include <albert/networkutil.h>
#include <albert/standarditem.h>
#include <albert/systemutil.h>
#include <albert/usagescoring.h>
#include <qt6keychain/keychain.h>
ALBERT_LOGGING_CATEGORY("github")
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;
using enum SavedSearch::Type;

namespace
{
static const auto keychain_service = u"albert.github"_s;
static const auto keychain_key = u"secrets"_s;
static const auto ck_saved_searches = "saved_searches"_L1;
}

Plugin::Plugin():
    search_cache_(api, saved_searches_, cacheLocation())
{
    // The handler order is the ordinal order of SavedSearch::Type.
    search_handlers_.emplace_back(make_unique<IssueSearchHandler>(api));
    search_handlers_.emplace_back(make_unique<RepoSearchHandler>(api));
    search_handlers_.emplace_back(make_unique<UserSearchHandler>(api));

    auto *cache_timer = new QTimer(this);
    cache_timer->start(60 * 60 * 1000);
    connect(cache_timer, &QTimer::timeout,
            this, [this] { search_cache_.updateCaches(); });

}

Plugin::~Plugin() = default;

void Plugin::initialize()
{
    QtConcurrent::run([this] {
        readSavedSearches();
    })
    .then(this, [this] {
        auto *job = new QKeychain::ReadPasswordJob(keychain_service, this);  // Deletes itself
        job->setKey(keychain_key);

        connect(job, &QKeychain::ReadPasswordJob::finished, this, [this, job] {
            if (job->error())
                WARN << "Failed to read secrets from keychain:" << job->errorString();
            else if (auto secrets = job->textData().split(QChar::Tabulation);
                     secrets.size() != 3)
                WARN << "Unexpected format of the secrets read from keychain.";
            else
            {
                api.oauth.setClientId(secrets[0]);
                api.oauth.setClientSecret(secrets[1]);
                api.oauth.setTokens(secrets[2]);
                DEBG << "Successfully read secrets from keychain.";
            }

            connect(&api.oauth, &OAuth2::clientIdChanged,     this, &Plugin::writeSecrets);
            connect(&api.oauth, &OAuth2::clientSecretChanged, this, &Plugin::writeSecrets);
            connect(&api.oauth, &OAuth2::tokensChanged,       this, &Plugin::writeSecrets);

            emit initialized();
            search_cache_.updateCaches();
        });

        job->start();
    })
    .onCanceled(this, [] {
        WARN << "Cancelled plugin initialization.";
    })
    .onFailed(this, [](const QUnhandledException &que) {
        if (que.exception())
            rethrow_exception(que.exception());
        else
            throw runtime_error("QUnhandledException::exception() returned nullptr.");
    })
    .onFailed(this, [](const exception &e) {
        CRIT << "Exception while initializing plugin:" << e.what();
    })
    .onFailed(this, [] {
        CRIT << "Unknown exception while initializing plugin.";
    });
}


vector<SavedSearch> Plugin::savedSearches() const
{
    lock_guard lock(saved_searches_mutex_);
    return saved_searches_;
}

void Plugin::setSavedSearches(const vector<SavedSearch> &saved_searches)
{
    {
        lock_guard lock(saved_searches_mutex_);
        if (saved_searches == saved_searches_)
            return;
        saved_searches_ = saved_searches;
    }
    writeSavedSearches();
    search_cache_.updateCaches();
}

void Plugin::writeSavedSearches()
{
    auto s = settings();
    s->beginGroup(ck_saved_searches);
    const auto searches = savedSearches();
    for (size_t type_index = 0; type_index < search_handlers_.size(); ++type_index)
    {
        vector<SavedSearch> saved_searches;
        for (const auto &saved : searches)
            if (static_cast<size_t>(saved.type) == type_index)
                saved_searches.push_back(saved);

        s->beginWriteArray(search_handlers_[type_index]->id().section(u'.', 1));  // drop "github."
        for (size_t search_index = 0; search_index < saved_searches.size(); ++search_index)
        {
            s->setArrayIndex(search_index);
            s->setValue("title", saved_searches.at(search_index).title);
            s->setValue("query", saved_searches.at(search_index).query);
            s->setValue("cache_limit", saved_searches.at(search_index).cache_limit);
            s->setValue("cache_interval", saved_searches.at(search_index).cache_interval);
        }
        s->endArray();
    }
}

void Plugin::readSavedSearches()
{
    vector<SavedSearch> saved_searches;
    if (auto s = settings();
        s->childGroups().contains(ck_saved_searches))
    {
        s->beginGroup(ck_saved_searches);
        for (size_t type_index = 0; type_index < search_handlers_.size(); ++type_index)
        {
            auto size = s->beginReadArray(search_handlers_[type_index]->id().section(u'.', 1)); // drop "github."
            for (int search_index = 0; search_index < size; ++search_index)
            {
                s->setArrayIndex(search_index);
                saved_searches.push_back({static_cast<SavedSearch::Type>(type_index),
                                          s->value("title").toString(),
                                          s->value("query").toString(),
                                          s->value("cache_limit", 0).toUInt(),
                                          s->value("cache_interval", 0).toUInt()});
            }
            s->endArray();
        }
        s->endGroup();
    }
    else
    {
        saved_searches = {
            {Issues, tr("Assigned issues"), u"is:open is:issue assignee:@me"_s},
            {Issues, tr("Created issues"), u"is:open is:issue author:@me"_s},
            {Issues, tr("Albert issues"), u"is:open is:issue org:albertlauncher"_s},
            {Issues, tr("Assigned pull requests"), u"is:open is:pr assignee:@me"_s},
            {Issues, tr("Created pull requests"), u"is:open is:pr author:@me"_s},
            {Issues, tr("Albert pull requests"), u"is:open is:pr org:albertlauncher"_s},
            {Issues, tr("Review requests"), u"is:open is:pr review-requested:@me"_s},
            {Issues, tr("Mentions"), u"mentions:@me"_s},
            {Issues, tr("Recent activity"), u"involves:@me"_s},
            {Repositories, tr("My repositories"), u"sort:updated-desc fork:true user:@me"_s},
            {Repositories, tr("Albert repositories"),
             u"sort:updated-desc fork:true archived:false org:albertlauncher"_s},
            {Repositories, tr("Archived Albert repositories"),
             u"sort:updated-desc fork:true archived:true org:albertlauncher"_s}
        };
    }
    lock_guard lock(saved_searches_mutex_);
    saved_searches_ = ::move(saved_searches);
}

void Plugin::writeSecrets()
{
    QStringList secrets = {api.oauth.clientId(),
                           api.oauth.clientSecret(),
                           api.oauth.accessToken()};

    auto job = new QKeychain::WritePasswordJob(keychain_service, this);  // Deletes itself
    job->setKey(keychain_key);
    job->setTextData(secrets.join(QChar::Tabulation));

    connect(job, &QKeychain::Job::finished, this, [=] {
        if (job->error())
            WARN << "Failed to write secrets to keychain:" << job->errorString();
        else
            DEBG << "Successfully wrote secrets to keychain.";
    });

    job->start();
}

vector<Extension*> Plugin::extensions()
{
    vector<Extension*> extensions{this};
    for (const auto &handler : search_handlers_)
        extensions.push_back(handler.get());
    extensions.push_back(&search_cache_);
    return extensions;
}

QWidget *Plugin::buildConfigWidget()
{
    return new ConfigWidget(*this, api.oauth);
}

QString Plugin::defaultTrigger() const { return u"gh "_s; }

void Plugin::handle(const QUrl &url)
{
    api.oauth.handleCallback(url);
    app().showSettings(id());
}

vector<RankItem> Plugin::rankItems(QueryContext &ctx)
{
    vector<RankItem> r;
    Matcher matcher(ctx);
    for (const auto &saved : savedSearches())
    {
        const auto handler_index = static_cast<size_t>(saved.type);
        if (handler_index >= search_handlers_.size())
            continue;
        const auto &handler = search_handlers_[handler_index];

        if (auto m = matcher.match(saved.title); m)
            {
                auto _q = handler->trigger() + saved.query;

                vector<Action> actions;

                actions.emplace_back(
                    u"show"_s,
                    Plugin::tr("Show"),
                    [=] { app().show(_q + QChar::Space); },
                    false);

                actions.emplace_back(u"github"_s, Plugin::tr("Show on GitHub"), [=] {
                    openUrl(u"https://github.com/search?q="_s + percentEncoded(saved.query));
                });

                // if (saved.cache_limit > 0)
                //     actions.emplace_back(u"refresh-cache"_s, Plugin::tr("Refresh cache"),
                //                          [this, saved] {
                //                              refreshCache(saved).then([] {});
                //                          });

                r.emplace_back(StandardItem::make(saved.title, saved.title, ::move(_q),
                                                  []{ return Icon::image(u":github"_s); },
                                                  ::move(actions)),
                               m);
        }
    }
    return r;
}
