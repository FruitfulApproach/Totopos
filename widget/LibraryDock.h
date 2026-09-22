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
	// A file was taken out of the library. Anyone holding it open is left
	// holding a diagram with nowhere to save itself back to, so they are told.
	void removed(const QString& path);
	// A file was renamed on disk. Whoever has it open says so in its tab -
	// which is why this is emitted only after the rename has happened, never
	// while the dialog is still up.
	void renamed(const QString& before, const QString& after);
	// A FOLDER was renamed, so every file under it has moved with it. Whoever
	// has one of them open follows by path: anything beginning with `before`
	// now begins with `after`.
	void folderRenamed(const QString& before, const QString& after);
	// A NEW, EMPTY DIAGRAM, asked for in a folder of the library. The panel
	// settles what it is to be CALLED and where it goes; making the diagram
	// itself belongs to the window, which is what knows how to build one and
	// how to put it in a tab - so the path is handed over and the window
	// does the rest.
	void createRequested(const QString& path);
	// a file the user picked, by clicking it: lay it over the diagram as a rule
	void chosen(const QString& path);
	void applyAllRequested();
	void stopRequested();

private slots:
	// the right-click menu: renaming and removing a file, and - for a folder
	// as well as a file - showing it on disk and copying its path
	void showContextMenu(const QPoint& at);
	void renameFile(const QString& path);
	// take it out of the library, off the disk. Asks first, unless the
	// asking has been turned off from the box that asks.
	void removeFile(const QString& path);
	// make a folder inside that one, asking what to call it
	void createFolder(const QString& inDir);
	// ask for a name and hand the path out to be made (see createRequested)
	void createDiagram(const QString& inDir);
	// rename a folder of the library; everything in it goes with it
	void renameFolder(const QString& dir);

private:
	// Where a FOLDER row is on disk. A file's path lives in Qt::UserRole, and
	// that role is read elsewhere as "the file this row is" - a single click
	// lays it over the diagram as a rule - so a folder cannot share it.
	enum { FolderRole = Qt::UserRole + 1 };

	void fill(QTreeWidgetItem* parent, const QString& path, int depth);
	// show it in a file manager, picked out in the folder it is in
	void showInExplorer(const QString& path);

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
