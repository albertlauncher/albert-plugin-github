// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
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
#include <albert/desktoputil.h>
#include <albert/filedownloader.h>
#include <albert/logging.h>
#include <albert/matcher.h>
#include <albert/networkutil.h>
#include <albert/standarditem.h>
#include <albert/systemutil.h>
#include <mutex>
ALBERT_LOGGING_CATEGORY("github")
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace std;

const QStringList Plugin::default_icon_urls{u":github"_s};
static const QStringList error_icon_urls{u"comp:?src1=%3Agithub&src2=qsp%3ASP_MessageBoxWarning"_s};

static const auto avatars           = "avatars";
static const auto ck_saved_searches = u"saved_searches"_s;
static const auto kck_secrets       = u"secrets"_s;
static const auto oauth_auth_url    = u"https://github.com/login/oauth/authorize"_s;
static const auto oauth_scope       = u"notifications,read:org,read:user"_s;
static const auto oauth_token_url   = u"https://github.com/login/oauth/access_token"_s;

Plugin::Plugin()
{
    tryCreateDirectory(cacheLocation() / avatars);

    connect(&oauth, &OAuth2::clientIdChanged, this, &Plugin::writeSecrets);
    connect(&oauth, &OAuth2::clientSecretChanged, this, &Plugin::writeSecrets);
    connect(&oauth, &OAuth2::tokensChanged, this, [this]{
        writeSecrets();
        api.setBearerToken(oauth.accessToken());
        if (oauth.error().isEmpty())
            INFO << "Tokens updated.";
        else
            WARN << oauth.error();
    });

    connect(&oauth, &OAuth2::tokensChanged,
            this, &Plugin::authorizedInitialization);

    oauth.setAuthUrl(oauth_auth_url);
    oauth.setScope(oauth_scope);
    oauth.setTokenUrl(oauth_token_url);
    oauth.setRedirectUri(u"%1://%2/"_s.arg(qApp->applicationName(), id()));
    oauth.setPkceEnabled(false);

    readSecrets();

    auto s = settings();
    auto size = s->beginReadArray(ck_saved_searches);
    for (int i = 0; i < size; ++i) {
        s->setArrayIndex(i);
        saved_searches.emplace_back(s->value("title").toString(), s->value("query").toString());
    }
    s->endArray();
}

Plugin::~Plugin() = default;

void Plugin::readSecrets()
{
    if (auto secrets = readKeychain(kck_secrets).split(QChar::Tabulation);
        secrets.size() == 3)
    {
        oauth.setClientId(secrets[0]);
        oauth.setClientSecret(secrets[1]);
        oauth.setTokens(secrets[2]);
    }
}

void Plugin::writeSecrets() const
{
    writeKeychain(kck_secrets,
                  QStringList{
                      oauth.clientId(),
                      oauth.clientSecret(),
                      oauth.accessToken()
                  }.join(QChar::Tabulation));
}


void Plugin::authorizedInitialization()
{
    if (const auto var = api.parseJson(await(api.user()));
        holds_alternative<QString>(var))
        WARN << get<QString>(var);
    else
    {
        const auto obj = get<QJsonDocument>(var).object();
        user_name  = obj["name"_L1].toString();
        user_login = obj["login"_L1].toString();
        user_bio   = obj["bio"_L1].toString();
        user_url   = u"https://github.com/%1"_s.arg(user_login);

        if (!saved_searches.empty())
            restoreDefaultSavedSearches();

        downloadAvatar(user_login, obj["avatar_url"_L1].toString());

        disconnect(&oauth, &OAuth2::tokensChanged, this, &Plugin::authorizedInitialization);
    }
}

void Plugin::handle(const QUrl &url)
{
    oauth.handleCallback(url);
    showSettings(id());
}

QString Plugin::makeAvatarPath(const QString owner) const
{ return QDir(cacheLocation() / avatars).filePath(owner) + u".png"_s; }

QString Plugin::makeMaskedIconUrl(const QString path) const
{ return u"mask:?src=%1&radius=2"_s.arg(QString::fromUtf8(QUrl::toPercentEncoding(path))); }

