#include "core/layout/GraphLayoutThread.h"
#include "core/layout/GridLayoutThread.h"

GraphLayoutThread::GraphLayoutThread(QObject* parent)
	: QThread(parent)
{
	qRegisterMetaType<LayoutGraph>("LayoutGraph");
	qRegisterMetaType<LayoutPlaces>("LayoutPlaces");
	qRegisterMetaType<LayoutBends>("LayoutBends");
}

void GraphLayoutThread::layOut(const LayoutGraph& graph)
{
	m_graph = graph;
	start();
}

void GraphLayoutThread::run()
{
	LayoutPlaces places;
	LayoutBends bends;
	compute(m_graph, places, bends);
	emit laidOut(places, bends);
}

QList<int> GraphLayoutThread::objectsIn(const LayoutGraph& graph, int parent)
{
	QList<int> kids;
	for (int i = 0; i < graph.nodes.size(); ++i)
	{
		const LayoutNode& node = graph.nodes.at(i);
		if (node.parent == parent && !node.isArrow)
			kids << i;
	}
	return kids;
}

QList<int> GraphLayoutThread::arrowsAmong(const LayoutGraph& graph, int parent, const QList<int>& kids)
{
	QList<int> arrows;
	for (int i = 0; i < graph.nodes.size(); ++i)
	{
		const LayoutNode& node = graph.nodes.at(i);
		if (node.parent != parent || !node.isArrow)
			continue;
		// An arrow with an end outside this set says nothing about how these
		// ones sit beside each other.
		if (kids.contains(node.domain) && kids.contains(node.codomain))
			arrows << i;
	}
	return arrows;
}

qreal GraphLayoutThread::onGrid(qreal value, qreal unit)
{
	return unit > 0 ? qRound(value / unit) * unit : value;
}

// ---------------------------------------------------------------- the list

QList<GraphLayouts::Kind> GraphLayouts::all()
{
	return { Kind{ QStringLiteral("grid"), QStringLiteral("Grid") } };
}

GraphLayoutThread* GraphLayouts::make(const QString& id, QObject* parent)
{
	if (id == QLatin1String("grid"))
		return new GridLayoutThread(parent);
	return nullptr;
}
