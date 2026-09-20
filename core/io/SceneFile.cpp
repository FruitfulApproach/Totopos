#include "core/io/SceneFile.h"

#include <QFile>
#include <QDataStream>
#include <QSaveFile>
#include <QFileInfo>
#include "art/DiagramScene.h"
#include "art/Category.h"
#include "art/Arrow.h"
#include "art/Functor.h"
#include "art/AtomicElement.h"
#include "art/RModule.h"
#include "core/history/SceneHistory.h"
#include "core/history/Mementos.h"
#include "core/props/MapsElements.h"

namespace
{
	const char kMagic[4] = { 'D', 'D', 'G', 'M' };
	const quint16 kVersion = 22;   // 22: the colour each name is written in   // 21: the colour of the paper   // 20: R-modules as a kind of their own, and the ring each is over   // 19: where things were put in the classical view, and which notation was in front   // 18: the two lines of work joined - what each node IS (which built-in a category is, elements, what the diagram in it claims) alongside node identity and arrow style   // 17: each node's own identity   // 16: what kind of arrow it is   // 15: struck off in red, a label dragged clear, and what a proof proves   // 13: which pieces are exact. 14: what it is, written on the outside   // 2: Exists such, hypotheses, commuting. 3: rounding, image links, mapping settings. 4: bends

	// THE TWO MEANINGS OF VERSION 15.
	//
	// Two lines of work each added fields and each called the result 15, so a
	// file that says 15 is in one of two dialects. They do not overlap: ours
	// put its fields in the COMMON part of a node record (struck off in red,
	// where the label was dragged to, what a proof proves), theirs in the
	// CATEGORY part and in the scene header (which built-in it is, what the
	// diagram drawn in it claims). From 18 on, every file carries both.
	//
	// Reading one dialect as the other walks off the end of a record, which is
	// how a load finds out it guessed wrong - see SceneFile::load, which tries
	// ours and reads the file again the other way if that fails.
	//
	// Passed down rather than kept in a global: rules are read on the
	// rule-search thread while the GUI thread may be opening a file.
	enum class Dialect { Ours, Theirs };

	QString kindOf(Node* node)
	{
		if (dynamic_cast<Functor*>(node) != nullptr)       return QStringLiteral("Functor");
		if (dynamic_cast<Arrow*>(node) != nullptr)         return QStringLiteral("Arrow");
		if (dynamic_cast<AtomicElement*>(node) != nullptr) return QStringLiteral("Element");
		// before Object, which it is one of. An older build reading this file
		// does not know the word and makes a plain object of it, which is what
		// it was before modules were a kind of their own.
		if (dynamic_cast<RModule*>(node) != nullptr)       return QStringLiteral("Module");
		if (dynamic_cast<Category*>(node) != nullptr)      return QStringLiteral("Category");
		return QStringLiteral("Object");
	}

	// a colour that is not valid means "none": no fill, no border
	void writeStyle(QDataStream& out, Node* node)
	{
		const QBrush fill = node->fill();
		const QPen border = node->border();
		out << (fill.style() == Qt::NoBrush ? QColor() : fill.color());
		out << (border.style() == Qt::NoPen ? QColor() : border.color());
		out << qreal(border.widthF());
	}

	void readStyle(QDataStream& in, Node* node)
	{
		QColor fill, border;
		qreal width = 1.5;
		in >> fill >> border >> width;
		node->setFill(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
		node->setBorder(border.isValid() ? QPen(border, width) : QPen(Qt::NoPen));
	}

	// One node and everything under it. Arrows are written where they sit but
	// carry the paths of their two ends, and are built in a second pass once
	// every object exists.
	void writeNode(QDataStream& out, Node* node)
	{
		out << kindOf(node) << node->id() << node->pos() << qreal(node->zValue());
		writeStyle(out, node);
		out << node->existsSuch() << node->isHypothesis();
		out << node->commutesInComponent()
		    << node->rowsExactInComponent() << node->columnsExactInComponent();
		// what it is the image of, so a live functor knows its own work again
		out << qreal(node->cornerRadius())
		    << node->data(MapsElements::ImageFunctorKey).toString()
		    << node->data(MapsElements::ImageSourceKey).toString()
		    << node->data(MapsElements::MappingIdKey).toString()
		    << node->data(MapsElements::ImageMappingKey).toString();

		// a label made out of other labels, and which nodes those are
		const QList<Node*> sources = node->labelSources();
		out << node->labelPattern() << qint32(sources.size());
		for (Node* source : sources)
			out << source->pathFromRoot() << source->id();

		// version 15: crossed out in red (taken away where this is applied as a
		// rule), and where the label has been dragged to. Every node has both;
		// an arrow's offset is also written in its own section, where it has
		// been since version 7, and the two say the same thing.
		out << node->markedForDeletion() << node->labelOffset();
		// version 17: WHICH node this is, as against what it is called
		out << node->key();
		// version 22: the colour the NAME is written in. Invalid means nobody
		// said, and it is written in the ink names are written in.
		out << node->labelColour();

		if (auto* category = dynamic_cast<Category*>(node))
		{
			out << category->properties() << qint32(category->nextObjectIndex()) << qint32(category->nextArrowIndex());
			out << category->rowsExact() << category->columnsExact();
			// WHICH category this is - a nested R-Mod used to come back as a
			// plain one, and its objects stopped being R-modules - and whether
			// it is a subcategory of the one it is drawn in
			out << category->builtInName() << category->isSubcategory();
			// and what the diagram drawn in it is put forward as
			out << category->commutes() << qint32(category->statementKind()) << category->statementName();
		}
		else if (auto* module = dynamic_cast<RModule*>(node))
		{
			// version 20: which ring it is a module over. R unless said otherwise.
			out << module->ring();
		}
		else if (auto* arrow = dynamic_cast<Arrow*>(node))
		{
			out << arrow->properties();
			out << (arrow->domain() != nullptr ? arrow->domain()->pathFromRoot() : QList<int>());
			out << (arrow->codomain() != nullptr ? arrow->codomain()->pathFromRoot() : QList<int>());

			// how a functor maps, when it is one
			auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key()));
			// The five bytes an older file kept one flag each for - live,
			// positions each way, bends each way - are two answers now, and
			// they go back where they came from: the first byte is whether
			// the image is live, and the four that were geometry hold the
			// one geometry answer (MapsElements::mirrorsGeometry). The format
			// does not move, and an older build reading this file still gets
			// something it understands.
			const quint8 live = quint8(maps != nullptr && maps->isLive() ? 1 : 0);
			const quint8 mirror = quint8(maps != nullptr && maps->mirrorsGeometry() ? 1 : 0);
			out << live << mirror << mirror << mirror << mirror;
			out << arrow->labelOffset();
			out << quint8(maps != nullptr && maps->isContravariant() ? 1 : 0);
			out << arrow->bends();   // the points its line is pulled through
			out << quint8(arrow->style());   // inclusion, monic, epic, invertible
		}

