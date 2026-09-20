#pragma once

#include <QIcon>

// The program's own icons, the ones drawn for it and carried in the binary
// (resources/resources.qrc). Everything else about the program is drawn from
// the system emoji font (see Emoji) - these are the two that are pictures.
namespace Branding
{
	// the title bar, and the window switcher
	QIcon windowIcon();
	// the taskbar button, where the icon is seen biggest and on its own
	QIcon taskbarIcon();
}
