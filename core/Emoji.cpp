#include "core/Emoji.h"

#include <QPainter>
#include <QPixmap>
#include <QFont>
#include <QFontDatabase>

namespace
{
	// a font that draws emoji in colour, whatever the platform calls it
	QFont emojiFont(int pixelSize)
	{
		static const QStringList candidates = {
			QStringLiteral("Segoe UI Emoji"),
			QStringLiteral("Noto Color Emoji"),
			QStringLiteral("Apple Color Emoji"),
		};
		for (const QString& family : candidates)
		{
			if (QFontDatabase::families().contains(family))
			{
				QFont font(family);
				font.setPixelSize(pixelSize);
				return font;
			}
		}
		QFont font;
		font.setPixelSize(pixelSize);
		return font;
	}
}

QString Emoji::detective()
{
	// U+1F575 sleuth, U+1F3FE medium-dark skin tone, ZWJ, U+2640 female sign,
	// U+FE0F so it is drawn as an emoji rather than as a symbol. Written out
	// in code points: an editor that does not understand the sequence would
	// otherwise take it apart.
	static const char32_t sequence[] = { 0x1F575, 0x1F3FE, 0x200D, 0x2640, 0xFE0F, 0 };
	return QString::fromUcs4(sequence);
}

QIcon Emoji::icon(const QString& glyph)
{
	QIcon icon;
	for (int size : { 16, 24, 32, 48, 64, 128, 256 })
	{
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::TextAntialiasing, true);
		painter.setFont(emojiFont(int(size * 0.82)));
		painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
		painter.end();
		icon.addPixmap(pixmap);
	}
	return icon;
}

QIcon Emoji::appIcon()
{
	// built once: it is asked for by the window, the taskbar and the switcher
	static const QIcon detectiveIcon = icon(detective());
	return detectiveIcon;
}

QFont Emoji::font(int pixelSize)
{
	return emojiFont(pixelSize);
}
