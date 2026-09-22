#include "widget/TabDragBar.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>

namespace
{
	// How far past the bar the cursor must go before the drag stops being a
	// reorder and becomes a carry. Far enough that a shaky hand on a
	// side-to-side drag does not tear the tab out of the bar it is in.
	const int kOutOfTheBar = 18;
}

TabDragBar::TabDragBar(QWidget* parent)
	: QTabBar(parent)
{
	setAcceptDrops(true);
	setMovable(true);           // within the bar, Qt's own reorder
	setChangeCurrentOnDrag(true);
}

QString TabDragBar::mimeType()
{
	return QStringLiteral("application/x-totopos-tab");
}

bool TabDragBar::hasLeftTheBar(const QPoint& at) const
{
	const QRect room = rect().adjusted(-kOutOfTheBar, -kOutOfTheBar, kOutOfTheBar, kOutOfTheBar);
	return !room.contains(at);
}

void TabDragBar::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_pressAt = event->pos();
		m_pressIndex = tabAt(event->pos());
	}
	QTabBar::mousePressEvent(event);
}

void TabDragBar::mouseMoveEvent(QMouseEvent* event)
{
	const bool held = (event->buttons() & Qt::LeftButton) != 0;
	if (!held || m_carrying || m_pressIndex < 0
	 || (event->pos() - m_pressAt).manhattanLength() < QApplication::startDragDistance()
	 || !hasLeftTheBar(event->pos()))
	{
		// still on the bar: this is a reorder, and it is Qt's
		QTabBar::mouseMoveEvent(event);
		return;
	}

	// OFF THE BAR: the tab is being taken somewhere. Qt's own move is ended
	// first - releasing its grab where the tab started - so the bar is not
	// left mid-reorder while the drag is in flight.
	QMouseEvent release(QEvent::MouseButtonRelease, m_pressAt, event->globalPosition(),
	                    Qt::LeftButton, Qt::NoButton, event->modifiers());
	QTabBar::mouseReleaseEvent(&release);

	auto* mime = new QMimeData;
	mime->setData(mimeType(), QByteArray::number(m_pressIndex));
	auto* carried = new QDrag(this);
	carried->setMimeData(mime);
	// a picture of the tab itself, so what is being carried is plain
	carried->setPixmap(grab(tabRect(m_pressIndex)));
	carried->setHotSpot(QPoint(carried->pixmap().width() / 2, carried->pixmap().height() / 2));

	m_carrying = true;
	carried->exec(Qt::MoveAction);
	m_carrying = false;
	m_pressIndex = -1;
}

void TabDragBar::mouseReleaseEvent(QMouseEvent* event)
{
	m_pressIndex = -1;
	QTabBar::mouseReleaseEvent(event);
}

void TabDragBar::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasFormat(mimeType()))
	{
		event->setDropAction(Qt::MoveAction);
		event->accept();
		return;
	}
	QTabBar::dragEnterEvent(event);
}

void TabDragBar::dragMoveEvent(QDragMoveEvent* event)
{
	if (event->mimeData()->hasFormat(mimeType()))
	{
		event->setDropAction(Qt::MoveAction);
		event->accept();
		return;
	}
	QTabBar::dragMoveEvent(event);
}

void TabDragBar::dropEvent(QDropEvent* event)
{
	if (!event->mimeData()->hasFormat(mimeType()))
	{
		QTabBar::dropEvent(event);
		return;
	}

	auto* source = qobject_cast<TabDragBar*>(event->source());
	const int from = event->mimeData()->data(mimeType()).toInt();
	if (source == nullptr || from < 0)
	{
		event->ignore();
		return;
	}
	// where it was let go of: before the tab under the cursor, or at the end
	// when it was dropped past the last one
	const int at = tabAt(event->position().toPoint());
	event->setDropAction(Qt::MoveAction);
	event->accept();
	emit tabDroppedIn(source, from, at);
}

TabGroup::TabGroup(QWidget* parent)
	: QTabWidget(parent)
{
	// before anything is put in it: setTabBar throws away the bar it replaces
	setTabBar(new TabDragBar(this));
}

TabDragBar* TabGroup::dragBar() const
{
	return qobject_cast<TabDragBar*>(tabBar());
}

void TabGroup::takeCarriedTab(TabDragBar* source, int from)
{
	if (source != nullptr && from >= 0)
		emit tabDroppedOnPage(source, from);
}

TabGroup* TabGroup::holding(QWidget* widget)
{
	for (QWidget* up = widget; up != nullptr; up = up->parentWidget())
		if (auto* group = qobject_cast<TabGroup*>(up))
			return group;
	return nullptr;
}
