#pragma once

#include <QGraphicsView>
#include <QStringList>

class QToolButton;
class QFrame;
class QComboBox;
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

public slots:
	void toggleMenu();
	void setMenuOpen(bool open);

signals:
	void categoryChanged(const QString& name);

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	void buildOverlay();
	void placeOverlay();

	QToolButton* m_toggle = nullptr;
	QFrame* m_panel = nullptr;
	QComboBox* m_category = nullptr;
	QPropertyAnimation* m_anim = nullptr;
	bool m_open = false;
};
