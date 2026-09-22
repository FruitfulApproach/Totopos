#include "art/Category.h"
#include "core/NodeKind.h"
#include "core/Palette.h"
#include "art/Arrow.h"
#include "core/props/MapsElements.h"
#include "art/DiagramScene.h"
#include "core/layout/GraphLayoutThread.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"
#include "core/props/CategoryProps.h"
#include "core/AppSettings.h"
#include <QMenu>

Category::Category(const QString& name, QGraphicsItem* parent)
	: Object(name, parent)
{
	// A translucent wash with a cobalt frame - unless a default says
	// otherwise, in which case that is what a new one starts in.
	// applyDepthAppearance settles both a moment later and reads the same
	// order (built-in, then by hand, then the default, then this); this is
	// only what it looks like before that runs.
	const QColor wantedFill = StartsAs::fill(this, false);
	const QColor wantedBorder = StartsAs::border(this, false);
	setFill(QBrush(wantedFill.isValid() ? wantedFill : Palette::faded(Palette::field(), 70)));
	setBorder(QPen(wantedBorder.isValid() ? wantedBorder : Palette::faded(Palette::cobalt(), 170), 1.2));
}

Category::~Category()
{
	// the props are QObject children: Qt deletes them with us
}

Category* Category::createLike(const Category* model, const QString& name, QGraphicsItem* parent)
{
	// A built-in knows its own class, and that class is what makes its
	// objects and its arrows: a subcategory of R-Mod has to BE an R-Mod, or
	// what is placed in it would not be an R-module.
	if (model != nullptr)
	{
		if (Category* built = createBuiltIn(model->builtInName(), parent))
		{
			built->setId(name);
			return built;
		}
	}
	auto* plain = new Category(name, parent);
	if (model != nullptr)
		plain->setProperties(model->properties());   // a custom category: the structure carries over
	return plain;
}

bool Category::isAmbient() const
{
	auto* diagram = dynamic_cast<DiagramScene*>(scene());
	return diagram != nullptr && diagram->ambientCategory() == this;
}

bool Category::labelIsLocked() const
{
	// the same test the combo is locked by, so the two never disagree
	return Object::labelIsLocked() || (isAmbient() && holdsAnything());
}

QString Category::labelLockTip() const
{
	if (isAmbient() && holdsAnything() && !Object::labelIsLocked())
		return QStringLiteral("Settled once the diagram has something in it. Start a new "
		                      "diagram to work in another category.");
	return Object::labelLockTip();
}

Category* Category::ambient() const
{
	return m_subcategory ? surroundingCategory() : nullptr;
}

void Category::setSubcategory(bool subcategory)
{
	if (m_subcategory == subcategory)
		return;
	prepareGeometryChange();
	m_subcategory = subcategory;
	// the dotted frame and the fainter fill are set with everything else that
	// follows the nesting, so one call puts both right
	refreshDepthAppearance();
	refreshFrame();
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

QString Category::contextTitle() const
{
	if (m_subcategory)
	{
		Category* home = surroundingCategory();
		return home != nullptr
			? QString("Subcategory %1 of %2").arg(id(), home->id())
			: QString("Subcategory %1").arg(id());
	}
	return Object::contextTitle();
}

// ------------------------------------------------ what the diagram in here says

bool Category::commutes() const
{
	// the canvas itself is the scene's business, so the panel over it and this
	// page are never two answers to one question
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
		return diagram->commutes();
	return Node::commutes();
}

void Category::setCommutes(bool commutes)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
	{
		diagram->setCommutes(commutes);
		return;
	}
	if (Node::commutes() == commutes)
		return;
	Node::setCommutes(commutes);
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		emit diagram->statementChanged(diagram->statementText());
		emit diagram->message(commutes
			? QString("The diagram in %1 commutes.").arg(id())
			: QString("%1 no longer claims the diagram in it commutes.").arg(id()));
	}
}

int Category::statementKind() const
{
	// THE CANVAS SPEAKS FOR THE FILE. Everything drawn is drawn in it, so
	// what IT is put forward as is what the file as a whole is - which is
	// also what its name on disk says. Every other node answers for itself.
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
		return int(diagram->statementKind());
	// Nothing else is put forward as anything: one statement per file, and
	// this is not the file (see Node, "WHAT THE PICTURE IS PUT FORWARD AS").
	return int(DiagramScene::Unstated);
}

