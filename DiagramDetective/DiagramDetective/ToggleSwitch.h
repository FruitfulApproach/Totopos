#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

// A switch the way a phone draws one: a track that turns green and a knob
// that slides across it. Checkable like any button; `toggled` is the signal.
class ToggleSwitch : public QAbstractButton
{
	Q_OBJECT
	Q_PROPERTY(qreal position READ position WRITE setPosition)

public:
	explicit ToggleSwitch(QWidget* parent = nullptr);

	QSize sizeHint() const override;
	// a narrower one, for a row that carries several
	void setCompact(bool compact);
	qreal position() const { return m_position; }
	void setPosition(qreal position);

protected:
	void paintEvent(QPaintEvent* event) override;
	void checkStateSet() override;    // follows setChecked(), animating
	void nextCheckState() override;
	// The click is handled here rather than left to QAbstractButton, whose
	// path runs through hitButton/nextCheckState and is easy to lose under a
	// stylesheet or an odd size policy. A switch has one job.
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	void animateTo(bool on);

	qreal m_position = 0.0;   // 0 off, 1 on
	bool m_compact = false;
	QPropertyAnimation* m_animation = nullptr;
};
