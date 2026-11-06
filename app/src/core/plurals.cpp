#include "core/plurals.h"

#include <QCoreApplication>
#include <QTranslator>

namespace pixelfx::core {

bool installPluralForms(QCoreApplication& app)
{
    auto* plurals = new QTranslator(&app);
    if (!plurals->load(QStringLiteral(":/i18n/pixelfx_en.qm"))) {
        delete plurals;
        return false;
    }
    return QCoreApplication::installTranslator(plurals);
}

} // namespace pixelfx::core
