#include "dialog/SettingsDialog.h"
#include "core/AppSettings.h"
#include "art/Category.h"

#include <QTreeWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QHeaderView>

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Settings");
	resize(760, 480);

	// ---- left: search over the page tree ----
	auto* left = new QWidget(this);
	auto* leftLayout = new QVBoxLayout(left);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	m_search = new QLineEdit(left);
	m_search->setPlaceholderText("Search settings (Ctrl+E)");
	m_search->setClearButtonEnabled(true);
	leftLayout->addWidget(m_search);
	m_tree = new QTreeWidget(left);
	m_tree->setHeaderHidden(true);
	m_tree->setRootIsDecorated(true);
	m_tree->setIndentation(14);
	m_tree->setMinimumWidth(200);
	leftLayout->addWidget(m_tree);

	// ---- right: the page ----
	auto* right = new QWidget(this);
	auto* rightLayout = new QVBoxLayout(right);
	rightLayout->setContentsMargins(12, 0, 0, 0);
	m_header = new QLabel(right);
	QFont hf = m_header->font();
	hf.setPointSize(hf.pointSize() + 3);
	hf.setBold(true);
	m_header->setFont(hf);
	rightLayout->addWidget(m_header);
	auto* scroll = new QScrollArea(right);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	m_stack = new QStackedWidget();
	scroll->setWidget(m_stack);
	rightLayout->addWidget(scroll, 1);

	auto* splitter = new QSplitter(this);
	splitter->addWidget(left);
	splitter->addWidget(right);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({ 230, 530 });

	// ---- bottom: OK / Cancel / Apply, and Reset for the page ----
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
	auto* reset = buttons->addButton("Reset page", QDialogButtonBox::ResetRole);
	connect(buttons, &QDialogButtonBox::accepted, this, [this] { applyAndNotify(); accept(); });
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::applyAndNotify);
	connect(reset, &QPushButton::clicked, this, &SettingsDialog::resetPageToDefaults);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(splitter, 1);
	layout->addWidget(buttons);

	buildPages();
	loadAll();
	m_tree->expandAll();
	connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* cur, QTreeWidgetItem*) { showPage(cur); });
	connect(m_search, &QLineEdit::textChanged, this, &SettingsDialog::filter);
	m_search->setFocus();
	if (!m_pages.isEmpty())
		m_tree->setCurrentItem(m_pages.first().item);
}

// ---------------------------------------------------------------- pages

