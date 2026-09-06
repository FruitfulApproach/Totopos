#include "Category.h"

Category::Category(const QString& name, QGraphicsItem* parent)
	: Object(name, parent)
{
	// a category is a translucent yellow region with a dodger-blue frame
	setFill(QBrush(QColor(255, 235, 0, 55)));
	setBorder(QPen(QColor(30, 144, 255, 170), 2));
}

Category::~Category()
{
}

QString Category::letterName(int index)
{
	// index 0 -> C; after Z the alphabet repeats with one more prime each time
	const int k = index + 2;
	QString name(QChar('A' + (k % 26)));
	const int primes = k / 26;
	for (int i = 0; i < primes; ++i)
		name += QChar(0x2032);   // prime
	return name;
}

void Category::adopt(Node* node, const QPointF& scenePos)
{
	if (node == nullptr)
		return;
	prepareGeometryChange();   // our frame is the union of our children
	node->setParentItem(this);
	node->setPos(mapFromScene(scenePos));   // a child's pos() is in OUR coordinates
}

Object* Category::createCanvasObject(const QPointF& scenePos)
{
	// our frame is the union of our children: tell the scene it is about to grow
	prepareGeometryChange();

	// A child's pos() is in the PARENT's coordinates. mapFromScene turns the
	// click, given in scene coordinates, into ours, so the object lands exactly
	// under the cursor wherever this category happens to sit, and stays there
	// when the category later moves (it moves with it).
	auto* object = new Object(letterName(m_nextIndex++), this);   // a child: joins the scene with us
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);   // above the category's fill
	return object;
}
