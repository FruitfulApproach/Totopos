#include "core/props/MapsElements.h"

#include <QMenu>
#include <QMap>
#include <QSet>
#include <QPair>
#include <QList>
#include <QUuid>
#include <utility>
#include <algorithm>
#include <QGraphicsScene>
#include <QLineF>
#include <QPainterPath>
#include "art/Arrow.h"
#include "art/Category.h"
#include "art/Object.h"
#include "core/AppSettings.h"
#include "core/Emoji.h"
#include "art/DiagramScene.h"
#include "art/Functor.h"
#include "core/history/SceneHistory.h"
#include <QPointer>

namespace
{
	// Delete every one of these, and survive one of them taking another with
	// it. Deleting a node announces itself (Node::deleted), and what listens
	// may delete further nodes - an arrow whose end has just gone, an image
	// of an image. If one of THOSE is also in this list, the plain pointer
	// left behind here would be deleted a second time, and what the scene
	// would be left holding is not a node at all. A guarded pointer simply
	// goes null and the second delete never happens.
	void deleteAll(const QList<Node*>& nodes)
	{
		QList<QPointer<Node>> safe;
		safe.reserve(nodes.size());
		for (Node* node : nodes)
			safe << QPointer<Node>(node);
		for (QPointer<Node>& node : safe)
		{
			if (node.isNull())
				continue;

			// OUT OF SIGHT AT ONCE, DESTROYED ON THE NEXT TURN OF THE LOOP.
			//
			// This runs from inside a signal, and the signal is often the
			// KEYBOARD: typing in a label emits idChanged on every keystroke
			// (Node::labelBeingEdited), which lands here through sync(). A
			// scene item destroyed in the middle of that is destroyed in the
			// middle of Qt's own update of it - the scene is left holding a
			// pointer it no longer owns, and the crash comes later, in the
			// next repaint, walking the index. Arrow::onObjectDeleted hides
			// and defers for exactly this reason; so does this.
			//
			// The stamps go FIRST: until it is really gone it is still a
			// child of the codomain, and the next sync would find it by them
			// and take it for an image that is still wanted.
			node->setData(MapsElements::DoomedKey, true);
			node->setData(MapsElements::ImageSourceKey, QString());
			node->setData(MapsElements::ImageMappingKey, QString());
			node->setData(MapsElements::ImageFunctorKey, QString());
			node->setVisible(false);
			node->setAcceptedMouseButtons(Qt::NoButton);
			node->deleteLater();
		}
	}
}

MapsElements::MapsElements(Arrow* arrow)
	: ArrowProp(arrow)
{
	// A functor is live by default: what else would an arrow between two
	// categories drawn on the canvas mean? An ordinary morphism - an R-linear
	// map, a homomorphism - waits to be asked, because its domain usually
	// holds only the few elements the chase is about.
	m_live = dynamic_cast<Functor*>(arrow) != nullptr;
	if (arrow != nullptr)
	{
		m_name = arrow->id();
		// its own identity, made once and kept with it (and in the file)
		m_mappingId = arrow->data(MappingIdKey).toString();
		if (m_mappingId.isEmpty())
		{
			m_mappingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
			arrow->setData(MappingIdKey, m_mappingId);
		}
		// whether or not it is live: images already drawn belong to this
		// arrow, and must follow it when it is relabelled
		connect(arrow, &Node::idChanged, this, &MapsElements::onFunctorRenamed);
	}
	if (m_live)
		listen(true);
}


void MapsElements::onFunctorRenamed()
{
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr)
		return;

	// Not on every keystroke: deleting "f" before typing "F" would take the
	// images through the empty name on the way. The rename lands when the
	// editor closes.
	if (F->isEditingLabel())
		return;

	const QString now = F->id();
	if (now == m_name)
		return;
	m_name = now;

	// Everything this mapping drew is still what it drew - it just goes by a
	// new name. Found by identity, so nothing is left behind under the old one
	// for a later sync to duplicate.
	m_syncing = true;
	for (QGraphicsItem* child : cod->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || !isOurImage(node))
			continue;
		Node* source = sourceWithKey(node->data(ImageSourceKey).toString());
		node->setData(ImageFunctorKey, now);
		if (source != nullptr)
			node->setId(applied(now, source->id()));
	}
	m_syncing = false;

	if (m_live)
		sync();
}
void MapsElements::removeImages()
{
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr)
		return;
	const QString name = m_name.isEmpty() ? F->id() : m_name;

	QList<Node*> objects, arrows;
	for (QGraphicsItem* child : cod->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || !isOurImage(node))
			continue;
		(dynamic_cast<Arrow*>(node) != nullptr ? arrows : objects) << node;
	}

	// arrows first, so none is left pointing at an object that has gone
	m_syncing = true;
	deleteAll(arrows);
	deleteAll(objects);
	m_syncing = false;
	// they are hidden now and destroyed on the next turn of the loop, so the
	// frame that was held open by them has to be taken in here
	cod->refreshFrame();
}


