#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QDebug>

inline void safeRemoveAndLog(QGraphicsItem* item, const char* className)
{
	if (item == nullptr)
		return;
	// Log for debug builds so we can see which item is being destroyed
	qDebug() << "safeRemoveAndLog: destroying" << className << "this=" << item << "scene=" << static_cast<void*>(item->scene());
	if (QGraphicsScene* s = item->scene())
	{
		// Take it out of view immediately and detach mouse handling so the
		// scene won't try to paint or receive input for it while it is
		// being torn down.
		item->setVisible(false);
		item->setAcceptedMouseButtons(Qt::NoButton);
		s->removeItem(item);
	}
}
