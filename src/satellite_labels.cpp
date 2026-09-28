#include "satellite_labels.h"
#include <QCoreApplication>
#include <cmath>

namespace Orbit {
QString displayName(const Satellite &satellite)
{
    if (satellite.number == 25544)
        return QCoreApplication::translate("Orbit", "国际空间站");
    if (satellite.number == 48274)
        return QCoreApplication::translate("Orbit", "天和核心舱（CSS）");
    if (satellite.number == 36086)
        return QCoreApplication::translate("Orbit", "探索号实验舱");
    if (satellite.number == 49044)
        return QCoreApplication::translate("Orbit", "科学号实验舱");
    if (satellite.number == 53239)
        return QCoreApplication::translate("Orbit", "问天实验舱");
    if (satellite.number == 54216)
        return QCoreApplication::translate("Orbit", "梦天实验舱");
    if (satellite.number == 20580)
        return QCoreApplication::translate("Orbit", "哈勃空间望远镜");
    return satellite.name;
}
QString illuminationName(int value)
{
    return value == 2   ? QCoreApplication::translate("Orbit", "地球阴影")
           : value == 1 ? QCoreApplication::translate("Orbit", "半影")
                        : QCoreApplication::translate("Orbit", "阳光照射");
}
QString directionName(double azimuth)
{
    static const char *names[]{QT_TRANSLATE_NOOP("Orbit", "北"), QT_TRANSLATE_NOOP("Orbit", "东北"),
                               QT_TRANSLATE_NOOP("Orbit", "东"), QT_TRANSLATE_NOOP("Orbit", "东南"),
                               QT_TRANSLATE_NOOP("Orbit", "南"), QT_TRANSLATE_NOOP("Orbit", "西南"),
                               QT_TRANSLATE_NOOP("Orbit", "西"), QT_TRANSLATE_NOOP("Orbit", "西北")};
    return QCoreApplication::translate("Orbit", names[static_cast<int>(std::lround(azimuth / 45.0)) % 8]);
}
} // namespace Orbit
