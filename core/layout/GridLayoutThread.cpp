#include "core/layout/GridLayoutThread.h"

#include <QSet>
#include <QPair>
#include <algorithm>
#include <cmath>

namespace
{
	// clear air between one cell and the next, in scene units
	const qreal kGapX = 70.0;
	const qreal kGapY = 55.0;
	// how far apart two arrows joining the same pair of objects are bowed
	const qreal kFanStep = 34.0;
}

void GridLayoutThread::compute(const LayoutGraph& graph, LayoutPlaces& places, LayoutBends& bends) const
{
	// EVERY node that holds anything, not just the canvas. A diagram nests -
	// a category holds objects, an object holds elements - and each of those
	// is a little diagram of its own to be tidied.
	QSet<int> parents;
	for (const LayoutNode& node : graph.nodes)
		if (!node.isArrow)
			parents.insert(node.parent);

	for (int parent : parents)
		placeChildrenOf(graph, parent, places);
	// second, and only once everything is placed: the arrows are drawn between
	// where the objects END UP, not where they started
	for (int parent : parents)
		fanArrowsOf(graph, parent, places, bends);
}

QPointF GridLayoutThread::centreOf(const LayoutGraph& graph, const LayoutPlaces& places, int index)
{
	const LayoutNode& node = graph.nodes.at(index);
	// left out of `places` means the layout had nothing to say about it, so it
	// is still where the snapshot found it
	return places.value(index, node.pos) + node.box.center();
}

void GridLayoutThread::fanArrowsOf(const LayoutGraph& graph, int parent, const LayoutPlaces& places,
                                   LayoutBends& bends) const
{
	const QList<int> kids = objectsIn(graph, parent);
	if (kids.size() < 2)
		return;

	// grouped by the PAIR they join, whichever way round each one runs: two
	// arrows drawn between the same two objects are the same line on the page
	// however they are read
	QHash<QPair<int, int>, QList<int>> byPair;
	for (int a : arrowsAmong(graph, parent, kids))
	{
		const LayoutNode& arrow = graph.nodes.at(a);
		if (arrow.domain == arrow.codomain)
			continue;   // a loop has no straight line to be bowed off
		byPair[qMakePair(qMin(arrow.domain, arrow.codomain),
		                 qMax(arrow.domain, arrow.codomain))] << a;
	}

	for (auto it = byPair.constBegin(); it != byPair.constEnd(); ++it)
	{
		QList<int> group = it.value();
		if (group.size() < 2)
			continue;   // alone between its two ends: its shape is its own
		std::sort(group.begin(), group.end());   // the same fan every time

		const QPointF from = centreOf(graph, places, it.key().first);
		const QPointF to = centreOf(graph, places, it.key().second);
		QPointF along = to - from;
		const qreal length = std::sqrt(along.x() * along.x() + along.y() * along.y());
		if (length < 1e-6)
			continue;   // one on top of the other: there is no line to bow off
		along /= length;
		const QPointF across(-along.y(), along.x());
		const QPointF middle = (from + to) / 2.0;

		// spaced either side of the straight line: with two arrows one bows
		// each way, with three the middle one stays straight
		for (int i = 0; i < group.size(); ++i)
		{
			const qreal step = i - (group.size() - 1) / 2.0;
			if (qFuzzyIsNull(step))
			{
				bends.insert(group.at(i), QList<QPointF>());   // straight through
				continue;
			}
			bends.insert(group.at(i), QList<QPointF>{ middle + across * (step * kFanStep) });
		}
	}
}

void GridLayoutThread::placeChildrenOf(const LayoutGraph& graph, int parent,
                                       LayoutPlaces& places) const
{
	const QList<int> kids = objectsIn(graph, parent);
	if (kids.size() < 2)
		return;   // one thing is already wherever it should be

	const QList<int> arrows = arrowsAmong(graph, parent, kids);

	// ---- which column each one falls in.
	//
	// Relaxation rather than a proper longest-path walk: every arrow pushes
	// its target one to the right of its source, repeated until nothing moves.
	// Bounded by the number of nodes, which is what keeps a CYCLE from pushing
	// its own tail round for ever - it simply stops improving and the cycle
	// comes out as a run.
	QHash<int, int> column;
	for (int k : kids)
		column.insert(k, 0);
	for (int pass = 0; pass < kids.size(); ++pass)
	{
		bool moved = false;
		for (int a : arrows)
		{
			const LayoutNode& arrow = graph.nodes.at(a);
			const int want = column.value(arrow.domain) + 1;
			if (want > column.value(arrow.codomain) && want < kids.size())
			{
				column[arrow.codomain] = want;
				moved = true;
			}
		}
		if (!moved)
			break;
	}

	// ---- the rows within each column, in the order they already sit, so a
	// tidy-up rearranges as little as it can
	QHash<int, QList<int>> byColumn;
	for (int k : kids)
		byColumn[column.value(k)] << k;
	for (auto it = byColumn.begin(); it != byColumn.end(); ++it)
	{
		QList<int>& run = it.value();
		std::stable_sort(run.begin(), run.end(), [&graph](int a, int b) {
			const QPointF pa = graph.nodes.at(a).pos;
			const QPointF pb = graph.nodes.at(b).pos;
			if (!qFuzzyCompare(pa.y(), pb.y()))
				return pa.y() < pb.y();
			return pa.x() < pb.x();
		});
	}

	// ---- one cell size for the lot, so it really is a grid: the widest and
	// the tallest thing here decide it, and everything sits in the middle of
	// its own cell
	qreal widest = 0;
	qreal tallest = 0;
	for (int k : kids)
	{
		widest = qMax(widest, graph.nodes.at(k).box.width());
		tallest = qMax(tallest, graph.nodes.at(k).box.height());
	}
	const qreal cellW = onGrid(widest + kGapX, graph.gridUnit);
	const qreal cellH = onGrid(tallest + kGapY, graph.gridUnit);

	// ---- and where the block goes. Keep its middle where the old one was, so
	// tidying a diagram does not also walk it off across the canvas.
	QRectF was;
	for (int k : kids)
	{
		const LayoutNode& node = graph.nodes.at(k);
		was |= node.box.translated(node.pos);
	}
	QList<int> columns = byColumn.keys();
	std::sort(columns.begin(), columns.end());
	int rows = 0;
	for (int c : columns)
		rows = qMax(rows, int(byColumn.value(c).size()));
	const qreal blockW = columns.size() * cellW;
	const qreal blockH = rows * cellH;
	const QPointF origin(was.center().x() - blockW / 2.0, was.center().y() - blockH / 2.0);

	for (int ci = 0; ci < columns.size(); ++ci)
	{
		const QList<int>& run = byColumn.value(columns.at(ci));
		for (int ri = 0; ri < run.size(); ++ri)
		{
			const int k = run.at(ri);
			const LayoutNode& node = graph.nodes.at(k);
			// the middle of the cell...
			const QPointF cell(origin.x() + ci * cellW + cellW / 2.0,
			                   origin.y() + ri * cellH + cellH / 2.0);
			// ...is where the middle of the node's FRAME should land, and a
			// node's pos() is its origin, which the frame sits at an offset
			// from. Hence the subtraction rather than a plain assignment.
			const QPointF topLeft(cell.x() - node.box.width() / 2.0,
			                      cell.y() - node.box.height() / 2.0);
			QPointF at = topLeft - node.box.topLeft();
			at.setX(onGrid(at.x(), graph.gridUnit));
			at.setY(onGrid(at.y(), graph.gridUnit));
			places.insert(k, at);
		}
	}
}