void Category::setStatementKind(int kind)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
	{
		diagram->setStatementKind(DiagramScene::StatementKind(kind));
		return;
	}
	// and nowhere else: a category drawn inside another is not a file, and a
	// file says one thing (see Node).
	Q_UNUSED(kind);
}

QString Category::statementName() const
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
		return diagram->statementName();
	return m_statementName;
}

void Category::setStatementName(const QString& name)
{
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()); diagram != nullptr && diagram->ambientCategory() == this)
	{
		diagram->setStatementName(name);
		return;
	}
	m_statementName = name;
}

QStringList Category::properties() const
{
	QStringList keys;
	for (const CategoryProp* p : m_props)
		keys << p->key();
	return keys;
}

bool Category::has(const QString& key) const
{
	for (const CategoryProp* p : m_props)
		if (p->key() == key)
			return true;
	return false;
}

void Category::addProperty(const QString& key)
{
	if (has(key))
		return;
	if (CategoryProp* p = CategoryProp::create(key, this))   // owned by us
		m_props.append(p);
	else
		qWarning("Category '%s': unknown property '%s'", qPrintable(id()), qPrintable(key));
}


QString Category::nextSubcategoryName() const
{
	// A subcategory of R-Mod is a CATEGORY, not a module, so it is not named
	// out of the module letters. No counter is kept for it: the first free
	// letter is found by asking what is already spoken for.
	for (int i = 0; i < 256; ++i)
	{
		const QString name = letterName(i, QChar('S'));
		if (!nameInUse(name))
			return name;
	}
	return letterName(0, QChar('S'));
}

Category* Category::createSubcategory(const QPointF& scenePos)
{
	prepareGeometryChange();   // our frame is the union of what we hold
	Category* sub = Category::createLike(this, nextSubcategoryName());
	sub->setSubcategory(true);
	sub->setParentItem(this);
	sub->setPos(mapFromScene(scenePos));
	sub->setZValue(1);
	sub->refreshDepthAppearance();
	sub->refreshFrame();
	refreshFrame();

	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		diagram->recordCreation(QString("Placed the subcategory %1 in %2").arg(sub->id(), id()),
		                        QList<Node*>{ sub });
		emit diagram->message(QString("%1 is a subcategory of %2. Double-click inside it to place its %3s.")
			.arg(sub->id(), id(), objectName()));
	}
	return sub;
}

void Category::populateActions(QMenu& menu)
{
	// first, because it is the one thing every category can do
	addObjectAction(menu, this);
	// A part of this category, placed right here. This is the whole of the
	// nesting: a subcategory is itself a category, so its own menu offers the
	// same entry, and so on down.
	const QPointF where = mapToScene(contextPos());
	QAction* sub = menu.addAction(QString("New subcategory of %1 here").arg(id()));
	sub->setToolTip(QString("A subcategory of %1: its objects are %2s of %1 and its arrows are %3s of %1. "
	                        "Drawn with a dotted border and a faint fill.")
		.arg(id(), objectName(), morphismName()));
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		// queued: the menu is still closing, and this puts a node in the scene
		QObject::connect(sub, &QAction::triggered, diagram, [this, where] {
			QMetaObject::invokeMethod(this, [this, where] { createSubcategory(where); }, Qt::QueuedConnection);
		});
	}
	else
	{
		sub->setEnabled(false);
	}
	// Tidy up. Built from GraphLayouts::all() so this menu and the View menu
	// can never fall out of step with each other.
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		QMenu* layout = menu.addMenu("Layout");
		for (const GraphLayouts::Kind& kind : GraphLayouts::all())
		{
			QAction* action = layout->addAction(kind.title);
			const QString id = kind.id;
			// queued for the same reason as above: the menu is still closing,
			// and this moves things about in the scene
			QObject::connect(action, &QAction::triggered, diagram, [diagram, id] {
				QMetaObject::invokeMethod(diagram, [diagram, id] { diagram->layOut(id); },
				                          Qt::QueuedConnection);
			});
		}
	}

	menu.addSeparator();

	if (m_props.isEmpty())
		return;

	// Construct, with the groups in a fixed order rather than whatever order
	// the properties happen to be in. Any group nothing asks for is taken out
	// again at the end, so a category with only products shows only Limits.
	auto* construct = new QMenu("Construct", &menu);
	QMenu* limits = construct->addMenu("Limits");
	QMenu* colimits = construct->addMenu("Colimits");
	QMenu* additive = construct->addMenu("Additive");

	const CategoryProp::MenuGroup group = [&](const QString& name) -> QMenu* {
		if (name == "Limits") return limits;
		if (name == "Colimits") return colimits;
		if (name == "Additive") return additive;
		return construct;   // belongs to no group: straight under Construct
	};

	for (CategoryProp* p : m_props)
		p->addConstructions(this, group);

	for (QMenu* sub : { limits, colimits, additive })
	{
		if (sub->isEmpty())
		{
			construct->removeAction(sub->menuAction());
			delete sub;
		}
	}

	if (construct->isEmpty())
	{
		delete construct;
	}
	else
	{
		menu.addMenu(construct);
		menu.addSeparator();
	}

	// what the diagram drawn in here is asserted to be. Exactness only when
	// there is such a thing here to assert - see exactnessDefined().
	if (exactnessDefined())
	{
		menu.addSection(QString("The diagram in %1").arg(id()));
		QAction* rows = menu.addAction("Rows exact");
		rows->setCheckable(true);
		rows->setChecked(m_rowsExact);
		rows->setToolTip("Every row of this diagram is an exact sequence: at each object along it, the image of "
		                 "the arrow coming in is the kernel of the arrow going out.");
		QObject::connect(rows, &QAction::toggled, &menu, [this](bool on) { setRowsExactRecorded(on); });

		QAction* columns = menu.addAction("Columns exact");
		columns->setCheckable(true);
		columns->setChecked(m_columnsExact);
		columns->setToolTip("The same, down each column. Rows and columns are asserted separately.");
		QObject::connect(columns, &QAction::toggled, &menu, [this](bool on) { setColumnsExactRecorded(on); });
		menu.addSeparator();
	}

	// anything a property offers that is not a construction
	for (CategoryProp* p : m_props)
		p->categoryContextMenu(menu, this);
}
void Category::setProperties(const QStringList& keys)
{
	qDeleteAll(m_props);
	m_props.clear();
	for (const QString& key : keys)
		addProperty(key);
}

