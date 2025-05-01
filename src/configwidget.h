// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "ui_configwidget.h"
#include <QWidget>
class Plugin;
namespace albert::util { class OAuth2; }

class ConfigWidget final : public QWidget
{
    Q_OBJECT

public:

    explicit ConfigWidget(Plugin &, albert::util::OAuth2 &);

    Ui::ConfigWidget ui;
    Plugin &plugin_;

private:

    void updateViewHeight();

};
