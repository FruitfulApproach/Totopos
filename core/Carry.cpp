#include "core/Carry.h"

#include <QApplication>
#include <QCursor>

Carry& Carry::instance()
{
	static Carry carried;
	return carried;
}

void Carry::pick(const QByteArray& payload, const QString& what)
{
	if (payload.isEmpty())
		return;
	letGo();   // whatever was in hand is put down: one thing is carried at a time
	m_payload = payload;
	m_what = what;

	// THE CURSOR IS THE WHOLE OF THE FEEDBACK, and it is a cursor rather than
	// a picture of what is held. A screenshot of the fragment dragged under
	// the mouse hides the very diagram it is being aimed at, and at the moment
	// of putting it down that is exactly what needs to be seen. The copy
	// cursor says "this will be put down here" and covers nothing.
	QApplication::setOverrideCursor(QCursor(Qt::DragCopyCursor));
	m_cursorShown = true;
	emit carryingChanged(true);
}

void Carry::drop()
{
	if (!isCarrying())
		return;
	letGo();
	emit carryingChanged(false);
}

void Carry::letGo()
{
	m_payload.clear();
	m_what.clear();
	if (m_cursorShown)
	{
		QApplication::restoreOverrideCursor();
		m_cursorShown = false;
	}
}