namespace
{
	// The runs of letters an auto-name walks round.
	//
	// Written out by code point rather than taken as a range, because the
	// Greek range has a final sigma sitting in the middle of it: as a NAME
	// that is the same letter as sigma, so a run built from the range would
	// offer the same variable twice and the second one would look like a
	// duplicate that could not be got rid of.
	QString latinUpper()
	{
		QString run;
		for (ushort c = 'A'; c <= 'Z'; ++c)
			run += QChar(c);
		return run;
	}

	QString latinLower()
	{
		QString run;
		for (ushort c = 'a'; c <= 'z'; ++c)
			run += QChar(c);
		return run;
	}

	QString greekLower()
	{
		QString run;
		for (ushort c = 0x3B1; c <= 0x3C9; ++c)
			if (c != 0x3C2)   // final sigma: the same letter, written differently
				run += QChar(c);
		return run;
	}

	QString greekUpper()
	{
		// only the eleven with a shape of their own. Capital alpha is a Roman
		// A on every screen there has ever been, and a variable nobody can
		// tell from another variable is worse than no variable.
		static const ushort kLetters[] = { 0x393, 0x394, 0x398, 0x39B, 0x39E,
		                                   0x3A0, 0x3A3, 0x3A5, 0x3A6, 0x3A8, 0x3A9 };
		QString run;
		for (ushort c : kLetters)
			run += QChar(c);
		return run;
	}

	// The run `letter` belongs to, or nothing when it belongs to none.
	QString alphabetOf(QChar letter)
	{
		static const QString kRuns[] = { latinUpper(), latinLower(), greekLower(), greekUpper() };
		for (const QString& run : kRuns)
			if (run.contains(letter))
				return run;
		return QString();
	}

	bool isPrime(QChar c)
	{
		// what the user typed, and what we write back: the apostrophe on the
		// keyboard and the prime that means the same thing
		return c == QChar(0x2032) || c == QLatin1Char('\'') || c == QChar(0x2019);
	}
}

QString Category::letterName(int index, QChar first)
{
	const QString alphabet = alphabetOf(first);
	if (alphabet.isEmpty() || index < 0)
		return QString(first);

	// The cycle is anchored at `first`, not at the top of the alphabet: from X
	// the names run X, Y, Z, A, B, ..., W and only THEN X', Y'. Counting the
	// primes off `index` rather than off the letter's position is the whole of
	// what makes that so - anchoring them at A instead would put a prime on
	// the very first wrap, and the second object in a category would be A'.
	const int n = alphabet.size();
	const int start = alphabet.indexOf(first);
	QString name(alphabet.at((start + index) % n));
	for (int i = 0; i < index / n; ++i)
		name += QChar(0x2032);
	return name;
}

