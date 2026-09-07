#pragma once

#include <QDockWidget>
#include <QString>

class DiagramScene;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

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

signals:
	// a file the user asked to open, by double-clicking it
	void opened(const QString& path);

private:
	void fill(QTreeWidgetItem* parent, const QString& path);

	DiagramScene* m_scene = nullptr;
	QTreeWidget* m_tree = nullptr;
	QLabel* m_where = nullptr;
};
