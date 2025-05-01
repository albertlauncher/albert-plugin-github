// Copyright (c) 2025-2025 Manuel Schneider

#pragma once
#include "ui_configwidget.h"
#include <QWidget>
class Plugin;

class ConfigWidget final : public QWidget
{
    Q_OBJECT

public:

    explicit ConfigWidget(Plugin &plugin);

private:

    void onButton_new();
    void onButton_remove();
    void onButton_restoreDefaults();

    Ui::ConfigWidget ui;
    Plugin &plugin;

};
