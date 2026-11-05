#include "services/notifier.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QVariantMap>

#ifdef PIXELFX_HAS_DBUS
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#endif

using namespace Qt::StringLiterals;

namespace pixelfx::services {

namespace {

[[maybe_unused]] const auto kService = u"org.freedesktop.Notifications"_s;
[[maybe_unused]] const auto kPath = u"/org/freedesktop/Notifications"_s;
[[maybe_unused]] const auto kPortal = u"org.freedesktop.portal.Desktop"_s;
[[maybe_unused]] const auto kPortalPath = u"/org/freedesktop/portal/desktop"_s;

bool inFlatpak()
{
    return QFileInfo::exists(u"/.flatpak-info"_s);
}

} // namespace

Notifier::Notifier(QObject* parent)
    : QObject(parent)
{
#ifdef PIXELFX_HAS_DBUS
    if (!inFlatpak()) {
        QDBusConnection::sessionBus().connect(kService, kPath, kService, u"ActionInvoked"_s, this,
                                              SLOT(onActionInvoked(uint, QString)));
    }
#endif
}

Notifier::~Notifier() = default;

bool Notifier::isAvailable()
{
#ifdef PIXELFX_HAS_DBUS
    const QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    return bus != nullptr && bus->isServiceRegistered(inFlatpak() ? kPortal : kService).value();
#else
    return false;
#endif
}

void Notifier::notify(const QString& title, const QString& body)
{
#ifdef PIXELFX_HAS_DBUS
    if (inFlatpak()) {
        // The portal names the app from the sandbox; a click opens the app's window.
        QDBusMessage call = QDBusMessage::createMethodCall(
            kPortal, kPortalPath, u"org.freedesktop.portal.Notification"_s, u"AddNotification"_s);
        call << u"batch"_s
             << QVariantMap{{u"title"_s, title}, {u"body"_s, body}, {u"priority"_s, u"normal"_s}};
        QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
        return;
    }
    const QString desktopId = QGuiApplication::desktopFileName(); // set by Application
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kService, u"Notify"_s);
    call << QGuiApplication::applicationDisplayName() << m_last << desktopId << title << body
         << QStringList{u"default"_s, tr("Show PixelFx")} << QVariantMap{{u"desktop-entry"_s, desktopId}}
         << -1;
    const QDBusReply<uint> reply = QDBusConnection::sessionBus().call(call);
    if (reply.isValid()) {
        m_last = reply.value();
    }
#else
    Q_UNUSED(title)
    Q_UNUSED(body)
#endif
}

void Notifier::onActionInvoked(uint id, const QString& action)
{
    if (id == m_last && action == u"default"_s) {
        Q_EMIT activated();
    }
}

} // namespace pixelfx::services
