#include "DiagramScene.h"

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

	auto* fresh = new Category(name);
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

bool DiagramScene::isCanvas(QGraphicsItem* item) const
{
	if (item == nullptr)
		return true;
	if (item == m_ambientCategory)
		return true;
	// the category's label is a plain text item parented to it
	return item->parentItem() == m_ambientCategory && dynamic_cast<Node*>(item) == nullptr;
}

void DiagramScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
	// let items under the cursor take the double-click first (an object's
	// label being edited, say); a double-click on the canvas creates an object
	QGraphicsScene::mouseDoubleClickEvent(event);

	if (!isCanvas(itemAt(event->scenePos(), QTransform())))
		return;

	if (m_ambientCategory == nullptr)
		setAmbientCategory("BigCat");
	m_ambientCategory->createCanvasObject(event->scenePos());
	event->accept();
}
