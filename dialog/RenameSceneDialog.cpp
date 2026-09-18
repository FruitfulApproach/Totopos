#include "dialog/RenameSceneDialog.h"

#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QDir>
#include <QShortcut>
#include <QKeySequence>

#include "core/io/SceneFile.h"
#include "art/DiagramScene.h"

namespace
{
	// -1 stands for "nothing picked yet", which is what the combo opens on.
	const int kNoKind = -1;

	// The kinds offered, in the order they are offered. A free drawing first,
	// because it is what a diagram is before it claims anything.
	const QList<int> kOffered = {
		DiagramScene::Unstated,
		DiagramScene::Theorem,
		DiagramScene::Definition,
		DiagramScene::Axiom,
		DiagramScene::Proof,
	};

	QString wordFor(int kind)
	{
		return kind == DiagramScene::Unstated
			? QStringLiteral("Free drawing")
			: DiagramScene::kindName(DiagramScene::StatementKind(kind));
	}

	// What Windows will not have in a filename, and what no filesystem should
	// be asked to take. The dot is ours: it is what separates the name from
	// the kind, so a name may not carry one of its own.
	bool isAllowed(QChar c)
	{
		static const QString forbidden = QStringLiteral("<>:\"/\\|?*.");
		return c.unicode() >= 32 && !forbidden.contains(c);
	}

	// Two paths, one of which does not exist yet. QFileInfo's own comparison
	// calls those unequal whatever they say, so compare what they SAY - and
	// without case, because Windows does.
	bool samePath(const QString& a, const QString& b)
	{
		return QString::compare(QFileInfo(a).absoluteFilePath(),
		                        QFileInfo(b).absoluteFilePath(), Qt::CaseInsensitive) == 0;
	}

	// CON, PRN, LPT1 and the rest still cannot be filenames on Windows, with
	// or without an extension.
	bool isReservedOnWindows(const QString& name)
	{
		static const QStringList reserved = {
			"con", "prn", "aux", "nul",
			"com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
			"lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
		};
		return reserved.contains(name.toLower());
	}
}

RenameSceneDialog::RenameSceneDialog(const QString& path, QWidget* parent)
	: QDialog(parent)
	, m_path(path)
{
	setWindowTitle(QStringLiteral("Rename"));
	setModal(true);

	auto* column = new QVBoxLayout(this);
	column->setSpacing(6);

	m_name = new QLineEdit(this);
	m_name->setPlaceholderText(QStringLiteral("Enter name"));
	m_name->setText(SceneFile::baseNameOf(path));
	m_name->selectAll();
	column->addWidget(m_name);

	// what is wrong with it, or what it will be called: directly under the
	// entry, where the eye already is
	m_state = new QLabel(this);
	m_state->setWordWrap(true);
	column->addWidget(m_state);

	auto* row = new QHBoxLayout();
	m_kind = new QComboBox(this);
	m_kind->addItem(QStringLiteral("Select type"), kNoKind);
	for (int kind : kOffered)
		m_kind->addItem(wordFor(kind), kind);

	// The file's own kind, so a rename does not quietly retype it. A kind this
	// list does not offer - a conjecture, a remark - is added rather than lost.
	const int was = SceneFile::kindFromFileName(path);
	if (m_kind->findData(was) < 0)
		m_kind->addItem(wordFor(was), was);
	m_kind->setCurrentIndex(m_kind->findData(was));
	row->addWidget(m_kind);
	row->addStretch();

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	row->addWidget(buttons);
	column->addLayout(row);

	// Ctrl+Enter is Ok from anywhere in the dialog, the same as it finishes a
	// label on the canvas
	for (const QKeySequence& keys : { QKeySequence(Qt::CTRL | Qt::Key_Return),
	                                  QKeySequence(Qt::CTRL | Qt::Key_Enter) })
	{
		auto* shortcut = new QShortcut(keys, this);
		connect(shortcut, &QShortcut::activated, this, [this] {
			if (m_ok->isEnabled())
				accept();
		});
	}

	connect(m_name, &QLineEdit::textChanged, this, &RenameSceneDialog::revalidate);
	connect(m_kind, &QComboBox::currentIndexChanged, this, &RenameSceneDialog::revalidate);
	revalidate();
}

QString RenameSceneDialog::baseName() const
{
	return m_name->text().trimmed();
}

int RenameSceneDialog::kind() const
{
	return m_kind->currentData().toInt();
}

QString RenameSceneDialog::fileName() const
{
	return SceneFile::fileNameFor(baseName(), kind());
}

QString RenameSceneDialog::newPath() const
{
	return QFileInfo(m_path).absoluteDir().absoluteFilePath(fileName());
}

QString RenameSceneDialog::whatIsWrong() const
{
	const QString name = baseName();
	if (name.isEmpty())
		return QStringLiteral("Give it a name.");
	if (m_kind->currentData().toInt() == kNoKind)
		return QStringLiteral("Say what kind of thing it is.");

	for (QChar c : name)
		if (!isAllowed(c))
			return c == QLatin1Char('.')
				? QStringLiteral("A dot separates the name from the kind, so the name cannot hold one.")
				: QString("A filename cannot hold %1.").arg(
					c.isSpace() ? QStringLiteral("that character") : QString("'%1'").arg(c));
	if (name.endsWith(QLatin1Char(' ')))
		return QStringLiteral("A name cannot end in a space.");
	if (isReservedOnWindows(name))
		return QString("%1 is a name Windows keeps for itself.").arg(name);
	if (fileName().size() > 200)
		return QStringLiteral("That is too long for a filename.");

	const QString target = newPath();
	if (samePath(target, m_path))
		return QStringLiteral("That is what it is called already.");
	if (QFileInfo::exists(target))
		return QString("%1 is already there. Pick another name.").arg(fileName());

	return QString();
}

void RenameSceneDialog::revalidate()
{
	const QString wrong = whatIsWrong();
	m_ok->setEnabled(wrong.isEmpty());
	if (wrong.isEmpty())
	{
		m_state->setStyleSheet(QStringLiteral("color: #555;"));
		m_state->setText(QString("It will be called %1.").arg(fileName()));
		return;
	}
	m_state->setStyleSheet(QStringLiteral("color: #b91c1c;"));
	m_state->setText(wrong);
}
