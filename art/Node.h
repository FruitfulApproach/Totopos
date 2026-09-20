#pragma once

#include <QGraphicsObject>
#include <QList>
#include <QStringList>
#include <QSet>
#include <QPointer>
#include <QPainter>
#include "art/NodeLabel.h"
#include <QBrush>
#include <QPen>
class Category;   // forward: Category derives (via Object) from Node, so it cannot be included here
class DiagramScene;

class QGraphicsSceneContextMenuEvent;
class QMenu;

class Node  : public QGraphicsObject
{
	Q_OBJECT

public:
	Node(const QString& id, QGraphicsItem *parent);

	// WHAT THIS NODE IS, as against what it is CALLED.
	//
	// Two objects may both be called X - a diagram is free to draw the same
	// letter twice, and often means to - but they are two objects, two
	// pointers, two things a functor has to carry over separately. Anything
	// that has to tell one from the other keys off this and never off the
	// label. (Getting that wrong is how a functor came to map three arrows
	// onto one: see MapsElements::imageOf.)
	//
	// It is written to the file with the node, so an image drawn before a
	// diagram was saved still knows what it is the image OF when it is read
	// back. Nothing shows it to the user; it is not a name.
	QString key() const { return m_key; }
	void setKey(const QString& key);

	// what was TYPED, markers and all: v_{x}, not the drawn v-with-subscript.
	// See Notation - the scripts belong to the drawing, never to the name.
	virtual QString id() const;
	virtual void setId(const QString& id);

	// put the keyboard in this node's label
	void editLabel();
	bool isEditingLabel() const { return m_idText != nullptr && m_idText->isEditing(); }
	// called by the label as it is typed in, and when the editor closes
	void labelBeingEdited(const QString& text);
	void finishLabelEdit(const QString& before, const QString& after);

	// A label made out of other labels: "%1 x %2" over the nodes it is built
	// from. Whenever one of THEM is relabelled - by hand, or by anything else -
	// this one follows, so a product is never left naming factors that have
	// been renamed.
	void setDerivedLabel(const QString& pattern, const QList<Node*>& sources);
	void clearDerivedLabel();

	// Read a label the user typed and tie it to the nodes it MENTIONS. Type
	// Hom(X,B) where X and B are drawn, and the label becomes a formula over
	// them: rename B to C and it says Hom(X,C) of its own accord. Whole names
	// only - a node called "o" is not found inside "Hom".
	void bindLabelReferences();
	// Does `text` use `name` as a name? xy^{-1} mentions y - two letters side
	// by side is a product - while Hom(X,B) does not mention a node called o.
	// Telling those apart is what `otherNames` is for: it is the other labels
	// drawn in the same place, and a name character beside the name counts as
	// a boundary when it is itself part of one of them. The same reading
	// bindLabelReferences makes, offered on its own because a rule has only
	// labels to go on and not the nodes behind them (core/rules/Rule.cpp).
	static bool labelMentions(const QString& text, const QString& name,
	                          const QStringList& otherNames = QStringList());
	QString labelPattern() const { return m_labelPattern; }
	QList<Node*> labelSources() const;

	// the font of the id label; changing it re-centres the label and reframes the node
	QFont labelFont() const;
	void setLabelFont(const QFont& font);

	// appearance: transparent and borderless by default; both settable from the
	// right-click colour chips
	const QBrush& fill() const { return m_fill; }
	const QPen& border() const { return m_border; }
	void setFill(const QBrush& fill);
	void setBorder(const QPen& border);
	// Set it AND put it in the scene's history, which is also what marks the
	// colour as CHOSEN (see hasChosenStyle). An invalid colour means none: no
	// fill, or no border at all. These are what the Properties panel calls;
	// nothing outside this class can reach recordStyleChange.
	void setFillRecorded(const QColor& colour);
	void setBorderRecorded(const QColor& colour);

	// THE COLOUR THE NAME IS WRITTEN IN.
	//
	// A node's own colour, kept apart from the fill and the frame: a label
	// is not the box it sits in, and a pale wash of a fill wants dark
	// letters whatever colour the wash is. An INVALID colour means nobody
	// has said - the label is then written in the ink the program draws
	// names in (Palette::ink, or whatever the setting says a new node
	// starts with).
	QColor labelColour() const { return m_labelColour; }
	void setLabelColour(const QColor& colour);
	void setLabelColourRecorded(const QColor& colour);

