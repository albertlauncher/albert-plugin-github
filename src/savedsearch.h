// Copyright (c) 2025-2026 Manuel Schneider

#pragma once
#include <QString>

struct SavedSearch
{
    enum class Type { Issues, Repositories, Users } type;
    QString title;
    QString query;
    uint cache_limit = 0;
    uint cache_interval = 0;
    bool operator==(const SavedSearch&) const = default;
};