void SettingsDialog::buildPages()
{
	{
		Page& p = addPage("Diagram", "Grid", "Nodes land on a hidden grid measured in scene units, the same whether a node sits directly on the canvas or inside a category.");
		addBool(p, "Snap nodes to the grid", AppSettings::SnapEnabled, "Positions round to the nearest grid point while dragging and when placed.");
		addDouble(p, "Grid unit", AppSettings::SnapUnit, 1.0, 500.0, 5.0, " px", "The spacing of the grid in scene units.");
		addBool(p, "Show the grid", AppSettings::ShowGrid, "Draw the grid points behind the diagram.");
		addBool(p, "Frame objects that hold nothing", AppSettings::FrameEmptyNodes,
			"Off, an object with nothing drawn inside it is just its label - no border, no fill - and the "
			"frame appears when something is placed in it. A colour chosen by hand always shows either way.");
		addBool(p, "Nodes push each other out of the way", AppSettings::Collision,
			"A node dragged into its neighbours shoves them along the way it is going, and they shove theirs.");
		addDouble(p, "Push clearance", AppSettings::CollisionEpsilon, 0.0, 40.0, 0.5, " px",
			"How much further than just-clear a shoved node is moved, so the two are not left touching.");
	}
	{
		Page& p = addPage("Diagram", "Categories", "What a new sketch starts with.");
		addChoice(p, "Default ambient category", AppSettings::DefaultCategory, Category::builtInNames(), "The category a new scene opens in.");
	}
	{
		Page& p = addPage("Diagram", "Arrows", "How arrows are drawn, and how big a target they are for the mouse.");
		addDouble(p, "Line width", AppSettings::ArrowLineWidth, 0.5, 12.0, 0.5, " px",
			"The thickness of the line. An arrow whose colour was chosen by hand keeps the width it was given.");
		addDouble(p, "Head length", AppSettings::ArrowHeadLength, 4.0, 40.0, 1.0, " px", "How long the head is.");
		addDouble(p, "Head spread", AppSettings::ArrowHeadWidth, 2.0, 24.0, 1.0, " px",
			"How far the two strokes of the head open out either side of the line. With the length, this "
			"is what sets the angle of the head.");
		addDouble(p, "Head line width", AppSettings::ArrowHeadLineWidth, 0.5, 12.0, 0.5, " px",
			"How thick the two strokes of the head are drawn.");
		addDouble(p, "Click width", AppSettings::ArrowHitWidth, 4.0, 60.0, 2.0, " px",
			"How near the line the mouse has to be to hit it. Wider is easier to grab and to bend.");
		addDouble(p, "Bend points linger", AppSettings::BendLingerMs, 0.0, 10000.0, 250.0, " ms",
			"How long the points a curve is pulled through stay up after the mouse has left the line. "
			"They come out on hover and are what a bend is dragged by, so taking them away the instant "
			"the mouse slips off the line leaves nothing to aim at. 0 does take them away at once.");
	}
	{
		Page& p = addPage("Diagram", "Commuting", "A diagram that is asserted to commute is read by comparing "
			"the paths between each pair of objects. A cycle makes endlessly many paths, so it cannot be read "
			"that way - the cycle is shown in red instead. A diagram that is not asserted to commute may of "
			"course go round in circles.");
		addBool(p, "Report cycles in a commuting diagram", AppSettings::CycleCheck,
			"Off, cycles are left alone even when Commutes is on.");
		addBool(p, "Report a name used for two things", AppSettings::NameCheck,
			"A label may mean one thing in a category and in everything drawn inside it. Two things in "
			"categories neither of which contains the other may share a name freely.");
	}
	{
		Page& p = addPage("Diagram", "Notation", "How labels are written.");
		addChoice(p, "Functor applied to a label", AppSettings::FunctorNotation, { "F(h)", "Fh" },
			"How the image of an object or an arrow under a functor is named: F(h) or Fh.");
		addDouble(p, "Label size", AppSettings::LabelPointSize, 4.0, 32.0, 0.5, " pt",
			"The size labels are drawn at. Each level of nesting takes a little off that.");
	}
	{
		Page& p = addPage("Tutor", "General", "Guided actions such as Define a product... can coach you step by step.");
		addBool(p, "Tutor mode: explain guided actions with remarks and an arrow", AppSettings::TutorEnabled, "Off, guided actions still run but keep quiet.");
	}
}

SettingsDialog::Page& SettingsDialog::addPage(const QString& category, const QString& title, const QString& blurb)
{
	// the tree: category > page
	QTreeWidgetItem* parent = nullptr;
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
		if (m_tree->topLevelItem(i)->text(0) == category)
			parent = m_tree->topLevelItem(i);
	if (parent == nullptr)
	{
		parent = new QTreeWidgetItem(m_tree, { category });
		parent->setFlags(parent->flags() & ~Qt::ItemIsSelectable);
	}
	auto* item = new QTreeWidgetItem(parent, { title });

	// the page: a blurb, then a form of options
	auto* widget = new QWidget();
	auto* layout = new QVBoxLayout(widget);
	layout->setContentsMargins(0, 4, 0, 0);
	auto* blurbLabel = new QLabel(blurb, widget);
	blurbLabel->setWordWrap(true);
	blurbLabel->setStyleSheet("color: palette(mid);");
	layout->addWidget(blurbLabel);
	auto* form = new QFormLayout();
	form->setObjectName("form");
	form->setLabelAlignment(Qt::AlignLeft);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	layout->addLayout(form);
	layout->addStretch();
	m_stack->addWidget(widget);

	m_pages.append(Page{ category, title, item, widget, {} });
	item->setData(0, Qt::UserRole, m_pages.size() - 1);
	return m_pages.last();
}

namespace
{
	QFormLayout* formOf(QWidget* page)
	{
		return page->findChild<QFormLayout*>("form");
	}
}

void SettingsDialog::addBool(Page& page, const QString& label, const char* key, const QString& tip)
{
	auto* box = new QCheckBox(label, page.widget);
	box->setToolTip(tip);
	formOf(page.widget)->addRow(box);
	page.options.append(Option{ label, key, box,
		[box, key] { box->setChecked(AppSettings::instance().value(key).toBool()); },
		[box, key] { AppSettings::instance().setValue(key, box->isChecked()); } });
}

