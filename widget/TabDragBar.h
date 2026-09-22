#pragma once

#include <QTabBar>
#include <QTabWidget>
#include <QPoint>
#include <QString>

// A TAB BAR WHOSE TABS CAN BE CARRIED OUT OF IT.
//
// QTabBar moves tabs about WITHIN itself and no further: setMovable is a
// reorder, not a carry. With the window split there are two bars and the
// obvious way to put a diagram on the other side is to drag its tab there, so
// the bar has to be able to hand a tab over.
//
// The two gestures are told apart by where the cursor goes. While it is still
// on the bar, the drag is a reorder and QTabBar does it, exactly as before.
// The moment it leaves the bar, the tab is being taken somewhere else, and a
// QDrag carries it - which is also what makes the other bar light up as a
// place to drop it.
//
// The bar says nothing about what a tab MEANS. It reports where a tab came
// from and where it was let go of, and the window - which is what knows that a
// tab is a diagram - does the moving (Totopos::moveDocumentTo).
class TabDragBar : public QTabBar
{
	Q_OBJECT

public:
	explicit TabDragBar(QWidget* parent = nullptr);

	// what a carried tab is called on the clipboard: ours alone, so nothing
	// else dropped on a tab bar is mistaken for one
	static QString mimeType();

signals:
	// A tab was let go of on THIS bar. `source` is the bar it came from - the
	// same one when a tab was dragged out and brought back - `from` is where
	// it sat there, and `at` is the position it was dropped before, or -1 for
	// the end.
	void tabDroppedIn(TabDragBar* source, int from, int at);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private:
	// has the cursor left the bar far enough to mean "somewhere else"?
	bool hasLeftTheBar(const QPoint& at) const;

	QPoint m_pressAt;
	int m_pressIndex = -1;
	bool m_carrying = false;   // a QDrag is in flight from this bar
};

// A GROUP OF TABS WHOSE TABS CAN BE CARRIED OUT OF IT.
//
// QTabWidget::setTabBar is protected - a tab widget's bar is its own
// business - so the only way to give it a different one is from inside. That
// is the whole of this class: a QTabWidget with a TabDragBar in it.
class TabGroup : public QTabWidget
{
	Q_OBJECT

public:
	explicit TabGroup(QWidget* parent = nullptr);
	TabDragBar* dragBar() const;

	// A TAB DROPPED ON WHAT THE GROUP IS SHOWING, rather than on its bar.
	//
	// The bar is a few millimetres tall and the page under it is the whole
	// side of the window, so the page is what people aim at - and on a group
	// showing nothing there is no bar worth hitting at all. The canvas gets
	// the drop first (it takes drops of its own) and hands it here, which
	// reports it exactly as the bar does.
	void takeCarriedTab(TabDragBar* source, int from);
	// the group `widget` is shown in, if it is shown in one
	static TabGroup* holding(QWidget* widget);

signals:
	// the same news as TabDragBar::tabDroppedIn, from a drop on the page: at
	// the end, there being no tab it was dropped before
	void tabDroppedOnPage(TabDragBar* source, int from);
};
