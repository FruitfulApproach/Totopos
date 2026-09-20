#include "dialog/Totopos.h"
#include "core/Emoji.h"
#include "core/Branding.h"
#include "core/AppSettings.h"
#include "core/Version.h"
#include "widget/SplashScreen.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(Version::name());
    app.setApplicationDisplayName(Version::name());
    app.setApplicationVersion(Version::number());

    // UP BEFORE ANYTHING SLOW, AND DOWN THE MOMENT THE WINDOW IS THERE.
    //
    // Everything between these two lines happens on this thread with no event
    // loop running, which is exactly why the splash says what it is doing:
    // each `say` paints the new line itself before the work behind it starts.
    SplashScreen splash;
    splash.show();

    // First, before anything reads a setting: the program used to be called
    // Diagram Detective and its settings are filed under that name.
    splash.say(QStringLiteral("Looking up what you set last time"));
    AppSettings::migrateFromOldName();

    splash.say(QStringLiteral("Setting out the pens"));
    // Ours if we are carrying it, the detective drawn from the emoji font if
    // the resources did not make it into the binary.
    const QIcon taskbar = Branding::taskbarIcon();
    app.setWindowIcon(taskbar.isNull() ? Emoji::appIcon() : taskbar);   // the taskbar and the window switcher

    splash.say(QStringLiteral("Laying out the workbench"));
    Totopos window;

    splash.say(QStringLiteral("Opening the canvas"));
    window.show();
    // stays up until the window is painted AND until it has been up long
    // enough to have been read (see finishWhenRead)
    splash.finishWhenRead(&window);
    return app.exec();
}
