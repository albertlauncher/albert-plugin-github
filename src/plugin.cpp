// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
#include "plugin.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QThread>
#include <albert/albert.h>
#include <albert/desktoputil.h>
#include <albert/filedownloader.h>
#include <albert/logging.h>
#include <albert/matcher.h>
#include <albert/networkutil.h>
#include <albert/standarditem.h>
#include <mutex>
ALBERT_LOGGING_CATEGORY("github")
using namespace albert;
using namespace std;
using namespace util;

struct {
    const char *kck_secrets       = "secrets";
    const char *ck_saved_searches = "saved_searches";
    const char *oauth_auth_url    = "https://github.com/login/oauth/authorize";
    const char *oauth_token_url   = "https://github.com/login/oauth/access_token";
    const char *oauth_scope       = "notifications,read:org,read:user";
} const strings;

Plugin::Plugin()
{
    connect(&oauth, &OAuth2::clientIdChanged, this, &Plugin::writeSecrets);
    connect(&oauth, &OAuth2::clientSecretChanged, this, &Plugin::writeSecrets);
    connect(&oauth, &OAuth2::tokensChanged, this, [this]{
        writeSecrets();
        api.setBearerToken(oauth.accessToken().toUtf8());
        if (oauth.accessToken().isEmpty())
            WARN << oauth.error();
        else
            INFO << "Tokens updated.";
    });

    connect(&oauth, &OAuth2::stateChanged, this, [this](OAuth2::State state)
    {
        using enum OAuth2::State;
        switch (state) {
        case NotAuthorized:
            if (auto e = oauth.error(); e.isEmpty())
                DEBG << "Not authorized.";
            else
                WARN << "Not authorized." << e;
            break;
        case Awaiting:
            DEBG << "Requested access…";
            break;
        case Granted:
            INFO << "Access granted.";
            break;
        }
    });

    connect(&oauth, &OAuth2::tokensChanged,
            this, &Plugin::authorizedInitialization);

    oauth.setAuthUrl(strings.oauth_auth_url);
    oauth.setScope(strings.oauth_scope);
    oauth.setTokenUrl(strings.oauth_token_url);
    oauth.setRedirectUri(QString("%1://%2/").arg(qApp->applicationName(), id()));
    oauth.setPkceEnabled(false);

    readSecrets();

    auto s = settings();
    auto size = s->beginReadArray(strings.ck_saved_searches);
    for (int i = 0; i < size; ++i) {
        s->setArrayIndex(i);
        saved_searches.emplace_back(s->value("title").toString(), s->value("query").toString());
    }
    s->endArray();
}

Plugin::~Plugin() = default;

void Plugin::readSecrets()
{
    if (auto secrets = readKeychain(strings.kck_secrets).split(QChar::Tabulation);
        secrets.size() == 3)
    {
        oauth.setClientId(secrets[0]);
        oauth.setClientSecret(secrets[1]);
        oauth.setTokens(secrets[2]);
    }
}

void Plugin::writeSecrets() const
{
    writeKeychain(strings.kck_secrets,
                  QStringList{
                      oauth.clientId(),
                      oauth.clientSecret(),
                      oauth.accessToken()
                  }.join(QChar::Tabulation));
}


void Plugin::authorizedInitialization()
{
    if (const auto var = parseJson(await(api.user()));
        holds_alternative<QString>(var))
        WARN << get<QString>(var);
    else
    {
        const auto obj = get<QJsonDocument>(var).object();
        user_name  = obj["name"].toString();
        user_login = obj["login"].toString();
        user_bio   = obj["bio"].toString();
        user_url   = QString("https://github.com/%1").arg(user_login);

        if (!saved_searches.empty())
            restoreDefaultSavedSearches();

        downloadAvatar(user_login, obj["avatar_url"].toString());

        disconnect(&oauth, &OAuth2::tokensChanged, this, &Plugin::authorizedInitialization);
    }
}

void Plugin::handle(const QUrl &url)
{
    oauth.handleCallback(url);
    showSettings(id());
}

QString Plugin::downloadAvatar(const QString &name, const QString &url)
{
    const auto file_path = QString("%1/%2.png").arg((cacheLocation() / "icons").c_str(), name);
    if (!QFile::exists(file_path))
    {
        auto fdl = new FileDownLoader(url, file_path, this);
        fdl->moveToThread(thread());
        connect(fdl, &FileDownLoader::finished, fdl, [fdl]{
            fdl->deleteLater();
            if (!fdl->success())
                WARN << fdl->error();
        });
        QMetaObject::invokeMethod(fdl, [fdl]{ fdl->start(); }, Qt::QueuedConnection);
    }
    return file_path;
}

