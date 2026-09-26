#include "transitions.h"

#include <QHash>

namespace Transitions {

const QStringList &all()
{
    static const QStringList names = {
        QStringLiteral("none"),
        QStringLiteral("fade"), QStringLiteral("fadeblack"), QStringLiteral("fadewhite"),
        QStringLiteral("fadegrays"), QStringLiteral("fadefast"), QStringLiteral("fadeslow"),
        QStringLiteral("dissolve"), QStringLiteral("distance"), QStringLiteral("pixelize"),
        QStringLiteral("wipeleft"), QStringLiteral("wiperight"), QStringLiteral("wipeup"),
        QStringLiteral("wipedown"), QStringLiteral("wipetl"), QStringLiteral("wipetr"),
        QStringLiteral("wipebl"), QStringLiteral("wipebr"),
        QStringLiteral("slideleft"), QStringLiteral("slideright"), QStringLiteral("slideup"),
        QStringLiteral("slidedown"),
        QStringLiteral("smoothleft"), QStringLiteral("smoothright"), QStringLiteral("smoothup"),
        QStringLiteral("smoothdown"),
        QStringLiteral("coverleft"), QStringLiteral("coverright"), QStringLiteral("coverup"),
        QStringLiteral("coverdown"),
        QStringLiteral("revealleft"), QStringLiteral("revealright"), QStringLiteral("revealup"),
        QStringLiteral("revealdown"),
        QStringLiteral("circlecrop"), QStringLiteral("rectcrop"), QStringLiteral("circleopen"),
        QStringLiteral("circleclose"), QStringLiteral("radial"),
        QStringLiteral("vertopen"), QStringLiteral("vertclose"), QStringLiteral("horzopen"),
        QStringLiteral("horzclose"),
        QStringLiteral("diagtl"), QStringLiteral("diagtr"), QStringLiteral("diagbl"),
        QStringLiteral("diagbr"),
        QStringLiteral("hlslice"), QStringLiteral("hrslice"), QStringLiteral("vuslice"),
        QStringLiteral("vdslice"),
        QStringLiteral("hlwind"), QStringLiteral("hrwind"), QStringLiteral("vuwind"),
        QStringLiteral("vdwind"),
        QStringLiteral("hblur"), QStringLiteral("squeezeh"), QStringLiteral("squeezev"),
        QStringLiteral("zoomin"),
    };
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
