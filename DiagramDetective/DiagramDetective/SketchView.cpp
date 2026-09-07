#include "SketchView.h"

#include <QToolButton>
#include <QFrame>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtMath>
#include "CategoryDialog.h"
#include "Category.h"
#include "DiagramScene.h"
#include "Tutor.h"
#include "AppSettings.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include "DiagramScene.h"
#include "ToggleSwitch.h"

namespace
{
	const QString kCustom = QStringLiteral("Custom...");
}

namespace
{
	const int kMargin = 8;
	const int kAnimMs = 120;   // snappy: the panel is a control, not a show
}

SketchView::SketchView(QWidget* parent)
	: QGraphicsView(parent)
{
	setRenderHint(QPainter::Antialiasing, true);
	setDragMode(QGraphicsView::RubberBandDrag);
	// zoom about the point under the cursor, not the view's centre
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	buildOverlay();
}

SketchView::~SketchView()
{
}

QStringList SketchView::builtInCategories()
{
	// the registry lives with the Category subclasses; the combo only lists it
	return Category::builtInNames();
}

QString SketchView::category() const
{
	return m_category ? m_category->currentText() : QString("BigCat");
}

void SketchView::setCategory(const QString& name)
{
	// Selects an entry of the combo: a built-in, or a custom category defined
	// earlier. The combo is only a selector; what a category IS lives in the
	// Category subclasses (see Category::createBuiltIn).
	if (m_category == nullptr)
		return;
	int i = m_category->findText(name);
	if (i < 0)
	{
		i = m_category->count() - 1;   // an unknown name is a custom one: before Custom...
		m_category->insertItem(i, name);
	}
	m_category->setCurrentIndex(i);
}