variant<FileDownloader *, QString> Plugin::downloadAvatar(const QString &name, const QUrl &url)
{
    lock_guard lock(downloads_mutex);

    // If currently downloading
    if (const auto it = downloads.find(name); it != downloads.cend())
        return it->second;

    // Exists on disk
    else if (const auto file_path = makeAvatarPath(name);
        QFile::exists(file_path))
        return file_path;

    // Else start download
    else
    {
        auto downloader = downloads.emplace(name, new FileDownloader(url, file_path, this))
                              .first->second; // always successful
        downloader->moveToThread(thread());

        auto on_finish = [this, name](bool success, const QString &path_or_error)
        {
            if (success)
            {
                lock_guard lock_(downloads_mutex);
                downloads.at(name)->deleteLater();
                downloads.erase(name);
            }
            else
                WARN << path_or_error;
        };

        connect(downloader, &FileDownloader::finished, this, on_finish, Qt::QueuedConnection);

        downloader->start();

        return downloader;
    }
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

    block_until = now + 200; // 30 per minute

    return valid;
}

static shared_ptr<Item> createErrorItem(const QString &error)
{
    WARN << error;
    return StandardItem::make(u"notify"_s, u"GitHub"_s, error, error_icon_urls);
}

void Plugin::handleTriggerQuery(Query &query)
{
    // Drop first word if it matches a media type and put the type to the search type flags
    const auto prefix = query.string().section(QChar::Space, 0, 0, QString::SectionSkipEmpty);
    auto query_string = query.string().section(QChar::Space, 1, -1, QString::SectionSkipEmpty);

    if (prefix == u"i"_s)
    {
        if (!debounce(query.isValid()))
            return;

        else if (const auto var = api.parseJson(await(api.searchIssues(query_string)));
            holds_alternative<QString>(var))
            query.add(createErrorItem(get<QString>(var)));

        else
        {
            const auto v =
                get<QJsonDocument>(var).object()["items"_L1].toArray()
                           | views::transform([this](const auto &val){
                                 auto item = IssueItem::make(*this, val.toObject());
                                 item->moveToThread(qApp->thread());
                                 return item;
                             });
            query.add({v.begin(), v.end()});
        }
    }

    else if (prefix == u"r"_s)
    {
        if (!debounce(query.isValid()))
            return;

        else if (const auto var = api.parseJson(await(api.searchRepositories(query_string)));
            holds_alternative<QString>(var))
            query.add(createErrorItem(get<QString>(var)));

        else
        {
            const auto v =
                get<QJsonDocument>(var).object()["items"_L1].toArray()
                           | views::transform([this](const auto &val){
                                 auto item = RepoItem::make(*this, val.toObject());
                                 item->moveToThread(qApp->thread());
                                 return item;
                             });
            query.add({v.begin(), v.end()});
        }
    }

    else if (prefix == u"u"_s)
    {
        if (query_string.isEmpty() && !user_login.isEmpty())
        {
            query.add(StandardItem::make(
                user_login,
                u"%1 @%2"_s.arg(user_name, user_login),
                user_bio,
                {makeMaskedIconUrl(makeAvatarPath(user_login))},
                {{u"open"_s, u"Open in browser"_s, [this]{ openUrl(user_url); }}}
            ));
        }

        else if (!debounce(query.isValid()))
            return;

        else if (const auto var = api.parseJson(await(api.searchUsers(query_string)));
            holds_alternative<QString>(var))
            query.add(createErrorItem(get<QString>(var)));

        else
        {
            const auto v =
                get<QJsonDocument>(var).object()["items"_L1].toArray()
                | views::transform([this](const auto &val){
                      auto item = AccountItem::make(*this, val.toObject());
                      item->moveToThread(qApp->thread());
                      return item;
                  });

            query.add({v.begin(), v.end()});
        }
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

    else
    {
        auto rank_items = handleGlobalQuery(query);
        applyUsageScore(&rank_items);
        ranges::sort(rank_items, greater());
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
                               t, t, _q, default_icon_urls,
                               {{u"show"_s, u"Show"_s, [=]{ show(_q); }, false}}
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
        s->beginWriteArray(ck_saved_searches);
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

    ss.emplace_back(tr("Assigned issues"), u"i is:issue sort:updated-desc state:open assignee:@me "_s);
    ss.emplace_back(tr("Created issues"), u"i is:issue sort:updated-desc state:open author:@me "_s);
    ss.emplace_back(tr("Mentions"), u"i is:issue sort:updated-desc state:open mentions:@me "_s);
    ss.emplace_back(tr("Recent activity"), u"i is:issue sort:updated-desc involves:@me "_s);
    ss.emplace_back(tr("Repositories"), u"r sort:updated-desc owner:%1 "_s.arg(user_login));

    setSavedSearches(ss);
}