		QList<Node*> children;
		for (QGraphicsItem* child : node->childItems())
			if (auto* kid = dynamic_cast<Node*>(child))
				children << kid;
		out << qint32(children.size());
		for (Node* child : children)
			writeNode(out, child);
	}

	// a label that names other nodes: they may not all exist yet while the
	// tree is being read, so these are tied up at the end
	struct PendingDerived
	{
		Node* node = nullptr;
		QString pattern;
		QList<QPair<QList<int>, QString>> sources;
	};

	// the node a (path, label) pair names: the path alone cannot name an
	// arrow, which is why the label goes with it
	Node* resolveRef(Node* ambient, const QList<int>& path, const QString& id)
	{
		Node* at = Node::fromPath(ambient, path);
		if (at == nullptr)
			return nullptr;
		if (at->id() == id)
			return at;
		for (QGraphicsItem* child : at->childItems())
			if (auto* node = dynamic_cast<Node*>(child))
				if (node->id() == id)
					return node;
		return nullptr;
	}

	void readDerived(QDataStream& in, QString& pattern, QList<QPair<QList<int>, QString>>& sources)
	{
		qint32 count = 0;
		in >> pattern >> count;
		if (in.status() != QDataStream::Ok || count < 0 || count > 10000)
		{
			in.setStatus(QDataStream::ReadCorruptData);
			return;
		}
		for (qint32 i = 0; i < count; ++i)
		{
			QList<int> path;
			QString id;
			in >> path >> id;
			sources.append(qMakePair(path, id));
		}
	}

	struct PendingArrow
	{
		Node* parent = nullptr;
		QString kind, id;
		QStringList props;
		QPointF pos;
		qreal z = 0;
		QColor fill, border;
		qreal width = 1.5;
		QList<int> domain, codomain;
		bool existsSuch = false;
		bool hypothesis = false;
		bool deleteMark = false;
		bool componentCommutes = true;
		bool componentRows = false, componentColumns = false;
		qreal radius = 5.0;
		QString imageFunctor, imageSource;
		QString mappingId, imageMapping;
		quint8 live = 1, reflect = 1, direction = 0;
		quint8 imagineBends = 1, reflectBends = 0, contravariant = 0;
		QPointF labelOffset;
		QList<QPointF> bends;
		quint8 style = 0;   // Arrow::Style::Plain
		QString key;
		QColor labelColour;   // invalid: written in the ink names are written in
		QString pattern;
		QList<QPair<QList<int>, QString>> patternSources;
	};

	void readNode(QDataStream& in, Node* parent, QList<PendingArrow>& pending,
	              QList<PendingDerived>& derived, quint16 version, Dialect dialect)
	{
		QString kind, id;
		QPointF pos;
		qreal z = 0;
		in >> kind >> id >> pos >> z;

		QColor fill, border;
		qreal width = 1.5;
		in >> fill >> border >> width;

		bool existsSuch = false;
		bool hypothesis = false;
		if (version >= 2)
			in >> existsSuch >> hypothesis;
		bool componentCommutes = true;
		bool componentRows = false, componentColumns = false;
		if (version >= 12)
			in >> componentCommutes;
		if (version >= 13)
			in >> componentRows >> componentColumns;

		qreal radius = 5.0;
		QString imageFunctor, imageSource, mappingId, imageMapping;
		if (version >= 3)
			in >> radius >> imageFunctor >> imageSource;
		if (version >= 8)
			in >> mappingId >> imageMapping;

		QString pattern;
		QList<QPair<QList<int>, QString>> patternSources;
		if (version >= 5)
			readDerived(in, pattern, patternSources);

		bool deleteMark = false;
		QPointF labelOffset;
		if (version >= 18 || (version >= 15 && dialect == Dialect::Ours))
			in >> deleteMark >> labelOffset;
		QString nodeKey;
		if (version >= 17)
			in >> nodeKey;
		QColor labelColour;
		if (version >= 22)
			in >> labelColour;

		Node* node = nullptr;
		if (kind == "Arrow" || kind == "Functor")
		{
			PendingArrow arrow;
			arrow.parent = parent;
			arrow.kind = kind;
			arrow.id = id;
			arrow.pos = pos;
			arrow.z = z;
			arrow.fill = fill;
			arrow.border = border;
			arrow.width = width;
			arrow.key = nodeKey;
			arrow.labelColour = labelColour;
			arrow.existsSuch = existsSuch;
			arrow.hypothesis = hypothesis;
			arrow.deleteMark = deleteMark;
			arrow.labelOffset = labelOffset;
			arrow.componentCommutes = componentCommutes;
			arrow.componentRows = componentRows;
			arrow.componentColumns = componentColumns;
			arrow.radius = radius;
			arrow.imageFunctor = imageFunctor;
			arrow.imageSource = imageSource;
			arrow.mappingId = mappingId;
			arrow.imageMapping = imageMapping;
			arrow.pattern = pattern;
			arrow.patternSources = patternSources;
			in >> arrow.props >> arrow.domain >> arrow.codomain;
			if (version >= 3)
				in >> arrow.live >> arrow.reflect >> arrow.direction;
			if (version >= 6)
				in >> arrow.imagineBends >> arrow.reflectBends;
			if (version >= 7)
				in >> arrow.labelOffset;
			if (version >= 9)
				in >> arrow.contravariant;
			if (version >= 4)
				in >> arrow.bends;
			if (version >= 16)
				in >> arrow.style;
			pending << arrow;
			// an arrow holds nothing but its own label
			qint32 count = 0;
			in >> count;
			if (in.status() != QDataStream::Ok || count < 0 || count > 100000)
			{
				in.setStatus(QDataStream::ReadCorruptData);
				return;
			}
			for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
				readNode(in, parent, pending, derived, version, dialect);
			return;
		}

		if (kind == "Category")
		{
			QStringList props;
			qint32 nextObject = 0, nextArrow = 0;
			in >> props >> nextObject >> nextArrow;
			bool rows = false, columns = false;
			if (version >= 10)
				in >> rows >> columns;
			QString builtIn;
			bool subcategory = false;
			bool commutes = false;
			qint32 statementKind = 0;
			QString statementName;
			if (version >= 18 || (version >= 15 && dialect == Dialect::Theirs))
			{
				in >> builtIn >> subcategory;
				in >> commutes >> statementKind >> statementName;
			}

			// The class comes first: it is what decides that an object placed
			// in here is an R-module rather than a plain object, so it cannot
			// be put on afterwards.
			Category* category = Category::createBuiltIn(builtIn, parent);
			if (category != nullptr)
				category->setId(id);
			else
				category = new Category(id, parent);
			if (builtIn.isEmpty())
				category->setProperties(props);   // a built-in comes with its own
			category->setNextObjectIndex(nextObject);
			category->setNextArrowIndex(nextArrow);
			category->setRowsExact(rows);
			category->setColumnsExact(columns);
			category->setSubcategory(subcategory);
			category->setCommutes(commutes);
			category->setStatementKind(statementKind);
			category->setStatementName(statementName);
			node = category;
		}
		else if (kind == "Element")
		{
			node = new AtomicElement(id, parent);
		}
		else if (kind == "Module")
		{
			auto* module = new RModule(id, parent);
			if (version >= 20)
			{
				QString ring;
				in >> ring;
				module->setRing(ring);
			}
			node = module;
		}
		else
		{
			node = new Object(id, parent);
		}

		node->setKey(nodeKey);
		node->setPos(pos);
		node->setZValue(z);
		node->setFill(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
		node->setBorder(border.isValid() ? QPen(border, width) : QPen(Qt::NoPen));
		node->setExistsSuch(existsSuch);
		node->setDeleteMark(deleteMark);
		node->setHypothesis(hypothesis);
		node->setLabelOffset(labelOffset);
		node->setLabelColour(labelColour);
		node->setCommutesInComponent(componentCommutes);
		node->setRowsExactInComponent(componentRows);
		node->setColumnsExactInComponent(componentColumns);
		node->setCornerRadius(radius);
		if (!imageFunctor.isEmpty())
		{
			node->setData(MapsElements::ImageFunctorKey, imageFunctor);
			node->setData(MapsElements::ImageSourceKey, imageSource);
		}
		if (!mappingId.isEmpty())
			node->setData(MapsElements::MappingIdKey, mappingId);
		if (!imageMapping.isEmpty())
			node->setData(MapsElements::ImageMappingKey, imageMapping);

		if (!pattern.isEmpty())
			derived.append(PendingDerived{ node, pattern, patternSources });

		qint32 count = 0;
		in >> count;
		// a count no diagram could have means the stream is misread: stop,
		// rather than build a million nodes out of noise
		if (in.status() != QDataStream::Ok || count < 0 || count > 100000)
		{
			in.setStatus(QDataStream::ReadCorruptData);
			return;
		}
		for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
			readNode(in, node, pending, derived, version, dialect);
	}
}

