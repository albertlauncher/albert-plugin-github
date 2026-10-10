// Copyright (c) 2025-2026 Manuel Schneider

#include "api.h"
#include "items.h"
#include "searchcache.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <albert/logging.h>
#include <QJsonArray>
#include <array>
#include <filesystem>
#include <format>
#include <set>
#include <system_error>
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

namespace
{

static constexpr const auto cache_dir_name = "searches"s;

static constexpr const auto type_names = array{"issues"s, "repositories"s, "users"s};  // order!

static filesystem::path cacheFileName(const SavedSearch &ss)
{
    return format("{}-{}.json",
                  type_names.at((int) ss.type),
                  QCryptographicHash::hash(ss.query.toUtf8(), QCryptographicHash::Sha256)
                      .toHex().data());
}

static shared_ptr<Item> itemFromJson(SavedSearch::Type type, const QJsonObject &o)
{
    switch (type) {
    case SavedSearch::Type::Issues:
        return IssueItem::fromJson(o);
    case SavedSearch::Type::Repositories:
        return RepositoryItem::fromJson(o);
    case SavedSearch::Type::Users:
        return UserItem::fromJson(o);
    }
    Q_UNREACHABLE();
    return {};
}

}

SearchCache::SearchCache(API &api,
                         const vector<SavedSearch> &saved_Searches,
                         filesystem::path plugin_cache_location) :
    api_(api),
    saved_searches_(saved_Searches),
    cache_location_(plugin_cache_location / cache_dir_name)
{
}

QString SearchCache::id() const { return QStringLiteral("github.searchcache"); }

QString SearchCache::name() const { return tr("GitHub search cache"); }

QString SearchCache::description() const
{ return tr("Fast search on cached GitHub saved searches"); }

QString SearchCache::defaultTrigger() const { return u"ghc "_s; }

void SearchCache::updateIndexItems()
{
    vector<IndexItem> index_items;
    for (const auto &saved_search : saved_searches_ | views::filter(&SavedSearch::cache_limit))
        for (const auto &raw_item : read(saved_search))
            if (auto item = itemFromJson(saved_search.type, raw_item.toObject()); item)
                index_items.emplace_back(item, item->text());
    setIndexItems(::move(index_items));
}

QJsonArray SearchCache::read(const SavedSearch &ss) const
{
    if (QFile file(cache_location_ / cacheFileName(ss));
        file.open(QIODevice::ReadOnly))
        if (const auto doc = QJsonDocument::fromJson(file.readAll()); doc.isArray())
            return doc.array();
    return {};
}

expected<void, QString> SearchCache::write(const SavedSearch &ss, const QJsonArray &items) const
{
    const auto path = cache_location_ / cacheFileName(ss);
    error_code ec;
    filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return unexpected(QString::fromLatin1(ec.message()));
    else if (QSaveFile file(path);
             !file.open(QIODevice::WriteOnly))
        return unexpected(file.errorString());
    else if (file.write(QJsonDocument(items).toJson(QJsonDocument::Compact));
             !file.commit())
        return unexpected(u"QSaveFile::commit failed."_s);
    else
        return {};
}

bool SearchCache::needsUpdate(const SavedSearch &ss) const
{
    const QFileInfo info(cache_location_ / cacheFileName(ss));
    return !info.exists()
           || (info.exists() && ss.cache_interval > 0
               && info.lastModified().secsTo(QDateTime::currentDateTime())
                      >= ss.cache_interval * 3600LL);
}

void SearchCache::remove(const SavedSearch &ss) const
{
    QFile::remove(cache_location_ / cacheFileName(ss));
}

QCoro::Task<expected<void, QString>> SearchCache::refreshCache(SavedSearch saved)
{
    if (saved.cache_limit == 0)
        co_return {};

    auto search = array{
        &API::searchIssues,
        &API::searchRepositories,
        &API::searchUsers
    }.at((int)saved.type);

    QJsonArray cached_items;
    auto gen = invoke(search, api_, saved.query, 10);
    for (auto it = co_await gen.begin();
         it != gen.end() && saved.cache_limit > cached_items.size();
         co_await ++it)
        if (const auto arr = *it)
            ranges::copy(*arr, std::back_inserter(cached_items));
        else
            co_return unexpected(arr.error());

    if (auto exp = write(saved, cached_items); !exp)
        co_return exp;

    co_return {};
}

void SearchCache::updateCaches()
{
    set<filesystem::path> dangling_caches;
    if (filesystem::exists(cache_location_))
        for (auto const& entry : filesystem::recursive_directory_iterator(cache_location_))
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                dangling_caches.insert(entry.path());

    for (const auto &ss : saved_searches_)
    {
        const auto path = cache_location_ / cacheFileName(ss);
        if (ss.cache_limit > 0)
            dangling_caches.erase(path);

        if (ss.cache_limit > 0 && needsUpdate(ss))
            refreshCache(ss).then(
                [this](expected<void, QString> result) {
                    if (!result)
                        WARN << "Failed to refresh GitHub search cache:" << result.error();
                    updateIndexItems();
                },
                [](const exception &e) {
                    WARN << "Exception while refreshing GitHub search cache:" << e.what();
                });
    }

    for (const auto &path : dangling_caches)
        if (error_code ec;
            !filesystem::remove(path, ec) && ec)
            WARN << "Failed to remove dangling GitHub cache:" << ec.message();

    updateIndexItems();
}
