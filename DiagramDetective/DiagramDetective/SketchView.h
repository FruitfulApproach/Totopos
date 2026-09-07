#pragma once

#include <QGraphicsView>
#include <QStringList>
#include <QPointF>

class QToolButton;
class QFrame;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QPropertyAnimation;
class ToggleSwitch;

// The view of a sketch. Over the canvas, top-right, floats a toggle button; it
// opens a small panel of controls that stays until toggled again. Only the
// controls take the mouse — everything around them is the canvas.
class SketchView : public QGraphicsView
{
	Q_OBJECT
	// animatable, so fitTo can slide the view rather than jump it
	Q_PROPERTY(QPointF sceneCentre READ sceneCentre WRITE setSceneCentre)

public:
	explicit SketchView(QWidget* parent = nullptr);
	~SketchView() override;

	// the ambient category chosen in the panel ("BigCat" by default)
	QString category() const;
	void setCategory(const QString& name);
	static QStringList builtInCategories();

	bool isMenuOpen() const { return m_open; }

	// the current zoom factor (1 = 100%)
	QPointF sceneCentre() const;
	void setSceneCentre(const QPointF& centre);

	qreal zoom() const { return m_zoom; }
	void setZoom(qreal factor);
	void resetZoom() { setZoom(1.0); }

public slots:
	void toggleMenu();
	void setMenuOpen(bool open);
	// Put this part of the scene in the middle of the view, filling it with a
	// little room to spare. Animated, because a jump leaves the eye behind.
	void fitTo(const QRectF& sceneRect);

	// the sentence the diagram makes, along the bottom of the view
	void setStatement(const QString& statement);
	// bring what is drawn back to the middle of the view, and fit it in
	void centreOnContents();
	void fitContents();
	// the chase is running: the button says so and stops asking
	void setChasing(bool chasing);
	// follow the diagram's own claim (a file may have set it)
	void setCommutes(bool commutes);
	void setStatementKind(int kind, const QString& name);

signals:
	void categoryChanged(const QString& name);
	// a category defined through Custom...: its name and the structure ticked
	void categoryDefined(const QString& name, const QStringList& properties);
	void commutesChanged(bool commutes);
	void chaseRequested();
	// what the picture is put forward as, and what it is called
	void statementKindPicked(int kind);
	void statementNamed(const QString& name);

protected:
	void resizeEvent(QResizeEvent* event) override;
	// the wheel zooms about the cursor (Ctrl not needed); scrolling is by drag/scrollbars
	void wheelEvent(QWheelEvent* event) override;

private:
	// the two states of the commuting claim, named and explained
	void refreshCommutesLabel(bool commutes);
	// what is drawn, in scene coordinates (not the overlays)
	QRectF contentsRect() const;
	void buildOverlay();
	void placeOverlay();
	void onCategoryPicked(int index);
	void defineCustomCategory();

	QToolButton* m_toggle = nullptr;
	QFrame* m_panel = nullptr;
	QComboBox* m_category = nullptr;
	QCheckBox* m_tutor = nullptr;
	ToggleSwitch* m_commutes = nullptr;
	QLabel* m_commutesLabel = nullptr;
	QPushButton* m_chase = nullptr;
	QLabel* m_mode = nullptr;
	QLabel* m_statement = nullptr;
	QComboBox* m_kind = nullptr;
	QLineEdit* m_statementName = nullptr;
	QPropertyAnimation* m_anim = nullptr;
	bool m_open = false;
	int m_lastCategoryIndex = 0;
	qreal m_zoom = 1.0;
};