QString SceneFile::extension() { return QStringLiteral("totopos"); }
QString SceneFile::filter() { return QStringLiteral("Totopos diagram (*.totopos)"); }

// ---------------------------------------------------------------- what a file is, by its name

QString SceneFile::kindInfix(int kind)
{
	switch (DiagramScene::StatementKind(kind))
	{
	case DiagramScene::Axiom:      return QStringLiteral("axiom");
	case DiagramScene::Definition: return QStringLiteral("definition");
	case DiagramScene::Theorem:    return QStringLiteral("theorem");
	case DiagramScene::Conjecture: return QStringLiteral("conjecture");
	case DiagramScene::Remark:     return QStringLiteral("remark");
	case DiagramScene::Proof:      return QStringLiteral("proof");
	case DiagramScene::Unstated:   break;
	}
	return QString();   // a free drawing needs no word for it
}

int SceneFile::kindFromFileName(const QString& path)
{
	// "<name>.<kind>.totopos": the piece between the last two dots
	QString name = QFileInfo(path).fileName();
	if (name.endsWith("." + extension(), Qt::CaseInsensitive))
		name.chop(extension().size() + 1);
	const int dot = name.lastIndexOf(QLatin1Char('.'));
	if (dot < 0)
		return DiagramScene::Unstated;
	const QString infix = name.mid(dot + 1).toLower();
	if (infix == QLatin1String("free-draw"))  return DiagramScene::Unstated;
	for (int kind = DiagramScene::Axiom; kind <= DiagramScene::Proof; ++kind)
		if (!kindInfix(kind).isEmpty() && infix == kindInfix(kind))
			return kind;
	return DiagramScene::Unstated;   // some other dot in the name: not a kind
}

QString SceneFile::fileNameFor(const QString& baseName, int kind)
{
	const QString infix = kindInfix(kind);
	return infix.isEmpty()
		? QString("%1.%2").arg(baseName, extension())
		: QString("%1.%2.%3").arg(baseName, infix, extension());
}