QWidget *Plugin::buildConfigWidget() { return new ConfigWidget(*this); }

static const bool &debounce(const bool &valid)
{
    static mutex m;
    static long long block_until = 0;
    auto now = QDateTime::currentMSecsSinceEpoch();

    unique_lock lock(m);

    while (block_until > QDateTime::currentMSecsSinceEpoch())
        if (valid)
            QThread::msleep(10);
        else
            return valid;

    block_until = now + 2000; // 30 per minute

    return valid;
}

void Plugin::handleTriggerQuery(Query &query)
{
    // Drop first word if it matches a media type and put the type to the search type flags
    const auto prefix = query.string().section(QChar::Space, 0, 0, QString::SectionSkipEmpty);
    auto query_string = query.string().section(QChar::Space, 1, -1, QString::SectionSkipEmpty);

    if (prefix == QStringLiteral("i"))
    {
        if (!debounce(query.isValid()))
            return;

        if (const auto var = parseJson(await(api.searchIssues(query_string)));
            holds_alternative<QString>(var))
            WARN << get<QString>(var);
        else
        {
            vector<shared_ptr<Item>> items;
            for (const auto &val : get<QJsonDocument>(var).object()["items"].toArray())
            {
                const auto obj = val.toObject();
                const auto title = obj["title"].toString();
                const auto url = obj["html_url"].toString();
                const auto number = obj["number"].toInt();
                const auto state = obj["state"].toString().toUpper();
                const auto repoName = obj["repository_url"].toString().section('/', -2);
                const auto description = QString("%1 #%2 %3").arg(repoName).arg(number).arg(state);

                items.emplace_back(StandardItem::make(
                    url, title, description, title, {":github"},
                    {{"open", "Open in browser", [url] { openUrl(url); }}}
                ));
            }
            query.add(::move(items));
        }
    }

    else if (prefix == QStringLiteral("r"))
    {
        if (!debounce(query.isValid()))
            return;

        if (const auto var = parseJson(await(api.searchRepositories(query_string)));
            holds_alternative<QString>(var))
            WARN << get<QString>(var);
        else
        {
            vector<shared_ptr<Item>> items;
            for (const auto &val : get<QJsonDocument>(var).object()["items"].toArray())
            {
                const auto obj = val.toObject();
                // const auto icon_url = obj["owner"]["avatar_url"].toString();
                const auto owner = obj["owner"]["login"].toString();
                const auto name = obj["name"].toString();
                const auto slug = obj["full_name"].toString();
                const auto url = obj["html_url"].toString();
                const auto description = obj["description"].toString();
                const auto forks = obj["forks_count"].toInt();
                const auto stars = obj["stargazers_count"].toInt();
                // const auto watchers = obj["watchers_count"].toInt();
                const auto issues = obj["open_issues_count"].toInt();
                const auto has_discussions = obj["has_discussions"].toBool();
                const auto has_issues = obj["has_issues"].toBool();
                const auto has_wiki = obj["has_wiki"].toBool();
                // const auto icon_path = downloadAvatar(owner, icon_url);

                QString desc(owner);
                if (stars)
                    desc.append(QStringLiteral(" ✨%1").arg(stars));
                if (forks)
                    desc.append(QStringLiteral(" 🍴%1").arg(forks));
                if (issues)
                    desc.append(QStringLiteral(" ⚠️%1").arg(issues));
                if (!description.isEmpty())
                    desc.append(" · " + description);

                vector<Action> actions{{ "o", "Open", [=] { openUrl(url); }}};
                if (has_issues){
                    actions.emplace_back("oi", "Open issues", [=]{ openUrl(url + "/issues"); });
                    actions.emplace_back("op", "Open pull requests", [=]{ openUrl(url + "/pulls"); });
                }
                if (has_discussions)
                    actions.emplace_back("od", "Open discussions", [=]{ openUrl(url + "/discussions"); });
                if (has_wiki)
                    actions.emplace_back("ow", "Open wiki", [=]{ openUrl(url + "/wiki"); });

                items.emplace_back(StandardItem::make(slug, name, desc, {":github"}, actions));
            }
            query.add(::move(items));
        }
    }

    else if (prefix == QStringLiteral("u"))
    {
        if (query_string.isEmpty() && !user_login.isEmpty())
        {
            query.add(StandardItem::make(
                user_login,
                QString("%1 @%2").arg(user_name, user_login),
                user_bio,
                {QString("%1/%2.png").arg((cacheLocation() / "icons").c_str(), user_login)},
                {{"open", "Open in browser", [this]{ openUrl(user_url); }}}
            ));
        }
        else
        {
            if (!debounce(query.isValid()))
                return;

            if (const auto var = parseJson(await(api.searchUsers(query_string)));
                holds_alternative<QString>(var))
                WARN << get<QString>(var);
            else
            {
                vector<shared_ptr<Item>> items;
                for (const auto &val : get<QJsonDocument>(var).object()["items"].toArray())
                {
                    const auto obj = val.toObject();
                    const auto login = obj["login"].toString();
                    const auto type = obj["type"].toString();
                    const auto html_url = obj["html_url"].toString();

                    items.emplace_back(StandardItem::make(
                        login, login, type, login, {":github"},
                        {{"open", "Open in browser", [html_url] { openUrl(html_url); }}}
                    ));
                }
                query.add(::move(items));
            }
        }
    }

    else if (prefix == QStringLiteral("n"))
    {
        if (!debounce(query.isValid()))
            return;

        if (const auto var = parseJson(await(api.notifications()));
            holds_alternative<QString>(var))
            WARN << get<QString>(var);
        else
        {
            vector<shared_ptr<Item>> items;
            Matcher matcher(query);

            for (const QJsonValue &val : get<QJsonDocument>(var).array())
            {
                const auto obj = val.toObject();
                const auto title = obj["subject"]["title"].toString();

                if (auto m = matcher.match(title); m)
                {
                    const auto slug = obj["repository"]["full_name"].toString();
                    const auto type = obj["subject"]["type"].toString();
                    const auto unread = obj["unread"].toBool();
                    const auto url = obj["subject"]["latest_comment_url"].isNull()
                                         ? obj["subject"]["latest_comment_url"].toString()
                                         : obj["subject"]["url"].toString();

                    items.emplace_back(StandardItem::make(
                        title,
                        title,
                        unread ? QStringLiteral("[UNREAD] %1 · %2").arg(type, slug)
                               : QStringLiteral("%1 · %2").arg(type, slug),
                        {":github"},
                        {{
                            "open",
                            "Open in browser",
                            [url, this] {
                                if (const auto v = parseJson(await(api.getLinkData(url)));
                                    holds_alternative<QString>(v))
                                    WARN << get<QString>(v);
                                else
                                    openUrl(get<QJsonDocument>(v).object()["html_url"].toString());
                            }
                        }}
                    ));
                }
            }

            query.add(::move(items));
        }
    }

    else
    {
        auto rank_items = handleGlobalQuery(query);
        applyUsageScore(&rank_items);
        sort(rank_items.begin(), rank_items.end());
        auto v = rank_items | views::transform(&RankItem::item);
        query.add({v.begin(), v.end()});
    }
}

