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
class QComboBox;
class QCheckBox;
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
	// the one arrow selected on its own, if that is what this is
	Arrow* soleArrow() const;
	// the mapping of the one arrow selected on its own, if that is what this is
	MapsElements* soleMapping() const;
	// the one node selected on its own, when it has a diagram drawn inside it
	Category* soleDiagramHome() const;

	// the one category selected on its own; with nothing (or everything)
	// selected, the category the whole diagram is drawn in
	Category* pageCategory() const;
	// the one category selected on its own, ambient or not
	Category* soleCategory() const;

	// open a colour dialog and give what comes back to every node selected;
	// `fill` picks which of the two colours is being changed
	void applyColour(bool fill);
	// the colour the NAME is written in, which is nobody else's business
	void applyTextColour();
	void applyBackgroundColour();
	// store the selected node's colours as what the next one placed starts in
	void applyDefaultLook();
	// The built-in category the selection IS, by name, or empty. Its colour
	// belongs to the built-in rather than to the node (see
	// AppSettings::categoryFill), which is what makes the chips change every
	// instance at once and the "Set default" beside them beside the point.
	QString builtInOfSelection() const;
	// the stylesheet that makes a chip wear its own colour; empty for none
	static QString colourSwatch(const QColor& colour);
	void applyExistsSuch(bool on);
	void applyDeleteMark(bool on);
	void applyRounding(int radius);
	void applyId(const QString& id);
	// turn the selected node into another kind of node ("object", "element",
	// "subcategory", "category:R-Mod", ...)
	void applyNodeKind(const QString& kindId);
	// which built-in the selected category is; "Custom..." defines a new one
	void applyCategoryKind(const QString& name);
	// fill in the Category page for that category, or hide it
	void refreshCategoryBox(Category* category);
	// the two states of the commuting claim, named and explained
	void refreshCommutesLabel(bool commutes);
	// the pieces of the diagram, each with its own claim to commute
	// the commuting and exactness switches for the node that holds a diagram

	DiagramScene* m_scene = nullptr;
	SketchView* m_view = nullptr;
	bool m_updating = false;   // filling the widgets in must not look like edits

	QLabel* m_header = nullptr;
	QLabel* m_hint = nullptr;

	QGroupBox* m_nodeBox = nullptr;
	QLineEdit* m_id = nullptr;
	// what this node IS: a generic object, an element, a category (and which
	// one), a subcategory of the category it is drawn in
	QComboBox* m_type = nullptr;
	QLabel* m_typeHint = nullptr;
	ToggleSwitch* m_exists = nullptr;
	ToggleSwitch* m_deleteMark = nullptr;
	// what the Appearance submenu of the right-click menu used to offer
	QPushButton* m_fillColour = nullptr;
	QPushButton* m_borderColour = nullptr;
	// "what the next one placed looks like", beside the two chips
	QPushButton* m_textColour = nullptr;
	QPushButton* m_setDefaultLook = nullptr;

	// Everything a CATEGORY is asked about: which one it is, whether the
	// diagram drawn in it commutes, what that diagram is put forward as, and
	// the chase. The same questions for the canvas and for a subcategory
	// eight levels down, because they are the same kind of thing.
	QGroupBox* m_categoryBox = nullptr;
	// "Diagram in category "R-Mod"." - what the dropdown below used to offer,
	// said instead of offered (see refreshCategoryBox)
	QLabel* m_categoryName = nullptr;
	// the colour of the paper, on the canvas's own page (see
	// DiagramScene::background)
	QPushButton* m_background = nullptr;
	QComboBox* m_categoryKind = nullptr;   // commented out in the .cpp; kept for its question

	QComboBox* m_statementKind = nullptr;



	QLabel* m_subcategoryHint = nullptr;

	QGroupBox* m_objectBox = nullptr;
	QSpinBox* m_radius = nullptr;

	// what an arrow is asserted to be, cancellable on the left / the right
	QGroupBox* m_arrowBox = nullptr;
	ToggleSwitch* m_monic = nullptr;
	ToggleSwitch* m_inclusion = nullptr;
	ToggleSwitch* m_epic = nullptr;
	// what the Style and Shape submenus of the right-click menu used to offer
	QComboBox* m_arrowStyle = nullptr;
	QPushButton* m_straighten = nullptr;

	// the diagram drawn INSIDE the selected node, when there is one

	ToggleSwitch* m_commutes = nullptr;
	QLabel* m_commutesLabel = nullptr;
	ToggleSwitch* m_rowsExact = nullptr;
	ToggleSwitch* m_columnsExact = nullptr;

	QGroupBox* m_mappingBox = nullptr;
	QLabel* m_mappingHint = nullptr;
	// whether there is an image at all (see MapsElements::isLive), and
	// separately whether geometry travels between the two sides
	ToggleSwitch* m_live = nullptr;
	ToggleSwitch* m_mirror = nullptr;
	ToggleSwitch* m_contravariant = nullptr;
	QPushButton* m_mapNow = nullptr;
};
