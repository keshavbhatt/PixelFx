// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include "AccountLicenseConfig.h"

#include <QCoreApplication>

namespace pixelfx::services {

/// The Ktechpit server PixelFx checks with and sells through.
inline constexpr auto kKtechpitServer = "https://ktechpit.com/USS/public/";

/// The app's AccountLicenseConfig: the shared Ktechpit server, app code PIXELFX, checkout slug
/// pixelfx, a 14-day evaluation. Settings follow the application name, so a --profile has its
/// own account.
inline AccountLicenseConfig appLicenseConfig()
{
    const QString server = QString::fromLatin1(kKtechpitServer);
    AccountLicenseConfig cfg;
    cfg.appCode = QStringLiteral("PIXELFX");
    cfg.appName = QStringLiteral("PixelFx");
    cfg.settingsOrgName = QCoreApplication::organizationName();
    cfg.settingsAppName = QCoreApplication::applicationName();
    cfg.checkStatusEndpoint = server + QStringLiteral("check/status.php");
    cfg.checkoutUrlTemplate = server + QStringLiteral("checkout.php?slug=pixelfx&accountId=%1");
    cfg.selfServicePortalUrl = server + QStringLiteral("user/");
    cfg.evaluationDurationSecs = 14LL * 24LL * 60LL * 60LL;
    return cfg;
}

} // namespace pixelfx::services
