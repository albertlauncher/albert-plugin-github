// Copyright (c) 2025-2026 Manuel Schneider

#pragma once
#include "savedsearch.h"
#include <QCoreApplication>
#include <QCoroTask>
#include <QString>
#include <albert/indexqueryhandler.h>
#include <expected>
#include <filesystem>
class API;
class QJsonArray;

class SearchCache : public albert::IndexQueryHandler
{
    Q_DECLARE_TR_FUNCTIONS(SearchCache)

public:
    SearchCache(API &api,
                const std::vector<SavedSearch> &saved_searches,
                std::filesystem::path plugin_cache_location);

    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString defaultTrigger() const override;
    void updateIndexItems() override;

    void updateCaches();

public:
    QCoro::Task<std::expected<void, QString>> refreshCache(SavedSearch);

    QJsonArray read(const SavedSearch &) const;
    std::expected<void, QString> write(const SavedSearch &, const QJsonArray &) const;
    bool needsUpdate(const SavedSearch &) const;
    void remove(const SavedSearch &) const;

private:
    API &api_;
    const std::vector<SavedSearch> &saved_searches_;
    std::filesystem::path cache_location_;
};