void SketchView::buildOverlay()
{
	// Both widgets are children of the view itself (not the viewport), created
	// after it, so they sit above the canvas and do not scroll with it. Each is
	// only as large as its contents: the rest of the view stays click-through.
	m_toggle = new QToolButton(this);
	m_toggle->setText(QString(QChar(0x2261)));   // three bars
	m_toggle->setToolTip("sketch controls");
	m_toggle->setCursor(Qt::PointingHandCursor);
	m_toggle->setCheckable(true);
	m_toggle->setFixedSize(34, 34);
	m_toggle->setStyleSheet(
		"QToolButton { background: rgba(30, 32, 44, 200); color: white; border: 1px solid rgba(255,255,255,60);"
		" border-radius: 17px; font-size: 18px; }"
		"QToolButton:hover { background: rgba(60, 64, 90, 230); }"
		"QToolButton:checked { background: rgba(99, 102, 241, 230); }");
	connect(m_toggle, &QToolButton::clicked, this, &SketchView::toggleMenu);

	m_panel = new QFrame(this);
	m_panel->setObjectName("sketchPanel");
	m_panel->setAttribute(Qt::WA_StyledBackground, true);
	m_panel->setStyleSheet(
		"QFrame#sketchPanel { background: rgba(30, 32, 44, 210); border: 1px solid rgba(255,255,255,60); border-radius: 10px; }"
		"QLabel { color: white; background: transparent; }"
		"QCheckBox { color: white; background: transparent; }"
		"QComboBox { min-width: 8em; }"
		"QPushButton { color: white; background: rgba(99, 102, 241, 235); border: none; border-radius: 8px; padding: 5px 10px; }"
		"QPushButton:hover { background: rgba(79, 70, 229, 245); }"
		"QPushButton:disabled { background: rgba(255,255,255,45); color: rgba(255,255,255,150); }");
	auto* layout = new QVBoxLayout(m_panel);
	layout->setContentsMargins(10, 8, 10, 8);
	layout->setSpacing(6);

	auto* row = new QHBoxLayout();
	row->setSpacing(6);
	row->addWidget(new QLabel("Category:", m_panel));
	m_category = new QComboBox(m_panel);
	m_category->addItems(builtInCategories());
	m_category->addItem(kCustom);   // last: defines a new one through the dialog
	m_category->setCurrentIndex(0);
	row->addWidget(m_category);
	layout->addLayout(row);

	// tutor mode: guided interactions coach with remarks and an arrow; off,
	// they run quietly. Always here, so it can be switched back on.
	m_tutor = new QCheckBox("Tutor mode", m_panel);
	m_tutor->setToolTip("Guided actions (Define a product...) explain each step with remarks and an arrow. Untick to run them quietly.");
	m_tutor->setChecked(Tutor::isEnabled());
	connect(m_tutor, &QCheckBox::toggled, this, [](bool on) {
		AppSettings::instance().setValue(AppSettings::TutorEnabled, on);
		AppSettings::instance().apply();
	});
	// the same setting from Tools > Settings: keep the box in step
	connect(&AppSettings::instance(), &AppSettings::changed, this, [this] {
		const QSignalBlocker block(m_tutor);
		m_tutor->setChecked(Tutor::isEnabled());
	});
	layout->addWidget(m_tutor);

	// does this diagram commute, or is nothing being claimed either way?
	auto* commutesRow = new QHBoxLayout();
	commutesRow->setSpacing(8);
	m_commutesLabel = new QLabel(m_panel);
	commutesRow->addWidget(m_commutesLabel);
	commutesRow->addStretch();
	m_commutes = new ToggleSwitch(m_panel);
	commutesRow->addWidget(m_commutes);
	layout->addLayout(commutesRow);
	connect(m_commutes, &QAbstractButton::toggled, this, [this](bool on) {
		refreshCommutesLabel(on);
		emit commutesChanged(on);
	});
	refreshCommutesLabel(false);

	// what this picture is being put forward as
	auto* kindRow = new QHBoxLayout();
	kindRow->setSpacing(6);
	kindRow->addWidget(new QLabel("This is:", m_panel));
	m_kind = new QComboBox(m_panel);
	m_kind->addItems(DiagramScene::kindNames());
	m_kind->setToolTip("The picture says the same thing either way; what changes is what saying it amounts "
	                   "to. An axiom is granted, a definition names something, a conjecture is neither, and "
	                   "a theorem owes a proof.");
	kindRow->addWidget(m_kind, 1);
	layout->addLayout(kindRow);
	connect(m_kind, &QComboBox::currentIndexChanged, this, &SketchView::statementKindPicked);

	m_statementName = new QLineEdit(m_panel);
	m_statementName->setPlaceholderText("Additive identity exists");
	m_statementName->setToolTip("What to call it, so it can be referred to from elsewhere.");
	layout->addWidget(m_statementName);
	connect(m_statementName, &QLineEdit::editingFinished, this, [this] {
		emit statementNamed(m_statementName->text().trimmed());
	});

	// which mode the diagram is in: a let, or a chase
	auto* modeRow = new QHBoxLayout();
	modeRow->setSpacing(8);
	modeRow->addWidget(new QLabel("Mode:", m_panel));
	m_mode = new QLabel("Let", m_panel);
	QFont modeFont = m_mode->font();
	modeFont.setBold(true);
	m_mode->setFont(modeFont);
	modeRow->addWidget(m_mode);
	modeRow->addStretch();
	layout->addLayout(modeRow);

	m_chase = new QPushButton("Start diagram chase", m_panel);
	layout->addWidget(m_chase);
	connect(m_chase, &QPushButton::clicked, this, &SketchView::chaseRequested);
	// more controls go here, one row each

	connect(m_category, &QComboBox::currentIndexChanged, this, &SketchView::onCategoryPicked);

	m_panel->adjustSize();
	m_panel->setMaximumHeight(0);
	m_panel->hide();

	m_anim = new QPropertyAnimation(m_panel, "maximumHeight", this);
	m_anim->setDuration(kAnimMs);
	m_anim->setEasingCurve(QEasingCurve::OutCubic);
	// a maximum height only CAPS the widget; the panel must be resized to follow it
	connect(m_anim, &QPropertyAnimation::valueChanged, this, [this](const QVariant& v) {
		m_panel->resize(m_panel->sizeHint().width(), v.toInt());
	});
	connect(m_anim, &QPropertyAnimation::finished, this, [this] {
		if (!m_open)
			m_panel->hide();
	});

	// the sentence the diagram makes, along the foot of the view
	m_statement = new QLabel(this);
	m_statement->setObjectName("statementBar");
	m_statement->setStyleSheet(
		"QLabel#statementBar { background: rgba(30, 32, 44, 212); color: white;"
		" border: 1px solid rgba(255,255,255,55); border-radius: 10px; padding: 7px 12px; font-size: 12px; }");
	m_statement->setWordWrap(true);
	m_statement->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_statement->hide();

	placeOverlay();
	m_toggle->raise();
	m_panel->raise();
}