QPointF MapsElements::freeSpotIn(Object* cod, const QPointF& wanted)
{
	// TWO MAPPINGS INTO ONE PLACE PUT THEIR IMAGES IN THE SAME SPOT.
	//
	// Each mapping draws its image at the same offset the source has in ITS
	// home, which is right and is what makes the image read as a copy of the
	// diagram it came from. But X drawn in C has ONE offset, and if both G.F
	// and G carry something to E then both land on it - G(F(X)) and
	// (G.F)(X) written one on top of the other, unreadable and hard even to
	// drag apart, because clicking there picks whichever is on top.
	//
	// So the spot is taken if something is already sitting on it, and the new
	// one steps down until it is clear. Only at the moment it is FIRST drawn:
	// from then on its position is its own and nothing moves it but a hand or
	// the mirror.
	// EVERYTHING HERE IS IN THE CODOMAIN'S OWN COORDINATES: the spot asked
	// for, the spots already taken, and the answer. That is the frame the
	// image is about to be given a position in, so nothing is mapped in or
	// out and there is no scene transform to get the wrong way round.
	if (cod == nullptr)
		return wanted;
	const qreal step = 34.0;
	QPointF at = wanted;
	for (int tries = 0; tries < 24; ++tries)
	{
		bool taken = false;
		for (QGraphicsItem* child : cod->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr || !node->isVisible() || dynamic_cast<Arrow*>(node) != nullptr)
				continue;
			if (node->mapRectToParent(node->boxRect()).contains(at))
			{
				taken = true;
				break;
			}
		}
		if (!taken)
			return at;
		at.setY(at.y() + step);
	}
	return at;
}

QString MapsElements::formula(const QString& functor)
{
	// An arrow with no name of its own is an inclusion: it carries x to x. Its
	// formula is the hole and nothing else, so applying it gives back what it
	// was applied to - not "(x)", which is what a nameless prefix came to.
	if (functor.trimmed().isEmpty())
		return QString(hole());
	if (functor.contains(hole()))
		return functor;

	// A NAME THAT IS NOT ONE THING IS BRACKETED FIRST.
	//
	// G∘F is a composite, not a letter: applied to X it reads (G∘F)(X), because
	// G∘F(X) says something else - it is G applied to F(X), which is the same
	// value here but is not the name of this node. Only a name that would be
	// read as a single thing can take the argument bare.
	const QString name = atomic(functor)
		? functor
		: QStringLiteral("(") + functor + QStringLiteral(")");

	// no hole written: it goes at the end, in whichever notation is set
	return AppSettings::instance().functorParentheses()
		? name + QStringLiteral("(") + hole() + QStringLiteral(")")
		: name + hole();
}

bool MapsElements::atomic(const QString& name)
{
	// One thing: a letter with whatever is decoration on it - a prime, a
	// subscript, a digit. An operator or a space between two parts makes it
	// two things, and two things need brackets before they take an argument.
	//
	// A name already wrapped in its own brackets - (G∘F), H(A,D) - is left
	// alone: it reads as one thing as it stands, and bracketing it again
	// would only stutter.
	const QString trimmed = name.trimmed();
	if (trimmed.isEmpty())
		return true;
	if (trimmed.endsWith(QLatin1Char(')')))
		return true;
	for (QChar c : trimmed)
		if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QChar(0x27)   // a prime
		    && !c.isMark())
			return false;
	return true;
}

QString MapsElements::applied(const QString& functor, const QString& element)
{
	// fill the FIRST hole: a two-holed name like H(.,.) applied to A leaves
	// H(A,.), a bifunctor with one side still open
	QString filled = formula(functor);
	const int at = filled.indexOf(hole());
	if (at < 0)
		return filled + element;   // cannot happen; belt and braces
	filled.replace(at, 1, element);
	return filled;
}

Node* MapsElements::domain() const
{
	return arrow() != nullptr ? arrow()->domain() : nullptr;
}
Object* MapsElements::codomain() const
{
	// An object will do, so long as things can be drawn in it: an R-module
	// holds elements, and the image of an element is one.
	auto* cod = arrow() != nullptr ? dynamic_cast<Object*>(arrow()->codomain()) : nullptr;
	return cod != nullptr && cod->canHoldNamedChildren() ? cod : nullptr;
}

void MapsElements::stamp(Node* image, const QString& functor, const Node* source) const
{
	image->setData(ImageFunctorKey, functor);
	image->setData(ImageSourceKey, source != nullptr ? source->key() : QString());
	image->setData(ImageMappingKey, m_mappingId);
}

bool MapsElements::isOurImage(const Node* node) const
{
	if (node == nullptr)
		return false;
	if (node->data(DoomedKey).toBool())
		return false;   // already on its way out: not ours any more
	if (!m_mappingId.isEmpty() && node->data(ImageMappingKey).toString() == m_mappingId)
		return true;
	// drawn before this mapping had an identity, or read from an older file
	Arrow* F = arrow();
	return F != nullptr && node->data(ImageMappingKey).toString().isEmpty()
	    && node->data(ImageFunctorKey).toString() == F->id();
}


Node* MapsElements::sourceWithKey(const QString& key) const
{
	Node* dom = domain();
	if (dom == nullptr || key.isEmpty())
		return nullptr;
	for (QGraphicsItem* child : dom->childItems())
		if (auto* node = dynamic_cast<Node*>(child); node != nullptr && node->key() == key)
			return node;
	return nullptr;
}

qreal MapsElements::columnStep()
{
	// Wide enough for an image and the label of an arrow beside it, and a
	// whole number of grid steps, so a column still lands on the grid.
	const qreal unit = Node::snapUnit() > 0 ? Node::snapUnit() : 25.0;
	return 3.0 * unit;
}

