#pragma once

class QCoreApplication;

namespace pixelfx::core {

/// Installs the English plural forms ("1 photo", "2 photos") on `app` (ADR-014); false when the
/// build has none (no Qt Linguist tools), and every count then reads in the plural.
bool installPluralForms(QCoreApplication& app);

} // namespace pixelfx::core
