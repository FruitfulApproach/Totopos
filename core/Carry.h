#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

// SOMETHING PICKED UP AND NOT YET PUT DOWN.
//
// Qt's drag and drop needs the button held down for the whole journey, which
// is the wrong gesture for carrying a piece of a diagram: the far end may be
// in another tab, on the other side of a split window, or several scrolls
// away, and all of that is done with the hand that would be holding the
// button. So a fragment is PICKED UP with one press and PUT DOWN with
// another, and in between it is simply carried - the cursor says so, and
// nothing is dragged.
//
// It lives here, outside any one scene, because that is the whole point: the
// diagram it came from and the diagram it lands in need not be the same one.
class Carry : public QObject
{
	Q_OBJECT

public:
	static Carry& instance();

	bool isCarrying() const { return !m_payload.isEmpty(); }
	const QByteArray& payload() const { return m_payload; }
	// what is being carried, said in words: "3 things"
	QString what() const { return m_what; }

	// Take it up. The cursor changes for as long as it is held, wherever the
	// mouse goes - it is the program that is carrying something, not one
	// window.
	void pick(const QByteArray& payload, const QString& what);
	// Put it down, or give up on it: both leave nothing carried and the
	// cursor as it was.
	void drop();

signals:
	void carryingChanged(bool carrying);

private:
	Carry() = default;
	void letGo();

	QByteArray m_payload;
	QString m_what;
	bool m_cursorShown = false;
};
