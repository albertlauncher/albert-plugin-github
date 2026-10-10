// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
#include "handlers.h"
#include "plugin.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <albert/oauth.h>
#include <albert/oauthconfigwidget.h>
#include <limits>
#include <optional>
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

namespace
{
optional<SavedSearch> editSearch(QWidget *parent, optional<SavedSearch> current = {})
{
    QDialog dialog(parent);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumWidth(512);
    dialog.setWindowTitle(current ? ConfigWidget::tr("Edit saved search")
                                  : ConfigWidget::tr("Add saved search"));

    auto *combobox_type = new QComboBox(&dialog);
    combobox_type->addItem(ConfigWidget::tr("Issue"));
    combobox_type->addItem(ConfigWidget::tr("Repository"));
    combobox_type->addItem(ConfigWidget::tr("User"));
    combobox_type->setCurrentIndex(current ? (int)current->type : 0);

    auto *title_edit = new QLineEdit(current ? current->title : QString{}, &dialog);
    auto *query_edit = new QLineEdit(current ? current->query : QString{}, &dialog);
    title_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    query_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *cache_enabled = new QCheckBox(ConfigWidget::tr("Cache results"), &dialog);
    cache_enabled->setChecked(current && current->cache_enabled);

    auto *cache_limit = new QSpinBox(&dialog);
    cache_limit->setRange(0, numeric_limits<int>::max());
    cache_limit->setSpecialValueText(ConfigWidget::tr("All items"));
    cache_limit->setSingleStep(100);
    cache_limit->setSuffix(ConfigWidget::tr(" items"));
    cache_limit->setValue(current ? current->cache_limit : 100);

    auto *cache_interval = new QSpinBox(&dialog);
    cache_interval->setRange(0, numeric_limits<int>::max());
    cache_interval->setSuffix(ConfigWidget::tr(" hours"));
    cache_interval->setSpecialValueText(ConfigWidget::tr("Once"));
    cache_interval->setValue(current ? current->cache_interval : 0);
    cache_limit->setEnabled(cache_enabled->isChecked());
    cache_interval->setEnabled(cache_enabled->isChecked());
    QObject::connect(cache_enabled, &QCheckBox::toggled, &dialog,
                     [cache_limit, cache_interval](bool enabled) {
                         cache_limit->setEnabled(enabled);
                         cache_interval->setEnabled(enabled);
                     });

    auto *form = new QFormLayout(&dialog);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(ConfigWidget::tr("Search type"), combobox_type);
    form->addRow(ConfigWidget::tr("Title"), title_edit);
    form->addRow(ConfigWidget::tr("Query"), query_edit);
    form->addRow(cache_enabled);
    form->addRow(ConfigWidget::tr("Cache limit"), cache_limit);
    form->addRow(ConfigWidget::tr("Cache interval"), cache_interval);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || title_edit->text().trimmed().isEmpty())
        return {};

    return SavedSearch{static_cast<SavedSearch::Type>(combobox_type->currentIndex()),
                       title_edit->text().trimmed(),
                       query_edit->text(),
                       (uint)cache_limit->value(),
                       (uint)cache_interval->value(),
                       cache_enabled->isChecked()};
}
}

ConfigWidget::ConfigWidget(Plugin &p, OAuth2 &oauth) : plugin_(p)
{
    ui.setupUi(this);

    auto oaw = new OAuthConfigWidget(oauth);
    ui.groupBox_oauth->layout()->addWidget(oaw);
    ui.groupBox_oauth->layout()->setContentsMargins({});

    const auto docs = u"https://docs.github.com/search-github/searching-on-github/"_s;
    const auto docs_users = docs + u"searching-users"_s;
    const auto docs_repos = docs + u"searching-for-repositories"_s;
    const auto docs_issues = docs + u"searching-issues-and-pull-requests"_s;

    ui.label_seach_info->setText(tr("See docs for [user](%1), [repo](%2) and [issue](%3) search.")
                                     .arg(docs_users, docs_repos, docs_issues));
    ui.label_seach_info->setOpenExternalLinks(true);
    ui.label_seach_info->setWordWrap(true);
    ui.label_seach_info->setTextFormat(Qt::MarkdownText);

    auto refreshList = [this] {
        ui.savedSearchesList->clear();
        const auto searches = plugin_.savedSearches();
        for (size_t si = 0; si < searches.size(); ++si)
        {
            auto *item = new QListWidgetItem(searches[si].title, ui.savedSearchesList);
            item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(si));
        }
    };

    auto updateButtons = [this] {
        const bool selected = ui.savedSearchesList->currentItem();
        ui.editSearchButton->setEnabled(selected);
        ui.removeSearchButton->setEnabled(selected);
    };

    refreshList();
    updateButtons();

    connect(ui.savedSearchesList, &QListWidget::currentRowChanged, this,
            [updateButtons] { updateButtons(); });

    connect(ui.addSearchButton, &QPushButton::clicked, this, [this, refreshList] {
        if (auto saved = editSearch(this))
        {
            auto searches = plugin_.savedSearches();
            searches.push_back(*saved);
            plugin_.setSavedSearches(searches);
            refreshList();
        }
    });

    auto editSelected = [this, refreshList, updateButtons] {
        auto *item = ui.savedSearchesList->currentItem();
        if (!item)
            return;
        const auto index = static_cast<int>(item->data(Qt::UserRole).toULongLong());
        auto searches = plugin_.savedSearches();
        if (auto edited = editSearch(this, searches.at(index)))
        {
            searches[index] = *edited;
            plugin_.setSavedSearches(searches);
            refreshList();
            updateButtons();
        }
    };

    auto removeSelected = [this, refreshList, updateButtons] {
        auto *item = ui.savedSearchesList->currentItem();
        if (!item)
            return;
        if (QMessageBox::question(this,
                                  tr("Remove saved search"),
                                  tr("Remove the saved search \"%1\"?").arg(item->text()),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes)
            return;
        const auto index = static_cast<int>(item->data(Qt::UserRole).toULongLong());
        auto searches = plugin_.savedSearches();
        searches.erase(searches.begin() + index);
        plugin_.setSavedSearches(searches);
        refreshList();
        updateButtons();
    };

    connect(ui.editSearchButton, &QPushButton::clicked, this, editSelected);
    connect(ui.removeSearchButton, &QPushButton::clicked, this, removeSelected);
    connect(ui.savedSearchesList, &QListWidget::itemDoubleClicked, this,
            [editSelected](QListWidgetItem *) { editSelected(); });

    ui.savedSearchesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.savedSearchesList, &QListWidget::customContextMenuRequested, this,
            [this, editSelected, removeSelected](const QPoint &pos) {
                if (auto *item = ui.savedSearchesList->itemAt(pos))
                    ui.savedSearchesList->setCurrentItem(item);
                else
                    return;

                QMenu menu(ui.savedSearchesList);
                menu.addAction(tr("Edit"), editSelected);
                menu.addAction(tr("Remove"), removeSelected);
                menu.exec(ui.savedSearchesList->viewport()->mapToGlobal(pos));
            });

    auto *delete_shortcut = new QShortcut(QKeySequence::Delete, ui.savedSearchesList);
    delete_shortcut->setContext(Qt::WidgetShortcut);
    connect(delete_shortcut, &QShortcut::activated, this, removeSelected);
}
