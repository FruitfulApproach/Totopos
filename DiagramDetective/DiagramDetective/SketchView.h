#pragma once

#include <QGraphicsView>
#include <QStringList>

class QToolButton;
class QFrame;
class QComboBox;
class QCheckBox;
class QPropertyAnimation;

// The view of a sketch. Over the canvas, top-right, floats a toggle button; it
// opens a small panel of controls that stays until toggled again. Only the
// controls take the mouse — everything around them is the canvas.
class SketchView : public QGraphicsView
{
	Q_OBJECT

public:
	explicit SketchView(QWidget* parent = nullptr);
	~SketchView() override;

	// the ambient category chosen in the panel ("BigCat" by default)
	QString category() const;
	void setCategory(const QString& name);
	static QStringList builtInCategories();

	bool isMenuOpen() const { return m_open; }

	// the current zoom factor (1 = 100%)
	qreal zoom() const { return m_zoom; }
	void setZoom(qreal factor);
	void resetZoom() { setZoom(1.0); }

public slots:
	void toggleMenu();
	void setMenuOpen(bool open);

signals:
	void categoryChanged(const QString& name);
	// a category defined through Custom...: its name and the structure ticked
	void categoryDefined(const QString& name, const QStringList& properties);

protected:
	void resizeEvent(QResizeEvent* event) override;
	// the wheel zooms about the cursor (Ctrl not needed); scrolling is by drag/scrollbars
	void wheelEvent(QWheelEvent* event) override;

private:
	void buildOverlay();
	void placeOverlay();
	void onCategoryPicked(int index);
	void defineCustomCategory();

	QToolButton* m_toggle = nullptr;
	QFrame* m_panel = nullptr;
	QComboBox* m_category = nullptr;
	QCheckBox* m_tutor = nullptr;
	QPropertyAnimation* m_anim = nullptr;
	bool m_open = false;
	int m_lastCategoryIndex = 0;
	qreal m_zoom = 1.0;
};
