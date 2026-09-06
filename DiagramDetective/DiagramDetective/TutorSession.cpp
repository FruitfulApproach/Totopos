#include "TutorSession.h"
#include "Tutor.h"
#include "DiagramScene.h"
#include "Node.h"

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QtMath>

namespace
{
	const int kOverlayZ = 100000;
}

// The bouncing arrow at the target: constant screen size at any zoom.
class TutorPointer : public QGraphicsItem
{
public:
	TutorPointer()
	{
		setFlag(ItemIgnoresTransformations, true);
		setAcceptedMouseButtons(Qt::NoButton);
		setZValue(kOverlayZ);
	}
	QRectF boundingRect() const override { return QRectF(-14, -40, 28, 40); }
	void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
	{
		p->setRenderHint(QPainter::Antialiasing, true);
		const QColor c(220, 38, 38);
		p->setPen(QPen(c, 3, Qt::SolidLine, Qt::RoundCap));
		p->drawLine(QPointF(0, -38), QPointF(0, -14));
		QPolygonF head;
		head << QPointF(0, 0) << QPointF(-10, -14) << QPointF(10, -14);
		p->setPen(Qt::NoPen);
		p->setBrush(c);
		p->drawPolygon(head);
	}
};

// A numbered badge at a pick.
class TutorBadge : public QGraphicsItem
{
public:
	TutorBadge(Node* node, int number) : m_node(node), m_number(number)
	{
		setFlag(ItemIgnoresTransformations, true);
		setAcceptedMouseButtons(Qt::NoButton);
		setZValue(kOverlayZ);
	}
	Node* node() const { return m_node; }
	QRectF boundingRect() const override { return QRectF(-11, -11, 22, 22); }
	void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
	{
		p->setRenderHint(QPainter::Antialiasing, true);
		p->setPen(QPen(Qt::white, 1.5));
		p->setBrush(QColor(99, 102, 241));
		p->drawEllipse(QRectF(-10, -10, 20, 20));
		QFont f = p->font();
		f.setBold(true);
		f.setPointSize(9);
		p->setFont(f);
		p->drawText(QRectF(-10, -10, 20, 20), Qt::AlignCenter, QString::number(m_number));
	}
private:
	Node* m_node;
	int m_number;
};

TutorSession::TutorSession(Tutor* tutor, DiagramScene* scene)
	: QObject(scene)
	, m_tutor(tutor)
	, m_scene(scene)
{
	m_timer.setInterval(33);
	connect(&m_timer, &QTimer::timeout, this, &TutorSession::tick);
}

TutorSession::~TutorSession()
{
	finish();
}

void TutorSession::start()
{
	// the bubble lives over the view that shows the scene
	QGraphicsView* view = m_scene->views().isEmpty() ? nullptr : m_scene->views().first();
	if (view != nullptr)
	{
		m_bubble = new QFrame(view);
		m_bubble->setObjectName("tutorBubble");
		m_bubble->setAttribute(Qt::WA_StyledBackground, true);
		m_bubble->setStyleSheet(
			"QFrame#tutorBubble { background: rgba(30, 32, 44, 225); border: 1px solid rgba(255,255,255,70); border-radius: 12px; }"
			"QLabel { color: white; background: transparent; font-size: 13px; }"
			"QPushButton { color: white; background: rgba(99, 102, 241, 230); border: none; border-radius: 8px; padding: 4px 12px; }"
			"QPushButton#cancel { background: rgba(255,255,255,40); }");
		auto* layout = new QVBoxLayout(m_bubble);
		layout->setContentsMargins(14, 10, 14, 10);
		auto* title = new QLabel(m_tutor->tutorTitle(), m_bubble);
		QFont bold = title->font();
		bold.setBold(true);
		title->setFont(bold);
		layout->addWidget(title);
		m_text = new QLabel(m_bubble);
		m_text->setWordWrap(true);
		m_text->setMaximumWidth(460);
		m_text->setVisible(Tutor::isEnabled());   // tutor off: the strip is just title + buttons
		layout->addWidget(m_text);
		auto* buttons = new QHBoxLayout();
		buttons->addStretch();
		auto* cancelBtn = new QPushButton("Cancel", m_bubble);
		cancelBtn->setObjectName("cancel");
		auto* doneBtn = new QPushButton("Done", m_bubble);
		buttons->addWidget(cancelBtn);
		buttons->addWidget(doneBtn);
		layout->addLayout(buttons);
		connect(cancelBtn, &QPushButton::clicked, this, &TutorSession::cancel);
		connect(doneBtn, &QPushButton::clicked, this, &TutorSession::done);
		m_bubble->show();
		m_bubble->raise();
	}

	m_pointer = new TutorPointer();
	m_scene->addItem(m_pointer);
	m_pointer->hide();

	m_scene->installEventFilter(this);   // clicks become picks, Enter / Esc Done / Cancel
	m_timer.start();
	m_tutor->onBegin(*this);
	placeBubble();
}