int Category::variableIndex(const QString& text, QChar first)
{
	// A variable is one letter and a run of primes: X, B'', an alpha', a
	// Gamma'''. Anything else - a word, a subscript, Hom(X,Y) - is a name the
	// person chose, and says nothing about where counting should carry on.
	if (text.isEmpty())
		return -1;
	const QChar letter = text.at(0);
	const QString alphabet = alphabetOf(letter);
	if (alphabet.isEmpty())
		return -1;
	int primes = 0;
	for (int at = 1; at < text.size(); ++at)
	{
		if (!isPrime(text.at(at)))
			return -1;
		++primes;
	}

	// Only meaningful against an anchor in the SAME run. Renaming an object to
	// a Greek letter says nothing about how far the Latin counting has got.
	if (first.isNull() || !alphabet.contains(first))
		return -1;
	const int n = alphabet.size();
	const int start = alphabet.indexOf(first);
	const int at = alphabet.indexOf(letter);
	return primes * n + ((at - start + n) % n);
}

void Category::noteNamed(const Node* child, const QString& name)
{
	const bool isArrow = dynamic_cast<const Arrow*>(child) != nullptr;
	const int at = variableIndex(name, isArrow ? firstArrowLetter() : firstLetter());
	if (at < 0)
		return;   // not a variable: leave the counting where it was

	// Start again FROM the name typed, not after it. The name itself is now
	// taken, so the next one along is what comes out - rename X to S and the
	// next object is T - but if that S is later deleted, the same rule hands
	// it straight back.
	if (isArrow)
		m_nextArrowIndex = at;
	else
		m_nextIndex = at;
}

bool Category::exactnessDefined() const
{
	return has(HasZeroObject::Key()) && has(HasKernels::Key());
}

bool Category::objectsAreSets() const
{
	return has(IsConcrete::Key());
}

QString Category::objectKind() const
{
	// a plain object, which holds nothing - see makeObject just below
	return NodeKind::object();
}

Object* Category::makeObject(const QString& name)
{
	// A PLAIN object, which holds nothing. An object used to be a category in
	// its own right, so objects nested without end and a subobject was drawn
	// inside the thing it was part of; that is gone. A subobject is now a node
	// beside its parent with an inclusion arrow between them, which is how it
	// is written anyway and which leaves both of them where they can be seen.
	//
	// The categories whose objects really ARE categories - BigCat, Cat -
	// override this and go on making categories.
	return new Object(name, this);   // a child: joins the scene with us
}

Arrow* Category::createArrow(const QString& name, Node* from, Node* to)
{
	auto* arrow = new Arrow(name, from, to, this);
	// An object here is a category in its own right, so what is drawn inside
	// it are its elements - and an arrow carries them over: x in M becomes
	// f(x) in N. Not live by default; a functor is (see MapsElements).
	arrow->addProperty(MapsElements::Key());
	arrow->setZValue(2);
	arrow->refreshDepthAppearance();
	arrow->refreshFrame();
	// and it goes where both its ends are, which is this category unless
	// something has been drawn round them (see Arrow::homeToCommonAncestor)
	arrow->homeToCommonAncestor();
	return arrow;
}

Arrow* Category::createCanvasArrow(Node* from, Node* to)
{
	return createArrow(nextArrowName(), from, to);
}

QRectF Category::emptyFrame()
{
	// A sheet, centred on the category's own origin: room for a square of
	// four objects and the arrows between them, which is the diagram most
	// people draw first. Nothing depends on the exact numbers - the frame is
	// the union of what is held the moment anything is held.
	return QRectF(-260, -180, 520, 360);
}