QString SceneFile::baseNameOf(const QString& path)
{
	QString name = QFileInfo(path).fileName();
	if (name.endsWith("." + extension(), Qt::CaseInsensitive))
		name.chop(extension().size() + 1);
	const int dot = name.lastIndexOf(QLatin1Char('.'));
	if (dot > 0)
	{
		const QString infix = name.mid(dot + 1).toLower();
		bool isKind = infix == QLatin1String("free-draw");
		for (int kind = DiagramScene::Axiom; !isKind && kind <= DiagramScene::Proof; ++kind)
			isKind = !kindInfix(kind).isEmpty() && infix == kindInfix(kind);
		if (isKind)
			name = name.left(dot);
	}
	return name;
}

SceneFile::Heading SceneFile::peek(const QString& path)
{
	Heading heading;
	// what it is is in the NAME: no need to open anything to learn that
	const int named = kindFromFileName(path);
	heading.kind = named;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return heading;
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_6_0);

	char magic[4] = { 0, 0, 0, 0 };
	in.readRawData(magic, 4);
	if (memcmp(magic, kMagic, 4) != 0)
		return heading;
	quint16 version = 0;
	in >> version;
	if (version < 14)
	{
		heading.valid = true;   // an older file: readable, but only its name says
		return heading;
	}
	quint16 kind = 0;
	in >> kind;
	// The name is a QString: a length, then the text. Read the length by hand
	// and refuse anything absurd - a damaged file must not become an attempt
	// to allocate gigabytes.
	quint32 bytes = 0;
	in >> bytes;
	if (bytes != 0xFFFFFFFFu && bytes > 0)
	{
		if (bytes > 8192 || in.status() != QDataStream::Ok)
			return heading;   // not a heading we believe: treat the file as unreadable
		QByteArray utf16(int(bytes), Qt::Uninitialized);
		if (in.readRawData(utf16.data(), int(bytes)) != int(bytes))
			return heading;
		// QDataStream writes a QString as UTF-16 big-endian, whatever the machine
		heading.name.reserve(int(bytes / 2));
		for (int i = 0; i + 1 < int(bytes); i += 2)
			heading.name.append(QChar(ushort((uchar(utf16.at(i)) << 8) | uchar(utf16.at(i + 1)))));
	}
	// the name has the last word; the kind inside is what it was saved as
	heading.kind = named != DiagramScene::Unstated ? named : int(kind);
	heading.valid = true;
	return heading;
}

static bool loadOneWay(DiagramScene* scene, const QString& path, QString* error, Dialect dialect);

bool SceneFile::load(DiagramScene* scene, const QString& path, QString* error)
{
	// Almost every file is ours, so that is tried first. One written by the
	// other line of work says 15 too but means something else by it: reading
	// it our way runs off the end of a record and fails, and it is then read
	// again the other way. The first failure is the one reported, since it is
	// the one that is nearly always the real complaint.
	QString ourError;
	if (loadOneWay(scene, path, &ourError, Dialect::Ours))
		return true;
	scene->clearDiagram();
	if (loadOneWay(scene, path, nullptr, Dialect::Theirs))
		return true;
	if (error != nullptr) *error = ourError;
	return false;
}

bool SceneFile::save(DiagramScene* scene, const QString& path, QString* error)
{
	if (scene == nullptr || scene->ambientCategory() == nullptr)
	{
		if (error != nullptr) *error = QStringLiteral("There is no diagram to save.");
		return false;
	}

	QByteArray body;
	{
		QDataStream out(&body, QIODevice::WriteOnly);
		out.setVersion(QDataStream::Qt_6_0);
		writeNode(out, scene->ambientCategory());
		out << scene->commutes() << scene->isChasing();
		out << qint32(scene->statementKind()) << scene->statementName();
		// what this KIND of statement needs besides the picture: the file a
		// proof proves, the proofs of a theorem, the word a definition defines
		out << scene->proves() << scene->provedBy() << scene->defines();

		// the history: what was DONE, never where anything was dragged to
		SceneHistory* history = scene->history();
		QList<Memento*> steps;
		for (int i = 0; i < history->position(); ++i)
		{
			Memento* memento = history->mementos().at(i);
			if (!memento->isPureGraphical())
				steps << memento;
		}
		out << qint32(steps.size());
		for (Memento* memento : steps)
			out << quint16(memento->typeTag()) << memento->describe() << memento->payload();

		// version 19: HOW IT WAS BEING LOOKED AT, and where things were put
		// while it was being looked at that way.
		//
		// Last in the body on purpose. Everything before this is read by two
		// dialects of an older format (see the note on kVersion) and neither
		// of them reaches here, so appending disturbs nothing that already
		// works - a file written by this build still opens in the old way up
		// to the point where the old way stops reading.
		//
		// The diagram itself is written exactly as before: there is one
		// diagram, in the succinct notation, and the classical view is only
		// ever a way of looking at it. What is saved here is the LOOKING.
		scene->syncClassicalPositions();
		const QHash<QString, QPointF> placed = scene->classicalPositions();
		out << quint8(scene->isClassical() ? 1 : 0) << qint32(placed.size());
		for (auto it = placed.constBegin(); it != placed.constEnd(); ++it)
			out << it.key() << it.value();

		// version 21: the colour of the paper. Invalid means none was chosen,
		// which is what an older file says by not being able to say anything.
		out << scene->background();
	}

	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly))
	{
		if (error != nullptr) *error = file.errorString();
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_6_0);
	out.writeRawData(kMagic, 4);
	out << kVersion;
	// in the clear, so a library can be listed without unpacking anything
	out << quint16(scene->statementKind()) << scene->statementName();
	out << qCompress(body, 9);   // binary, and small
	if (!file.commit())
	{
		if (error != nullptr) *error = file.errorString();
		return false;
	}
	return true;
}

