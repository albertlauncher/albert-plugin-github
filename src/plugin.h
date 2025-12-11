// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "github.h"
#include <albert/extensionplugin.h>
#include <albert/oauth.h>
#include <albert/globalqueryhandler.h>
#include <albert/urlhandler.h>
#include <memory>
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

    std::vector<albert::Extension*> extensions() override;
    QWidget* buildConfigWidget() override;
    QString defaultTrigger() const override;
    void handle(const QUrl &) override;
    std::vector<albert::RankItem> rankItems(albert::QueryContext &) override;

    // void writeSavedSearches();
    // void readSavedSearches();
    // void restoreDefaultSavedSearches();

    void authorizedInitialization();

    github::RestApi api;
    std::vector<std::unique_ptr<GithubSearchHandler>> search_handlers_;

};
