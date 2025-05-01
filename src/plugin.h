// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "github.h"
#include <albert/extensionplugin.h>
#include <albert/globalqueryhandler.h>
#include <albert/oauth.h>
#include <albert/urlhandler.h>

struct SavedSearch {
    QString name;
    QString query;
    bool operator==(SavedSearch const &) const = default;
};

class Plugin final : public albert::ExtensionPlugin,
                     public albert::GlobalQueryHandler,
                     private albert::UrlHandler
{
    ALBERT_PLUGIN
public:

    Plugin();
    ~Plugin() override;

    void setSavedSearches(const std::vector<SavedSearch>&);
    const std::vector<SavedSearch> &savedSearches() const;
    void restoreDefaultSavedSearches();

    albert::util::OAuth2 oauth;

signals:

    void savedSearchesChanged();

private:

    void handle(const QUrl &) override;
    void setTrigger(const QString&) override;
    QWidget* buildConfigWidget() override;
    std::vector<albert::RankItem> handleGlobalQuery(const albert::Query &) override;
    void handleTriggerQuery(albert::Query&) override;

    void readSecrets();
    void writeSecrets() const;
    void authorizedInitialization();
    QString downloadAvatar(const QString &name, const QString &url);

    QString trigger;
    QString user_name;
    QString user_login;
    QString user_bio;
    QString user_url;
    std::vector<SavedSearch> saved_searches;
    github::RestApi api;

};
