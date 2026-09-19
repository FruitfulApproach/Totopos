#include "core/props/MapsElements.h"

#include <QMenu>
#include <QMap>
#include <QSet>
#include <QPair>
#include <QList>
#include <QUuid>
#include <utility>
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
	image->setBends(source->bends());   // the same points, the same shape
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
		source->setBends(image->bends());
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

	// BY THE SAME AMOUNT, not to the same place: the image keeps whatever
	// position it was given, and simply travels with what it is the image of.
	m_syncing = true;
	image->setPos(image->pos() + delta);
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
		m_syncing = true;
		node->setPos(node->pos() + delta);
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

	// the objects
	QMap<Node*, Node*> image;
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* source = dynamic_cast<Node*>(child);
		if (source == nullptr || dynamic_cast<Arrow*>(source) != nullptr)
			continue;
		if (source->id().isEmpty())
		{
			// MID-EDIT IS NOT GONE.
			//
			// Opening the editor selects the whole name, so the first
			// keystroke empties it for an instant. Read as a source that no
			// longer has a name, its image counted as stale and was taken
			// away and drawn again on every keypress - which is both a great
			// deal of work for nothing and the way items came to be deleted
			// from inside the keyboard handler. It keeps its image, and its
			// old name, until the editor closes.
			if (source->isEditingLabel())
				live << source->key();
			continue;
		}
		live << source->key();
		Node* img = imageOf(cod, name, source);
		if (img == nullptr)
		{
			// where it first appears: at the same offset as its source. From
			// then on its position is ITS OWN - it moves when the source
			// moves, by the same amount, but it can be put wherever you like.
			img = cod->createNamedChild(applied(name, source->id()), cod->mapToScene(source->pos()));
			stamp(img, name, source);
			img->setVisible(m_live);
		}
		else if (img->id() != applied(name, source->id()))
		{
			img->setId(applied(name, source->id()));   // the source was relabelled
		}
		image.insert(source, img);
		connect(source, &Node::moved, this, &MapsElements::onSourceMoved, Qt::UniqueConnection);
		connect(source, &Node::idChanged, this, &MapsElements::sync, Qt::UniqueConnection);
		connect(source, &Node::deleted, this, &MapsElements::onSourceDeleted, Qt::UniqueConnection);
		connect(img, &Node::moved, this, &MapsElements::onImageMoved, Qt::UniqueConnection);
		connect(source, &Node::labelOffsetChanged, this, &MapsElements::onSourceLabelMoved, Qt::UniqueConnection);
		connect(img, &Node::labelOffsetChanged, this, &MapsElements::onImageLabelMoved, Qt::UniqueConnection);
	}

	// the arrows, between the images of their ends
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* source = dynamic_cast<Arrow*>(child);
		if (source == nullptr)
			continue;
		// An EQUALS is nameless for good: it says the two ends are the same
		// thing and there is nothing else to call it. Everything else with no
		// name is either being typed into or is not ours to carry across.
		if (source->id().isEmpty() && source->style() != Arrow::Style::Equals)
		{
			if (source->isEditingLabel())
				live << source->key();   // being typed into: not gone (see above)
			continue;
		}
		Node* from = image.value(source->domain());
		Node* to = image.value(source->codomain());
		if (from == nullptr || to == nullptr)
			continue;   // an end outside C: not ours to map
		// contravariant: the image of f : X -> Y runs F(Y) -> F(X)
		if (m_contravariant)
			std::swap(from, to);
		live << source->key();
		// AN EQUALS CROSSES BECAUSE A MAP IS A MAP.
		//
		// x = y in M says the two are the same element, and a map sends the
		// same element to the same value: f(x) = f(y), whatever f is. It is
		// not something an R-linear map does over and above being a function -
		// it is what a function IS - so this is handled here, once, at the
		// level of elements, and every kind of arrow that carries elements
		// across gets it: an R-module homomorphism because it is a map of
		// sets in particular.
		//
		// It is drawn, never named: an equals has no label (see
		// Arrow::Style::Equals), so unlike an image morphism there is nothing
		// to apply the functor's name to, and nothing to rename later.
		const bool equals = source->style() == Arrow::Style::Equals;

		Node* img = imageOf(cod, name, source);
		if (img == nullptr)
		{
			if (equals)
			{
				// Not through Category::createArrow: the two ends are ELEMENTS,
				// and what holds them is an object (a module), which is not a
				// category and makes no morphisms. An equals between two of the
				// things inside it is not a morphism either.
				auto* drawn = new Arrow(QString(), from, to, cod);
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
				auto* codCat = dynamic_cast<Category*>(cod);
				if (codCat == nullptr)
					continue;   // elements of a module have no arrows between them
				img = codCat->createArrow(applied(name, source->id()), from, to);
				stamp(img, name, source);
				img->setVisible(m_live);
			}
		}
		else if (equals)
		{
			// it may have been drawn as an ordinary image and then said to be
			// an equals, or the other way about
			if (auto* imageArrow = dynamic_cast<Arrow*>(img); imageArrow != nullptr
			    && imageArrow->style() != Arrow::Style::Equals)
				imageArrow->setStyle(Arrow::Style::Equals);
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
			// the variance may have been switched since it was drawn
			if (imageArrow->domain() != from)
				imageArrow->setDomain(from);
			if (imageArrow->codomain() != to)
				imageArrow->setCodomain(to);
			connect(imageArrow, &Arrow::bendsChanged, this, &MapsElements::onImageBends, Qt::UniqueConnection);
			connect(source, &Node::labelOffsetChanged, this, &MapsElements::onSourceLabelMoved, Qt::UniqueConnection);
			connect(imageArrow, &Node::labelOffsetChanged, this, &MapsElements::onImageLabelMoved, Qt::UniqueConnection);
			if (imageArrow->bends() != source->bends())
				imageArrow->setBends(source->bends());
		}
	}

	// and whatever we drew for something that is no longer there: arrows
	// first, so nothing is left pointing at an object that has gone
	QList<Node*> staleObjects, staleArrows;
	for (QGraphicsItem* child : cod->childItems())
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
		cod->refreshFrame();   // hidden already; the frame no longer holds them

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

