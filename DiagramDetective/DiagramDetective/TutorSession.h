#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QPointer>

class Tutor;
class DiagramScene;
class Node;
class QGraphicsItem;
class QFrame;
class QLabel;
class TutorPointer;

// One run of a Tutor: the remark bubble over the view (with Done / Cancel),
// the bouncing arrow at the target in the scene, numbered badges on the
// picks, and the event filter that turns clicks into picks and Enter / Esc
// into Done / Cancel. Owned by the scene; destroys itself when finished.
class TutorSession : public QObject
{
	Q_OBJECT

public:
	TutorSession(Tutor* tutor, DiagramScene* scene);
	~TutorSession() override;

	DiagramScene* scene() const { return m_scene; }
	Tutor* tutor() const { return m_tutor; }
	const QList<Node*>& picks() const { return m_picks; }

	// the remark to show, and what the arrow points at (nullptr: no arrow)
	void say(const QString& remark, QGraphicsItem* pointAt = nullptr);

	void start();

public slots:
	void done();
	void cancel();

signals:
	void ended(TutorSession* session);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	void finish();
	void addBadge(Node* node);
	void placeBubble();
	void tick();

	Tutor* m_tutor = nullptr;
	DiagramScene* m_scene = nullptr;
	QList<Node*> m_picks;
	QGraphicsItem* m_target = nullptr;
	TutorPointer* m_pointer = nullptr;
	QList<QGraphicsItem*> m_badges;
	QPointer<QFrame> m_bubble;
	QLabel* m_text = nullptr;
	QTimer m_timer;
	qreal m_phase = 0;
	bool m_finished = false;
};