bool MapsElements::domainIsRow() const
{
	Node* dom = domain();
	if (dom == nullptr)
		return false;

	// The spread of what is drawn in there, taken from the objects' own
	// places: wider than it is tall is a row. Arrows are left out - an arrow
	// is drawn BETWEEN two objects and has no place of its own to add.
	QRectF spread;
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || !node->isVisible() || dynamic_cast<Arrow*>(node) != nullptr)
			continue;
		spread |= node->mapRectToParent(node->boxRect());
	}
	// Nothing drawn, or a single object: there is no direction to it, and a
	// column is the arrangement to fall back on.
	return spread.width() > spread.height();
}

QPointF MapsElements::columnOffset() const
{
	const qreal along = column() * columnStep();
	// a quarter turn when the diagram is a row: the copies stack downwards
	return domainIsRow() ? QPointF(0, along) : QPointF(along, 0);
}

int MapsElements::column() const
{
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr || F->scene() == nullptr)
		return 0;

	// WHERE EACH FUNCTOR ARRIVES, not where its arrow happens to lie.
	//
	// The mappings that draw into the same place are read in the order their
	// arrows COME IN at it, and that is a point on the codomain's edge: the
	// end of the line. An arrow's middle is not the same thing - two
	// functors drawn from opposite corners meet the same edge a long way
	// from where their middles are - and it was the middle that put H in the
	// second column when its arrow arrives to the LEFT of I's.
	QList<QPair<QPointF, const MapsElements*>> peers;
	for (QGraphicsItem* item : F->scene()->items())
	{
		auto* line = dynamic_cast<Arrow*>(item);
		if (line == nullptr)
			continue;
		auto* maps = dynamic_cast<MapsElements*>(line->prop(Key()));
		if (maps == nullptr || maps->codomain() != cod)
			continue;
		const QPainterPath drawn = line->curve();
		const QPointF arrives = drawn.isEmpty()
			? line->sceneBoundingRect().center()
			: line->mapToScene(drawn.pointAtPercent(1.0));
		peers.append({ arrives, maps });
	}
	if (peers.size() < 2)
		return 0;

	// ALONG WHICHEVER AXIS THEY ACTUALLY SPREAD.
	//
	// Functors coming in along one edge of the codomain are ordered ALONG
	// that edge: arriving on the left edge they are read top to bottom,
	// arriving on the top edge, left to right. Nothing has to know which
	// edge that is - the arrival points say so themselves, by being spread
	// out on one axis and level on the other.
	QRectF spread;
	for (const auto& peer : peers)
		spread |= QRectF(peer.first, QSizeF(0.01, 0.01));
	const bool acrossThePage = spread.width() > spread.height();

	// Ties broken by the mapping's own identity, so two functors arriving at
	// the very same point still get a column each - and always the same one,
	// rather than swapping places every time the scene is walked.
	std::sort(peers.begin(), peers.end(),
	          [acrossThePage](const QPair<QPointF, const MapsElements*>& a,
	                          const QPair<QPointF, const MapsElements*>& b) {
		const qreal at = acrossThePage ? a.first.x() : a.first.y();
		const qreal bt = acrossThePage ? b.first.x() : b.first.y();
		if (!qFuzzyCompare(at + 1.0, bt + 1.0))
			return at < bt;
		return a.second->mappingId() < b.second->mappingId();
	});

	for (int at = 0; at < peers.size(); ++at)
		if (peers.at(at).second == this)
			return at;
	return 0;
}

namespace
{
	// THE RUN AN ARROW MAKES, in its own coordinates: the middle of what it
	// leaves to the middle of what it arrives at.
	//
	// NOT the drawn curve. The curve already has the bends in it - they are
	// what moves its ends along the two frames - so reading the frame off it
	// and then writing bends back against that frame is a loop: each sync
	// would read a shape the last sync's bend had already altered, and the
	// bow would grow every time. The two ends do not move when the line
	// bends, so they are a frame that stays put.
	bool runOf(const Arrow* line, QPointF& from, QPointF& to)
	{
		if (line == nullptr)
			return false;
		Node* leaves = line->domain();
		Node* arrives = line->codomain();
		if (leaves != nullptr && arrives != nullptr)
		{
			from = line->mapFromItem(leaves, leaves->boxRect().center());
			to = line->mapFromItem(arrives, arrives->boxRect().center());
			if (QLineF(from, to).length() > 1e-6)
				return true;
		}
		// half-drawn, or the two ends on top of one another: the line as
		// drawn is all there is to go on
		const QPainterPath drawn = line->curve();
		if (drawn.isEmpty())
			return false;
		from = drawn.pointAtPercent(0.0);
		to = drawn.pointAtPercent(1.0);
		return QLineF(from, to).length() > 1e-6;
	}
}

QList<QPointF> MapsElements::carriedBends(const Arrow* from, const Arrow* to, bool reversed)
{
	if (from == nullptr || to == nullptr)
		return QList<QPointF>();
	const QList<QPointF> shape = from->bends();
	if (shape.isEmpty())
		return shape;

	QPointF theirStart, theirEnd, ourStart, ourEnd;
	if (!runOf(from, theirStart, theirEnd) || !runOf(to, ourStart, ourEnd))
		return shape;   // no run to read it against: the points themselves are the best guess

	const QLineF theirs(theirStart, theirEnd);
	const QLineF ours(ourStart, ourEnd);
	const qreal theirLength = theirs.length();
	const qreal ourLength = ours.length();
	const QPointF theirAlong = (theirEnd - theirStart) / theirLength;
	const QPointF theirAside(-theirAlong.y(), theirAlong.x());
	const QPointF ourAlong = (ourEnd - ourStart) / ourLength;
	const QPointF ourAside(-ourAlong.y(), ourAlong.x());

	QList<QPointF> carried;
	carried.reserve(shape.size());
	for (const QPointF& bend : shape)
	{
		const QPointF offset = bend - theirStart;
		qreal along = QPointF::dotProduct(offset, theirAlong) / theirLength;
		qreal aside = QPointF::dotProduct(offset, theirAside) / theirLength;
		// A CONTRAVARIANT IMAGE RUNS THE OTHER WAY, and its run is read from
		// the other end: the same curve is then the same distance from the
		// far end, and bowed to the other side, or the copy comes out
		// mirrored.
		if (reversed)
		{
			along = 1.0 - along;
			aside = -aside;
		}
		carried << ourStart + ourAlong * (along * ourLength) + ourAside * (aside * ourLength);
	}
	// The order goes with the direction: a line reversed meets its points
	// back to front.
	if (reversed)
		std::reverse(carried.begin(), carried.end());
	return carried;
}

