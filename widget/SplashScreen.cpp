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
	// THE PLATE IS THE LOGO. Everything below is written against these and
	// scaled once, at the end, to whatever the screen is: a splash screen
	// laid out in device pixels is a different picture on every machine, and
	// this one is the same picture everywhere.
	//
	// The height is the WIDTH TIMES THE LOGO'S OWN PROPORTIONS (414 x 256),
	// so the artwork fills the card exactly and nothing is cropped. Sizing
	// the card first and fitting the logo into it is what left the old splash
	// with a band of white above and below - and cropping a logo to make a
	// rectangle work is a logo nobody chose. Give the card the logo's shape
	// instead and neither happens.
	const int kWidth  = 560;
	const int kHeight = 560 * 256 / 414;   // 346
	const int kMargin = 34;
	const int kRadius = 18;

	// THE WRITING SITS ON THE ARTWORK, so it is given a wash to stand on:
	// deepest at the TOP edge and fading out downwards. Without it the name
	// is legible or not depending on what happens to be behind it, which is
	// not something a splash screen can be left to luck.
	//
	// It runs the WHOLE height of the card. Covering only the part the
	// writing is in put a hard edge across the picture where the band began:
	// the wash was already at full strength on its first row, so the artwork
	// above it was untouched and the join read as a line ruled across the
	// logo. A wash has to start at nothing or start at the edge, and this one
	// starts at the edge.
	const qreal kScrimFadesBy = 0.75;   // gone by three quarters of the way down

	// the rows, measured up from the bottom edge rather than down from the
	// logo: the artwork now owns the whole card, so there is no bottom of it
	// to hang them from
	const int kSayingBaseline = kHeight - kMargin + 10;
	const int kVersionTop = kSayingBaseline - 54;
	const int kNameTop = kVersionTop - 46;

	// How long the splash stays up at the least. Long enough to read the name
	// and the version and to notice the line underneath; short enough that
	// nobody opening the program to get on with something feels held.
	//
	// LONGER IN A DEBUG BUILD, and on purpose: the splash is the one part of
	// the program nobody sees for long enough to judge, because starting up
	// is fast and it is gone. Two and a half seconds is time to look at it
	// while working on it. A release build keeps the short wait - that one is
	// for people who opened the program to get on with something.
#ifdef QT_DEBUG
	const qint64 kLeastVisibleMs = 2500;
#else
	const qint64 kLeastVisibleMs = 900;
#endif

	const QColor kInk      = QColor(0x1E, 0x20, 0x2C);   // the near-black the wash deepens to
	const QColor kOnArt    = QColor(0xFF, 0xFF, 0xFF);   // the name, written over the artwork
	const QColor kQuiet    = QColor(0xD7, 0xDA, 0xE6);   // for what is only there to be glanced at
	const QColor kEdge     = QColor(0x63, 0x66, 0xF1);   // the blue of the diagrams
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

	// THE CARD IS THE ARTWORK, CLIPPED TO ITS ROUNDED CORNERS.
	//
	// The clip is what makes the corners round: the logo is drawn over the
	// whole plate and the corners simply are not painted, so there is no
	// frame drawn ON the picture and no white showing through behind it.
	const QRectF card(1.0, 1.0, kWidth - 2.0, kHeight - 2.0);
	QPainterPath rounded;
	rounded.addRoundedRect(card, kRadius, kRadius);
	painter.setClipPath(rounded);

	const QPixmap logo(QStringLiteral(":/img/TotoposLogo.png"));
	if (!logo.isNull())
	{
		// Scaled to FILL. The card was given the logo's own proportions, so
		// this is an exact fit and expanding takes nothing off - but it is
		// written as a fill rather than a fit so that artwork of some other
		// shape covers the card instead of floating in the middle of it.
		const QSize covering = logo.size().scaled(QSize(kWidth, kHeight), Qt::KeepAspectRatioByExpanding);
		const QRect at((kWidth - covering.width()) / 2, (kHeight - covering.height()) / 2,
		               covering.width(), covering.height());
		painter.drawPixmap(at, logo);
	}
	else
	{
		painter.fillPath(rounded, kInk);   // nothing to show: a plain card, not a hole
	}

	// The wash the writing stands on, DEEPEST AT THE TOP and fading out
	// downwards - the other way up from a photograph's caption bar, and the
	// way round that suits this artwork.
	QLinearGradient scrim(QPointF(0, 0), QPointF(0, kHeight));
	scrim.setColorAt(0.0, QColor(kInk.red(), kInk.green(), kInk.blue(), 225));
	scrim.setColorAt(kScrimFadesBy / 2.0, QColor(kInk.red(), kInk.green(), kInk.blue(), 110));
	scrim.setColorAt(kScrimFadesBy, QColor(kInk.red(), kInk.green(), kInk.blue(), 0));
	scrim.setColorAt(1.0, QColor(kInk.red(), kInk.green(), kInk.blue(), 0));
	painter.fillRect(QRectF(0, 0, kWidth, kHeight), scrim);

	// and the edge, drawn last so it sits over both, in the blue the diagrams
	// are drawn in
	painter.setClipping(false);
	painter.setPen(QPen(kEdge, 2));
	painter.drawPath(rounded);

	// the name, large and light: a wide tracking would be better still, but
	// letter spacing is a font property and this one is whatever is installed
	QFont title = painter.font();
	title.setPointSizeF(30);
	title.setWeight(QFont::Light);
	painter.setFont(title);
	painter.setPen(kOnArt);
	painter.drawText(QRect(0, kNameTop, kWidth, 46), Qt::AlignHCenter | Qt::AlignVCenter,
	                 Version::name());

	// the version, small and quiet, directly under the name
	QFont version = painter.font();
	version.setPointSizeF(9.5);
	version.setWeight(QFont::Normal);
	painter.setFont(version);
	painter.setPen(kQuiet);
	painter.drawText(QRect(0, kVersionTop, kWidth, 20), Qt::AlignHCenter | Qt::AlignVCenter,
	                 QStringLiteral("version %1").arg(Version::number()));

	// a hairline above the subtext, so the line that keeps changing is visibly
	// a different kind of thing from the two that do not
	painter.setPen(QPen(QColor(255, 255, 255, 46), 1));
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
