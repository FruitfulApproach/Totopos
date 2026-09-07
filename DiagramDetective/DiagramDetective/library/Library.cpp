#include "Library.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "../DiagramScene.h"
#include "../Category.h"
#include "../Arrow.h"
#include "../history/SceneHistory.h"
#include "../history/Mementos.h"
#include "../Object.h"
#include "../io/SceneFile.h"

namespace
{
	// the places a library could reasonably be, nearest first
	QStringList candidates()
	{
		const QString app = QCoreApplication::applicationDirPath();
		return {
			app + "/library",
			app + "/../library",
			app + "/../../library",          // x64/Debug -> the project folder
			app + "/../../../library",
		};
	}
}

QString Library::root()
{
	for (const QString& path : candidates())
		if (QFileInfo(path).isDir())
			return QDir(path).absolutePath();
	return QString();
}

QString Library::ensureRoot()
{
	const QString existing = root();
	if (!existing.isEmpty())
		return existing;
	// the project folder, two up from x64/Debug, is where the sources are
	const QString wanted = QDir(QCoreApplication::applicationDirPath() + "/../../library").absolutePath();
	QDir().mkpath(wanted);
	return QDir(wanted).absolutePath();
}

int Library::seedExamples(QString* error)
{
	const QString base = ensureRoot();
	if (base.isEmpty())
	{
		if (error != nullptr) *error = QStringLiteral("There is nowhere to put the library.");
		return 0;
	}

	int written = 0;
	const QString snake = base + "/hom-alg/lemma/snake-lemma.totopos";
	if (!QFileInfo::exists(snake))
	{
		QDir().mkpath(QFileInfo(snake).absolutePath());
		if (writeSnakeLemma(snake, error))
			++written;
	}
	return written;
}

namespace
{
	// One object of the diagram, placed where it belongs and named.
	Object* at(Category* home, const QString& name, qreal x, qreal y)
	{
		return home->createObject(name, home->mapToScene(QPointF(x, y)));
	}

	Arrow* joins(Category* home, const QString& name, Node* from, Node* to)
	{
		return home->createArrow(name, from, to);
	}
}

bool Library::writeSnakeLemma(const QString& path, QString* error)
{
	DiagramScene scene;
	scene.setAmbientCategory("R-Mod");
	Category* home = scene.ambientCategory();
	if (home == nullptr)
	{
		if (error != nullptr) *error = QStringLiteral("R-Mod could not be made.");
		return false;
	}

	// ---- the setup ------------------------------------------------------
	// Two exact rows and three maps between them. The zeros are told apart:
	// one name means one thing, and these are two different zero modules.
	Object* A = at(home, "A", -300, -150);
	Object* B = at(home, "B", -100, -150);
	Object* C = at(home, "C", 100, -150);
	Object* zeroRight = at(home, "0'", 300, -150);
	Arrow* f = joins(home, "f", A, B);
	Arrow* g = joins(home, "g", B, C);
	Arrow* epi = joins(home, "e", C, zeroRight);
	scene.recordCreation("Let A -f-> B -g-> C -> 0 be exact.",
		{ A, B, C, zeroRight, f, g, epi });

	Object* zeroLeft = at(home, "0", -500, 0);
	Object* A2 = at(home, "A'", -300, 0);
	Object* B2 = at(home, "B'", -100, 0);
	Object* C2 = at(home, "C'", 100, 0);
	Arrow* mono = joins(home, "m", zeroLeft, A2);
	Arrow* f2 = joins(home, "f'", A2, B2);
	Arrow* g2 = joins(home, "g'", B2, C2);
	scene.recordCreation("Let 0 -> A' -f'-> B' -g'-> C' be exact.",
		{ zeroLeft, A2, B2, C2, mono, f2, g2 });

	Arrow* a = joins(home, "a", A, A2);
	Arrow* b = joins(home, "b", B, B2);
	Arrow* c = joins(home, "c", C, C2);
	scene.recordCreation("Let a, b, c make the two squares commute.", { a, b, c });

	// ---- the kernels and cokernels --------------------------------------
	Object* kerA = at(home, "Ker a", -300, -350);
	Object* kerB = at(home, "Ker b", -100, -350);
	Object* kerC = at(home, "Ker c", 100, -350);
	Arrow* iA = joins(home, "i", kerA, A);
	Arrow* iB = joins(home, "j", kerB, B);
	Arrow* iC = joins(home, "k", kerC, C);
	kerA->setDerivedLabel("Ker %1", { a });
	kerB->setDerivedLabel("Ker %1", { b });
	kerC->setDerivedLabel("Ker %1", { c });
	scene.recordCreation("Take the kernels of a, b and c, with their inclusions.",
		{ kerA, kerB, kerC, iA, iB, iC });

	Arrow* kerF = joins(home, "f|", kerA, kerB);
	Arrow* kerG = joins(home, "g|", kerB, kerC);
	scene.recordCreation("f and g carry Ker a into Ker b and Ker b into Ker c: restrict them.",
		{ kerF, kerG });

	Object* cokA = at(home, "Coker a", -300, 200);
	Object* cokB = at(home, "Coker b", -100, 200);
	Object* cokC = at(home, "Coker c", 100, 200);
	Arrow* pA = joins(home, "p", A2, cokA);
	Arrow* pB = joins(home, "q", B2, cokB);
	Arrow* pC = joins(home, "r", C2, cokC);
	cokA->setDerivedLabel("Coker %1", { a });
	cokB->setDerivedLabel("Coker %1", { b });
	cokC->setDerivedLabel("Coker %1", { c });
	scene.recordCreation("Take the cokernels of a, b and c, with their projections.",
		{ cokA, cokB, cokC, pA, pB, pC });

	Arrow* cokF = joins(home, "f''", cokA, cokB);
	Arrow* cokG = joins(home, "g''", cokB, cokC);
	scene.recordCreation("f' and g' descend to the cokernels.", { cokF, cokG });

	// ---- the chase ------------------------------------------------------
	scene.recordNote("Chase. Take x in Ker c, and read it as an element of C.");
	scene.recordNote("g is onto, so there is some y in B with g(y) = x.");
	scene.recordNote("Then g'(b(y)) = c(g(y)) = c(x) = 0, because the right square commutes and x is in Ker c.");
	scene.recordNote("So b(y) lies in Ker g', which is Im f' because the bottom row is exact.");
	scene.recordNote("Choose z in A' with f'(z) = b(y). f' is injective, so z is determined by y.");
	scene.recordNote("A different choice of y changes z by something in Im a, so its class in Coker a does not move.");

	Arrow* delta = joins(home, "d", kerC, cokA);
	delta->setExistsSuch(true);   // what the lemma produces: drawn dotted
	delta->addBend(QPointF(320, -75));
	scene.recordCreation("That class is d(x). It is the connecting map d : Ker c -> Coker a.", { delta });

	scene.recordNote("d is well defined and R-linear by the two steps above.");
	scene.recordNote("Ker a -> Ker b -> Ker c -d-> Coker a -> Coker b -> Coker c is exact: "
	                 "at each place, one inclusion follows from the choices above and the other from a chase "
	                 "back the way we came.");

	// ---- what it all is -------------------------------------------------
	scene.setCommutes(true);
	scene.setStatementName(QStringLiteral("Snake lemma"));
	scene.setStatementKind(DiagramScene::Theorem);

	// the rows are exact, and everything here is one connected piece
	for (const DiagramScene::Component& piece : scene.components())
		for (Node* node : piece.objects)
		{
			node->setRowsExactInComponent(true);
			node->setCommutesInComponent(true);
		}

	return SceneFile::save(&scene, path, error);
}
