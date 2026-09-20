#include "core/Branding.h"

#include <QPixmap>

namespace
{
	// An icon from one picture, handed to Qt at the sizes a title bar, a
	// switcher and a taskbar ask for. The PNG is the largest of them, so every
	// size is a reduction and none is blown up.
	QIcon fromResource(const char* path)
	{
		const QPixmap art(QString::fromLatin1(path));
		if (art.isNull())
			return QIcon();   // nothing carried: the caller falls back

		QIcon icon;
		for (int size : { 16, 24, 32, 48, 64, 128, 256 })
			icon.addPixmap(art.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		return icon;
	}
}

QIcon Branding::windowIcon()
{
	static const QIcon icon = fromResource(":/img/TotoposWindowIcon.png");
	return icon;
}

QIcon Branding::taskbarIcon()
{
	static const QIcon icon = fromResource(":/img/TotoposTaskbarIcon.png");
	return icon;
}