	// Was the fill or the border chosen by hand (the colour chips)? A frame
	// that was asked for is always drawn; one that is merely the default is
	// only drawn when there is something inside to frame.
	bool hasChosenStyle() const { return m_styleChosen; }
	// The colours a kind of node is drawn in when nobody has said otherwise.
	// Not setFill/setBorder: those record that a colour was ASKED for, which
	// is what makes a frame show round a node holding nothing.
	void setDefaultLook(const QBrush& fill, const QPen& border);

	// does this node hold any other node? (its label does not count)
	bool holdsAnything() const;

	// Can an arrow be drawn OUT of this? An arrow joins two objects, so an
	// element - the one thing here that is not an object - says no, and the
	// menu leaves the entry out rather than offering something that would be
	// refused as soon as it was aimed.
	virtual bool canStartArrow() const { return true; }
	// where its label is drawn, in this node's own coordinates
	QRectF labelRect() const;

	// Lit up for a moment - pointing at it from somewhere else, a list of
	// components say. Green wins over every colour the node has been given,
	// because it is not about the node's own appearance at all.
	bool isHighlighted() const { return m_highlighted; }
	void setHighlight(bool on);

	// Whether the connected component this node belongs to is asserted to
	// commute. Kept on the members rather than on the component: components
	// are worked out afresh from the arrows every time, and would have nowhere
	// to keep it. Every component commutes until told otherwise.
	bool commutesInComponent() const { return m_componentCommutes; }
	void setCommutesInComponent(bool commutes);

	// The same for exactness. A connected component is a diagram; a lone
	// object is not, which is why these do not belong to an object as such.
	bool rowsExactInComponent() const { return m_componentRowsExact; }
	void setRowsExactInComponent(bool exact) { m_componentRowsExact = exact; }
	bool columnsExactInComponent() const { return m_componentColumnsExact; }
	void setColumnsExactInComponent(bool exact) { m_componentColumnsExact = exact; }

	// Part of something the diagram cannot mean - a ring in a diagram that is
	// asserted to commute, or a name doing the work of two. Drawn bright red
	// until it is put right.
	bool hasError() const { return m_inCycle; }
	void setError(bool error);

	// A frame that is drawn whatever this node holds. An object with nothing
	// inside it is just its label, which is right for an R-module called M
	// and wrong for anything whose SHAPE is part of what it claims - a
	// subcategory says so with a dotted frame, empty or not.
	virtual bool alwaysFramed() const { return false; }

	// How round the corners of the frame are drawn, in scene units. 0 is a
	// plain rectangle.
	qreal cornerRadius() const { return m_cornerRadius; }

	// THE RADIUS AS DRAWN, which is not the radius it is SET to.
	//
	// The number above is the one on the Properties page and in the file: the
	// radius this node would have on the canvas. Everything else about a
	// nested node is drawn a step smaller for each node it sits inside - the
	// line widths, the head of an arrow, the size of the label - and the
	// corners have to come off the same step or they do not. Left alone, a
	// 13px round on a box two levels down, which is a third of the size, ate
	// the whole of its short side and drew an ellipse.
	//
	// Nothing reads this back: it is for painting and for hit-testing, so
	// what is clicked is the shape that is drawn.
	qreal drawnCornerRadius() const { return m_cornerRadius * depthScale(); }
	void setCornerRadius(qreal radius);

	// set Exists such AND put it in the scene's history (it changes what the
	// diagram claims, so it is a step, not a look)
	void setExistsSuchRecorded(bool existsSuch);
	static DiagramScene* diagramOf(const Node* node);

	// "Exists such": this part of the diagram is not given, it is what is
	// CLAIMED TO EXIST. Drawn DASHED - a dashed border round an object, a
	// dashed line for an arrow - and read as the existential part of the
	// statement the diagram makes.
	bool existsSuch() const { return m_existsSuch; }
	void setExistsSuch(bool existsSuch);

	// Marked with a red X: when this diagram is read as a rule, this item is
	// part of the PREMISE - it still has to be found - and it is taken OUT
	// where the rule is applied. "For all such scenes, there is one with these
	// things gone and these dotted things drawn in."
	//
	// Dotted and crossed out are opposites: a thing cannot be claimed to exist
	// and struck off at once, so setting either clears the other.
	bool markedForDeletion() const { return m_deleteMark; }
	void setDeleteMark(bool marked);
	// the same, and put it in the scene's history (it changes what the rule
	// does, so it is a step, not a look)
	void setDeleteMarkRecorded(bool marked);
	// The line a claim of existence is drawn with: long dashes, put on a pen
	// that is otherwise ready to draw. Long, because a subcategory's border
	// is DOTTED (Category::applyDepthAppearance) and the two must not read as
	// the same line - "there exists such an X" and "this is a part of R-Mod"
	// are different claims. The lengths are in scene units rather than pen
	// widths, so every dashed thing in the diagram is dashed alike however
	// thick its line happens to be drawn.
	static void applyExistsDash(QPen& pen);
	// how long each dash is, and how much line is left out between them
	static constexpr qreal ExistsDashLength = 9.0;
	static constexpr qreal ExistsDashGap = 5.0;