QPointF MapsElements::imagePlace(const Node* source) const
{
	// ABSOLUTE, AND NOTHING ELSE - bar the column this mapping draws in.
	//
	// The image of X is AT X's place - the same point, read in the codomain's
	// own coordinates instead of the domain's - moved along by however many
	// mappings draw into the same place before this one (sideways for a
	// domain drawn as a column, downwards for one drawn as a row). Both are
	// children, so the two local frames are read as one frame, and the
	// codomain ends up a copy of the arrangement in the domain, one copy per
	// functor, side by side in the order the functors are drawn.
	//
	// Nothing is remembered between the two and nothing is added up. Carrying
	// MOVES across as deltas is what let them drift: a move the image made on
	// its own account - a collision push, a grid snap, a frame settling - was
	// not a move of the source, so the gap it opened was kept for ever. An
	// offset remembered per image had the same fault in slower motion, since
	// a gap once opened was simply adopted as the arrangement.
	if (source == nullptr)
		return QPointF();
	return source->pos() + columnOffset();
}

Node* MapsElements::imageOf(Object* cod, const QString& functor, const Node* source) const
{
	if (cod == nullptr || source == nullptr)
		return nullptr;
	const QString key = source->key();
	Node* byName = nullptr;
	const QString name = applied(functor, source->id());
	for (QGraphicsItem* child : cod->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr)
			continue;
		// ours, and of THAT NODE: found however it happens to be called, and
		// never confused with the image of another object of the same name
		if (isOurImage(node) && !key.isEmpty() && node->data(ImageSourceKey).toString() == key)
			return node;
		// Adopting by name is the last resort: an image drawn by the one-shot
		// before it was stamped, or read from a file written before nodes had
		// keys - those carry the source's LABEL where the key now goes, which
		// is the very confusion this is undoing. Taking one re-stamps it with
		// the key, so the migration happens once and the SECOND object of the
		// same name no longer matches and gets an image of its own.
		const QString stamped = node->data(ImageSourceKey).toString();
		if (byName == nullptr && !node->data(DoomedKey).toBool() && node->id() == name
		 && (stamped.isEmpty() || stamped == source->id()))
			byName = node;
	}
	if (byName != nullptr)
		stamp(byName, functor, source);
	return byName;
}
void MapsElements::listen(bool on)
{
	Arrow* F = arrow();
	auto* scene = F != nullptr ? dynamic_cast<DiagramScene*>(F->scene()) : nullptr;
	if (scene == nullptr)
		return;
	if (on)
	{
		// anything drawn or deleted, and anything undone or redone
		connect(scene, &DiagramScene::nodesAdded, this, &MapsElements::sync, Qt::UniqueConnection);
		connect(scene, &DiagramScene::nodesRemoved, this, &MapsElements::sync, Qt::UniqueConnection);
		connect(scene->history(), &SceneHistory::changed, this, &MapsElements::sync, Qt::UniqueConnection);
	}
	else
	{
		disconnect(scene, nullptr, this, nullptr);
		disconnect(scene->history(), nullptr, this, nullptr);
	}
}

void MapsElements::setMirrorsGeometry(bool mirror)
{
	if (m_mirror == mirror)
		return;
	m_mirror = mirror;

	// GEOMETRY ONLY. Off, the two sides stop travelling with each other and
	// each keeps the arrangement it has; the image itself is untouched - it
	// stays drawn, stays on show, and goes on following what is drawn and
	// deleted in the domain. Putting the image away is setLive(false), which
	// is a different question and a different switch.
	emit settingsChanged();
}

void MapsElements::setLive(bool live)
{
	if (m_live == live)
		return;
	m_live = live;

	// Off, the image is put AWAY, not given up: the nodes are hidden, and
	// whatever is drawn inside them is hidden with them and comes back
	// untouched. On, they are shown again and brought up to date at once.
	if (Object* cod = codomain())
	{
		for (QGraphicsItem* child : cod->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node != nullptr && isOurImage(node))
				node->setVisible(live);
		}
		// the codomain's frame is the union of what it HOLDS AND SHOWS
		cod->refreshFrame();
	}

	listen(live);
	if (live)
		sync();
	emit settingsChanged();
}

void MapsElements::setContravariant(bool contravariant)
{
	if (m_contravariant == contravariant)
		return;
	m_contravariant = contravariant;
	if (m_live)
		sync();   // the image arrows turn round
	emit settingsChanged();
}

void MapsElements::onSourceBends(Arrow* source)
{
	if (m_syncing || !m_mirror || source == nullptr)
		return;
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr)
		return;
	auto* image = dynamic_cast<Arrow*>(imageOf(cod, F->id(), source));
	if (image == nullptr)
		return;
	m_syncing = true;
	image->setBends(carriedBends(source, image, isContravariant()));   // the same SHAPE
	m_syncing = false;
}