// One attempt at reading the file, in one dialect.
static bool loadOneWay(DiagramScene* scene, const QString& path, QString* error, Dialect dialect)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		if (error != nullptr) *error = file.errorString();
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_6_0);

	char magic[4] = { 0, 0, 0, 0 };
	in.readRawData(magic, 4);
	if (memcmp(magic, kMagic, 4) != 0)
	{
		if (error != nullptr) *error = QStringLiteral("That is not a Totopos file.");
		return false;
	}
	quint16 version = 0;
	in >> version;
	if (version > kVersion)
	{
		if (error != nullptr) *error = QStringLiteral("That file was written by a newer version.");
		return false;
	}
	if (version >= 14)
	{
		quint16 kind = 0;
		QString name;
		in >> kind >> name;   // read again from the body below; this copy is for listing
	}
	QByteArray compressed;
	in >> compressed;
	const QByteArray body = qUncompress(compressed);
	if (body.isEmpty())
	{
		if (error != nullptr) *error = QStringLiteral("The file is empty or damaged.");
		return false;
	}

	QDataStream data(body);
	data.setVersion(QDataStream::Qt_6_0);

	// the ambient category first: it decides what the canvas is
	QString kind, id;
	QPointF pos;
	qreal z = 0;
	data >> kind >> id >> pos >> z;
	QColor fill, border;
	qreal width = 1.5;
	data >> fill >> border >> width;
	bool ambientExistsSuch = false, ambientHypothesis = false;
	if (version >= 2)
		data >> ambientExistsSuch >> ambientHypothesis;
	bool ambientComponentCommutes = true;
	bool ambientRows = false, ambientColumns = false;
	if (version >= 12)
		data >> ambientComponentCommutes;
	if (version >= 13)
		data >> ambientRows >> ambientColumns;
	qreal ambientRadius = 5.0;
	QString ambientImageFunctor, ambientImageSource;
	if (version >= 3)
		data >> ambientRadius >> ambientImageFunctor >> ambientImageSource;
	// writeNode writes these for EVERY node, the ambient one included; reading
	// the ambient by hand and leaving them out put every later field two
	// strings early, and the child count that came out of that was garbage
	QString ambientMappingId, ambientImageMapping;
	if (version >= 8)
		data >> ambientMappingId >> ambientImageMapping;
	QString ambientPattern;
	QList<QPair<QList<int>, QString>> ambientPatternSources;
	if (version >= 5)
		readDerived(data, ambientPattern, ambientPatternSources);
	// writeNode writes these for EVERY node, the ambient one included
	bool ambientDeleteMark = false;
	QPointF ambientLabelOffset;
	if (version >= 18 || (version >= 15 && dialect == Dialect::Ours))
		data >> ambientDeleteMark >> ambientLabelOffset;
	// ...and its key, which writeNode also writes for every node. Leaving this
	// out put every later field one string early, and the props list that came
	// out of that was garbage - which is what "the file does not add up" is.
	QString ambientKey;
	if (version >= 17)
		data >> ambientKey;
	QStringList props;
	qint32 nextObject = 0, nextArrow = 0;
	data >> props >> nextObject >> nextArrow;
	bool ambientRowsExact = false, ambientColumnsExact = false;
	if (version >= 10)
		data >> ambientRowsExact >> ambientColumnsExact;
	// Which category the canvas IS. The whole block has to be read here, and
	// in this order, whether or not it is used: everything after it in the
	// stream depends on the reading having got this far.
	QString ambientBuiltIn;
	bool ambientSubcategory = false;
	bool ambientClaimsCommutes = false;
	qint32 ambientStatementKind = 0;
	QString ambientStatementName;
	if (version >= 18 || (version >= 15 && dialect == Dialect::Theirs))
	{
		data >> ambientBuiltIn >> ambientSubcategory;
		data >> ambientClaimsCommutes >> ambientStatementKind >> ambientStatementName;
	}

	scene->clearDiagram();
	// the class is what makes the canvas R-Mod, rather than something merely
	// CALLED R-Mod; whatever the file has named it goes on afterwards
	scene->setAmbientCategory(ambientBuiltIn.isEmpty() ? id : ambientBuiltIn);
	Category* ambient = scene->ambientCategory();
	if (ambient == nullptr)
	{
		if (error != nullptr) *error = QStringLiteral("The file names a category that cannot be made.");
		return false;
	}
	ambient->setId(id);
	ambient->setKey(ambientKey);
	if (ambientBuiltIn.isEmpty())
		ambient->setProperties(props);   // a built-in brings its own structure
	ambient->setNextObjectIndex(nextObject);
	ambient->setNextArrowIndex(nextArrow);
	ambient->setRowsExact(ambientRowsExact);
	ambient->setColumnsExact(ambientColumnsExact);
	// what the canvas claims is the SCENE's, and is read again at the end of
	// the body; these two are the same numbers, and are left to that reading
	Q_UNUSED(ambientClaimsCommutes);
	Q_UNUSED(ambientStatementKind);
	Q_UNUSED(ambientStatementName);
	Q_UNUSED(ambientSubcategory);
	if (fill.isValid()) ambient->setFill(QBrush(fill));
	if (border.isValid()) ambient->setBorder(QPen(border, width));
	ambient->setLabelOffset(ambientLabelOffset);

	QList<PendingArrow> pending;
	QList<PendingDerived> derived;
	qint32 count = 0;
	data >> count;
	if (data.status() != QDataStream::Ok || count < 0 || count > 100000)
	{
		if (error != nullptr) *error = QStringLiteral("The file does not add up: its contents cannot be read.");
		return false;
	}
	for (qint32 i = 0; i < count && data.status() == QDataStream::Ok; ++i)
		readNode(data, ambient, pending, derived, version, dialect);
	if (data.status() != QDataStream::Ok)
	{
		if (error != nullptr) *error = QStringLiteral("The file is damaged part way through.");
		return false;
	}

	// now every object exists, so the arrows can find their ends
	for (const PendingArrow& p : pending)
	{
		Node* from = Node::fromPath(ambient, p.domain);
		Node* to = Node::fromPath(ambient, p.codomain);
		if (from == nullptr || to == nullptr)
			continue;   // an end that is no longer there: the arrow is dropped
		// stamped first: MapsElements reads this in its constructor
		Arrow* arrow = nullptr;
		auto* domCat = dynamic_cast<Category*>(from);
		auto* codCat = dynamic_cast<Category*>(to);
		if (p.kind == "Functor" && domCat != nullptr && codCat != nullptr)
			arrow = new Functor(p.id, domCat, codCat, p.parent);
		else
			arrow = new Arrow(p.id, from, to, p.parent);
		if (!p.mappingId.isEmpty())
			arrow->setData(MapsElements::MappingIdKey, p.mappingId);
		if (!p.imageMapping.isEmpty())
			arrow->setData(MapsElements::ImageMappingKey, p.imageMapping);
		arrow->setZValue(p.z);
		arrow->setProperties(p.props);
		arrow->addProperty(MapsElements::Key());   // every arrow can carry elements over
		if (p.fill.isValid()) arrow->setFill(QBrush(p.fill));
		if (p.border.isValid())
			arrow->setBorder(QPen(p.border, p.width));   // what was saved for it
		arrow->setExistsSuch(p.existsSuch);
		arrow->setDeleteMark(p.deleteMark);
		arrow->setHypothesis(p.hypothesis);
		arrow->setCommutesInComponent(p.componentCommutes);
		arrow->setRowsExactInComponent(p.componentRows);
		arrow->setColumnsExactInComponent(p.componentColumns);
		arrow->setCornerRadius(p.radius);
		arrow->setBends(p.bends);
		arrow->setStyle(static_cast<Arrow::Style>(p.style));
		arrow->setKey(p.key);
		arrow->setLabelOffset(p.labelOffset);
		arrow->setLabelColour(p.labelColour);
		if (!p.pattern.isEmpty())
			derived.append(PendingDerived{ arrow, p.pattern, p.patternSources });
		if (!p.imageFunctor.isEmpty())
		{
			arrow->setData(MapsElements::ImageFunctorKey, p.imageFunctor);
			arrow->setData(MapsElements::ImageSourceKey, p.imageSource);
		}

		// how it maps, if it maps at all. Live goes last: switching it on is
		// what brings the image up to date, and by now everything exists.
		if (auto* maps = dynamic_cast<MapsElements*>(arrow->prop(MapsElements::Key())))
		{
			// first: it decides which images this mapping recognises as its own
			maps->setMappingId(p.mappingId);
			maps->setContravariant(p.contravariant != 0);
			// the first byte is whether the image is live at all; the next
			// one carried positions across, and now carries all of the
			// geometry (an older file agreed with itself about the four)
			maps->setLive(p.live != 0);
			maps->setMirrorsGeometry(p.reflect != 0);
		}
	}

	// now that every object and every arrow is there, the labels built out of
	// other labels can find what they are built from
	for (const PendingDerived& entry : derived)
	{
		QList<Node*> sources;
		for (const auto& ref : entry.sources)
			if (Node* source = resolveRef(ambient, ref.first, ref.second))
				sources << source;
		if (entry.node != nullptr && !sources.isEmpty())
			entry.node->setDerivedLabel(entry.pattern, sources);
	}

	// what the diagram claims, and whether a chase was under way
	if (version >= 2)
	{
		bool commutes = false, chasing = false;
		data >> commutes >> chasing;
		scene->setCommutes(commutes);
		scene->setChasing(chasing);
		if (version >= 11)
		{
			qint32 kind = 0;
			QString name;
			data >> kind >> name;
			scene->setStatementName(name);
			scene->setStatementKind(DiagramScene::StatementKind(kind));
		}
		if (version >= 18 || (version >= 15 && dialect == Dialect::Ours))
		{
			QString proves, defines;
			QStringList provedBy;
			data >> proves >> provedBy >> defines;
			scene->setProves(proves);
			scene->setProvedBy(provedBy);
			scene->setDefines(defines);
		}
	}

	// the history as it was: steps that can be read, not walked back
	qint32 steps = 0;
	data >> steps;
	for (qint32 i = 0; i < steps; ++i)
	{
		quint16 tag = 0;
		QString description;
		QByteArray payload;
		data >> tag >> description >> payload;
		scene->history()->record(new SealedStep(description, tag, payload, false));
	}

	// version 19: the layout the classical view was left in, and whether that
	// is the view this diagram was last being read in. The positions go on
	// first, so that switching over below finds them already there.
	bool wasClassical = false;
	if (version >= 19)
	{
		quint8 classical = 0;
		qint32 placedCount = 0;
		data >> classical >> placedCount;
		QHash<QString, QPointF> placed;
		for (qint32 i = 0; i < placedCount && data.status() == QDataStream::Ok; ++i)
		{
			QString key;
			QPointF at;
			data >> key >> at;
			placed.insert(key, at);
		}
		if (data.status() == QDataStream::Ok)
		{
			scene->setClassicalPositions(placed);
			wasClassical = classical != 0;
		}
	}

	if (version >= 21)
	{
		QColor paper;
		data >> paper;
		if (data.status() == QDataStream::Ok)
			scene->setBackground(paper);
	}

	// The NAME says what this is. snake-lemma.theorem.totopos is a theorem
	// whatever was last ticked in the panel before it was saved, so renaming a
	// file is how a conjecture becomes a theorem.
	// qualified: this is a free function now, not a member of SceneFile
	if (const int named = SceneFile::kindFromFileName(path); named != DiagramScene::Unstated)
		scene->setStatementKind(DiagramScene::StatementKind(named));

	ambient->refreshDepthAppearance();
	ambient->refreshFrame();
	// last of all: the classical view reads the finished diagram, so it can
	// only be built once there is one
	if (wasClassical)
		scene->setNotation(DiagramScene::Notation::Classical);
	return true;
}