	// Added while chasing: part of the hypotheses of the statement, not of
	// the setup it started from.
	bool isHypothesis() const { return m_hypothesis; }
	void setHypothesis(bool hypothesis) { m_hypothesis = hypothesis; }

	// the category this node was filed in, if any (see setCategory)
	Category* category() const;
	void setCategory(Category* category) { m_category = category; }
	// the nearest Category among this item's ancestors — the category it is drawn inside
	Category* surroundingCategory() const;

	virtual ~Node();

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	// A node's frame is the union of what it holds; Object and Arrow give it
	// their own shape. The base needs one even so: Qt asks a node for its
	// bounding rect WHILE IT IS STILL BEING BUILT (setId puts a label in and
	// tells the scene the geometry changed, and the scene then asks this item
	// and its ancestors for their rects). At that moment the subclass does not
	// exist yet, so a pure virtual here is a crash.
	QRectF boundingRect() const override { return childFrame(); }

	// THE BOX, which is not the bounding rect. A node is drawn as a box round
	// what it holds, and its name sits wherever the name goes - centred for an
	// object, above the frame for a category, beside the line for an arrow -
	// which is very often OUTSIDE that box. Two rects, then, and they answer
	// different questions:
	//
	//   boxRect()       what gets the border and the fill, what arrows aim at,
	//                   where the handles hang, what shoves its neighbours.
	//   boundingRect()  every pixel the item and its children draw, the box
	//                   AND the label. Qt's contract: draw outside this and
	//                   the scene leaves litter behind and mis-indexes the
	//                   item, so each subclass unions the label back in.
	//
	// Never paint with boundingRect(): the box would jump to swallow a label
	// the moment one was dragged clear of it.
	virtual QRectF boxRect() const { return squareIfSingleGlyph(contentFrame()); }

	// Our frame is the union of what we hold, so it changes whenever a child
	// arrives, leaves, moves or resizes. Call this once the change is COMPLETE
	// (never from itemChange - see the note there): it re-indexes this node
	// and its ancestors, repaints them, and refreshes the bold labels.
	void refreshFrame();

	// MY SHAPE CHANGED, I DID NOT GO ANYWHERE.
	//
	// A null delta is that news, and everything that draws itself from this
	// node's FRAME rather than its position needs it: an arrow joins the
	// edges of its two ends, so a box that grows or shrinks moves the join
	// without moving the box. Cheap enough to say on every step of a drag,
	// which is when it is most needed - unlike refreshFrame, which re-indexes
	// and re-settles labels and is for a change that has finished.
	void announceShapeChange() { emit moved(this, QPointF()); }

	// how many nodes this one sits inside (0 at the top)
	int nesting() const;
	// HOW BIG EVERYTHING ABOUT THIS NODE SHOULD BE.
	//
	// A line width, a dot, a cross, the label's point size: all written for an
	// outermost node, and all taken down a step for every node this one is
	// drawn inside. One factor for the lot, so that a thing deep in the
	// nesting is small in every respect at once rather than being a small box
	// with a full-size name on it. Floored, or the deepest nodes end up drawn
	// with nothing and named in nothing.
	//
	// This is for what the drawing INVENTS - dotted for "exists such", red for
	// an error, the green of a highlight, the size text is set in. A width or
	// a size asked for by hand is what was asked for and is not scaled.
	qreal depthScale() const;
	// the same for a depth already in hand (applyDepthAppearance is given one)
	static qreal depthScale(int depth);

	// Where this node sits, as the indexes to walk down from the topmost node
	// (the ambient category), counting only the objects - arrows are not
	// stepped through, so a path keeps its meaning whatever arrows are drawn.
	// The path of an arrow is the path of the category it is drawn in.
	QList<int> pathFromRoot() const;
	// the node at that path below `root`, or nullptr
	static Node* fromPath(Node* root, const QList<int>& path);
	// Looks that follow the nesting: the deeper a node, the smaller its label
	// (and the fainter a category's fill). Applies to this node and everything
	// inside it, so it is right again after a move between parents.
	void refreshDepthAppearance();