QRectF Category::boxRect() const
{
	const QRectF held = squareIfSingleGlyph(contentFrame().adjusted(-5.4, -5.4, 5.4, 5.4));
	if (containedCount() != 0 || !isAmbient())
		return held;

	// EMPTY, AND THE CANVAS: the room to draw the first thing in.
	//
	// Only the canvas. A category DRAWN in something is a box standing in it,
	// and an empty one is as small as its name - which is right: it is a
	// thing, and things are the size of what they hold. Giving every empty
	// category the room below made each freshly placed one a sheet the size
	// of the window, and since an arrow leaves its domain at the FRAME, an
	// arrow drawn out of a fresh category came flying in from the far corner
	// of that sheet.
	//
	// The canvas is not a thing standing anywhere, so it has no such size to
	// be: it is the paper, and an empty sheet of paper is the whole sheet.
	// The name is unioned in as well, in case it sits outside the room.
	return emptyFrame() | held;
}

void Category::becameAmbient()
{
	// At the origin, which is where the canvas begins: not above a frame,
	// because there is no frame drawn for it to be above.
	setLabelOffset(QPointF());
	refreshLabelWeight(containedCount());
	refreshDepthAppearance();   // no fill and no border, now that it is the canvas
	refreshFrame();
}

void Category::applyDepthAppearance(int depth)
{
	Node::applyDepthAppearance(depth);
	if (isAmbient())
	{
		// THE CANVAS IS NOT A THING DRAWN ON THE CANVAS.
		//
		// Everything here is drawn IN the ambient category, so a box round it
		// would be a box round the whole picture - a border on the paper,
		// saying nothing, and a wash of colour behind every object that made
		// the objects' own fills harder to tell apart. Its name in bold at
		// the origin says which world this is, and that is the whole of what
		// needs saying.
		setFill(QBrush(Qt::NoBrush));
		setBorder(QPen(Qt::NoPen));
		return;
	}
	// EVERY R-Mod IS THE SAME R-Mod, SO THEY ARE ALL DRAWN THE SAME.
	//
	// A built-in's colour belongs to the built-in, not to any one node that
	// happens to be it: it is kept under the NAME (AppSettings::categoryFill)
	// and read back here, so setting it on one changes every R-Mod in every
	// open diagram at once. A category the user defined has no such name to
	// be filed under and keeps the look below.
	//
	// Unset - which is how everything starts - means the look below stands.
	const QString built = builtInName();
	QColor wantedFill = AppSettings::instance().categoryFill(built);
	QColor wantedBorder = AppSettings::instance().categoryBorder(built);

	// AND WHAT THIS ONE WAS ASKED TO BE, which this used to paint straight
	// over.
	//
	// Every category is given its colours here, at every depth refresh - and
	// a fresh one is refreshed the moment it is made. So a colour chosen by
	// hand, or a default asked for with "Set default", was applied in the
	// constructor and then painted over a moment later by the yellow below:
	// a new category came out in the original colours however the default had
	// been set.
	//
	// The order is: the BUILT-IN's colour first (every R-Mod is the same
	// R-Mod, wherever it is drawn), then this node's own if it was coloured
	// by hand, then the default a new one starts in, and only then the
	// built-in look. What changes with depth is the ALPHA, which is set
	// absolutely below and so does not pile up when this runs again.
	if (!wantedFill.isValid() && hasChosenStyle() && fill().style() != Qt::NoBrush)
		wantedFill = fill().color();
	if (!wantedBorder.isValid() && hasChosenStyle() && border().style() != Qt::NoPen)
		wantedBorder = border().color();
	if (!wantedFill.isValid())
		wantedFill = StartsAs::fill(this, false);
	if (!wantedBorder.isValid())
		wantedBorder = StartsAs::border(this, false);

	if (m_subcategory)
	{
		// A subcategory is a PART of the category it is drawn in, not a thing
		// standing in it, and is drawn as such: barely there, and outlined in
		// dots, so it reads differently from an object of that category.
		//
		// It is still an R-Mod, so it is still R-Mod's colour - only fainter,
		// which is what tells a part from a thing.
		QColor fill = wantedFill.isValid() ? wantedFill : Palette::field();
		fill.setAlpha(qMax(10, 32 - 6 * depth));
		setFill(QBrush(fill));
		QColor edge = wantedBorder.isValid() ? wantedBorder : Palette::cobalt();
		edge.setAlpha(qMax(90, 190 - 18 * depth));
		QPen dotted(edge, qMax(0.8, 1.8 - 0.2 * depth));
		dotted.setStyle(Qt::DotLine);
		setBorder(dotted);
		return;
	}

	// A bright yellow canvas, in dodger blue. Each level of nesting fades a
	// little so the yellow does not pile up into orange - and a colour chosen
	// for the built-in fades by the same steps, so nesting reads the same
	// whatever it is coloured.
	QColor fill = wantedFill.isValid() ? wantedFill : Palette::field();
	fill.setAlpha(qMax(28, 105 - 18 * depth));
	setFill(QBrush(fill));
	QColor edge = wantedBorder.isValid() ? wantedBorder : Palette::cobalt();
	edge.setAlpha(qMax(110, 220 - 20 * depth));
	setBorder(QPen(edge, qMax(0.8, 2.2 - 0.3 * depth)));
}

