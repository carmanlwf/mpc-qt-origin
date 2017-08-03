#include <QCoreApplication>
#include <QDBusPendingReply>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include "unix_qscreensaver.h"

QScreenSaver::QScreenSaver(QObject *parent)
    : QAbstractScreenSaver(parent),
      dbusScreensaver("org.freedesktop.ScreenSaver",
           "/ScreenSaver",
           "org.freedesktop.ScreenSaver"),
      dbusLogin("org.freedesktop.login1",
                "/org/freedesktop/login1",
                "org.freedesktop.login1.Manager",
               QDBusConnection::systemBus()),
      dbusLoginSelf("org.freedesktop.login1",
                    "/org/freedesktop/login1/user/self",
                    "org.freedesktop.login1.User",
                    QDBusConnection::systemBus())
{
}

QSet<QScreenSaver::Ability> QScreenSaver::abilities()
{
    QSet<Ability> abilities;

    if (dbusScreensaver.isValid())
        abilities << Inhibit << Uninhibit << LaunchSaver << LockScreen;

    if (dbusLogin.isValid()) {
        auto check = [&](QString method, QScreenSaver::Ability ability) {
            QDBusReply<QString> reply = dbusLogin.call(method);
            if (reply.isValid() && reply.value() == "yes")
                abilities << ability;
        };
        check("CanHibernate", Hibernate);
        check("CanSuspend", Suspend);
        check("CanPowerOff", Shutdown);
    }
    if (dbusLoginSelf.isValid()
            && dbusLoginSelf.metaObject()->indexOfMethod("Terminate()") != -1) {
        abilities << LogOff;
    }
    return abilities;
}

void QScreenSaver::inhibitSaver(const QString &reason)
{
    if (isInhibiting || inhibitCalled)
        return;

    inhibitCalled = true;

    QDBusPendingCall async = dbusScreensaver.asyncCall("Inhibit",
        QCoreApplication::applicationName(), reason);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusScreensaver_inhibit);
}

void QScreenSaver::uninhibitSaver()
{
    if (!isInhibiting)
        return;

    QDBusPendingCall async = dbusScreensaver.asyncCall("UnInhibit", inhibitToken);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusScreensaver_uninhibit);

}

void QScreenSaver::launchSaver()
{
    QDBusPendingCall async = dbusScreensaver.asyncCall("SetActive", true);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusScreensaver_launch);

}

void QScreenSaver::lockScreen()
{
    QDBusPendingCall async = dbusScreensaver.asyncCall("Lock");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusScreensaver_lock);

}

void QScreenSaver::hibernateSystem()
{
    if (!isInhibiting)
        return;

    QDBusPendingCall async = dbusLogin.asyncCall("Hibernate");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusLogin_hibernate);
}

void QScreenSaver::suspendSystem()
{
    if (!isInhibiting)
        return;

    QDBusPendingCall async = dbusLogin.asyncCall("Suspend");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusLogin_suspend);
}

void QScreenSaver::shutdownSystem()
{
    if (!isInhibiting)
        return;

    QDBusPendingCall async = dbusLogin.asyncCall("PowerOff");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusLogin_shutdown);
}


void QScreenSaver::logOff()
{
    QDBusPendingCall async = dbusLoginSelf.asyncCall("Terminate");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QScreenSaver::dbusLoginSelf_logoff);

    emit failed(LogOff);
}

void QScreenSaver::dbusScreensaver_inhibit(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<uint32_t> reply = *call;
    if (reply.isError()) {
        emit failed(Inhibit);
    } else {
        inhibitToken = reply.argumentAt<0>();
        isInhibiting = true;
        emit inhibitedSaver();
    }
    inhibitCalled = false;
    call->deleteLater();
}

void QScreenSaver::dbusScreensaver_uninhibit(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError()) {
        emit failed(Uninhibit);
    } else {
        isInhibiting = false;
        emit uninhibitedSaver();
    }
    call->deleteLater();
}

void QScreenSaver::dbusScreensaver_launch(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError())
        emit failed(LaunchSaver);
    else
        emit launchedSaver();
    call->deleteLater();
}

void QScreenSaver::dbusScreensaver_lock(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError())
        emit failed(LockScreen);
    else
        emit screenLocked();
    call->deleteLater();
}

void QScreenSaver::dbusLogin_hibernate(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError())
        emit failed(Hibernate);
    else
        emit systemHibernated();
    call->deleteLater();
}

void QScreenSaver::dbusLogin_suspend(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError())
        emit failed(Suspend);
    else
        emit systemSuspended();
    call->deleteLater();
}

void QScreenSaver::dbusLogin_shutdown(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError())
        emit failed(Shutdown);
    else
        emit systemShutdown();
    call->deleteLater();
}

void QScreenSaver::dbusLoginSelf_logoff(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;
    if (reply.isError())
        emit failed(LogOff);
    else
        emit loggedOff();
    call->deleteLater();
}


