#pragma once

#include <QGraphicsView>
#include <QStringList>
#include <QPointF>
#include <QByteArray>

class QToolButton;
class QFrame;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QPropertyAnimation;
class ToggleSwitch;
class CanvasActionBar;

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
	// Everything drawn is an object or an arrow OF the ambient category, so
	// once there is anything at all the choice is fixed: changing it would
	// leave every one of them meaning something else.
	void setCategoryLocked(bool locked);
	static QStringList builtInCategories();

	bool isMenuOpen() const { return m_open; }

	// the pills along the foot of the canvas; the window fills them in
	CanvasActionBar* actionBar() const { return m_actions; }

	// the current zoom factor (1 = 100%)
	QPointF sceneCentre() const;
	void setSceneCentre(const QPointF& centre);

	qreal zoom() const { return m_zoom; }
	void setZoom(qreal factor);
	void resetZoom() { setChosenZoom(1.0); }

	// THE ZOOM THE USER ASKED FOR, which is not always the zoom in force.
	//
	// A pane that has been made small cannot show the diagram at the size it
	// was being looked at, so the view backs off until the whole of it is in
	// view again (see keepInView). What was ASKED for is kept here, and
	// restored as soon as there is room for it - otherwise dragging a
	// splitter narrow and wide again would leave the diagram tiny, having
	// silently taken the shrink for a decision.
	qreal chosenZoom() const { return m_chosenZoom; }
	void setChosenZoom(qreal factor);
	// a step in or out, the size the menu and the shortcuts use
	void zoomBy(qreal times) { setChosenZoom(m_chosenZoom * times); }
	// Fit the zoom to the room there is: never larger than what was asked
	// for, never smaller than kLeastAutoZoom. Called when the view is
	// resized, which is what a split window does constantly.
	void keepInView();

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
	// a piece is being held over this view: the category it would land in, or
	// empty when it has gone away again
	void dropTargetChanged(const QString& category);
	// a pill along the foot of the canvas was pressed; the id is whatever the
	// window put there (see CanvasActionBar)
	void canvasActionTriggered(const QString& id);

public slots:
	// The scene has a piece of itself to carry off (Ctrl and drag). A drag has
	// to start from a widget, which a scene is not, so this is where it starts.
	// Let go over another tab and the piece lands there.
	void carryFragment(const QByteArray& payload);

protected:
	// THE RIGHT BUTTON DRAGS.
	//
	// On a node it carries the node, exactly as the left button does. On the
	// canvas it pans the view - which is what a right drag over empty space
	// means nearly everywhere - rather than sweeping out a rubber band, which
	// the left button is still for.
	//
	// Either way, a drag is not a menu: a right press that MOVED swallows the
	// context menu that would otherwise arrive when the button comes up.
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;

	void resizeEvent(QResizeEvent* event) override;
	// the wheel zooms about the cursor (Ctrl not needed); scrolling is by drag/scrollbars
	void wheelEvent(QWheelEvent* event) override;

	// a piece of a diagram carried in from somewhere - this view, another tab,
	// another copy of the program - lands where it is let go of
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dragLeaveEvent(QDragLeaveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private:
	// the right button is down and moving the view, not a node
	bool m_panning = false;
	// where it was last seen, in the viewport - a pan is worked out from the
	// step, not from where it began, so it cannot drift
	QPoint m_panFrom;
	// it moved far enough to be a drag, so no menu when it comes up
	bool m_rightDragged = false;

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
	// the row of pills along the foot of the canvas, over the statement bar:
	// what the diagram is offering right now (see CanvasActionBar)
	CanvasActionBar* m_actions = nullptr;
	QComboBox* m_kind = nullptr;
	QLineEdit* m_statementName = nullptr;
	QPropertyAnimation* m_anim = nullptr;
	bool m_open = false;
	int m_lastCategoryIndex = 0;
	qreal m_zoom = 1.0;
	qreal m_chosenZoom = 1.0;   // what was asked for; m_zoom may be smaller to fit
	bool m_keepingInView = false;   // inside keepInView: a scale of its own must not start another
};
