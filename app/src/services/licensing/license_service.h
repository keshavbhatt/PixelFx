// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include "AccountLicenseManager.h"
#include "core/licensing/pro_features.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>

namespace pixelfx::services {

/// The account and its Pro state (one instance, owned by the window, or by the command line
/// batch): wraps AccountLicenseManager, which keeps the Account ID, the 14-day trial every new
/// install starts with, and the server's last answer, checked on start and weekly. PixelFx keeps
/// working offline with that answer.
class LicenseService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LicenseService)

public:
    enum class Tier
    {
        Pro,   ///< a purchase (or a Ktechpit Pass) covers this Account ID
        Trial, ///< inside the 14-day trial
        Free,  ///< the trial is over and nothing was bought
    };
    Q_ENUM(Tier)

    explicit LicenseService(QObject* parent = nullptr);
    ~LicenseService() override;

    /// Loads or makes the Account ID and reads the last answer; with `online`, asks the server
    /// when a check is due.
    void start(bool online = true);
    /// Asks the server now (Check again).
    void refresh();

    [[nodiscard]] QString accountId() const { return m_accountId; }
    /// Virtual so tests can stand in for each tier.
    [[nodiscard]] virtual Tier tier() const;
    /// Pro features may run: bought, or inside the trial.
    [[nodiscard]] bool hasPro() const { return tier() != Tier::Free; }
    /// The trial's last moment, invalid before the Account ID is made.
    [[nodiscard]] QDateTime trialEnd() const;
    [[nodiscard]] int trialDays() const;
    /// Days of the trial left, counting a part day as one; 0 once it is over.
    [[nodiscard]] int trialDaysLeft() const;
    /// A purchase for good, rather than until a date.
    [[nodiscard]] bool isForever() const;
    /// When a purchase for a time runs out, invalid for Forever.
    [[nodiscard]] QDateTime paidUntil() const;
    /// When the server last answered, invalid before its first answer.
    [[nodiscard]] QDateTime lastChecked() const;
    [[nodiscard]] bool checking() const { return m_checking; }

    /// The PixelFx checkout for this Account ID, telling it which prompt sent the buyer.
    [[nodiscard]] QUrl checkoutUrl(core::licensing::Source source) const;
    /// The self-service portal (purchases, receipts, the Ktechpit Pass).
    [[nodiscard]] QUrl portalUrl() const;
    /// The portal's Ktechpit Pass page with PixelFx and this Account ID filled in.
    [[nodiscard]] QUrl passUrl() const;
    /// The PixelFx page on ktechpit.com, for the command line.
    [[nodiscard]] static QUrl productUrl();

Q_SIGNALS:
    void statusChanged();
    void checkingChanged(bool checking);

private:
    AccountLicenseManager m_manager;
    AccountLicenseManager::LicenseStatus m_status;
    QString m_accountId;
    bool m_checking = false;
};

} // namespace pixelfx::services
