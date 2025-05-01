// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
#include "plugin.h"
#include <QStyle>
#include <albert/albert.h>
#include <albert/oauth.h>
#include <albert/oauthconfigwidget.h>
#include <albert/widgetsutil.h>
using namespace albert;
using namespace std;
using namespace util;


class SavedSearchesModel final : public QAbstractTableModel
{
    Plugin &plugin_;

public:
    SavedSearchesModel(Plugin &plugin, QObject *parent):
        QAbstractTableModel(parent),
        plugin_(plugin)
    {
        connect(&plugin, &Plugin::savedSearchesChanged, this, [this](){
            beginResetModel();
            endResetModel();
        });
    }

    int rowCount(const QModelIndex&) const override { return plugin_.savedSearches().size(); }

    int columnCount(const QModelIndex&) const override { return 2; }

    Qt::ItemFlags flags(const QModelIndex&) const override
    { return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable; }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        {
            if (section == 0)
                return ConfigWidget::tr("Name");
            else if (section == 1)
                return ConfigWidget::tr("Query");
        }
        return {};
    }

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= static_cast<int>(plugin_.savedSearches().size()))
            return QVariant();

        const auto &ss = plugin_.savedSearches()[static_cast<ulong>(index.row())];

        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
        {
            if (index.column() == 0)
                return ss.name;
            else if (index.column() == 1)
                return ss.query;
        }
        }
        return {};
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (!index.isValid())
            return false;

        if (role == Qt::EditRole)
        {
            auto ss = plugin_.savedSearches();

            if (index.column() == 0)
            {
                ss[index.row()].name = value.toString();
                plugin_.setSavedSearches(ss);
                return true;
            }
            else if (index.column() == 1)
            {
                ss[index.row()].query = value.toString();
                plugin_.setSavedSearches(ss);
                return true;
            }
        }
        return false;
    }
};

ConfigWidget::ConfigWidget(Plugin &p):
    plugin(p)
{
    ui.setupUi(this);

    auto oaw = new OAuthConfigWidget(plugin.oauth);
    ui.groupBox_oauth->layout()->addWidget(oaw);

    ui.tableView_searches->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui.tableView_searches->horizontalHeader()->setStretchLastSection(true);
    ui.tableView_searches->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui.tableView_searches->setModel(new SavedSearchesModel(plugin, ui.tableView_searches));

    connect(ui.pushButton_new, &QPushButton::clicked,
            this, &ConfigWidget::onButton_new);

    connect(ui.pushButton_remove, &QPushButton::clicked,
            this, &ConfigWidget::onButton_remove);

    connect(ui.pushButton_restoreDefaults, &QPushButton::clicked,
            this, &ConfigWidget::onButton_restoreDefaults);
}

void ConfigWidget::onButton_new()
{
    auto ss = plugin.savedSearches();
    ss.push_back({"New Search", ""});
    plugin.setSavedSearches(ss);
}

void ConfigWidget::onButton_remove()
{
    auto index = ui.tableView_searches->currentIndex();
    if (!index.isValid())
        return;

    auto ss = plugin.savedSearches();
    ss.erase(ss.begin() + index.row());
    plugin.setSavedSearches(ss);
}

void ConfigWidget::onButton_restoreDefaults()
{
    if (question(tr("Do you really want to restore the default saved searches?"))
        == QMessageBox::Yes)
        plugin.restoreDefaultSavedSearches();
}
