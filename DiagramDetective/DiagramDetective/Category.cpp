#include "Category.h"
#include "props/CategoryProps.h"
#include <QMenu>

Category::Category(const QString& name, QGraphicsItem* parent)
	: Object(name, parent)
{
	// a category is a translucent yellow region with a dodger-blue frame
	setFill(QBrush(QColor(255, 235, 0, 55)));
	setBorder(QPen(QColor(30, 144, 255, 170), 2));
}

Category::~Category()
{
	// the props are QObject children: Qt deletes them with us
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

void Category::populateContextMenu(QMenu& menu)
{
	Node::populateContextMenu(menu);
	// each property knows what it lets the user do here
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

QString Category::letterName(int index, QChar first)
{
	// index 0 -> first; after Z the alphabet wraps round with one more prime each time
	const int start = first.toUpper().unicode() - 'A';
	const int k = start + index;
	QString name(QChar('A' + (k % 26)));
	const int primes = k / 26;
	for (int i = 0; i < primes; ++i)
		name += QChar(0x2032);   // prime
	return name;
}

Object* Category::makeObject(const QString& name)
{
	return new Object(name, this);   // a child: joins the scene with us
}

void Category::adopt(Node* node, const QPointF& scenePos)
{
	if (node == nullptr)
		return;
	prepareGeometryChange();   // our frame is the union of our children
	node->setParentItem(this);
	node->setPos(mapFromScene(scenePos));   // a child's pos() is in OUR coordinates
}

Object* Category::createObject(const QString& name, const QPointF& scenePos)
{
	prepareGeometryChange();
	Object* object = makeObject(name);
	object->setParentItem(this);
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);
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
	Object* object = makeObject(letterName(m_nextIndex++, firstLetter()));
	object->setParentItem(this);   // makeObject normally did this already; a custom one might not
	object->setPos(mapFromScene(scenePos));
	object->setZValue(1);   // above the category's fill
	return object;
}
