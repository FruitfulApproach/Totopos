#pragma once

#include <QDockWidget>

class DiagramScene;
class ToggleSwitch;
class QListWidget;
class QLabel;

// What a commuting diagram actually SAYS: every pair of paths with the same
// two ends, as an equation. The switch writes composites as g o f or as gf.
class CommutativeEquationsDock : public QDockWidget
{
	Q_OBJECT

public:
	explicit CommutativeEquationsDock(QWidget* parent = nullptr);

	void setScene(DiagramScene* scene);

public slots:
	void refresh();

private:
	DiagramScene* m_scene = nullptr;
	QLabel* m_note = nullptr;
	QListWidget* m_equations = nullptr;
	ToggleSwitch* m_ring = nullptr;
};