int MapsElements::mapDiagram()
{
	Arrow* F = arrow();
	Node* dom = domain();
	Object* cod = codomain();
	if (F == nullptr || dom == nullptr || cod == nullptr)
		return 0;

	const QString name = F->id();
	QMap<Node*, Node*> image;
	QList<Node*> made;
	int drawn = 0;

	// The objects first, each at the same offset from its category's origin,
	// so the image sits in the codomain as the original sits in the domain.
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* object = dynamic_cast<Node*>(child);
		if (object == nullptr || dynamic_cast<Arrow*>(object) != nullptr || object->id().isEmpty())
			continue;   // the category's own label is not a node; arrows come next
		Node* already = imageOf(cod, name, object);
		if (already == nullptr)
		{
			already = cod->createNamedChild(applied(name, object->id()), cod->mapToScene(object->pos()));
			stamp(already, name, object);
			already->setVisible(true);   // asked for by hand: shown
			made << already;
			++drawn;
		}
		image.insert(object, already);
	}

	// then the arrows, between the images of their ends
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* a = dynamic_cast<Arrow*>(child);
		if (a == nullptr || a->id().isEmpty())
			continue;
		Node* from = image.value(a->domain());
		Node* to = image.value(a->codomain());
		if (from == nullptr || to == nullptr)
			continue;   // an end outside the domain: not ours to map
		if (m_contravariant)
			std::swap(from, to);
		if (imageOf(cod, name, a) != nullptr)
			continue;   // already drawn
		auto* codCat = dynamic_cast<Category*>(cod);
		if (codCat == nullptr)
			continue;   // elements of a module have no arrows between them
		Node* drawnArrow = codCat->createArrow(applied(name, a->id()), from, to);
		stamp(drawnArrow, name, a);
		drawnArrow->setVisible(true);
		made << drawnArrow;
		++drawn;
	}

	if (auto* scene = dynamic_cast<DiagramScene*>(cod->scene()))
		scene->recordCreation(QString("Mapped %1 into %2 by %3").arg(dom->id(), cod->id(), name), made);
	return drawn;
}