void SketchView::setStatement(const QString& statement)
{
	if (m_statement == nullptr)
		return;
	m_statement->setText(statement);
	m_statement->setVisible(!statement.isEmpty());
	placeOverlay();
}

void SketchView::setChasing(bool chasing)
{
	if (m_mode != nullptr)
		m_mode->setText(chasing ? "Chasing" : "Let");
	if (m_chase == nullptr)
		return;
	// never a dead button: it is what starts the chase and what ends it
	m_chase->setText(chasing ? "End the chase" : "Start diagram chase");
	m_chase->setToolTip(chasing
		? "Go back to a let: the diagram is what you are given again, and adding to it assumes nothing."
		: "Start the diagram chase (Ctrl+Shift+Enter): from then on anything you draw is forced into "
		  "the hypotheses of the statement.");
}

void SketchView::toggleMenu()
{
	setMenuOpen(!m_open);
}

void SketchView::setMenuOpen(bool open)
{
	if (open == m_open)
		return;
	m_open = open;
	m_toggle->setChecked(open);
	const int full = m_panel->sizeHint().height();
	m_anim->stop();
	m_anim->setStartValue(m_panel->maximumHeight());
	m_anim->setEndValue(open ? full : 0);
	if (open)
	{
		m_panel->show();
		m_panel->raise();
	}
	m_anim->start();
}

void SketchView::placeOverlay()
{
	// top-right, inside the frame; the panel hangs under the toggle, right-aligned
	const int right = width() - kMargin;
	m_toggle->move(right - m_toggle->width(), kMargin);
	const QSize hint = m_panel->sizeHint();
	m_panel->resize(hint.width(), m_open ? hint.height() : m_panel->height());
	m_panel->move(right - m_panel->width(), kMargin + m_toggle->height() + 4);

	if (m_statement != nullptr && !m_statement->text().isEmpty())
	{
		m_statement->setFixedWidth(qMax(160, width() - 2 * kMargin));
		m_statement->adjustSize();
		m_statement->move(kMargin, height() - m_statement->height() - kMargin);
		m_statement->raise();
	}
}

void SketchView::onCategoryPicked(int index)
{
	if (index < 0)
		return;
	if (m_category->itemText(index) == kCustom)
	{
		defineCustomCategory();
		return;
	}
	m_lastCategoryIndex = index;
	emit categoryChanged(m_category->itemText(index));
}

void SketchView::defineCustomCategory()
{
	CategoryDialog dialog(window());
	if (dialog.exec() != QDialog::Accepted || dialog.name().isEmpty())
	{
		// back to what was selected, quietly
		const QSignalBlocker block(m_category);
		m_category->setCurrentIndex(m_lastCategoryIndex);
		return;
	}
	const QString name = dialog.name();
	int i = m_category->findText(name);
	if (i < 0)
	{
		i = m_category->count() - 1;   // before Custom...
		m_category->insertItem(i, name);
	}
	emit categoryDefined(name, dialog.properties());
	m_category->setCurrentIndex(i);   // -> onCategoryPicked -> categoryChanged
}

void SketchView::resizeEvent(QResizeEvent* event)
{
	QGraphicsView::resizeEvent(event);
	placeOverlay();
}

void SketchView::setZoom(qreal factor)
{
	factor = qBound(0.1, factor, 8.0);
	if (qFuzzyCompare(factor, m_zoom))
		return;
	// scale RELATIVE to the current transform so the anchor (cursor) holds still
	scale(factor / m_zoom, factor / m_zoom);
	m_zoom = factor;
}

void SketchView::wheelEvent(QWheelEvent* event)
{
	// one notch = 120 units; trackpads deliver finer steps, so scale by the amount
	const qreal notches = event->angleDelta().y() / 120.0;
	if (qFuzzyIsNull(notches))
	{
		QGraphicsView::wheelEvent(event);
		return;
	}
	setZoom(m_zoom * qPow(1.15, notches));
	event->accept();
}

QRectF SketchView::contentsRect() const
{
	// the ambient category holds everything drawn; asking the scene would also
	// take in the handle bar and the arrow preview, which are not the diagram
	if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		if (Category* ambient = diagram->ambientCategory())
			return ambient->sceneBoundingRect();
	return scene() != nullptr ? scene()->itemsBoundingRect() : QRectF();
}

