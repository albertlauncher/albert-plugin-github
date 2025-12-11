// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
#include "handlers.h"
#include "items.h"
#include "plugin.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QThread>
#include <albert/albert.h>
#include <albert/iconutil.h>
#include <albert/logging.h>
#include <albert/matcher.h>
#include <albert/networkutil.h>
#include <albert/standarditem.h>
#include <albert/systemutil.h>
#include <albert/usagescoring.h>
#include <mutex>
ALBERT_LOGGING_CATEGORY("github")
using namespace Qt::StringLiterals;
using namespace albert;
using namespace github;
using namespace std;

static const auto kck_secrets = u"secrets"_s;
static const auto ck_saved_searches = "saved_searches"_L1;

Plugin::Plugin()
{
    search_handlers_.emplace_back(make_unique<UserSearchHandler>(api));
    search_handlers_.emplace_back(make_unique<RepoSearchHandler>(api));
    search_handlers_.emplace_back(make_unique<IssueSearchHandler>(api));

    // Read saved searches

    if (auto s = settings();
        s->childGroups().contains(ck_saved_searches))
    {
        s->beginGroup(ck_saved_searches);
        for (const auto &handler : search_handlers_)
        {
            vector<pair<QString, QString>> saved_searches;

            auto size = s->beginReadArray(handler->id().section(u'.', 1)); // drop "github."
            for (int i = 0; i < size; ++i)
            {
                s->setArrayIndex(i);
                saved_searches.emplace_back(s->value("title").toString(),
                                            s->value("query").toString());
            }
            s->endArray();

            handler->setSavedSearches(saved_searches);
        }
    }
    else
    {
        for (const auto &handler : search_handlers_)
            handler->setSavedSearches(handler->defaultSearches());
    }

    // Write saved searches on changes

    const auto writeSavedSearches = [this]{
        auto s = settings();
        s->beginGroup(ck_saved_searches);
        for (const auto &handler : search_handlers_)
        {
            const auto saved_searches = handler->savedSearches();

            s->beginWriteArray(handler->id().section(u'.', 1));  // drop "github."
            CRIT << s->fileName() << s->group() <<handler->id();
            for (size_t i = 0; i < saved_searches.size(); ++i)
            {
                s->setArrayIndex(i);
                s->setValue("title", saved_searches.at(i).first),
                s->setValue("query", saved_searches.at(i).second);
            }
            s->endArray();
        }
    };

    for (const auto &handler : search_handlers_)
        connect(handler.get(), &GithubSearchHandler::savedSearchesChanged,
                this, writeSavedSearches);

    // Write the secrets on changes
    const auto writeAuthConfig = [this]{
        writeKeychain(kck_secrets,
                      QStringList{
                          api.oauth.clientId(),
                          api.oauth.clientSecret(),
                          api.oauth.accessToken()
                      }.join(QChar::Tabulation),
                      [] {
                          DEBG << "Successfully wrote GitHub OAuth credentials to keychain.";
                      },
                      [](const QString &error){
                          WARN << "Failed to write GitHub OAuth credentials to keychain:" << error;
                      });
    };

    const auto connect_oauth_signals = [this, writeAuthConfig]{
        connect(&api.oauth, &OAuth2::clientIdChanged, this, writeAuthConfig);
        connect(&api.oauth, &OAuth2::clientSecretChanged, this, writeAuthConfig);
        connect(&api.oauth, &OAuth2::tokensChanged, this, writeAuthConfig);
    };

    // Read the secrets
    readKeychain(kck_secrets,
                 [this, connect_oauth_signals](const QString &value){
                     if (auto secrets = value.split(QChar::Tabulation);
                         secrets.size() == 3)
                     {
                         api.oauth.setClientId(secrets[0]);
                         api.oauth.setClientSecret(secrets[1]);
                         api.oauth.setTokens(secrets[2]);
                         DEBG << "Successfully read GitHub OAuth credentials from keychain.";
                     }
                     else
                         WARN << "Unexpected format of the GitHub OAuth credentials read from keychain.";
                     connect_oauth_signals();
                 },
                 [connect_oauth_signals](const QString & error){
                     WARN << "Failed to read GitHub OAuth credentials from keychain:" << error;
                     connect_oauth_signals();
                 });
}

Plugin::~Plugin() = default;

vector<Extension*> Plugin::extensions()
{
    vector<Extension*> extensions{this};
    for (const auto &handler : search_handlers_)
        extensions.push_back(handler.get());
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
    showSettings(id());
}

