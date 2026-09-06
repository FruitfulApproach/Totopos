#pragma once

#include "Object.h"

// A category drawn as an object; the objects placed "in" it are its child items.
class Category  : public Object
{
	Q_OBJECT

public:
	Category(const QString& name, QGraphicsItem* parent = nullptr);
	~Category();

	// Put a fresh generic object at a scene position, as a child of this
	// category. Names run C, D, ..., Z, then A', B', ..., Z', then A'', ...
	Object* createCanvasObject(const QPointF& scenePos);

	// Take an existing node in as a child, keeping it at the given scene
	// position (where the user sees it).
	void adopt(Node* node, const QPointF& scenePos);

	// the name the next createCanvasObject will use
	QString nextObjectName() const { return letterName(m_nextIndex); }
	static QString letterName(int index);

	// a category frames its objects with more room than a plain object gives its label
	QRectF boundingRect() const override { return childrenBoundingRect().adjusted(-18, -18, 18, 18); }

private:
	int m_nextIndex = 0;
};
