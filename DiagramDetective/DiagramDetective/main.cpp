#include "DiagramDetective.h"
#include "Emoji.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(Emoji::appIcon());   // the taskbar and the window switcher
    DiagramDetective window;
    window.show();
    return app.exec();
}
