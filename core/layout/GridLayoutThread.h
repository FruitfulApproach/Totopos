#pragma once

#include "core/layout/GraphLayoutThread.h"

// The naive one: put everything on a grid.
//
// Columns come from the arrows. An object with nothing pointing at it starts
// in the first column, and every arrow pushes its target one column further
// right, so a diagram that reads left to right is laid out reading left to
// right. Objects sharing a column are stacked in the order they already sit
// in, so a tidy-up moves things as little as it can get away with.
//
// It makes no attempt to shorten arrows, avoid crossings, or treat a cycle as
// anything but a run of arrows that stops pushing. That is the whole of what
// "naive" means here - and for the square and triangular diagrams this program
// is for, it is most of what is wanted.
class GridLayoutThread : public GraphLayoutThread
{
	Q_OBJECT

public:
	explicit GridLayoutThread(QObject* parent = nullptr) : GraphLayoutThread(parent) {}

	QString title() const override { return QStringLiteral("Grid"); }
	QString id() const override { return QStringLiteral("grid"); }

protected:
	void compute(const LayoutGraph& graph, LayoutPlaces& places, LayoutBends& bends) const override;

private:
	// place the children of one node, and say nothing about anyone else's
	void placeChildrenOf(const LayoutGraph& graph, int parent, LayoutPlaces& places) const;

	// FAN OUT THE ARROWS THAT WOULD BE DRAWN ON TOP OF EACH OTHER.
	//
	// The grid says nothing about arrows - they follow their ends - and that
	// is fine until several of them share those ends. A biproduct is the
	// ordinary case: i1 and p1 run between the same two objects in opposite
	// directions, and drawn straight they are the SAME LINE, with their labels
	// in the same place. So every group of arrows joining one PAIR of objects
	// (either way round - the direction does not move the line) is bowed
	// apart: one bend point each, spaced evenly either side of the straight
	// line between them, in index order so a second tidy-up gives the same
	// answer as the first. A pair joined by a single arrow is left alone -
	// whatever shape it was given by hand is its own.
	void fanArrowsOf(const LayoutGraph& graph, int parent, const LayoutPlaces& places,
	                 LayoutBends& bends) const;
	// the middle of a node's frame once the places are in, in its parent's
	// coordinates (a node left out of `places` has not moved)
	static QPointF centreOf(const LayoutGraph& graph, const LayoutPlaces& places, int index);
};
