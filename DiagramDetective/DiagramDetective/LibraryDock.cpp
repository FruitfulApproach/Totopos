#include "LibraryDock.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QUrl>

#include "DiagramScene.h"
#include "library/Library.h"
#include "io/SceneFile.h"

LibraryDock::LibraryDock(QWidget* parent)
	: QDockWidget("Library", parent)
{
	setObjectName("libraryDock");
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	auto* body = new QWidget(this);
	auto* layout = new QVBoxLayout(body);
	layout->setContentsMargins(8, 6, 8, 8);
	layout->setSpacing(6);

	m_tree = new QTreeWidget(body);
	m_tree->setHeaderLabels({ "Name", "Is" });
	m_tree->setColumnWidth(0, 200);
	m_tree->setAlternatingRowColors(true);
	m_tree->setToolTip("Double-click a file to open it. The steps that made it are kept inside, so a proof "
	                   "can be walked through afterwards.");
	layout->addWidget(m_tree, 1);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
		const QString path = item->data(0, Qt::UserRole).toString();
		if (!path.isEmpty())
			emit opened(path);
	});
	// a single click lays the file over the diagram as a rule: everywhere its
	// premise fits lights up, with a button to apply it there
	connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
		const QString path = item->data(0, Qt::UserRole).toString();
		if (!path.isEmpty())
			emit chosen(path);
	});

	// the rule in force, and the two things to do about it
	m_ruleState = new QLabel("Click a file to lay it over the diagram as a rule; double-click to open it.", body);
	m_ruleState->setWordWrap(true);
	layout->addWidget(m_ruleState);

	auto* ruleButtons = new QHBoxLayout();
	m_applyAll = new QPushButton("Apply to all", body);
	m_applyAll->setEnabled(false);
	m_applyAll->setToolTip("Draw the rule's conclusion in at every place it fits, at once.");
	connect(m_applyAll, &QPushButton::clicked, this, &LibraryDock::applyAllRequested);
	ruleButtons->addWidget(m_applyAll);
	m_stop = new QPushButton("Take it off", body);
	m_stop->setEnabled(false);
	m_stop->setToolTip("Stop showing where the rule fits. Esc does the same.");
	connect(m_stop, &QPushButton::clicked, this, &LibraryDock::stopRequested);
	ruleButtons->addWidget(m_stop);
	ruleButtons->addStretch();
	layout->addLayout(ruleButtons);

	auto* buttons = new QHBoxLayout();
	auto* refresh = new QPushButton("Refresh", body);
	connect(refresh, &QPushButton::clicked, this, &LibraryDock::rescan);
	buttons->addWidget(refresh);

	auto* examples = new QPushButton("Write the examples", body);
	examples->setToolTip("Write the diagrams that ship with the program into the library, if they are not "
	                     "already there.");
	connect(examples, &QPushButton::clicked, this, &LibraryDock::seed);
	buttons->addWidget(examples);
	buttons->addStretch();
	layout->addLayout(buttons);

	m_where = new QLabel(body);
	m_where->setWordWrap(true);
	m_where->setEnabled(false);
	layout->addWidget(m_where);

	setWidget(body);
	rescan();
}

void LibraryDock::fill(QTreeWidgetItem* parent, const QString& path, int depth)
{
	// A library is a few folders deep and a few hundred files at most. Bounds
	// on both, and no following of links, so a wrong root - or a junction
	// that loops - cannot turn a refresh into a walk of the whole disk.
	const int kMaxDepth = 8;
	const int kMaxEntries = 2000;
	if (depth > kMaxDepth || m_folders + m_files > kMaxEntries)
	{
		m_truncated = true;
		return;
	}

	QDir dir(path);
	const QDir::Filters dirFilter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
	for (const QFileInfo& entry : dir.entryInfoList(dirFilter, QDir::Name))
	{
		++m_folders;
		auto* branch = new QTreeWidgetItem(parent, { entry.fileName(), QString() });
		fill(branch, entry.absoluteFilePath(), depth + 1);
		if (branch->childCount() == 0)
			delete branch;   // nothing of ours down there
		else
			branch->setExpanded(true);
	}

	for (const QFileInfo& entry : dir.entryInfoList({ "*.totopos" }, QDir::Files | QDir::NoSymLinks, QDir::Name))
	{
		++m_files;
		const SceneFile::Heading heading = SceneFile::peek(entry.absoluteFilePath());
		const QString kind = heading.valid && heading.kind != DiagramScene::Unstated
			? DiagramScene::kindName(DiagramScene::StatementKind(heading.kind))
			: QString();
		auto* leaf = new QTreeWidgetItem(parent, { entry.completeBaseName(), kind });
		leaf->setData(0, Qt::UserRole, entry.absoluteFilePath());
		if (!heading.name.isEmpty())
			leaf->setToolTip(0, heading.name);
	}
}

void LibraryDock::rescan()
{
	m_tree->clear();
	const QString base = Library::root();
	if (base.isEmpty())
	{
		m_where->setText("No library folder yet. Write the examples to make one.");
		return;
	}
	QElapsedTimer clock;
	clock.start();
	m_folders = 0;
	m_files = 0;
	m_truncated = false;

	auto* rootItem = new QTreeWidgetItem(m_tree, { QDir(base).dirName(), QString() });
	fill(rootItem, base, 0);
	rootItem->setExpanded(true);

	// say what was done and how long it took: a scan that is slow should be
	// able to tell us where it went
	m_where->setText(QString("%1\n%2 folder%3, %4 file%5 in %6 ms%7")
		.arg(base)
		.arg(m_folders).arg(m_folders == 1 ? "" : "s")
		.arg(m_files).arg(m_files == 1 ? "" : "s")
		.arg(clock.elapsed())
		.arg(m_truncated ? " - stopped early: that is far too much for a library" : ""));
}

void LibraryDock::seed()
{
	QString error;
	const int written = Library::seedExamples(&error);
	rescan();
	if (!error.isEmpty())
		m_where->setText(error);
	else if (written == 0)
		m_where->setText("The examples were already there.");
	else
		m_where->setText(QString("Wrote %1 example%2.").arg(written).arg(written == 1 ? "" : "s"));
}

void LibraryDock::setRuleState(const QString& name, int matches)
{
	const bool active = !name.isEmpty();
	m_applyAll->setEnabled(active && matches > 0);
	m_stop->setEnabled(active);
	if (!active)
		m_ruleState->setText("Click a file to lay it over the diagram as a rule; double-click to open it.");
	else if (matches == 0)
		m_ruleState->setText(QString("%1 fits nowhere in this diagram.").arg(name));
	else
		m_ruleState->setText(QString("%1 fits in %2 place%3 - lit up in the diagram. Press Apply at one, "
		                             "or apply to all.").arg(name).arg(matches).arg(matches == 1 ? "" : "s"));
}