void Category::adopt(Node* node, const QPointF& scenePos)
{
	if (node == nullptr)
		return;
	prepareGeometryChange();   // our frame is the union of our children
	node->setParentItem(this);
	node->setPos(mapFromScene(scenePos));   // a child's pos() is in OUR coordinates
	node->refreshDepthAppearance();
	node->refreshFrame();   // it is whole now: our frame can grow to hold it
}

Object* Category::createObject(const QString& name, const QPointF& scenePos)
{
	prepareGeometryChange();
	Object* object = makeObject(name);
	object->setParentItem(this);
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);
	object->refreshDepthAppearance();
	object->refreshFrame();
	return object;
}

Object* Category::createCanvasObject(const QPointF& scenePos)
{
	// our frame is the union of our children: tell the scene it is about to grow
	prepareGeometryChange();

	// A child's pos() is in the PARENT's coordinates. mapFromScene turns the
	// click, given in scene coordinates, into ours, so the object lands exactly
	// under the cursor wherever this category happens to sit, and stays there
	// when the category later moves (it moves with it).
	Object* object = makeObject(nextObjectName());
	object->setParentItem(this);   // makeObject normally did this already; a custom one might not
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);   // above the category's fill
	object->refreshDepthAppearance();
	object->refreshFrame();   // it is whole now: our frame can grow to hold it
	return object;
}

namespace
{
	bool nameUsedUnder(const QGraphicsItem* parent, const QString& name)
	{
		for (QGraphicsItem* child : parent->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // a label
			if (node->id() == name || nameUsedUnder(node, name))
				return true;
		}
		return false;
	}
}

bool Category::nameInUse(const QString& name) const
{
	if (name.isEmpty())
		return false;
	const QGraphicsItem* root = this;
	while (root->parentItem() != nullptr)
		root = root->parentItem();
	return root->childItems().isEmpty() ? false : nameUsedUnder(root, name);
}

QString Category::freshName(int from, QChar first) const
{
	// The first name from `from` onwards that nobody is using - scanning
	// rather than counting, and that is what lets a name come BACK. Place A,
	// B, C, D and delete C, and the next object is C again, then E: the count
	// is not a tally of how many objects there have ever been, it is a place
	// to start looking from.
	for (int at = qMax(0, from); at < from + 512; ++at)
	{
		const QString name = letterName(at, first);
		if (!nameInUse(name))
			return name;
	}
	return letterName(qMax(0, from), first);   // rather than spin for ever
}

// ---------------------------------------------------------------- exactness

void Category::setRowsExact(bool exact)
{
	if (m_rowsExact == exact)
		return;
	m_rowsExact = exact;
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

void Category::setColumnsExact(bool exact)
{
	if (m_columnsExact == exact)
		return;
	m_columnsExact = exact;
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		emit diagram->statementChanged(diagram->statementText());
}

void Category::setRowsExactRecorded(bool exact)
{
	if (m_rowsExact == exact)
		return;
	const bool before = m_rowsExact;
	setRowsExact(exact);
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		diagram->history()->record(new ExactnessChanged(
			exact ? QString("%1 has exact rows").arg(id()) : QString("%1 no longer claims exact rows").arg(id()),
			this, true, before, exact));
		emit diagram->message(exact
			? QString("The rows of %1 are exact.").arg(id())
			: QString("%1 no longer claims its rows are exact.").arg(id()));
	}
}

void Category::setColumnsExactRecorded(bool exact)
{
	if (m_columnsExact == exact)
		return;
	const bool before = m_columnsExact;
	setColumnsExact(exact);
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
	{
		diagram->history()->record(new ExactnessChanged(
			exact ? QString("%1 has exact columns").arg(id()) : QString("%1 no longer claims exact columns").arg(id()),
			this, false, before, exact));
		emit diagram->message(exact
			? QString("The columns of %1 are exact.").arg(id())
			: QString("%1 no longer claims its columns are exact.").arg(id()));
	}
}
