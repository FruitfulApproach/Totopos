#pragma once

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include "Object.h"
#include "Category.h"

// The diagram's scene. A QGraphicsScene is a QObject, not a widget: it has no
// designer form; its window is whichever view shows it. Everything drawn lives
// inside the AMBIENT category, an item sitting at the centre of the scene.
class DiagramScene : public QGraphicsScene
{
	Q_OBJECT

public:
	explicit DiagramScene(QObject* parent = nullptr);
	~DiagramScene() override;

	Category* ambientCategory() const { return m_ambientCategory; }

public slots:
	// switch the ambient category by name (BigCat, Ab, R-Mod, ...); the
	// objects already placed move over to the new one
	void setAmbientCategory(const QString& name);

signals:
	void ambientCategoryChanged(Category* category);

protected:
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
	// true when a double-click at this item should create an object: on the
	// background, or on the ambient category itself (its frame or label)
	bool isCanvas(QGraphicsItem* item) const;

	Category* m_ambientCategory = nullptr;
};
