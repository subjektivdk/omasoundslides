// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#include "transitions.h"

#include <QHash>

namespace Transitions {

namespace {
constexpr double Fast = 0.5;
constexpr double Medium = 1.0;
constexpr double Slow = 2.0;
}

const QList<Preset> &presets()
{
    static const QList<Preset> list = {
        {QStringLiteral("cut"), QStringLiteral("Straight cut"), QStringLiteral("none"), 0},
        {QStringLiteral("crossfade-fast"), QStringLiteral("Crossfade – Fast"), QStringLiteral("fade"), Fast},
        {QStringLiteral("crossfade-medium"), QStringLiteral("Crossfade – Medium"), QStringLiteral("fade"), Medium},
        {QStringLiteral("crossfade-slow"), QStringLiteral("Crossfade – Slow"), QStringLiteral("fade"), Slow},
        {QStringLiteral("fadeout-fast"), QStringLiteral("Fade out/in – Fast"), QStringLiteral("fadeblack"), Fast},
        {QStringLiteral("fadeout-medium"), QStringLiteral("Fade out/in – Medium"), QStringLiteral("fadeblack"), Medium},
        {QStringLiteral("fadeout-slow"), QStringLiteral("Fade out/in – Slow"), QStringLiteral("fadeblack"), Slow},
    };
    return list;
}

const QStringList &all()
{
    static const QStringList names = {QStringLiteral("none"), QStringLiteral("fade"), QStringLiteral("fadeblack")};
    return names;
}

QString canonical(const QString &name)
{
    static const QHash<QString, QString> aliases = {
        {QStringLiteral("crossfade"), QStringLiteral("fade")},
        {QStringLiteral("cut"), QStringLiteral("none")},
        {QStringLiteral("straightcut"), QStringLiteral("none")},
        {QStringLiteral("fadeout"), QStringLiteral("fadeblack")},
    };

    const QString key = name.trimmed().toLower();
    if (aliases.contains(key))
        return aliases.value(key);
    if (all().contains(key))
        return key;
    return {};
}

bool isCut(const QString &canonicalName)
{
    return canonicalName == QLatin1String("none");
}

}
