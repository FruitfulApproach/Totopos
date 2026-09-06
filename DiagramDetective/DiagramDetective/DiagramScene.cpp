#include "DiagramScene.h"
#include "categories/BuiltInCategories.h"
#include "TutorSession.h"

DiagramScene::DiagramScene(QObject* parent)
	: QGraphicsScene(parent)
{
	// a FIXED scene rect: without one the rect follows the items, and the view
	// re-centres the scene whenever it changes; the first node placed would
	// jump away from the cursor
	setSceneRect(-4000, -4000, 8000, 8000);
	setAmbientCategory("BigCat");
}

DiagramScene::~DiagramScene()
{
}

void DiagramScene::setAmbientCategory(const QString& name)
{
	if (m_ambientCategory != nullptr && m_ambientCategory->id() == name)
		return;

	// a built-in is its own subclass (it knows what it is made of); anything
	// else is a plain category the user defined
	Category* fresh = Category::createBuiltIn(name);
	if (fresh == nullptr)
		fresh = new Category(name);
	fresh->setPos(0, 0);
	// the ambient category IS the canvas: it neither moves nor gets selected,
	// so a press on it starts a rubber band, not a drag of everything
	fresh->setFlag(QGraphicsItem::ItemIsMovable, false);
	fresh->setFlag(QGraphicsItem::ItemIsSelectable, false);
	fresh->setFlag(QGraphicsItem::ItemIsFocusable, false);
	fresh->setZValue(-1);
	addItem(fresh);

	if (m_ambientCategory != nullptr)
	{
		// the objects drawn so far carry over, at the same scene positions
		for (QGraphicsItem* child : m_ambientCategory->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // the category's own label
			fresh->adopt(node, child->scenePos());
		}
		removeItem(m_ambientCategory);
		delete m_ambientCategory;
	}

	m_ambientCategory = fresh;
	emit ambientCategoryChanged(fresh);
}

Category* DiagramScene::categoryAt(QGraphicsItem* item) const
{
	if (item == nullptr)
		return m_ambientCategory;
	if (auto* category = dynamic_cast<Category*>(item))
		return category;
	// a label is a plain text item parented to its node
	if (dynamic_cast<Node*>(item) == nullptr)
		return dynamic_cast<Category*>(item->parentItem());
	return nullptr;   // some other node: its own business
}

void DiagramScene::beginSession(TutorSession* session)
{
	if (!m_session.isNull() && m_session != session)
		m_session->cancel();
	m_session = session;
}

void DiagramScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
	QGraphicsScene::contextMenuEvent(event);   // an item under the cursor takes it
	if (event->isAccepted())
		return;
	if (m_ambientCategory != nullptr)
	{
		m_ambientCategory->popupContextMenu(event->screenPos());
		event->accept();
	}
}

void DiagramScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
	// let items under the cursor take the double-click first (an object's
	// label being edited, say); a double-click on the canvas creates an object
	QGraphicsScene::mouseDoubleClickEvent(event);

	Category* category = categoryAt(itemAt(event->scenePos(), QTransform()));
	if (category == nullptr)
		return;

	// the category decides what it is made of: a category in BigCat, a set in Set, ...
	category->createCanvasObject(event->scenePos());
	event->accept();
}