// ---------------------------------------------------------------- fragments

namespace
{
	const char kFragMagic[4] = { 'D', 'D', 'F', 'R' };
	const quint16 kFragVersion = 2;   // 2: what kind of arrow it is

	// One record of a fragment. Flat, in pre-order, so a node's parent is
	// always written before it and an index is all it takes to name either.
	struct FragRecord
	{
		qint32 parent = -1;
		QString kind;
		QString id;
		QPointF pos;
		qreal z = 0;
		QColor fill, border;
		qreal width = 1.5;
		bool existsSuch = false, deleteMark = false, hypothesis = false;
		bool componentCommutes = true, rowsExact = false, columnsExact = false;
		qreal radius = 5.0;
		QPointF labelOffset;
		QString labelPattern;
		QList<qint32> labelSources;
		// a category
		QStringList catProps;
		bool catRowsExact = false, catColumnsExact = false;
		// an arrow
		QStringList arrowProps;
		qint32 domain = -1, codomain = -1;
		QList<QPointF> bends;
		quint8 arrowStyle = 0;   // Arrow::Style::Plain
	};

	// is `item` this node, or inside it?
	bool isWithin(const QGraphicsItem* item, const QGraphicsItem* ancestor)
	{
		for (const QGraphicsItem* p = item; p != nullptr; p = p->parentItem())
			if (p == ancestor)
				return true;
		return false;
	}

