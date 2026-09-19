#include "widget/SplashScreen.h"

#include "core/Version.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QShowEvent>

namespace
{
	// The plate, in its own coordinates. Everything below is written against
	// these and scaled once, at the end, to whatever the screen is: a splash
	// screen laid out in device pixels is a different picture on every
	// machine, and this one is the same picture everywhere.
	const int kWidth  = 520;
	const int kHeight = 320;
	const int kMargin = 34;
	// where the subtext sits: low enough to be clear of the version, high
	// enough not to touch the frame
	const int kSayingBaseline = kHeight - kMargin - 6;

	// How long the splash stays up at the least. Long enough to read the name
	// and the version and to notice the line underneath; short enough that
	// nobody opening the program to get on with something feels held.
	const qint64 kLeastVisibleMs = 900;

	const QColor kInk      = QColor(0x1E, 0x20, 0x2C);   // the near-black everything is written in
	const QColor kQuiet    = QColor(0x6B, 0x70, 0x84);   // for what is only there to be glanced at
	const QColor kEdge     = QColor(0x63, 0x66, 0xF1);   // the blue of the diagrams
	const QColor kPaper    = QColor(0xFF, 0xFF, 0xFF);
	const QColor kPaperLow = QColor(0xF3, 0xF4, 0xFB);   // a shade, so the plate is not a flat sheet
}

SplashScreen::SplashScreen()
	: QSplashScreen(plate())
{
	// It is a splash screen: no frame, no taskbar entry, and above whatever
	// is already on the screen.
	setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);
}

QPixmap SplashScreen::plate()
{
	// Drawn at the screen's own resolution and told so, which is what keeps
	// the text crisp on a scaled display: the same drawing, more pixels.
	const qreal ratio = qApp->primaryScreen() != nullptr
		? qApp->primaryScreen()->devicePixelRatio()
		: 1.0;
	QPixmap pixmap(int(kWidth * ratio), int(kHeight * ratio));
	pixmap.setDevicePixelRatio(ratio);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

	// the card itself: white paper with a faint fall of grey, rounded, and
	// bordered in the blue the diagrams are drawn in
	const QRectF card(1.0, 1.0, kWidth - 2.0, kHeight - 2.0);
	QLinearGradient paper(card.topLeft(), card.bottomLeft());
	paper.setColorAt(0.0, kPaper);
	paper.setColorAt(1.0, kPaperLow);
	QPainterPath rounded;
	rounded.addRoundedRect(card, 18, 18);
	painter.fillPath(rounded, paper);
	painter.setPen(QPen(kEdge, 2));
	painter.drawPath(rounded);

	// THE LOGO, given the top two thirds and fitted inside it.
	//
	// Scaled to fit rather than to fill: a logo cropped to make a rectangle
	// work is a logo nobody chose. Whatever is left over is white space,
	// which is what the rest of the card is made of anyway.
	const QPixmap logo(QStringLiteral(":/img/TotoposLogo.png"));
	if (!logo.isNull())
	{
		const QRect room(kMargin, kMargin - 6, kWidth - 2 * kMargin, 150);
		const QSize fitted = logo.size().scaled(room.size(), Qt::KeepAspectRatio);
		const QRect at(room.x() + (room.width() - fitted.width()) / 2,
		               room.y() + (room.height() - fitted.height()) / 2,
		               fitted.width(), fitted.height());
		painter.drawPixmap(at, logo);
	}

	// the name, large and light: a wide tracking would be better still, but
	// letter spacing is a font property and this one is whatever is installed
	QFont title = painter.font();
	title.setPointSizeF(30);
	title.setWeight(QFont::Light);
	painter.setFont(title);
	painter.setPen(kInk);
	painter.drawText(QRect(0, kMargin + 148, kWidth, 46), Qt::AlignHCenter | Qt::AlignVCenter,
	                 Version::name());

	// the version, small and quiet, directly under the name
	QFont version = painter.font();
	version.setPointSizeF(9.5);
	version.setWeight(QFont::Normal);
	painter.setFont(version);
	painter.setPen(kQuiet);
	painter.drawText(QRect(0, kMargin + 192, kWidth, 20), Qt::AlignHCenter | Qt::AlignVCenter,
	                 QStringLiteral("version %1").arg(Version::number()));

	// a hairline above the subtext, so the line that keeps changing is visibly
	// a different kind of thing from the two that do not
	painter.setPen(QPen(QColor(0, 0, 0, 24), 1));
	painter.drawLine(kMargin + 40, kSayingBaseline - 30, kWidth - kMargin - 40, kSayingBaseline - 30);

	painter.end();
	return pixmap;
}

void SplashScreen::showEvent(QShowEvent* event)
{
	QSplashScreen::showEvent(event);
	if (!m_up.isValid())
		m_up.start();
}

void SplashScreen::finishWhenRead(QWidget* window)
{
	// Whatever is left of the minimum, spent with the event loop turning:
	// the splash goes on painting, and the window behind it goes on getting
	// ready to be shown.
	const qint64 so_far = m_up.isValid() ? m_up.elapsed() : kLeastVisibleMs;
	for (qint64 left = kLeastVisibleMs - so_far; left > 0; left = kLeastVisibleMs - m_up.elapsed())
		QApplication::processEvents(QEventLoop::AllEvents, int(left));
	finish(window);
}

void SplashScreen::say(const QString& what)
{
	m_saying = what;
	repaint();
	// The work being announced runs on this same thread and will not give the
	// event loop a turn, so the paint is delivered here or not at all.
	QApplication::processEvents();
}

void SplashScreen::drawContents(QPainter* painter)
{
	if (m_saying.isEmpty())
		return;

	painter->setRenderHint(QPainter::TextAntialiasing, true);
	QFont subtext = painter->font();
	subtext.setPointSizeF(9.5);
	subtext.setItalic(true);
	painter->setFont(subtext);
	painter->setPen(kQuiet);

	// THE DOTS ARE PART OF THE SENTENCE, not part of the message.
	//
	// Written here rather than by every caller, so no call site can forget
	// them and none has to remember how many: a caller says what it is doing
	// and this says that it is still doing it.
	const QString line = m_saying.endsWith(QLatin1Char('.'))
		? m_saying
		: m_saying + QStringLiteral("...");
	painter->drawText(QRect(kMargin, kSayingBaseline - 22, kWidth - 2 * kMargin, 24),
	                  Qt::AlignHCenter | Qt::AlignVCenter, line);
}
