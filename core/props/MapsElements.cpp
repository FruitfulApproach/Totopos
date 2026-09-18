#include "core/props/MapsElements.h"

#include <QMenu>
#include <QMap>
#include <QUuid>
#include <utility>
#include "art/Arrow.h"
#include "art/Category.h"
#include "core/AppSettings.h"
#include "core/Emoji.h"
#include "art/DiagramScene.h"
#include "art/Functor.h"
#include "core/history/SceneHistory.h"

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
	Category* cod = codomain();
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
	Category* cod = codomain();
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
	for (Node* node : arrows)
		delete node;
	for (Node* node : objects)
		delete node;
	m_syncing = false;
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
	// no hole written: it goes at the end, in whichever notation is set
	return AppSettings::instance().functorParentheses()
		? functor + QLatin1Char('(') + hole() + QLatin1Char(')')
		: functor + hole();
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
Category* MapsElements::codomain() const
{
	return arrow() != nullptr ? dynamic_cast<Category*>(arrow()->codomain()) : nullptr;
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

Node* MapsElements::imageOf(Category* cod, const QString& functor, const Node* source) const
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
		if (byName == nullptr && node->id() == name
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

void MapsElements::setLive(bool live)
{
	if (m_live == live)
		return;
	m_live = live;
	listen(live);
	if (live)
		sync();
	emit settingsChanged();
}

void MapsElements::setImaginesPosition(bool imagines)
{
	if (m_imaginesPosition == imagines)
		return;
	m_imaginesPosition = imagines;
	if (m_live)
		sync();
	emit settingsChanged();
}

void MapsElements::setReflectsPosition(bool reflects)
{
	if (m_reflectsPosition == reflects)
		return;
	m_reflectsPosition = reflects;
	emit settingsChanged();
	// nothing to sync: this one only says what a DRAG of an image does
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

void MapsElements::setImaginesBends(bool imagines)
{
	if (m_imaginesBends == imagines)
		return;
	m_imaginesBends = imagines;
	if (m_live)
		sync();   // bring the images' shapes into line at once
	emit settingsChanged();
}

void MapsElements::setReflectsBends(bool reflects)
{
	if (m_reflectsBends == reflects)
		return;
	m_reflectsBends = reflects;
	emit settingsChanged();
}

void MapsElements::onSourceBends(Arrow* source)
{
	if (m_syncing || !m_live || !m_imaginesBends || source == nullptr)
		return;
	Arrow* F = arrow();
	Category* cod = codomain();
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
	if (m_syncing || !m_live || !m_reflectsBends || image == nullptr)
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
	if (m_syncing || !m_live || !m_imaginesPosition || source == nullptr || delta.isNull())
		return;
	Arrow* F = arrow();
	Category* cod = codomain();
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
	if (m_syncing || !m_live || !m_reflectsPosition || image == nullptr || delta.isNull())
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
}void MapsElements::onSourceDeleted(Node* source)
{
	if (!m_live || source == nullptr)
		return;
	Arrow* F = arrow();
	Category* cod = codomain();
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

void MapsElements::sync()
{
	Arrow* F = arrow();
	Node* dom = domain();
	Category* cod = codomain();
	if (!m_live || F == nullptr || dom == nullptr || cod == nullptr || dom == cod || m_syncing)
		return;

	// The functor itself may have been taken out of the diagram - deleted, or
	// undone. There is no mapping without it, so its images go too; they come
	// back with it, because the next sync draws them again.
	if (F->scene() == nullptr || F->parentItem() == nullptr)
	{
		removeImages();
		return;
	}

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
		if (source == nullptr || dynamic_cast<Arrow*>(source) != nullptr || source->id().isEmpty())
			continue;
		live << source->key();
		Node* img = imageOf(cod, name, source);
		if (img == nullptr)
		{
			// where it first appears: at the same offset as its source. From
			// then on its position is ITS OWN - it moves when the source
			// moves, by the same amount, but it can be put wherever you like.
			img = cod->createObject(applied(name, source->id()), cod->mapToScene(source->pos()));
			stamp(img, name, source);
			img->setVisible(m_imagesVisible);
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
	}

	// the arrows, between the images of their ends
	for (QGraphicsItem* child : dom->childItems())
	{
		auto* source = dynamic_cast<Arrow*>(child);
		if (source == nullptr || source->id().isEmpty())
			continue;
		Node* from = image.value(source->domain());
		Node* to = image.value(source->codomain());
		if (from == nullptr || to == nullptr)
			continue;   // an end outside C: not ours to map
		// contravariant: the image of f : X -> Y runs F(Y) -> F(X)
		if (m_contravariant)
			std::swap(from, to);
		live << source->key();
		Node* img = imageOf(cod, name, source);
		if (img == nullptr)
		{
			img = cod->createArrow(applied(name, source->id()), from, to);
			stamp(img, name, source);
			img->setVisible(m_imagesVisible);
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
			if (m_imaginesBends && imageArrow->bends() != source->bends())
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
	for (Node* node : staleArrows)
		delete node;
	for (Node* node : staleObjects)
		delete node;

	if (scene != nullptr)
		scene->history()->suspend(false);
	m_syncing = false;
}




void MapsElements::arrowContextMenu(QMenu& menu, Arrow* arrow)
{
	Node* dom = arrow->domain();
	auto* cod = dynamic_cast<Category*>(arrow->codomain());
	if (dom == nullptr || cod == nullptr)
	{
		// Shown greyed rather than left out: a missing entry looks like a bug,
		// and the reason is worth knowing.
		QAction* why = menu.addAction("Map the elements");
		why->setEnabled(false);
		why->setToolTip(QString("Nothing to map into: %1 cannot hold anything drawn in it, so there is "
		                        "nowhere for the image to go.")
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

void MapsElements::setImagesVisible(bool visible)
{
	if (m_imagesVisible == visible)
		return;
	m_imagesVisible = visible;

	Category* cod = codomain();
	if (cod == nullptr)
		return;
	for (QGraphicsItem* child : cod->childItems())
	{
		auto* node = dynamic_cast<Node*>(child);
		if (node != nullptr && isOurImage(node))
			node->setVisible(visible);   // and everything drawn inside it
	}
	// the codomain's frame is the union of what it HOLDS AND SHOWS
	cod->refreshFrame();
	emit settingsChanged();
}

int MapsElements::mapDiagram()
{
	Arrow* F = arrow();
	Node* dom = domain();
	Category* cod = codomain();
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
			already = cod->createObject(applied(name, object->id()), cod->mapToScene(object->pos()));
			stamp(already, name, object);
			already->setVisible(m_imagesVisible);
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
		Node* drawnArrow = cod->createArrow(applied(name, a->id()), from, to);
		stamp(drawnArrow, name, a);
		drawnArrow->setVisible(m_imagesVisible);
		made << drawnArrow;
		++drawn;
	}

	if (auto* scene = dynamic_cast<DiagramScene*>(cod->scene()))
		scene->recordCreation(QString("Mapped %1 into %2 by %3").arg(dom->id(), cod->id(), name), made);
	return drawn;
}
