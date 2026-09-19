#pragma once

#include <QThread>
#include <QList>
#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QMetaType>

// A diagram as a layout algorithm sees it: plain data, no QGraphicsItem,
// nothing owned by the GUI thread.
//
// Same reasoning as core/rules/Pattern.h - the work is done on another thread,
// and the live items belong to this one. The snapshot is taken here, handed
// over, and what comes back is a set of positions to apply.
struct LayoutNode
{
	int parent = -1;       // index of the node this is drawn in; -1 for the root
	bool isArrow = false;  // arrows are not placed: they follow their ends
	int domain = -1;       // an arrow's ends, as indices
	int codomain = -1;
	QPointF pos;           // where it sits now, in its PARENT's coordinates
	QRectF box;            // its frame, in its OWN coordinates (boxRect)

	// WHERE THE USER PUT IT, as against where something has just appeared.
	//
	// A tidy-up after a rule should not rearrange the diagram somebody has
	// already arranged: it should find a place for what has just been drawn
	// in and leave everything else alone. Anything that was already on the
	// canvas is settled; only what this step made is not.
	bool settled = true;
};

struct LayoutGraph
{
	QList<LayoutNode> nodes;
	qreal gridUnit = 25.0;   // 0 when the grid is off
};

// Where each node should end up: index -> position in its parent's
// coordinates. Named, because the comma in QHash<int, QPointF> would split
// Q_DECLARE_METATYPE's argument list in two and the macro would not compile.
using LayoutPlaces = QHash<int, QPointF>;

// And the shape each arrow should be drawn with: index -> the points its line
// is pulled through, in its PARENT'''s coordinates (the same frame the places
// are in; the scene maps them into the arrow'''s own before setting them).
// Only arrows the layout has something to say about are in here - one arrow
// alone between two objects is left exactly as it was drawn.
using LayoutBends = QHash<int, QList<QPointF>>;

Q_DECLARE_METATYPE(LayoutGraph)
Q_DECLARE_METATYPE(LayoutPlaces)
Q_DECLARE_METATYPE(LayoutBends)

// One way of tidying a diagram up, on a thread of its own.
//
// A diagram nests - a category holds objects, an object holds elements - so
// every one of these has to work down the nesting rather than over one flat
// list. What each subclass differs in is only how it places the children of
// ONE node; the walk is the same for all of them.
class GraphLayoutThread : public QThread
{
	Q_OBJECT

public:
	explicit GraphLayoutThread(QObject* parent = nullptr);

	// what the menus call it: "Grid"
	virtual QString title() const = 0;
	// and how it is written in a file or a setting: "grid"
	virtual QString id() const = 0;

	// Take a copy of the diagram and start. The thread deletes itself when it
	// has answered.
	void layOut(const LayoutGraph& graph);

signals:
	// index -> where that node should sit, in its parent's coordinates.
	// Anything left out stays where it is.
	void laidOut(const LayoutPlaces& places, const LayoutBends& bends);

protected:
	void run() override;

	// The whole of what a subclass provides. Runs on this thread and must
	// touch nothing but its argument.
	virtual void compute(const LayoutGraph& graph, LayoutPlaces& places, LayoutBends& bends) const = 0;

	// the children of `parent` that are placeable - objects, never arrows
	static QList<int> objectsIn(const LayoutGraph& graph, int parent);
	// the arrows drawn in `parent` whose two ends are both among `kids`
	static QList<int> arrowsAmong(const LayoutGraph& graph, int parent, const QList<int>& kids);
	// round to the grid, when there is one
	static qreal onGrid(qreal value, qreal unit);

private:
	LayoutGraph m_graph;
};

// Which layouts there are, for the menus to list and for the scene to make one
// by name. A new algorithm is added here and appears in both menus at once.
namespace GraphLayouts
{
	struct Kind
	{
		QString id;
		QString title;
	};

	QList<Kind> all();
	// nullptr when the name is not one of them
	GraphLayoutThread* make(const QString& id, QObject* parent = nullptr);
}
