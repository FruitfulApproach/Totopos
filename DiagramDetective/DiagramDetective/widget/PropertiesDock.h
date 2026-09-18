#pragma once

#include <QDockWidget>
#include <QList>

class DiagramScene;
class Node;
class Arrow;
class Category;
class MapsElements;
class ToggleSwitch;
class QLabel;
class QLineEdit;
class QGroupBox;
class QSpinBox;
class QPushButton;
class QListWidget;
class SketchView;

// The properties of whatever is selected in the diagram, a page per kind of
// thing: what any node has, what an object has, and - when the selection is a
// single arrow that carries elements over - how that mapping behaves.
//
// This is the SELECTION's properties. Tools > Settings is the other thing:
// what the program does in general.
class PropertiesDock : public QDockWidget
{
	Q_OBJECT

public:
	explicit PropertiesDock(QWidget* parent = nullptr);

	void setScene(DiagramScene* scene);
	// the view a component is shown in when its row is double-clicked
	void setView(SketchView* view) { m_view = view; }

public slots:
	// read the selection and show what can be set for it
	void refresh();

private:
	void build();
	QList<Node*> selection() const;
	// the mapping of the one arrow selected on its own, if that is what this is
	MapsElements* soleMapping() const;
	// the one node selected on its own, when it has a diagram drawn inside it
	Category* soleDiagramHome() const;

	void applyExistsSuch(bool on);
	void applyDeleteMark(bool on);
	void applyRounding(int radius);
	void applyId(const QString& id);
	// the pieces of the diagram, each with its own claim to commute
	void refreshComponents();
	void lightUp(const QList<Node*>& objects, const QList<Arrow*>& arrows, bool on);

	DiagramScene* m_scene = nullptr;
	SketchView* m_view = nullptr;
	bool m_updating = false;   // filling the widgets in must not look like edits

	QLabel* m_header = nullptr;
	QLabel* m_hint = nullptr;

	QGroupBox* m_nodeBox = nullptr;
	QLineEdit* m_id = nullptr;
	ToggleSwitch* m_exists = nullptr;
	ToggleSwitch* m_deleteMark = nullptr;

	QGroupBox* m_objectBox = nullptr;
	QSpinBox* m_radius = nullptr;

	// the diagram drawn INSIDE the selected node, when there is one
	QGroupBox* m_insideBox = nullptr;
	ToggleSwitch* m_rowsExact = nullptr;
	ToggleSwitch* m_columnsExact = nullptr;

	QGroupBox* m_componentBox = nullptr;
	QListWidget* m_components = nullptr;

	QGroupBox* m_mappingBox = nullptr;
	QLabel* m_mappingHint = nullptr;
	ToggleSwitch* m_showImage = nullptr;
	ToggleSwitch* m_live = nullptr;
	ToggleSwitch* m_imagine = nullptr;
	ToggleSwitch* m_reflect = nullptr;
	ToggleSwitch* m_contravariant = nullptr;
	ToggleSwitch* m_imagineBends = nullptr;
	ToggleSwitch* m_reflectBends = nullptr;
	QPushButton* m_mapNow = nullptr;
};
