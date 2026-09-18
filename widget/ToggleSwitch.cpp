#include "widget/ToggleSwitch.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QCursor>
#include <QMouseEvent>

ToggleSwitch::ToggleSwitch(QWidget* parent)
	: QAbstractButton(parent)
{
	setCheckable(true);
	setCursor(QCursor(Qt::PointingHandCursor));
	setFocusPolicy(Qt::TabFocus);
	// a fixed size, so nothing in a layout can stretch it into something that
	// no longer looks or behaves like a switch
	setFixedSize(46, 26);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_animation = new QPropertyAnimation(this, "position", this);
	m_animation->setDuration(140);
	m_animation->setEasingCurve(QEasingCurve::InOutCubic);
}

QSize ToggleSwitch::sizeHint() const
{
	return m_compact ? QSize(34, 18) : QSize(46, 26);
}

void ToggleSwitch::setCompact(bool compact)
{
	if (m_compact == compact)
		return;
	m_compact = compact;
	setFixedSize(sizeHint());
	updateGeometry();
	update();
}

void ToggleSwitch::setPosition(qreal position)
{
	m_position = position;
	update();
}

void ToggleSwitch::animateTo(bool on)
{
	m_animation->stop();
	m_animation->setStartValue(m_position);
	m_animation->setEndValue(on ? 1.0 : 0.0);
	m_animation->start();
}

void ToggleSwitch::checkStateSet()
{
	animateTo(isChecked());
}

void ToggleSwitch::nextCheckState()
{
	setChecked(!isChecked());
}

void ToggleSwitch::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		event->accept();   // taken here: the release below does the toggling
		return;
	}
	QAbstractButton::mousePressEvent(event);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (rect().contains(event->position().toPoint()))
			setChecked(!isChecked());   // emits toggled
		event->accept();
		return;
	}
	QAbstractButton::mouseReleaseEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	const QRectF r(0, 0, width(), height());
	const qreal radius = r.height() / 2.0;

	// While the animation runs it says how far across the knob is; the rest of
	// the time the CHECKED STATE does, so the switch always shows what it is
	// even if the animation never ran.
	const qreal travel = m_animation != nullptr && m_animation->state() == QAbstractAnimation::Running
		? m_position : (isChecked() ? 1.0 : 0.0);

	const QColor off(120, 120, 128, 110);
	const QColor on(52, 199, 89);   // the green a phone uses
	const QColor track(
		int(off.red()   + (on.red()   - off.red())   * travel),
		int(off.green() + (on.green() - off.green()) * travel),
		int(off.blue()  + (on.blue()  - off.blue())  * travel),
		int(off.alpha() + (on.alpha() - off.alpha()) * travel));

	painter.setPen(Qt::NoPen);
	painter.setBrush(track);
	painter.drawRoundedRect(r, radius, radius);

	const qreal margin = 2.0;
	const qreal knob = r.height() - 2 * margin;
	const qreal x = margin + travel * (r.width() - knob - 2 * margin);
	painter.setBrush(Qt::white);
	painter.drawEllipse(QRectF(x, margin, knob, knob));

	if (hasFocus())
	{
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(QColor(99, 102, 241), 1.5));
		painter.drawRoundedRect(r.adjusted(0.75, 0.75, -0.75, -0.75), radius, radius);
	}
}