	// everything under `node`, node first, in the order a fragment numbers them
	void collectFragment(Node* node, QList<Node*>& out)
	{
		out << node;
		for (QGraphicsItem* child : node->childItems())
			if (auto* kid = dynamic_cast<Node*>(child))
				collectFragment(kid, out);
	}
}

QString SceneFile::fragmentMimeType()
{
	return QStringLiteral("application/x-diagramdetective-fragment");
}

QByteArray SceneFile::copyFragment(const QList<Node*>& nodes)
{
	// The tops of the selection: a node whose ancestor is also selected comes
	// with that ancestor, and would otherwise be copied twice.
	QList<Node*> tops;
	for (Node* node : nodes)
	{
		if (node == nullptr || tops.contains(node))
			continue;
		bool covered = false;
		for (Node* other : nodes)
			if (other != nullptr && other != node && isWithin(node, other))
			{
				covered = true;
				break;
			}
		if (!covered)
			tops << node;
	}
	if (tops.isEmpty())
		return QByteArray();

	// every node of the fragment, numbered
	QList<Node*> all;
	for (Node* top : tops)
		collectFragment(top, all);
	QHash<Node*, qint32> indexOf;
	for (qint32 i = 0; i < all.size(); ++i)
		indexOf.insert(all.at(i), i);

	// where the fragment sits, so it can be put down somewhere else with its
	// shape kept: the top-left of what is being carried
	QRectF bounds;
	for (Node* top : tops)
		bounds |= top->sceneBoundingRect();
	const QPointF origin = bounds.topLeft();

	QString ambient;
	if (DiagramScene* diagram = Node::diagramOf(tops.first()); diagram != nullptr && diagram->ambientCategory() != nullptr)
		ambient = diagram->ambientCategory()->id();

	QByteArray body;
	{
		QDataStream out(&body, QIODevice::WriteOnly);
		out.setVersion(QDataStream::Qt_6_0);
		out << ambient << qint32(all.size());

		for (Node* node : all)
		{
			auto* arrow = dynamic_cast<Arrow*>(node);
			const qint32 parent = indexOf.value(dynamic_cast<Node*>(node->parentItem()), -1);
			out << parent << kindOf(node) << node->id();
			// a top-level node is placed by where it was on the canvas; a child
			// keeps the place it has inside its parent
			out << (parent < 0 ? node->scenePos() - origin : node->pos()) << qreal(node->zValue());
			writeStyle(out, node);
			out << node->existsSuch() << node->markedForDeletion() << node->isHypothesis();
			out << node->commutesInComponent() << node->rowsExactInComponent() << node->columnsExactInComponent();
			out << qreal(node->cornerRadius()) << node->labelOffset();

			// a label built out of other labels: it can only be carried over
			// when every label it is built from is coming along too
			QList<qint32> sources;
			for (Node* source : node->labelSources())
				if (indexOf.contains(source))
					sources << indexOf.value(source);
			const bool whole = sources.size() == node->labelSources().size();
			out << (whole ? node->labelPattern() : QString()) << sources;

			if (arrow != nullptr)
				out << arrow->properties()
				    << indexOf.value(arrow->domain(), -1)
				    << indexOf.value(arrow->codomain(), -1)
				    << arrow->bends()
				    << quint8(arrow->style());
			else if (auto* category = dynamic_cast<Category*>(node))
				out << category->properties() << category->rowsExact() << category->columnsExact();
		}
	}

	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out.writeRawData(kFragMagic, 4);
	out << kFragVersion;
	out << qCompress(body, 6);
	return bytes;
}

