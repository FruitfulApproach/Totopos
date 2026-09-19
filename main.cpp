#include "Totopos.h"
#include "core/Emoji.h"
#include "core/AppSettings.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // First, before anything reads a setting: the program used to be called
    // Diagram Detective and its settings are filed under that name.
    AppSettings::migrateFromOldName();
    app.setWindowIcon(Emoji::appIcon());   // the taskbar and the window switcher
    Totopos window;
    window.show();
    return app.exec();
}
