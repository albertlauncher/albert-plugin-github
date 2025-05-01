// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "github.h"
#include <albert/extensionplugin.h>
#include <albert/filedownloader.h>
#include <albert/globalqueryhandler.h>
#include <albert/oauth.h>
#include <albert/urlhandler.h>
#include <mutex>

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
    QString makeAvatarPath(const QString owner) const;
    QString makeMaskedIconUrl(const QString owner) const;
    std::variant<albert::util::FileDownloader*, QString>
    downloadAvatar(const QString &name, const QUrl &url);

    QString trigger;
    github::RestApi api;
    std::vector<SavedSearch> saved_searches;
    QString user_name;
    QString user_login;
    QString user_bio;
    QString user_url;
    std::mutex downloads_mutex;
    std::map<QString, albert::util::FileDownloader*> downloads;

    static const QStringList default_icon_urls;

    friend class GitHubItem;

};
