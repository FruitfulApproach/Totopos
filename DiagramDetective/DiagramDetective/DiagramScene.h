#pragma once

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QPointer>
#include "TutorSession.h"   // QPointer needs the complete type
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

	// one guided interaction at a time: a new one cancels the running one
	void beginSession(TutorSession* session);
	TutorSession* session() const { return m_session; }

public slots:
	// switch the ambient category by name (BigCat, Ab, R-Mod, ...); the
	// objects already placed move over to the new one
	void setAmbientCategory(const QString& name);

signals:
	void ambientCategoryChanged(Category* category);

protected:
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
	// a right-click on the background is a right-click on the ambient category
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
	// the category a double-click at this item places into: the category
	// hit (its frame or label), the ambient one for the background, none
	// when some other node was hit
	Category* categoryAt(QGraphicsItem* item) const;

	Category* m_ambientCategory = nullptr;
	QPointer<TutorSession> m_session;
};
