#pragma once

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

protected:
	void drawContents(QPainter* painter) override;

private:
	// the plate everything is painted on: the logo, the name and the frame,
	// all of which stay the same for as long as the splash is up
	static QPixmap plate();

	QString m_saying;
};