vector<RankItem> Plugin::handleGlobalQuery(const albert::Query &query)
{
    vector<RankItem> r;
    Matcher matcher(query);
    for (const auto &handler : search_handlers_)
        for (const auto &[t, q] : handler->savedSearches())
            if (auto m = matcher.match(t); m)
            {
                auto _q = handler->trigger() + q;

                vector<Action> actions;

                actions.emplace_back(
                    u"show"_s, Plugin::tr("Show"), [=] { show(_q + QChar::Space); }, false);

                actions.emplace_back(u"github"_s, Plugin::tr("Show on GitHub"), [=] {
                    openUrl(u"https://github.com/search?q="_s + percentEncoded(q));
                });

                r.emplace_back(StandardItem::make(t, t, ::move(_q),
                                                  []{ return makeImageIcon(u":github"_s); },
                                                  ::move(actions)),
                               m);
            }
    return r;
}

// void Plugin::readSavedSearches()
// {
    // if (QFile file(savedSearchesFilePath());
    //     !file.exists())
    //     return;

    // else if (!file.open(QIODevice::ReadOnly))
    //     WARN << "Failed to read saved searches:" << file.errorString();

    // else
    // {
    //     QJsonParseError error;
    //     if (const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    //         error.error != QJsonParseError::NoError)
    //         WARN << "Failed to parse JSON (saved searches):" << error.errorString();

    //     else
    //     {
    //         const auto root_obj = doc.object();

    //         for (auto handler : initializer_list<GithubSearchHandler*> {&user_search_handler,
    //                                                                     &repo_search_handler,
    //                                                                     &issue_search_handler})
    //         {
    //             std::flat_map<QString, QString> saved_searches;

    //             const auto cat_object = root_obj[handler->id().section(u'.', 1)].toObject();
    //             for (auto it = cat_object.begin(); it != cat_object.end(); ++it)
    //                 saved_searches.emplace(it.key(), it.value().toString());

    //             handler->setSavedSearches(std::move(saved_searches));
    //         }
    //     }
    // }
// }

// void Plugin::writeSavedSearches()
// {
    // QJsonObject saved_searches;
    // for (const auto handler : initializer_list<GithubSearchHandler*>{&user_search_handler,
    //                                                                   &repo_search_handler,
    //                                                                   &issue_search_handler})
    // {
    //     QJsonObject searches;
    //     for (const auto &[title, query] : handler->savedSearches())
    //         searches[title] = query;

    //     saved_searches[handler->id().section(u'.', 1)] = searches;  // drop "github."
    // }

    // if (!QDir(savedSearchesFilePath().parent_path()).mkpath(u".."_s))
    //     WARN << "Failed to create directory:" << savedSearchesFilePath().parent_path();

    // else if (QFile file(savedSearchesFilePath());
    //          !file.open(QIODevice::WriteOnly))
    //     WARN << "Failed to write saved searches:" << file.errorString();

    // else if (file.write(QJsonDocument(saved_searches).toJson()) == -1)
    //     WARN << "Failed to write saved searches:" << file.errorString();

    // else
    //     DEBG << "Sucessfully wrote saved searches to" << file.fileName();
// }


// void Plugin::restoreDefaultSavedSearches()
// {
//     issue_search_handler.setSavedSearches({
//         {Plugin::tr("Assigned issues"),        u"is:open is:issue assignee:@me"_s},
//         {Plugin::tr("Created issues"),         u"is:open is:issue author:@me"_s},
//         {Plugin::tr("Assigned pull requests"), u"is:open is:pr assignee:@me"_s},
//         {Plugin::tr("Created pull requests"),  u"is:open is:pr author:@me"_s},
//         {Plugin::tr("Review requests"),        u"is:open is:pr review-requested:@me"_s},
//         {Plugin::tr("Mentions"),               u"mentions:@me"_s},
//         {Plugin::tr("Recent activity"),        u"involves:@me"_s}
//     });

//     repo_search_handler.setSavedSearches({
//         {
//             Plugin::tr("My repositories"),
//             u"sort:updated-desc fork:true user:@me"_s
//         },
//         {
//             Plugin::tr("Albert repositories"),
//             u"sort:updated-desc fork:true archived:false org:albertlauncher"_s
//         },
//         {
//             Plugin::tr("Archived Albert repositories"),
//             u"sort:updated-desc fork:true archived:true org:albertlauncher"_s
//         }
//     });

//     writeSavedSearches();
// }