	// the right-click menu, built by populateContextMenu, shown at a screen point
	void popupContextMenu(const QPoint& screenPos);
	// The same, told WHERE in this node it was opened, so an entry that is
	// about that spot - placing something there - lands under the cursor. A
	// right-click on the canvas is a right-click on the ambient category, and
	// arrives this way.
	void popupContextMenu(const QPoint& screenPos, const QPointF& itemPos);

	// Put an "Add object" entry on `menu` that places a new object in `home`
	// at the point the menu was opened on. This is how objects are made now:
	// a double-click begins an arrow, so the canvas gesture that used to place
	// one is only left on the empty canvas.
	void addObjectAction(QMenu& menu, Category* home);

	// What this is, in words, at the head of its menu: "R-module M",
	// "R-linear map f", "Category C". The kind comes from the category it is
	// drawn in, which is what knows.
	virtual QString contextTitle() const;
	// "r-linear map" -> "R-linear map"
	static QString sentenceCase(const QString& text);

	// The hidden snap grid: node positions land on multiples of the unit,
	// measured in SCENE coordinates, so nesting makes no difference — an
	// object inside a category snaps to the same grid as the category itself.
	// THE NEAREST EMPTY GRID POINT TO A WANTED SPOT, PREFERRING THE ONE BELOW.
	//
	// Everything put into a node by hand - Add object, Add element, the image
	// a functor draws - wants to land where it was asked for, and must not
	// land on top of what is already there. Right-clicking a node and asking
	// for another one asks for it AT THE CURSOR, which is on that node: taken
	// at its word, the new one is drawn exactly over the old, and the only
	// sign of it is that the label changed.
	//
	// So the spot asked for is put on the grid and, if something is sitting
	// there, the grid is searched outwards a ring at a time. Below comes
	// first at every distance, because a list of things grows downwards and
	// that is where the eye looks for the new one.
	//
	// `wanted` and the answer are both in `parent`'s own coordinates.
	static QPointF freeGridSpotIn(const Node* parent, const QPointF& wanted);

	static qreal snapUnit() { return s_snapUnit; }
	static void setSnapUnit(qreal unit) { s_snapUnit = unit; }
	static bool snapEnabled() { return s_snapEnabled; }
	static void setSnapEnabled(bool on) { s_snapEnabled = on; }
	// a position (in this item's parent coordinates) moved onto the grid
	QPointF snapped(const QPointF& parentPos) const;

	// Nodes with the same parent do not sit on top of one another. A node
	// moved into its neighbours pushes them ALONG THE WAY IT IS GOING, far
	// enough to be clear and a little further, and they push theirs in turn.
	static bool collisionEnabled() { return s_collisionEnabled; }
	static void setCollisionEnabled(bool on) { s_collisionEnabled = on; }
	// Only a node the user is dragging shoves its neighbours. Everything else
	// that moves a node - opening a file, undo, a functor carrying an image
	// along - must leave the diagram where it was put.
	static void setUserDragging(bool dragging) { s_userDragging = dragging; }
	static bool userDragging() { return s_userDragging; }
	static qreal collisionEpsilon() { return s_pushEpsilon; }
	static void setCollisionEpsilon(qreal epsilon) { s_pushEpsilon = epsilon; }

signals:
	void idChanged(Node* thisNode, const QString& id);
	void deleted(Node* thisNode);
	void moved(Node* thisNode, const QPointF& delta);
	// The label has been moved clear of where it would otherwise sit, BY THIS
	// MUCH - dragged by hand, or written by a mapping keeping two sides in
	// step. Both, for the same reason `moved` is emitted for both: a chain of
	// functors C -> D -> E carries a change along by each mapping hearing
	// what the one before it did. A mapping holds m_syncing while it writes,
	// so its OWN answer never comes back round, but the next mapping along is
	// a different one and hears it.
	//
	// Not emitted when the node merely re-places its own label (a reframe, an
	// arrow redrawn): that is the same placement worked out again, not a move.
	void labelOffsetChanged(Node* thisNode, const QPointF& delta);
	void styleChanged(Node* thisNode);

protected:
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

	// The right-click menu, in this order: what this is, what it can DO
	// (populateActions, which each kind of node fills in), how it looks, what
	// it asserts, and finally deleting it.
	virtual void populateContextMenu(QMenu& menu);
	// what this kind of node offers - constructions, mappings, the shape of a
	// line. Submenus, not a flat list.
	virtual void populateActions(QMenu& menu) { Q_UNUSED(menu); }

