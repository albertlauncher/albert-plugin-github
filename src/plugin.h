// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "api.h"
#include "savedsearch.h"
#include <albert/extensionplugin.h>
#include <albert/oauth.h>
#include <albert/globalqueryhandler.h>
#include <albert/urlhandler.h>
#include <memory>
#include <mutex>
#include <vector>

class GithubSearchHandler;


class Plugin final : public albert::ExtensionPlugin,
                     public albert::GlobalQueryHandler,
                     private albert::UrlHandler
{
    ALBERT_PLUGIN

public:

    Plugin();
    ~Plugin() override;
    void initialize() override;

    std::vector<albert::Extension*> extensions() override;
    QWidget* buildConfigWidget() override;
    QString defaultTrigger() const override;
    void handle(const QUrl &) override;
    std::vector<albert::RankItem> rankItems(albert::QueryContext &) override;

    std::vector<SavedSearch> savedSearches() const;
    void setSavedSearches(const std::vector<SavedSearch> &);

private:

    void writeSecrets();
    void writeSavedSearches();
    void readSavedSearches();

    API api;
    std::vector<std::unique_ptr<GithubSearchHandler>> search_handlers_;
    mutable std::mutex saved_searches_mutex_;
    std::vector<SavedSearch> saved_searches_;
};