void MapsElements::onImageBends(Arrow* image)
{
	if (m_syncing || !m_mirror || image == nullptr)
		return;
	Node* dom = domain();
	if (dom == nullptr)
		return;
	const QString key = image->data(ImageSourceKey).toString();
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* source = dynamic_cast<Arrow*>(child);
		if (source == nullptr || key.isEmpty() || source->key() != key)
			continue;
		m_syncing = true;
		source->setBends(carriedBends(image, source, isContravariant()));
		m_syncing = false;
		return;
	}
}



void MapsElements::onSourceMoved(Node* source, const QPointF& delta)
{
	// m_syncing is held by whichever side is writing, so the answering move
	// never comes back round
	if (m_syncing || !m_mirror || source == nullptr || delta.isNull())
		return;
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr)
		return;
	Node* image = imageOf(cod, F->id(), source);
	if (image == nullptr)
		return;

	// TO THE SOURCE'S PLACE, not by the amount the source travelled: see
	// imagePlace. The delta this was handed says only THAT it moved.
	m_syncing = true;
	image->setPos(imagePlace(source));
	m_syncing = false;
}

void MapsElements::onImageMoved(Node* image, const QPointF& delta)
{
	if (m_syncing || !m_mirror || image == nullptr || delta.isNull())
		return;
	Node* dom = domain();
	if (dom == nullptr)
		return;
	const QString key = image->data(ImageSourceKey).toString();
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || key.isEmpty() || node->key() != key)
			continue;
		// Dragged by hand: the source goes to the matching place - the one
		// this mapping's column came from, so dragging an image in the second
		// column does not haul its source a column sideways.
		m_syncing = true;
		node->setPos(image->pos() - columnOffset());
		m_syncing = false;
		return;
	}
}

void MapsElements::onSourceLabelMoved(Node* source, const QPointF& delta)
{
	// A label dragged clear of its node is part of how the diagram is laid
	// out, so it crosses like a position does: BY THE SAME AMOUNT, so the
	// image label keeps whatever placement it was given and simply travels
	// with the one it answers to.
	if (m_syncing || !m_mirror || source == nullptr || delta.isNull())
		return;
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr)
		return;
	Node* image = imageOf(cod, F->id(), source);
	if (image == nullptr)
		return;
	m_syncing = true;
	image->setLabelOffset(image->labelOffset() + delta);
	m_syncing = false;
}

void MapsElements::onImageLabelMoved(Node* image, const QPointF& delta)
{
	if (m_syncing || !m_mirror || image == nullptr || delta.isNull())
		return;
	Node* dom = domain();
	if (dom == nullptr)
		return;
	const QString key = image->data(ImageSourceKey).toString();
	if (key.isEmpty())
		return;
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || node->key() != key)
			continue;
		m_syncing = true;
		node->setLabelOffset(node->labelOffset() + delta);
		m_syncing = false;
		return;
	}
}

void MapsElements::onSourceDeleted(Node* source)
{
	if (!m_live || source == nullptr)
		return;
	Arrow* F = arrow();
	Object* cod = codomain();
	if (F == nullptr || cod == nullptr)
		return;

	// the source is inside its own destructor: its label is still readable,
	// which is how the image it stands for is found
	Node* image = imageOf(cod, F->id(), source);
	if (image == nullptr)
		return;

	// deleteLater, not delete: we are inside ~Node, and the image may itself
	// be the source of another functor's image. Its own destructor emits
	// deleted() in turn, and the chain carries on from there.
	image->deleteLater();
}