	// where the last right-click landed, in this item's coordinates: a menu
	// entry may be about that spot rather than about the whole item
	QPointF contextPos() const { return m_contextPos; }

	// the id label item (for subclasses that place it themselves, e.g. an arrow)
	QGraphicsTextItem* label() const { return m_idText; }
	NodeLabel* labelItem() const { return m_idText; }

public:
	// A label that can be picked up and put somewhere else. A LEAF object's
	// label is the object - there is nothing else drawn of it - so it stays put
	// and is the handle the object is carried by. A node that HOLDS something
	// is drawn as a box round what it holds, and then its name is a thing of
	// its own: it can be dragged anywhere, including outside the box. An arrow
	// says yes for the same reason - its label has to get out of the way of
	// whatever the line crosses. The label itself calls these, so they are
	// public.
	// A NAME THAT IS NOT THIS NODE'S TO CHANGE.
	//
	// An image drawn by a functor is called H(X): made out of the functor's
	// name and the source's, and remade from those every time either changes.
	// Typing over it here would be undone by the next sync, so the editor
	// does not open on it at all - the label says so instead, with a lock.
	// Rename the functor, or rename X.
	// Virtual: the ambient category's name is locked once anything is drawn
	// in it, for its own reason (see Category::labelIsLocked).
	virtual bool labelIsLocked() const;

	// Why it is locked, in words, for the label's tooltip. Only asked when
	// labelIsLocked() says yes.
	virtual QString labelLockTip() const;

	virtual bool labelIsMovable() const { return holdsAnything(); }
	virtual void labelMoved(const QPointF& pos);
	virtual void labelDragFinished(const QPointF& fromPos);

	// Does the label follow the frame? Only a node drawn as a BOX round what
	// it holds has a frame to follow. An arrow's label rides its line and is
	// placed from labelAnchor(), so it has nothing to do with any of this.
	virtual bool labelFollowsBox() const { return holdsAnything(); }

	// Where the label has been dragged to, as a displacement from where it
	// would otherwise sit, so it travels with the node rather than staying
	// where the screen was. For an object that is exactly the label's CENTRE
	// in this node's coordinates, which is what makes it follow the frame
	// (see settleLabel).
	QPointF labelOffset() const { return m_labelOffset; }
	virtual void setLabelOffset(const QPointF& offset);
	// the label can be picked up or not: keep the flag and the cursor in step
	// with what this node currently holds
	void refreshLabelMovability();

protected:

	// recompute a derived label from the labels it is built out of
	void refreshDerivedLabel();

	// note a colour change in the scene's history (purely graphical)
	void recordStyleChange(const QBrush& fillBefore, const QPen& borderBefore);
	void recordLabelColourChange(const QColor& before);
	// put the colour on the label item itself
	void applyLabelColour();

	// Where this node's label goes. An object centres it; an arrow puts it
	// beside the middle of its line, wherever it has been dragged to. It is
	// asked for on every keystroke while the label is being typed, so it must
	// not be the object's rule applied to everything.
	virtual void placeLabel();

	// a submenu of colour chips: the built-in palette, None, and Custom...
	QMenu* colourMenu(const QString& title, const QColor& current, QMenu* parent, std::function<void(const QColor&)> apply);


	// A node's frame is the union of its children (see Object::boundingRect),
	// so a child moving or resizing changes every ancestor's rect: they must be
	// told BEFORE (to re-index) and AFTER (to repaint). Call these around any
	// change to this item's own geometry.
	void ancestorsPrepareGeometryChange();
	void ancestorsUpdate();

	// The union of our children, in OUR coordinates - what every frame here is
	// built from. Use this and NEVER Qt's childrenBoundingRect(): see the note
	// on the implementation for why that one can abort the process.
	QRectF childFrame() const;

	// What the BOX is drawn round: the nodes and arrows held here, and not this
	// node's own label - so a name dragged clear of the box does not drag the
	// box after it. A node holding nothing is its label, and gets that instead.
	QRectF contentFrame() const;

	// A SINGLE LETTER IS DRAWN IN A SQUARE. A glyph's rect is taller than it
	// is wide, so O, X, M and the rest each came out as a narrow upright
	// box - a row of them read as a row of different shapes rather than as a
	// row of objects. Taking the longer side for both makes one square, and
	// keeping the centre where it was means the letter does not shift: the
	// box only grows outwards around it.
	//
	// Only for a node holding nothing. A node that holds something is a frame
	// round what it holds, and squaring that would push its contents off
	// centre - so the rect comes back untouched.
	QRectF squareIfSingleGlyph(const QRectF& box) const;