QList<Node*> SceneFile::pasteFragment(const QByteArray& payload, Category* into,
                                      const QPointF& atScenePos, int* dropped)
{
	QList<Node*> made;
	if (dropped != nullptr)
		*dropped = 0;
	if (into == nullptr || payload.isEmpty())
		return made;

	QDataStream head(payload);
	head.setVersion(QDataStream::Qt_6_0);
	char magic[4] = { 0, 0, 0, 0 };
	head.readRawData(magic, 4);
	if (memcmp(magic, kFragMagic, 4) != 0)
		return made;
	quint16 version = 0;
	head >> version;
	if (version > kFragVersion)
		return made;
	QByteArray compressed;
	head >> compressed;
	const QByteArray body = qUncompress(compressed);
	if (body.isEmpty())
		return made;

	QDataStream in(body);
	in.setVersion(QDataStream::Qt_6_0);
	QString ambient;
	qint32 count = 0;
	in >> ambient >> count;
	if (in.status() != QDataStream::Ok || count < 0 || count > 100000)
		return made;

	QList<FragRecord> records;
	records.reserve(count);
	for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
	{
		FragRecord r;
		in >> r.parent >> r.kind >> r.id >> r.pos >> r.z;
		in >> r.fill >> r.border >> r.width;
		in >> r.existsSuch >> r.deleteMark >> r.hypothesis;
		in >> r.componentCommutes >> r.rowsExact >> r.columnsExact;
		in >> r.radius >> r.labelOffset;
		in >> r.labelPattern >> r.labelSources;
		if (r.kind == QLatin1String("Arrow") || r.kind == QLatin1String("Functor"))
		{
			in >> r.arrowProps >> r.domain >> r.codomain >> r.bends;
			if (version >= 2)
				in >> r.arrowStyle;
		}
		else if (r.kind == QLatin1String("Category"))
			in >> r.catProps >> r.catRowsExact >> r.catColumnsExact;
		// a parent is always written before its child: anything else is not a
		// fragment of ours, however well it started
		if (r.parent >= i)
			return made;
		records.append(r);
	}
	if (in.status() != QDataStream::Ok || records.size() != count)
		return made;

	// A name means one thing in a diagram, so anything already spoken for
	// takes a prime, as many as it needs.
	auto freshen = [into](const QString& wanted) {
		if (wanted.isEmpty() || !into->nameInUse(wanted))
			return wanted;
		QString name = wanted;
		for (int guard = 0; guard < 26 && into->nameInUse(name); ++guard)
			name += QChar(0x2032);
		return name;
	};

	QList<Node*> built;
	for (int i = 0; i < records.size(); ++i)
		built.append(nullptr);

	// the objects first, parents before children
	for (int i = 0; i < records.size(); ++i)
	{
		const FragRecord& r = records.at(i);
		if (r.kind == QLatin1String("Arrow") || r.kind == QLatin1String("Functor"))
			continue;
		Category* home = into;
		if (r.parent >= 0)
		{
			home = dynamic_cast<Category*>(built.at(r.parent));
			if (home == nullptr)
				continue;   // its parent did not come: nor does it
		}
		// a top-level node is placed where the fragment was put down; a child
		// keeps the place it had inside its parent
		const QPointF scenePos = r.parent < 0 ? atScenePos + r.pos : home->mapToScene(r.pos);
		Object* object = home->createObject(freshen(r.id), scenePos);
		if (object == nullptr)
			continue;
		object->setZValue(r.z);
		object->setFill(r.fill.isValid() ? QBrush(r.fill) : QBrush(Qt::NoBrush));
		object->setBorder(r.border.isValid() ? QPen(r.border, r.width) : QPen(Qt::NoPen));
		object->setExistsSuch(r.existsSuch);
		object->setDeleteMark(r.deleteMark);
		object->setHypothesis(r.hypothesis);
		object->setCommutesInComponent(r.componentCommutes);
		object->setRowsExactInComponent(r.rowsExact);
		object->setColumnsExactInComponent(r.columnsExact);
		object->setCornerRadius(r.radius);
		object->setLabelOffset(r.labelOffset);
		if (auto* category = dynamic_cast<Category*>(object))
		{
			category->setProperties(r.catProps);
			category->setRowsExact(r.catRowsExact);
			category->setColumnsExact(r.catColumnsExact);
		}
		built[i] = object;
		made << object;
	}

	// then the arrows: neither end can be found before it is there
	for (int i = 0; i < records.size(); ++i)
	{
		const FragRecord& r = records.at(i);
		if (r.kind != QLatin1String("Arrow") && r.kind != QLatin1String("Functor"))
			continue;
		Node* from = r.domain >= 0 && r.domain < built.size() ? built.at(r.domain) : nullptr;
		Node* to = r.codomain >= 0 && r.codomain < built.size() ? built.at(r.codomain) : nullptr;
		Category* home = r.parent < 0 ? into : dynamic_cast<Category*>(built.at(r.parent));
		if (from == nullptr || to == nullptr || home == nullptr)
		{
			// an end that did not come along: this arrow joins nothing
			if (dropped != nullptr)
				++*dropped;
			continue;
		}
		Arrow* arrow = home->createArrow(freshen(r.id), from, to);
		if (arrow == nullptr)
			continue;
		arrow->setZValue(r.z);
		arrow->setProperties(r.arrowProps);
		arrow->addProperty(MapsElements::Key());   // every arrow can carry elements over
		if (r.fill.isValid())
			arrow->setFill(QBrush(r.fill));
		if (r.border.isValid())
			arrow->setBorder(QPen(r.border, r.width));
		arrow->setExistsSuch(r.existsSuch);
		arrow->setDeleteMark(r.deleteMark);
		arrow->setHypothesis(r.hypothesis);
		arrow->setCommutesInComponent(r.componentCommutes);
		arrow->setRowsExactInComponent(r.rowsExact);
		arrow->setColumnsExactInComponent(r.columnsExact);
		arrow->setBends(r.bends);
		arrow->setStyle(static_cast<Arrow::Style>(r.arrowStyle));
		arrow->setLabelOffset(r.labelOffset);
		built[i] = arrow;
		made << arrow;
	}

	// now everything is there, a label built out of other labels can find them
	for (int i = 0; i < records.size(); ++i)
	{
		const FragRecord& r = records.at(i);
		if (r.labelPattern.isEmpty() || built.at(i) == nullptr)
			continue;
		QList<Node*> sources;
		for (qint32 index : r.labelSources)
			if (index >= 0 && index < built.size() && built.at(index) != nullptr)
				sources << built.at(index);
		if (!sources.isEmpty() && sources.size() == r.labelSources.size())
			built.at(i)->setDerivedLabel(r.labelPattern, sources);
	}

	into->refreshDepthAppearance();
	into->refreshFrame();
	return made;
}