bool MapsElements::closesALoop() const
{
	Arrow* F = arrow();
	Node* dom = domain();
	Object* cod = codomain();
	if (F == nullptr || dom == nullptr || cod == nullptr)
		return false;
	if (dom == cod)
		return true;   // its own domain: the first lap is already the second

	auto* board = dynamic_cast<DiagramScene*>(F->scene());
	if (board == nullptr)
		return false;

	// every OTHER live mapping, as an edge from what it reads to what it draws
	QList<QPair<Node*, Node*>> edges;
	for (QGraphicsItem* item : board->items())
	{
		auto* other = dynamic_cast<Arrow*>(item);
		if (other == nullptr || other == F)
			continue;
		auto* maps = dynamic_cast<MapsElements*>(other->prop(MapsElements::Key()));
		if (maps == nullptr || !maps->isLive())
			continue;
		Node* from = maps->domain();
		Node* to = maps->codomain();
		if (from != nullptr && to != nullptr)
			edges.append(qMakePair(from, to));
	}

	// can what we draw into reach what we read from?
	QSet<Node*> seen;
	QList<Node*> front;
	front << cod;
	seen << cod;
	for (int at = 0; at < front.size(); ++at)
	{
		for (const QPair<Node*, Node*>& edge : edges)
		{
			if (edge.first != front.at(at) || seen.contains(edge.second))
				continue;
			if (edge.second == dom)
				return true;
			seen << edge.second;
			front << edge.second;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Recursive helpers for sync() and mapDiagram()
// ---------------------------------------------------------------------------

// Create the image of `source` (a non-arrow) inside `imgCat`, or find the
// existing one. Always returns a Category when the source is a Category,
// regardless of what imgCat::makeObject would produce.
Object* MapsElements::makeOrFindImage(MapsElements* self, Object* imgCat,
                                       const QString& name, Node* source, bool live, bool mirror)
{
	Node* img = self->imageOf(imgCat, name, source);
	if (img == nullptr)
	{
		const QPointF scenePos = imgCat->mapToScene(self->imagePlace(source));
		if (dynamic_cast<Category*>(source) != nullptr)
		{
			// A CATEGORY SOURCE MUST PRODUCE A CATEGORY IMAGE.
			//
			// imgCat->createNamedChild calls makeObject, which returns a plain
			// Object for a plain Category. That leaves the image category empty
			// and unable to hold anything. Creating it directly gives the right type.
			auto* cat = new Category(applied(name, source->id()), imgCat);
			cat->setPos(imgCat->mapFromScene(scenePos));
			cat->setZValue(1);
			cat->refreshDepthAppearance();
			cat->refreshFrame();
			img = cat;
		}
		else
		{
			img = imgCat->createNamedChild(applied(name, source->id()), scenePos);
		}
		self->stamp(img, name, source);
		img->setVisible(live);
	}
	else
	{
		const QString wanted = applied(name, source->id());
		if (img->id() != wanted)
			img->setId(wanted);
		if (mirror && img->pos() != self->imagePlace(source))
			img->setPos(self->imagePlace(source));
	}
	return dynamic_cast<Object*>(img);
}

void MapsElements::syncObjectsRecursive(Node* sourceCat, Object* imgCat,
                                         const QString& name,
                                         QMap<Node*, Node*>& imageMap,
                                         QStringList& live)
{
	for (QGraphicsItem* child : sourceCat->childItems())
	{
		auto* source = dynamic_cast<Node*>(child);
		if (source == nullptr || dynamic_cast<Arrow*>(source) != nullptr)
			continue;
		if (source->id().isEmpty())
		{
			// MID-EDIT IS NOT GONE: keep its image alive until the editor closes
			if (source->isEditingLabel())
				live << source->key();
			continue;
		}
		live << source->key();
		Object* img = makeOrFindImage(this, imgCat, name, source, m_live, m_mirror);
		if (img == nullptr)
			continue;
		imageMap.insert(source, img);
		connect(source, &Node::moved, this, &MapsElements::onSourceMoved, Qt::UniqueConnection);
		connect(source, &Node::idChanged, this, &MapsElements::sync, Qt::UniqueConnection);
		connect(source, &Node::deleted, this, &MapsElements::onSourceDeleted, Qt::UniqueConnection);
		connect(img, &Node::moved, this, &MapsElements::onImageMoved, Qt::UniqueConnection);
		connect(source, &Node::labelOffsetChanged, this, &MapsElements::onSourceLabelMoved, Qt::UniqueConnection);
		connect(img, &Node::labelOffsetChanged, this, &MapsElements::onImageLabelMoved, Qt::UniqueConnection);

		// RECURSE INTO NESTED CATEGORIES.
		//
		// A functor maps the whole diagram, however deeply nested: objects and
		// arrows inside a subcategory of C must appear inside the image of that
		// subcategory in F(C). The image was just made a Category above, so it
		// can hold children; descend into it now.
		if (auto* srcCat2 = dynamic_cast<Category*>(source))
			if (auto* dstCat2 = dynamic_cast<Category*>(img))
				syncObjectsRecursive(srcCat2, dstCat2, name, imageMap, live);
	}
}

void MapsElements::syncArrowsRecursive(Node* sourceCat, Object* imgCat,
                                        const QString& name,
                                        const QMap<Node*, Node*>& imageMap,
                                        QStringList& live)
{
	// Recurse into nested categories first (objects already exist there)
	for (QGraphicsItem* child : sourceCat->childItems())
	{
		auto* srcNode = dynamic_cast<Node*>(child);
		if (srcNode == nullptr || dynamic_cast<Arrow*>(srcNode) != nullptr)
			continue;
		if (auto* srcCat2 = dynamic_cast<Category*>(srcNode))
			if (auto* dstCat2 = dynamic_cast<Category*>(imageMap.value(srcCat2)))
				syncArrowsRecursive(srcCat2, dstCat2, name, imageMap, live);
	}

	// Then handle the arrows at this level
	for (QGraphicsItem* child : sourceCat->childItems())
	{
		auto* source = dynamic_cast<Arrow*>(child);
		if (source == nullptr)
			continue;
		if (source->id().isEmpty() && source->style() != Arrow::Style::Equals)
		{
			if (source->isEditingLabel())
				live << source->key();
			continue;
		}
		Node* from = imageMap.value(source->domain());
		Node* to = imageMap.value(source->codomain());
		if (from == nullptr || to == nullptr)
			continue;
		if (m_contravariant)
			std::swap(from, to);
		live << source->key();
		const bool equals = source->style() == Arrow::Style::Equals;

		Node* img = imageOf(imgCat, name, source);
		if (img == nullptr)
		{
			if (equals)
			{
				auto* drawn = new Arrow(QString(), from, to, imgCat);
				drawn->setStyle(Arrow::Style::Equals);
				drawn->setZValue(2);
				drawn->refreshDepthAppearance();
				drawn->refreshFrame();
				img = drawn;
				stamp(img, name, source);
				img->setVisible(m_live);
			}
			else
			{
				auto* codCat = dynamic_cast<Category*>(imgCat);
				if (codCat == nullptr)
					continue;
				img = codCat->createArrow(applied(name, source->id()), from, to);
				stamp(img, name, source);
				img->setVisible(m_live);
			}
		}
		else if (equals)
		{
			if (auto* ia = dynamic_cast<Arrow*>(img);
			    ia != nullptr && ia->style() != Arrow::Style::Equals)
				ia->setStyle(Arrow::Style::Equals);
			if (!img->id().isEmpty())
				img->setId(QString());
		}
		else if (img->id() != applied(name, source->id()))
		{
			img->setId(applied(name, source->id()));
		}
		connect(source, &Node::idChanged, this, &MapsElements::sync, Qt::UniqueConnection);
		connect(source, &Node::deleted, this, &MapsElements::onSourceDeleted, Qt::UniqueConnection);
		connect(source, &Arrow::bendsChanged, this, &MapsElements::onSourceBends, Qt::UniqueConnection);
		if (auto* imageArrow = dynamic_cast<Arrow*>(img))
		{
			if (imageArrow->domain() != from)
				imageArrow->setDomain(from);
			if (imageArrow->codomain() != to)
				imageArrow->setCodomain(to);
			connect(imageArrow, &Arrow::bendsChanged, this, &MapsElements::onImageBends, Qt::UniqueConnection);
			connect(source, &Node::labelOffsetChanged, this, &MapsElements::onSourceLabelMoved, Qt::UniqueConnection);
			connect(imageArrow, &Node::labelOffsetChanged, this, &MapsElements::onImageLabelMoved, Qt::UniqueConnection);
			const QList<QPointF> shape = carriedBends(source, imageArrow, isContravariant());
			if (imageArrow->bends() != shape)
				imageArrow->setBends(shape);
		}
	}
}

void MapsElements::removeStaleRecursive(Object* imgCat, const QStringList& live)
{
	// Recurse into nested image categories first
	for (QGraphicsItem* child : imgCat->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || !isOurImage(node) || dynamic_cast<Arrow*>(node) != nullptr)
			continue;
		if (auto* nested = dynamic_cast<Category*>(node))
			removeStaleRecursive(nested, live);
	}
	// Now remove stale items at this level (arrows before objects)
	QList<Node*> staleObjects, staleArrows;
	for (QGraphicsItem* child : imgCat->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node == nullptr || !isOurImage(node))
			continue;
		if (live.contains(node->data(ImageSourceKey).toString()))
			continue;
		(dynamic_cast<Arrow*>(node) != nullptr ? staleArrows : staleObjects) << node;
	}
	deleteAll(staleArrows);
	deleteAll(staleObjects);
	if (!staleArrows.isEmpty() || !staleObjects.isEmpty())
		imgCat->refreshFrame();
}

// ---------------------------------------------------------------------------

void MapsElements::sync()
{
	Arrow* F = arrow();
	Node* dom = domain();
	Object* cod = codomain();
	if (!m_live || F == nullptr || dom == nullptr || cod == nullptr || dom == cod || m_syncing)
		return;

	// Round in circles (see closesALoop): switched off rather than run. Not
	// through setLive - that would sync again from inside sync -
	// but by hand, and the images already drawn are left exactly where they
	// are. Turning it back on will refuse again until the loop is broken.
	if (closesALoop())
	{
		m_live = false;
		listen(false);
		if (auto* board = dynamic_cast<DiagramScene*>(F->scene()))
			QMetaObject::invokeMethod(board, "message", Qt::QueuedConnection,
				Q_ARG(QString, QString("%1 is not kept live: %2 leads back round to %3, so what it "
				                       "drew would come home as something new to draw, over and over. "
				                       "Break the loop, or map it once by hand.")
					.arg(F->id().isEmpty() ? QStringLiteral("That mapping") : F->id(),
					     cod->id(), dom->id())));
		emit settingsChanged();
		return;
	}

	// The functor itself may have been taken out of the diagram - deleted, or
	// undone. There is no mapping without it, so its images go too; they come
	// back with it, because the next sync draws them again.
	if (F->scene() == nullptr || F->parentItem() == nullptr)
	{
		removeImages();
		return;
	}

	// AND A HARD FLOOR UNDER ALL OF IT.
	//
	// Mappings answer one another - what one draws is news to the next - and
	// answering is what puts this on the stack. closesALoop() catches the
	// shape that never ends; this catches anything else that starts to run
	// away, whatever it is. Eight deep is far more than any chain of functors
	// anyone draws; a ninth is a bug, and it stops here rather than in the
	// middle of a repaint.
	static int s_depth = 0;
	if (s_depth >= 8)
		return;
	struct Depth
	{
		int& n;
		explicit Depth(int& count) : n(count) { ++n; }
		~Depth() { --n; }
	} depth(s_depth);

	m_syncing = true;
	m_name = F->id();
	auto* scene = dynamic_cast<DiagramScene*>(F->scene());
	if (scene != nullptr)
		scene->history()->suspend(true);   // the image is a consequence, not a step

	const QString name = F->id();
	QStringList live;   // the KEYS of the sources that still exist
	QMap<Node*, Node*> imageMap;

	syncObjectsRecursive(dom, cod, name, imageMap, live);
	syncArrowsRecursive(dom, cod, name, imageMap, live);
	removeStaleRecursive(cod, live);

	if (scene != nullptr)
		scene->history()->suspend(false);
	m_syncing = false;
}




void MapsElements::arrowContextMenu(QMenu& menu, Arrow* arrow)
{
	Node* dom = arrow->domain();
	Object* cod = codomain();
	if (dom == nullptr || cod == nullptr)
	{
		// Shown greyed rather than left out: a missing entry looks like a bug,
		// and the reason is worth knowing.
		QAction* why = menu.addAction("Map the elements");
		why->setEnabled(false);
		why->setToolTip(QString("Nothing to map into: nothing can be drawn inside %1, so there is "
		                        "nowhere for the image to go. An object holds elements only when "
		                        "its category is concrete - when its objects have underlying sets.")
			.arg(arrow->codomain() != nullptr ? arrow->codomain()->id() : QStringLiteral("the far end")));
		return;
	}

	// One thing, the thing you came for. How the mapping BEHAVES - live, and
	// what it carries across - is a property of the arrow, and properties live
	// in the Properties dock.
	QAction* once = menu.addAction(QString("Map the elements of %1 into %2").arg(dom->id(), cod->id()));
	once->setToolTip(QString("Everything drawn in %1 appears in %2 under %3: an x becomes %4. "
	                         "The Properties dock has the rest: keeping it live, and what it carries over.")
		.arg(dom->id(), cod->id(), arrow->id(), applied(arrow->id(), "x")));
	QObject::connect(once, &QAction::triggered, this, [this] { mapDiagram(); });
}

void MapsElements::setMappingId(const QString& id)
{
	if (id.isEmpty() || id == m_mappingId)
		return;
	m_mappingId = id;
	if (Arrow* F = arrow())
		F->setData(MappingIdKey, id);
}

// ---------------------------------------------------------------------------
// One-shot recursive mapping for mapDiagram()
// ---------------------------------------------------------------------------

void MapsElements::mapObjectsRecursive(MapsElements* self, Node* sourceCat, Object* imgCat,
                                        const QString& name, bool contravariant,
                                        QMap<Node*, Node*>& imageMap, QList<Node*>& made, int& drawn)
{
	for (QGraphicsItem* child : sourceCat->childItems())
	{
		auto* source = dynamic_cast<Node*>(child);
		if (source == nullptr || dynamic_cast<Arrow*>(source) != nullptr || source->id().isEmpty())
			continue;
		Node* already = self->imageOf(imgCat, name, source);
		if (already == nullptr)
		{
			const QPointF spot = freeSpotIn(imgCat, source->pos());
			const QPointF scenePos = imgCat->mapToScene(spot);
			if (dynamic_cast<Category*>(source) != nullptr)
			{
				auto* cat = new Category(applied(name, source->id()), imgCat);
				cat->setPos(imgCat->mapFromScene(scenePos));
				cat->setZValue(1);
				cat->refreshDepthAppearance();
				cat->refreshFrame();
				already = cat;
			}
			else
			{
				already = imgCat->createNamedChild(applied(name, source->id()), scenePos);
			}
			self->stamp(already, name, source);
			already->setVisible(true);
			made << already;
			++drawn;
		}
		imageMap.insert(source, already);

		if (auto* srcCat2 = dynamic_cast<Category*>(source))
			if (auto* dstCat2 = dynamic_cast<Category*>(already))
				mapObjectsRecursive(self, srcCat2, dstCat2, name, contravariant, imageMap, made, drawn);
	}
}

void MapsElements::mapArrowsRecursive(MapsElements* self, Node* sourceCat, Object* imgCat,
                                       const QString& name, bool contravariant,
                                       const QMap<Node*, Node*>& imageMap, QList<Node*>& made, int& drawn)
{
	// Recurse into nested categories first
	for (QGraphicsItem* child : sourceCat->childItems())
	{
		auto* srcNode = dynamic_cast<Node*>(child);
		if (srcNode == nullptr || dynamic_cast<Arrow*>(srcNode) != nullptr)
			continue;
		if (auto* srcCat2 = dynamic_cast<Category*>(srcNode))
			if (auto* dstCat2 = dynamic_cast<Category*>(imageMap.value(srcCat2)))
				mapArrowsRecursive(self, srcCat2, dstCat2, name, contravariant, imageMap, made, drawn);
	}

	for (QGraphicsItem* child : sourceCat->childItems())
	{
		auto* a = dynamic_cast<Arrow*>(child);
		if (a == nullptr || a->id().isEmpty())
			continue;
		Node* from = imageMap.value(a->domain());
		Node* to = imageMap.value(a->codomain());
		if (from == nullptr || to == nullptr)
			continue;
		if (contravariant)
			std::swap(from, to);
		if (self->imageOf(imgCat, name, a) != nullptr)
			continue;
		auto* codCat = dynamic_cast<Category*>(imgCat);
		if (codCat == nullptr)
			continue;
		Node* drawnArrow = codCat->createArrow(MapsElements::applied(name, a->id()), from, to);
		self->stamp(drawnArrow, name, a);
		drawnArrow->setVisible(true);
		made << drawnArrow;
		++drawn;
	}
}

// ---------------------------------------------------------------------------

int MapsElements::mapDiagram()
{
	Arrow* F = arrow();
	Node* dom = domain();
	Object* cod = codomain();
	if (F == nullptr || dom == nullptr || cod == nullptr)
		return 0;

	const QString name = F->id();
	QMap<Node*, Node*> imageMap;
	QList<Node*> made;
	int drawn = 0;

	mapObjectsRecursive(this, dom, cod, name, m_contravariant, imageMap, made, drawn);
	mapArrowsRecursive(this, dom, cod, name, m_contravariant, imageMap, made, drawn);

	if (auto* scene = dynamic_cast<DiagramScene*>(cod->scene()))
		scene->recordCreation(QString("Mapped %1 into %2 by %3").arg(dom->id(), cod->id(), name), made);
	return drawn;
}