vector<RankItem> Plugin::handleGlobalQuery(const albert::Query &query)
{
    vector<RankItem> r;
    Matcher matcher(query);
    for (const auto &[t, q] : saved_searches)
        if (auto m = matcher.match(t); m)
        {
            auto _q = trigger + q;

            r.emplace_back(StandardItem::make(
                               t, t, _q, {":github"},
                               {{"show", "Show", [=]{ show(_q); }, false}}
                            ), m);

        }

    return r;
}

void Plugin::setTrigger(const QString &t) { trigger = t; }

const vector<SavedSearch> &Plugin::savedSearches() const { return saved_searches; }

void Plugin::setSavedSearches(const vector<SavedSearch> &ss)
{
    if (ss != saved_searches)
    {
        saved_searches = ss;

        auto s = settings();
        s->beginWriteArray(strings.ck_saved_searches);
        int i = 0;
        for (const auto &[t, q] : ss) {
            s->setArrayIndex(i++);
            s->setValue("title", t);
            s->setValue("query", q);
        }
        s->endArray();

        emit savedSearchesChanged();
    }
}

void Plugin::restoreDefaultSavedSearches()
{
    vector<SavedSearch> ss;

    ss.emplace_back("Assigned issues", "i is:issue sort:updated-desc state:open assignee:@me ");
    ss.emplace_back("Created issues", "i is:issue sort:updated-desc state:open author:@me ");
    ss.emplace_back("Mentions", "i is:issue sort:updated-desc state:open mentions:@me ");
    ss.emplace_back("Recent activity", "i is:issue sort:updated-desc involves:@me ");
    ss.emplace_back("Repositories", QString("r sort:updated-desc owner:%1 ").arg(user_login));

    setSavedSearches(ss);
}
