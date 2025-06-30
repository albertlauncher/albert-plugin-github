// Copyright (c) 2025-2025 Manuel Schneider

#include "configwidget.h"
#include "handlers.h"
#include "plugin.h"
#include <QMouseEvent>
#include <QStyledItemDelegate>
#include <albert/oauth.h>
#include <albert/oauthconfigwidget.h>
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace std;

class RemoveButtonDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RemoveButtonDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QRect decorationRect(const QStyleOptionViewItem &option) const
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, opt.index); // populates icon, etc.

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        return style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease &&
            index.column() == 0 && index.parent().isValid())
        {
            auto *me = static_cast<QMouseEvent *>(event);
            QRect iconRect = decorationRect(option);

            if (iconRect.contains(me->pos()))
            {
                emit removeRequested(index);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void removeRequested(const QModelIndex &index);

};

class SavedSearchItemModel : public QAbstractItemModel
{
    const vector<unique_ptr<GithubSearchHandler>> &handlers_;
    QIcon remove_icon = qApp->style()->standardIcon(QStyle::SP_LineEditClearButton);

public:

    SavedSearchItemModel(const vector<unique_ptr<GithubSearchHandler>> &handlers,
                         QObject *parent = nullptr)
        : QAbstractItemModel(parent), handlers_(handlers)
    {
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override
    {
        if (!hasIndex(row, column, parent))
            return {};

        if (!parent.isValid())
            return createIndex(row, column, static_cast<quintptr>(-1));

        return createIndex(row, column, parent.row());
    }

    QModelIndex parent(const QModelIndex &index) const override
    {
        if (!index.isValid() || index.internalId() == static_cast<quintptr>(-1))
            return {};
        return createIndex(static_cast<int>(index.internalId()), 0, static_cast<quintptr>(-1));
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        if (!parent.isValid())
            return handlers_.size();
        if (parent.internalId() == static_cast<quintptr>(-1)) {
            int handler_row = parent.row();
            if (handler_row >= 0 && handler_row < static_cast<int>(handlers_.size()))
                return static_cast<int>(handlers_[handler_row]->savedSearches().size() + 1); // virt
        }
        return 0;
    }

    int columnCount(const QModelIndex &) const override { return 2; }

    QVariant headerData(int section, Qt::Orientation, int role) const override
    {
        if (role == Qt::DisplayRole)
            return section == 0 ? ConfigWidget::tr("Title")
                                : ConfigWidget::tr("Query");
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid())
            return Qt::NoItemFlags;
        else if (index.parent().isValid())
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        else
            return Qt::ItemIsEnabled;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
            return {};

        else if (auto parent = index.parent();
                 !parent.isValid())
        {
            if (role == Qt::DisplayRole)
            {
                const auto &h = handlers_.at(index.row());
                return index.column() == 0 ? h->name() : QString{};
            }
            else if (role == Qt::FontRole)
            {
                QFont font;
                font.setItalic(true);
                return font;
            }
        }
        else
        {
            if (index.row() == (int)handlers_.at(parent.row())->savedSearches().size())  // vrow
            {
                if (role == Qt::DisplayRole)
                    return index.column() == 0 ? ConfigWidget::tr("New search") : u"…"_s;
                else if (role == Qt::ForegroundRole)
                    return qApp->palette().placeholderText();
            }
            else
            {
                if (role == Qt::DisplayRole || role == Qt::EditRole)
                {
                    const auto ss = handlers_.at(parent.row())->savedSearches().at(index.row());
                    return index.column() == 0 ? ss.first : ss.second;
                }
                else if (role == Qt::DecorationRole && index.column() == 0)
                {
                    return remove_icon;
                }
            }
        }
        return {};
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (!index.isValid() || role != Qt::EditRole || value.toString().isEmpty())
            return false;

        else if (auto parent = index.parent();
                 parent.isValid())
        {
            if (index.row() == (int)handlers_.at(parent.row())->savedSearches().size())  // vrow
                insertRows(index.row(), 1, parent);

            const auto &h = handlers_.at(parent.row());
            auto saved_searches = h->savedSearches();
            if (index.column() == 0)
                saved_searches[index.row()].first = value.toString();
            else
                saved_searches[index.row()].second = value.toString();
            h->setSavedSearches(saved_searches);
            emit dataChanged(index, index);
            return true;
        }
        return false;
    }

    bool insertRows(int row, int count, const QModelIndex &parent = {}) override
    {
        if (parent.isValid())
        {
            const auto &h = handlers_.at(parent.row());
            auto saved_searches = h->savedSearches();
            beginInsertRows(parent, row, row + count - 1);
            for (int i = 0; i < count; ++i)
                saved_searches.emplace_back(ConfigWidget::tr("New search"), QString{});
            h->setSavedSearches(saved_searches);
            endInsertRows();
            return true;
        }
        return false;
    }

    bool removeRows(int row, int count, const QModelIndex &parent = {}) override
    {
        if (parent.isValid())
        {
            const auto &h = handlers_.at(parent.row());
            auto saved_searches = h->savedSearches();
            beginRemoveRows(parent, row, row + count - 1);
            saved_searches.erase(saved_searches.begin() + row,
                                 saved_searches.begin() + row + count);
            h->setSavedSearches(saved_searches);
            endRemoveRows();
            return true;
        }
        return false;
    }
};


ConfigWidget::ConfigWidget(Plugin &p, OAuth2 &oauth) :
    plugin_(p)
{
    ui.setupUi(this);

    auto oaw = new OAuthConfigWidget(oauth);
    ui.groupBox_oauth->layout()->addWidget(oaw);
    ui.groupBox_oauth->layout()->setContentsMargins({});

    const auto docs = u"https://docs.github.com/search-github/searching-on-github/"_s;
    const auto docs_users = docs + u"searching-users"_s;
    const auto docs_repos = docs + u"searching-for-repositories"_s;
    const auto docs_issues = docs + u"searching-issues-and-pull-requests"_s;

    ui.label_seach_info->setText(tr("See the GitHub [user](%1), [repo](%2) "
                                    "and [issue](%3) search documentation.")
                                     .arg(docs_users, docs_repos, docs_issues));
    ui.label_seach_info->setOpenExternalLinks(true);
    ui.label_seach_info->setWordWrap(true);
    ui.label_seach_info->setTextFormat(Qt::MarkdownText);

    auto model = new SavedSearchItemModel(plugin_.search_handlers_, this);
    ui.treeView->setModel(model);

    auto *delegate = new RemoveButtonDelegate(ui.treeView);
    ui.treeView->setItemDelegate(delegate);

    connect(delegate, &RemoveButtonDelegate::removeRequested,
            this, [model](const QModelIndex &index)
            { model->removeRow(index.row(), index.parent()); });

    ui.treeView->expandAll();

    ui.treeView->resizeColumnToContents(0);
    connect(ui.treeView->model(), &QAbstractItemModel::dataChanged,
            this, [tv=ui.treeView] { tv->resizeColumnToContents(0); });
}

#include "configwidget.moc"