void TutorSession::say(const QString& remark, QGraphicsItem* pointAt)
{
	const bool coaching = Tutor::isEnabled();
	if (m_text != nullptr)
	{
		m_text->setText(remark);
		m_text->setVisible(coaching);
	}
	m_target = coaching ? pointAt : nullptr;
	if (m_pointer != nullptr)
		m_pointer->setVisible(m_target != nullptr);
	placeBubble();
}

void TutorSession::placeBubble()
{
	if (m_bubble.isNull())
		return;
	m_bubble->adjustSize();
	QWidget* view = m_bubble->parentWidget();
	if (view != nullptr)
		m_bubble->move((view->width() - m_bubble->width()) / 2, 10);
}

void TutorSession::tick()
{
	// the arrow bounces above its target and follows it; badges follow their nodes
	m_phase += 0.25;
	if (m_pointer != nullptr && m_target != nullptr)
	{
		const QRectF r = m_target->sceneBoundingRect();
		m_pointer->setPos(r.center().x(), r.top() - 6 - 6 * qAbs(qSin(m_phase)));
	}
	for (QGraphicsItem* b : m_badges)
	{
		auto* badge = static_cast<TutorBadge*>(b);
		const QRectF r = badge->node()->sceneBoundingRect();
		badge->setPos(r.right(), r.top());
	}
}

void TutorSession::addBadge(Node* node)
{
	auto* badge = new TutorBadge(node, m_picks.size());
	m_scene->addItem(badge);
	m_badges.append(badge);
	tick();
}

bool TutorSession::eventFilter(QObject* watched, QEvent* event)
{
	if (watched != m_scene || m_finished)
		return QObject::eventFilter(watched, event);

	switch (event->type())
	{
	case QEvent::GraphicsSceneMousePress:
	{
		auto* me = static_cast<QGraphicsSceneMouseEvent*>(event);
		if (me->button() != Qt::LeftButton)
			return true;
		// the node under the cursor: a label hit counts for its node
		QGraphicsItem* item = m_scene->itemAt(me->scenePos(), QTransform());
		while (item != nullptr && dynamic_cast<Node*>(item) == nullptr)
			item = item->parentItem();
		if (auto* node = dynamic_cast<Node*>(item))
		{
			if (m_tutor->onPick(*this, node))
			{
				m_picks.append(node);
				addBadge(node);
			}
		}
		return true;   // the scene never sees the press: nothing gets dragged or selected
	}
	case QEvent::GraphicsSceneMouseRelease:
	case QEvent::GraphicsSceneMouseDoubleClick:
	case QEvent::GraphicsSceneContextMenu:
		return true;
	case QEvent::KeyPress:
	{
		auto* ke = static_cast<QKeyEvent*>(event);
		if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) { done(); return true; }
		if (ke->key() == Qt::Key_Escape) { cancel(); return true; }
		break;
	}
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

void TutorSession::done()
{
	if (m_finished)
		return;
	if (m_tutor->onDone(*this))
		finish();
}

void TutorSession::cancel()
{
	if (m_finished)
		return;
	m_tutor->onCancel(*this);
	finish();
}

void TutorSession::finish()
{
	if (m_finished)
		return;
	m_finished = true;
	m_timer.stop();
	if (m_scene != nullptr)
	{
		m_scene->removeEventFilter(this);
		if (m_pointer != nullptr) { m_scene->removeItem(m_pointer); delete m_pointer; m_pointer = nullptr; }
		for (QGraphicsItem* b : m_badges) { m_scene->removeItem(b); delete b; }
		m_badges.clear();
	}
	if (!m_bubble.isNull())
		m_bubble->deleteLater();
	emit ended(this);
	deleteLater();
}
