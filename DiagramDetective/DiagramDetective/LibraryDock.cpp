#include "LibraryDock.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
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

void LibraryDock::fill(QTreeWidgetItem* parent, const QString& path)
{
	QDir dir(path);

	for (const QFileInfo& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
	{
		auto* branch = new QTreeWidgetItem(parent, { entry.fileName(), QString() });
		branch->setFirstColumnSpanned(false);
		fill(branch, entry.absoluteFilePath());
		if (branch->childCount() == 0)
			delete branch;   // nothing of ours down there
		else
			branch->setExpanded(true);
	}

	for (const QFileInfo& entry : dir.entryInfoList({ "*.totopos" }, QDir::Files, QDir::Name))
	{
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
	m_where->setText(base);

	auto* rootItem = new QTreeWidgetItem(m_tree, { QDir(base).dirName(), QString() });
	fill(rootItem, base);
	rootItem->setExpanded(true);
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
