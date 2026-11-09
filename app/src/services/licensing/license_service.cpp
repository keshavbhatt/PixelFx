// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "services/licensing/license_service.h"

#include "services/licensing/license_config.h"
#include "services/logging.h"

#include <QUrlQuery>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace pixelfx::services {

namespace {

constexpr qint64 kDay = 24LL * 60LL * 60LL;

bool lifetime(const QString& type)
{
    return type.trimmed().toLower().contains(u"lifetime"_s);
}

} // namespace

LicenseService::LicenseService(QObject* parent)
    : QObject(parent)
    , m_manager(appLicenseConfig())
{
    connect(&m_manager, &AccountLicenseManager::licenseCheckInProgressChanged, this, [this](bool inProgress) {
        m_checking = inProgress;
        Q_EMIT checkingChanged(inProgress);
    });
    connect(&m_manager, &AccountLicenseManager::licenseStatusUpdated, this,
            [this](const AccountLicenseManager::LicenseStatus& status) {
                m_status = status;
                qCInfo(lcPro) << "pro:"
                              << (status.active             ? "bought"
                                  : status.evaluationActive ? "trial"
                                                            : "free")
                              << (status.fromNetworkError ? "(last known)" : "");
                Q_EMIT statusChanged();
            });
    connect(&m_manager, &AccountLicenseManager::serialReady, this,
            [this](const QString& serial) { m_accountId = serial; });
}

LicenseService::~LicenseService() = default;

void LicenseService::start(bool online)
{
#ifdef QT_DEBUG
    // Debug builds only (rewrite kit R-ADR-010): PIXELFX_DEBUG_LICENSE=pro|trial|free|neterr
    // stands in for the server so every state can be seen.
    if (const QString sim = qEnvironmentVariable("PIXELFX_DEBUG_LICENSE"); !sim.isEmpty()) {
        using S = AccountLicenseManager::DebugLicenseState;
        m_manager.setDebugLicenseState(sim == u"pro"_s     ? S::Pro
                                       : sim == u"trial"_s ? S::EvaluationActive
                                       : sim == u"free"_s  ? S::EvaluationExpired
                                                           : S::NetworkError);
        online = true; // the simulated answer comes from checkPurchase
    }
#endif
    m_manager.ensureSerial();
    m_accountId = m_manager.serial();
    // The last answer, until the server's comes (or for good, offline).
    m_status.evaluationActive = m_manager.isEvaluationActive();
    m_status.hasPaidAccess = m_manager.hasPremiumAccess();
    m_status.active = m_status.hasPaidAccess && !m_status.evaluationActive;
    m_status.evaluationEndTimestamp = m_manager.storeValue(u"license/eval_start_utc"_s, 0LL).toLongLong() +
                                      m_manager.config().evaluationDurationSecs;
    m_status.licenseType = m_manager.storeValue(u"license/type"_s).toString();
    m_status.expiryTimestamp = m_manager.storeValue(u"license/expiry_timestamp"_s, 0LL).toLongLong();
    Q_EMIT statusChanged();
    if (online) {
        m_manager.checkPurchase(m_accountId);
    }
}

void LicenseService::refresh()
{
    m_manager.checkPurchase(m_accountId, /*force=*/true);
}

LicenseService::Tier LicenseService::tier() const
{
    if (m_status.active) {
        return Tier::Pro;
    }
    return m_status.evaluationActive ? Tier::Trial : Tier::Free;
}

QDateTime LicenseService::trialEnd() const
{
    return m_status.evaluationEndTimestamp > 0
               ? QDateTime::fromSecsSinceEpoch(m_status.evaluationEndTimestamp)
               : QDateTime();
}

int LicenseService::trialDays() const
{
    return static_cast<int>(m_manager.config().evaluationDurationSecs / kDay);
}

int LicenseService::trialDaysLeft() const
{
    if (tier() != Tier::Trial) {
        return 0;
    }
    const qint64 left = m_status.evaluationEndTimestamp - QDateTime::currentSecsSinceEpoch();
    return static_cast<int>(qMax<qint64>(0, (left + kDay - 1) / kDay));
}

bool LicenseService::isForever() const
{
    return lifetime(m_status.licenseType) || m_status.expiryTimestamp <= 0;
}

QDateTime LicenseService::paidUntil() const
{
    return isForever() ? QDateTime() : QDateTime::fromSecsSinceEpoch(m_status.expiryTimestamp);
}

QDateTime LicenseService::lastChecked() const
{
    const qint64 at = m_manager.storeValue(u"license/last_check_utc"_s, 0LL).toLongLong();
    return at > 0 ? QDateTime::fromSecsSinceEpoch(at) : QDateTime();
}

QUrl LicenseService::checkoutUrl(core::licensing::Source source) const
{
    QUrl url(m_manager.checkoutUrl());
    QUrlQuery query(url);
    query.addQueryItem(u"source"_s, core::licensing::sourceName(source));
    url.setQuery(query);
    return url;
}

QUrl LicenseService::portalUrl() const
{
    return QUrl(m_manager.selfServicePortalUrl());
}

QUrl LicenseService::passUrl() const
{
    QUrl url(QString::fromLatin1(kKtechpitServer) + u"user/ktechpit_pass.php"_s);
    QUrlQuery query;
    query.addQueryItem(u"app"_s, u"pixelfx"_s);
    query.addQueryItem(u"acc_id"_s, m_accountId);
    url.setQuery(query);
    return url;
}

QUrl LicenseService::passCheckoutUrl() const
{
    QUrl url(QString::fromLatin1(kKtechpitServer) + u"checkout.php"_s);
    QUrlQuery query;
    query.addQueryItem(u"slug"_s, u"ktechpit-pass"_s);
    query.addQueryItem(u"accountId"_s, m_accountId);
    // The website counts pass visits and sales by the link they came from.
    query.addQueryItem(u"source"_s, u"in_app_pixelfx"_s);
    url.setQuery(query);
    return url;
}

QUrl LicenseService::productUrl()
{
    return QUrl(QString::fromLatin1(kKtechpitServer) + u"product.php?slug=pixelfx"_s);
}

} // namespace pixelfx::services
