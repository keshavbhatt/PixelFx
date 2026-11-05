#pragma once

#include <QObject>
#include <QString>

namespace pixelfx::services {

/// The desktop's notifications (FEATURES S6, ADR-011): on Linux the notification portal inside
/// Flatpak and org.freedesktop.Notifications elsewhere, over D-Bus. Elsewhere, and when no
/// notification service answers, nothing is shown.
class Notifier : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Notifier)

public:
    explicit Notifier(QObject* parent = nullptr);
    ~Notifier() override;

    /// Shows one notification, replacing the last one this sent.
    void notify(const QString& title, const QString& body);
    /// Whether a notification service can be reached.
    [[nodiscard]] bool isAvailable() const;

Q_SIGNALS:
    /// The notification was clicked: the window should come forward.
    void activated();

private Q_SLOTS:
    void onActionInvoked(uint id, const QString& action);

private:
    uint m_last = 0;
};

} // namespace pixelfx::services