void SketchView::centreOnContents()
{
	const QRectF rect = contentsRect();
	centerOn(rect.isEmpty() ? QPointF(0, 0) : rect.center());
}

void SketchView::fitContents()
{
	QRectF rect = contentsRect();
	if (rect.isEmpty())
	{
		resetZoom();
		centerOn(0, 0);
		return;
	}
	rect.adjust(-40, -40, 40, 40);   // a little air around it
	fitInView(rect, Qt::KeepAspectRatio);

	// fitInView sets the transform behind our back: read the zoom back out of
	// it, and hold it to the range the wheel uses
	qreal factor = transform().m11();
	const qreal clamped = qBound(qreal(0.1), factor, qreal(8.0));
	if (!qFuzzyCompare(factor, clamped))
	{
		scale(clamped / factor, clamped / factor);
		factor = clamped;
	}
	m_zoom = factor;
}

void SketchView::refreshCommutesLabel(bool commutes)
{
	if (m_commutesLabel == nullptr)
		return;

	// The off state is NOT the claim that the diagram fails to commute. It is
	// the absence of a claim: the paths may or may not agree, and the diagram
	// says nothing about it either way.
	const QString name = commutes ? QStringLiteral("Commutative") : QStringLiteral("Non-commutative");
	const QString tip = commutes
		? QStringLiteral("Commutative: every pair of paths with the same two ends is asserted to be the "
		                 "same arrow. The statement then reads \"... such that the diagram commutes\", the "
		                 "Equations dock lists what that says, and a cycle becomes an error - a ring gives "
		                 "endlessly many paths, which cannot be read this way.")
		: QStringLiteral("Non-commutative means NOT NECESSARILY COMMUTATIVE. It is not the claim that the "
		                 "diagram fails to commute: it is the absence of any claim. Two paths with the same "
		                 "ends may or may not be the same arrow, nothing is asserted either way, and cycles "
		                 "are perfectly all right.");

	m_commutesLabel->setText(name);
	m_commutesLabel->setToolTip(tip);
	if (m_commutes != nullptr)
		m_commutes->setToolTip(tip);
}

void SketchView::setCommutes(bool commutes)
{
	if (m_commutes == nullptr)
		return;
	const QSignalBlocker block(m_commutes);   // following, not deciding
	m_commutes->setChecked(commutes);
	refreshCommutesLabel(commutes);
}

void SketchView::setStatementKind(int kind, const QString& name)
{
	if (m_kind != nullptr)
	{
		const QSignalBlocker block(m_kind);   // following, not deciding
		m_kind->setCurrentIndex(kind);
	}
	if (m_statementName != nullptr && m_statementName->text() != name)
	{
		const QSignalBlocker block(m_statementName);
		m_statementName->setText(name);
	}
}

void SketchView::fitTo(const QRectF& sceneRect)
{
	if (sceneRect.isEmpty())
		return;
	const QRectF room = sceneRect.adjusted(-60, -60, 60, 60);

	// the zoom that would fit it, kept inside what the wheel allows
	const qreal wide = viewport()->width() / qMax(1.0, room.width());
	const qreal tall = viewport()->height() / qMax(1.0, room.height());
	const qreal target = qBound(0.1, qMin(wide, tall), 8.0);

	// scale about the view's middle rather than under the mouse, then walk the
	// centre over: the eye follows a move it can see
	const QGraphicsView::ViewportAnchor anchor = transformationAnchor();
	setTransformationAnchor(QGraphicsView::AnchorViewCenter);
	scale(target / m_zoom, target / m_zoom);
	m_zoom = target;
	setTransformationAnchor(anchor);

	auto* slide = new QPropertyAnimation(this, "sceneCentre", this);
	slide->setDuration(220);
	slide->setEasingCurve(QEasingCurve::OutCubic);
	slide->setStartValue(mapToScene(viewport()->rect().center()));
	slide->setEndValue(room.center());
	slide->start(QAbstractAnimation::DeleteWhenStopped);
}

QPointF SketchView::sceneCentre() const
{
	return mapToScene(viewport()->rect().center());
}

void SketchView::setSceneCentre(const QPointF& centre)
{
	centerOn(centre);
}
