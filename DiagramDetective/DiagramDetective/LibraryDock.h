#pragma once

#include <QDockWidget>
#include <QString>

class DiagramScene;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

// The library, as it sits on disk. Folders are folders; a leaf is one
// .totopos file, shown by what it says it is - Theorem, Definition, Axiom -
// without opening it.
class LibraryDock : public QDockWidget
{
	Q_OBJECT

public:
	explicit LibraryDock(QWidget* parent = nullptr);

	void setScene(DiagramScene* scene) { m_scene = scene; }

public slots:
	void rescan();
	// write the examples that ship with the program, then rescan
	void seed();

	// what the scene says about the rule currently laid over the diagram
	void setRuleState(const QString& name, int matches);

signals:
	// a file the user asked to open, by double-clicking it
	void opened(const QString& path);
	// a file the user picked, by clicking it: lay it over the diagram as a rule
	void chosen(const QString& path);
	void applyAllRequested();
	void stopRequested();

private:
	void fill(QTreeWidgetItem* parent, const QString& path, int depth);

	// what the last scan did, so a slow one can say where it went
	int m_folders = 0;
	int m_files = 0;
	bool m_truncated = false;

	DiagramScene* m_scene = nullptr;
	QTreeWidget* m_tree = nullptr;
	QLabel* m_where = nullptr;
	QLabel* m_ruleState = nullptr;
	QPushButton* m_applyAll = nullptr;
	QPushButton* m_stop = nullptr;
};
