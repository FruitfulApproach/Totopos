#pragma once

#include <QWidget>
#include <QList>
#include <QString>

class QHBoxLayout;

// A ROW OF PILLS ALONG THE FOOT OF THE CANVAS: what can be done RIGHT NOW.
//
// Not a toolbar. A toolbar shows everything the program can do and leaves you
// to work out which of it applies; this shows only what the diagram is
// currently offering, and is empty the rest of the time - which is most of the
// time, and is the point. Nothing is drawn when there is nothing to say.
//
// Two things put pills here:
//
//   * the selection. Pick out one arrow and "Map elements by f" appears,
//     because that is a thing you can do to that arrow and to nothing else.
//     Let go of it and the pill goes.
//   * a PINNED rule from the Applicable Rules panel that fits the diagram as
//     it stands. Pinning says "I am going to want this one repeatedly" - and
//     a rule you use repeatedly should not need a trip to a dock each time.
//
// It sits over the view, not in the scene, so it keeps its size and its place
// while the diagram is panned and zoomed under it. Everything around the pills
// is the canvas and takes the mouse as the canvas does.
class CanvasActionBar : public QWidget
{
	Q_OBJECT

public:
	// One pill. `id` comes back with triggered() and is how the caller tells
	// which was pressed; nothing here knows what any of them mean.
	struct Action
	{
		QString id;
		QString text;
		QString tooltip;
	};

	explicit CanvasActionBar(QWidget* parent = nullptr);

	// Replace what is offered. An empty list hides the bar entirely rather
	// than leaving an empty strip sitting over the diagram.
	void setActions(const QList<Action>& actions);
	bool isEmpty() const { return m_actions.isEmpty(); }

signals:
	void triggered(const QString& id);

	// The pills changed, so the bar is a different size and wants placing
	// again. It cannot place itself: where it sits is the view's business.
	void changed();

private:
	QHBoxLayout* m_row = nullptr;
	QList<Action> m_actions;
};