	// what this node looks like at that nesting depth; subclasses extend it
	virtual void applyDepthAppearance(int depth);

	// shove the neighbours this node has just moved into
	void pushSiblings(const QPointF& delta);

	// The nodes this node contains: its child items other than its own label,
	// and only the ones you can SEE - a hidden child is not part of the frame
	// (childFrame) and does not make this node count as holding anything.
	// Arrows are nodes too, so an arrow drawn inside this one counts.
	int containedCount(const QGraphicsItem* except = nullptr) const;
	// a node that contains others shows its id in bold
	virtual void refreshLabelWeight(int contained);

private:
	// keep the label centred on the item's origin, so setPos() positions the node's CENTRE
	void centreLabel();

	// THE LABEL FOLLOWS THE FRAME.
	//
	// A node that holds things is drawn as a box round them, and its name is
	// put wherever the user wants it - very often outside that box, above a
	// corner. Move the things inside and the box changes shape underneath the
	// name, which otherwise stays where it was and drifts away from what it
	// names.
	//
	// So the label's place is remembered AGAINST the box, per axis:
	//
	//   inside the box   it keeps its fraction of the way across, so an edge
	//                    that moves by d moves the label by k*d, where k is
	//                    the label's distance from the OPPOSITE edge as a
	//                    fraction of the span (k = 1 at the edge that moved,
	//                    0 at the far one, 1/2 in the middle);
	//   outside the box  it keeps its distance from the nearer edge, k = 1,
	//                    so a name sitting 12px above the top stays 12px above
	//                    the top wherever the top goes.
	//
	// Both axes are worked out the same way and independently, which is what
	// makes all four sides come out right - a name above the top-left corner
	// follows the top edge rigidly and the left edge proportionally.
	//
	// It is done ONCE PER TURN OF THE EVENT LOOP, on purpose. Reading a file
	// or drawing a rule in builds a node's contents child by child, and a
	// label that chased every step would end up somewhere its own arithmetic
	// chose rather than where it was put. Coalesced, the whole of that is one
	// change, and the first settle of a node that has just gained a frame only
	// REMEMBERS it - the saved position is what the user meant.
	void settleLabelSoon();
	void settleLabel();
	// take the frame as it stands now to be the one the label was placed against
	void rememberLabelBox();
	// Put the name just above the top edge of the frame, centred on it. Where
	// a PARENT's name goes: see settleLabel for when this is used.
	void placeLabelAboveBox();
	// refreshFrame() on the next turn of the event loop, at most once
	void scheduleFrameRefresh();

protected:
	QPointF m_labelOffset;   // where the user put the label, from where it would sit

private:
	NodeLabel* m_idText = nullptr;
	QString m_labelPattern;
	QList<QPointer<Node>> m_labelSources;
	bool m_settingDerivedLabel = false;
	// invalid: the name is written in the ink names are written in
	QColor m_labelColour;
	QBrush m_fill = Qt::NoBrush;
	QPen m_border = Qt::NoPen;
	bool m_existsSuch = false;
	bool m_deleteMark = false;
	bool m_hypothesis = false;
	bool m_inCycle = false;
	bool m_highlighted = false;
	bool m_componentCommutes = true;
	bool m_componentRowsExact = false;
	bool m_componentColumnsExact = false;
	bool m_styleChosen = false;
	// how round its corners are; the default is a setting, so a diagram drawn
	// today looks like the last one (see AppSettings::NodeCornerRadius)
	qreal m_cornerRadius = 0.0;
	QPointF m_lastPos;
	QPointF m_contextPos;
	bool m_frameRefreshQueued = false;

	// the frame as it was when the label was last placed against it, and
	// whether there has ever been one (see settleLabel)
	QRectF m_labelBox;
	bool m_labelBoxValid = false;
	bool m_labelSettleQueued = false;

	QString m_key;
	static quint64 s_nextKey;

	Category* m_category = nullptr;

	static qreal s_snapUnit;
	static bool s_snapEnabled;

	static bool s_collisionEnabled;
	static bool s_userDragging;
	static qreal s_pushEpsilon;
	static int s_pushDepth;          // a shove that starts another: bounded, so a ring ends
	static QSet<Node*> s_pushed;     // already moved this round, so nothing is shoved twice
};
