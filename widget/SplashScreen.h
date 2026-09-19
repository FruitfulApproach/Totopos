#pragma once

#include <QElapsedTimer>
#include <QSplashScreen>
#include <QString>

// THE FIRST THING THE PROGRAM SHOWS, AND THE ONLY THING IT SAYS WHILE IT
// STARTS.
//
// A splash screen earns its place by answering two questions a person asks
// while they wait: what IS this, and is it still going? The first is the logo
// and the version under it; the second is the line of subtext, which names the
// piece of the program being made ready and trails off in dots.
//
// Drawn rather than composed out of widgets, because a splash screen is a
// single image shown before there is a main window to put widgets in: it is
// one pixmap, painted once at the size the screen is, and repainted only when
// the message changes.
class SplashScreen : public QSplashScreen
{
	Q_OBJECT

public:
	SplashScreen();

	// What is being got ready, said in a few words and written under the
	// version with a trail of dots after it: "Reading the rule library...".
	// Processes pending events, so the window is actually repainted before
	// the caller goes back to the work it is announcing.
	void say(const QString& what);

	// HOLD IT UP LONG ENOUGH TO BE READ, then let the window take over.
	//
	// Starting up is fast, which is the problem: a splash screen that is
	// gone in fifty milliseconds is a flash of something nobody saw, and
	// reads as a glitch rather than as the program opening. This waits out
	// whatever is left of a decent minimum before handing over - and waits
	// for nothing at all when the work really did take that long.
	void finishWhenRead(QWidget* window);

protected:
	void showEvent(QShowEvent* event) override;

	void drawContents(QPainter* painter) override;

private:
	// the plate everything is painted on: the logo, the name and the frame,
	// all of which stay the same for as long as the splash is up
	static QPixmap plate();

	QString m_saying;
	QElapsedTimer m_up;   // since it appeared
};
