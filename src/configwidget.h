// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "ui_configwidget.h"
#include <QWidget>
class Plugin;
namespace albert { class OAuth2; }

class ConfigWidget final : public QWidget
{
    Q_OBJECT

public:

    explicit ConfigWidget(Plugin &, albert::OAuth2 &);

    Ui::ConfigWidget ui;
    Plugin &plugin_;

};