void SettingsDialog::addDouble(Page& page, const QString& label, const char* key, double min, double max, double step, const QString& suffix, const QString& tip)
{
	auto* spin = new QDoubleSpinBox(page.widget);
	spin->setRange(min, max);
	spin->setSingleStep(step);
	spin->setDecimals(1);
	spin->setSuffix(suffix);
	spin->setToolTip(tip);
	spin->setMaximumWidth(140);
	formOf(page.widget)->addRow(label, spin);
	page.options.append(Option{ label, key, spin,
		[spin, key] { spin->setValue(AppSettings::instance().value(key).toDouble()); },
		[spin, key] { AppSettings::instance().setValue(key, spin->value()); } });
}

void SettingsDialog::addChoice(Page& page, const QString& label, const char* key, const QStringList& choices, const QString& tip)
{
	auto* combo = new QComboBox(page.widget);
	combo->addItems(choices);
	combo->setToolTip(tip);
	combo->setMaximumWidth(220);
	formOf(page.widget)->addRow(label, combo);
	page.options.append(Option{ label, key, combo,
		[combo, key] {
			const QString v = AppSettings::instance().value(key).toString();
			int i = combo->findText(v);
			if (i < 0) { combo->addItem(v); i = combo->count() - 1; }
			combo->setCurrentIndex(i);
		},
		[combo, key] { AppSettings::instance().setValue(key, combo->currentText()); } });
}

// ---------------------------------------------------------------- behaviour

void SettingsDialog::loadAll()
{
	for (Page& p : m_pages)
		for (Option& o : p.options)
			o.load();
}

void SettingsDialog::storeAll()
{
	for (Page& p : m_pages)
		for (Option& o : p.options)
			o.store();
}

void SettingsDialog::applyAndNotify()
{
	storeAll();
	AppSettings::instance().apply();
}

void SettingsDialog::showPage(QTreeWidgetItem* item)
{
	if (item == nullptr)
		return;
	const QVariant idx = item->data(0, Qt::UserRole);
	if (!idx.isValid())
	{
		// a category row: show its first page
		if (item->childCount() > 0)
			m_tree->setCurrentItem(item->child(0));
		return;
	}
	const Page& p = m_pages[idx.toInt()];
	m_header->setText(p.category + QChar(0x2009) + QChar(0x203A) + QChar(0x2009) + p.title);
	m_stack->setCurrentWidget(p.widget);
}

void SettingsDialog::filter(const QString& text)
{
	// a page stays visible when its title or one of its option labels matches;
	// options that do not match are dimmed, not hidden, so the page keeps its shape
	const QString needle = text.trimmed();
	QTreeWidgetItem* firstHit = nullptr;
	for (Page& p : m_pages)
	{
		bool pageHit = needle.isEmpty() || p.title.contains(needle, Qt::CaseInsensitive) || p.category.contains(needle, Qt::CaseInsensitive);
		bool anyOption = false;
		for (Option& o : p.options)
		{
			const bool hit = needle.isEmpty() || o.label.contains(needle, Qt::CaseInsensitive);
			anyOption = anyOption || hit;
			o.editor->setEnabled(true);
			o.editor->setStyleSheet(hit || needle.isEmpty() ? QString() : QStringLiteral("color: palette(mid);"));
		}
		const bool visible = pageHit || anyOption;
		p.item->setHidden(!visible);
		if (visible && firstHit == nullptr)
			firstHit = p.item;
	}
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* cat = m_tree->topLevelItem(i);
		bool any = false;
		for (int c = 0; c < cat->childCount(); ++c)
			any = any || !cat->child(c)->isHidden();
		cat->setHidden(!any);
	}
	if (firstHit != nullptr && (m_tree->currentItem() == nullptr || m_tree->currentItem()->isHidden()))
		m_tree->setCurrentItem(firstHit);
}

void SettingsDialog::resetPageToDefaults()
{
	QTreeWidgetItem* item = m_tree->currentItem();
	if (item == nullptr || !item->data(0, Qt::UserRole).isValid())
		return;
	// every option on the page back to its default, in the editor only —
	// OK / Apply stores it, Cancel forgets it
	for (Option& o : m_pages[item->data(0, Qt::UserRole).toInt()].options)
	{
		const QVariant d = AppSettings::defaultValue(o.key);
		if (auto* box = qobject_cast<QCheckBox*>(o.editor))
			box->setChecked(d.toBool());
		else if (auto* spin = qobject_cast<QDoubleSpinBox*>(o.editor))
			spin->setValue(d.toDouble());
		else if (auto* combo = qobject_cast<QComboBox*>(o.editor))
			combo->setCurrentText(d.toString());
	}
}
