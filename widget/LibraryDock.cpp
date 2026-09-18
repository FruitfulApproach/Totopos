#include "widget/LibraryDock.h"

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

#include "art/DiagramScene.h"
#include "core/rules/Library.h"
#include "dialog/RenameSceneDialog.h"
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QCheckBox>
#include "core/AppSettings.h"
#include "core/Emoji.h"
#include "core/io/SceneFile.h"

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
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &LibraryDock::showContextMenu);

	// a single click lays the file over the diagram as a rule: everywhere its
	// premise fits lights up, with a button to apply it there
	connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
		const QString path = item->data(0, Qt::UserRole).toString();
		if (!path.isEmpty())
			emit chosen(path);
	});

	// the rule in force, and the two things to do about it
	m_ruleState = new QLabel("Click a file to lay it over the diagram as a rule; double-click to open it. "
	                         "Solid is what the rule looks for, dotted is what it draws in, and a red cross "
	                         "is what it takes away.", body);
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

	auto* examples = new QPushButton("Write the standard rules", body);
	examples->setToolTip("Write the rules that ship with the program into the library: a folder per "
	                     "category, with the identities and composites that hold anywhere, then kernels, "
	                     "cokernels, images, biproducts and exact sequences where they belong. Anything "
	                     "already on disk is left exactly as it is.");
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
		// What a file IS is written in its name - kernel.definition.totopos -
		// so a library of hundreds is listed without opening any of them. The
		// name of the statement still comes from inside, which is cheap: it is
		// in the clear at the front.
		const QString path = entry.absoluteFilePath();
		const int namedKind = SceneFile::kindFromFileName(path);
		const SceneFile::Heading heading = SceneFile::peek(path);
		const int kindValue = namedKind != DiagramScene::Unstated
			? namedKind
			: (heading.valid ? heading.kind : int(DiagramScene::Unstated));
		const QString kind = kindValue != DiagramScene::Unstated
			? DiagramScene::kindName(DiagramScene::StatementKind(kindValue))
			: QStringLiteral("Drawing");
		auto* leaf = new QTreeWidgetItem(parent, { SceneFile::baseNameOf(path), kind });
		leaf->setData(0, Qt::UserRole, path);
		leaf->setToolTip(0, heading.name.isEmpty() ? entry.fileName() : heading.name);
	}
}

void LibraryDock::showContextMenu(const QPoint& at)
{
	QTreeWidgetItem* item = m_tree->itemAt(at);
	if (item == nullptr)
		return;
	const QString path = item->data(0, Qt::UserRole).toString();
	if (path.isEmpty())
		return;   // a folder, not a file

	QMenu menu(this);
	QAction* rename = menu.addAction(QString("Rename %1...").arg(QFileInfo(path).fileName()));
	connect(rename, &QAction::triggered, this, [this, path] { renameFile(path); });

	menu.addSeparator();
	QAction* remove = menu.addAction(QString("%1  Remove %2 from the library")
		.arg(Emoji::remove(), QFileInfo(path).fileName()));
	remove->setToolTip("Take this file out of the library and off the disk.");
	connect(remove, &QAction::triggered, this, [this, path] { removeFile(path); });

	menu.exec(m_tree->viewport()->mapToGlobal(at));
}

void LibraryDock::renameFile(const QString& path)
{
	RenameSceneDialog dialog(path, this);
	if (dialog.exec() != QDialog::Accepted)
		return;   // nothing said, nothing done: the name is as it was

	const QString target = dialog.newPath();
	if (!QFile::rename(path, target))
	{
		QMessageBox::warning(this, QStringLiteral("Rename"),
			QString("%1 could not be renamed to %2. Something else may have it open.")
				.arg(QFileInfo(path).fileName(), dialog.fileName()));
		return;
	}
	rescan();
	// only now, with the file actually moved, does the rest of the program hear
	emit renamed(path, target);
}

void LibraryDock::removeFile(const QString& path)
{
	const QString name = QFileInfo(path).fileName();

	// Asked once, and then only if the asking has not been turned off. The
	// question is worth asking at all because this is the one entry here that
	// reaches off the panel and onto the disk.
	if (AppSettings::instance().warnOnLibraryRemove())
	{
		QMessageBox ask(this);
		ask.setIcon(QMessageBox::Warning);
		ask.setWindowTitle(QStringLiteral("Remove from the library"));
		ask.setText(QString("Remove %1 from the library?").arg(name));
		ask.setInformativeText("It goes from the disk as well as from this panel - to the "
		                       "Recycle Bin, so it can be fetched back from there. Anything "
		                       "you have open stays open.");
		ask.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
		ask.setDefaultButton(QMessageBox::Cancel);

		auto* again = new QCheckBox(QStringLiteral("Do not ask me again"), &ask);
		ask.setCheckBox(again);

		if (ask.exec() != QMessageBox::Yes)
			return;
		// Only once the answer is yes: saying no and having the box remember it
		// would be remembering the wrong half of the answer.
		if (again->isChecked())
			AppSettings::instance().setValue(AppSettings::WarnOnLibraryRemove, false);
	}

	// To the Recycle Bin rather than straight out: this runs with no question
	// asked once the nag is off, so it had better be something that can be
	// undone somewhere. Only if the bin refuses is the file removed outright.
	QFile file(path);
	if (!file.moveToTrash() && !file.remove())
	{
		QMessageBox::warning(this, QStringLiteral("Remove from the library"),
			QString("%1 could not be removed. Something else may have it open.").arg(name));
		return;
	}
	rescan();
	emit removed(path);
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
		m_where->setText("The standard rules were already there.");
	else
		m_where->setText(QString("Wrote %1 rule%2.").arg(written).arg(written == 1 ? "" : "s"));
}

void LibraryDock::setRuleState(const QString& name, int matches)
{
	const bool active = !name.isEmpty();
	m_applyAll->setEnabled(active && matches > 0);
	m_stop->setEnabled(active);
	if (!active)
		m_ruleState->setText("Click a file to lay it over the diagram as a rule; double-click to open it. "
		                     "Solid is what the rule looks for, dotted is what it draws in, and a red cross "
		                     "is what it takes away.");
	else if (matches == 0)
		m_ruleState->setText(QString("%1 fits nowhere in this diagram.").arg(name));
	else
		m_ruleState->setText(QString("%1 fits in %2 place%3 - lit up in the diagram. Press Apply at one, "
		                             "or apply to all.").arg(name).arg(matches).arg(matches == 1 ? "" : "s"));
}
