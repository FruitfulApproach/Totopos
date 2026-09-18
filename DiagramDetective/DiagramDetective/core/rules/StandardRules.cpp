#include "core/rules/Library.h"
#include "core/rules/RuleBuilder.h"

#include <QFileInfo>
#include <functional>

#include "art/Category.h"
#include "art/Object.h"
#include "art/Arrow.h"
#include "core/io/SceneFile.h"

// The rules that ship with the program.
//
// Each is a picture, written by the same code that reads it back. The reading
// is always the same: what is drawn SOLID is quantified over, what is DOTTED is
// what the rule says there then is, and what is CROSSED OUT IN RED is what it
// takes away. A rule with neither dotted nor crossed-out parts says only that
// its scene is there, and citing it is a step of a proof.
//
// A rule applies in the category it is drawn in, found by name, so the ones
// that hold in any category are written once and put in every category's
// folder. What a file IS - axiom, definition, theorem - is in its name.

namespace
{
	// ------------------------------------------------------------ in any category

	// Every object has an identity on it. The loop is named after the object it
	// sits on - 1_M, not just 1 - because two identities on two objects are two
	// different arrows, and one name here means one thing.
	bool identity(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Identity", DiagramScene::Axiom);
		Object* A = rule.obj("A", 0, 0);
		Arrow* one = rule.someArr("1_A", A, A);
		rule.bend(one, -70, -90);
		rule.bend(one, 70, -90);
		rule.derived(one, "1_%1", { A });
		rule.note("Every object carries an identity arrow on it, written 1_M.");
		return rule.write(cat + "/identity.axiom.totopos", error);
	}

	// Two arrows that meet compose.
	bool composite(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Composite", DiagramScene::Axiom);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 0, 0);
		Object* C = rule.obj("C", 200, 0);
		Arrow* f = rule.arr("f", A, B);
		Arrow* g = rule.arr("g", B, C);
		Arrow* gf = rule.someArr("gf", A, C);
		rule.bend(gf, 0, 140);
		// the name follows whatever the two arrows turn out to be called
		rule.derived(gf, "%1%2", { g, f });
		rule.commutes(true);
		rule.note("Arrows that meet compose, and the triangle commutes.");
		return rule.write(cat + "/composite.axiom.totopos", error);
	}

	// The same, the other way about: the pair is REPLACED by its composite.
	// Sound because taking arrows out of a commuting diagram leaves one that
	// commutes still - it says less, not something else.
	bool composePair(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Compose the pair", DiagramScene::Theorem);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 0, 0);
		Object* C = rule.obj("C", 200, 0);
		Arrow* f = rule.arr("f", A, B);
		Arrow* g = rule.arr("g", B, C);
		Arrow* gf = rule.someArr("gf", A, C);
		rule.bend(gf, 0, 140);
		rule.derived(gf, "%1%2", { g, f });
		rule.gone(f);
		rule.gone(g);
		rule.commutes(true);
		rule.note("The two arrows are put away and their composite drawn in their place. "
		          "What is left is what the diagram still says about A and C.");
		return rule.write(cat + "/compose-pair.theorem.totopos", error);
	}

	// A loop on an object can always be put away: a diagram that commutes goes
	// on commuting when an arrow is taken out of it. It says LESS, not
	// something else - which is what a red cross means here.
	bool eraseLoop(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Erase a loop", DiagramScene::Theorem);
		Object* A = rule.obj("A", 0, 0);
		Arrow* loop = rule.arr("e", A, A);
		rule.bend(loop, -70, -90);
		rule.bend(loop, 70, -90);
		rule.gone(loop);
		rule.commutes(true);
		rule.note("An arrow from an object to itself is put away. What is left still commutes - "
		          "it simply says less. Use it to clear away an identity that has served its turn.");
		return rule.write(cat + "/erase-loop.theorem.totopos", error);
	}

	// ------------------------------------------------------------ products

	bool product(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Product", DiagramScene::Definition);
		Object* A = rule.obj("A", -200, 150);
		Object* B = rule.obj("B", 200, 150);
		Object* P = rule.some("A × B", 0, -100);
		rule.derived(P, "%1 × %2", { A, B });
		rule.someArr("p₁", P, A);
		rule.someArr("p₂", P, B);
		rule.defines("product");
		rule.note("The product of A and B, with its two projections. Any pair of arrows into "
		          "A and B factors through it, and in only one way.");
		return rule.write(cat + "/product.definition.totopos", error);
	}

	bool coproduct(const QString& cat, const QString& sign, QString* error)
	{
		RuleBuilder rule(cat, "Coproduct", DiagramScene::Definition);
		Object* A = rule.obj("A", -200, -150);
		Object* B = rule.obj("B", 200, -150);
		Object* S = rule.some(QString("A %1 B").arg(sign), 0, 100);
		rule.derived(S, QString("%1 %2 %3").arg("%1", sign, "%2"), { A, B });
		rule.someArr("i₁", A, S);
		rule.someArr("i₂", B, S);
		rule.defines("coproduct");
		rule.note("The coproduct of A and B, with its two injections. Any pair of arrows out of "
		          "A and B factors through it, and in only one way.");
		return rule.write(cat + "/coproduct.definition.totopos", error);
	}

	// ------------------------------------------------------------ additive and abelian

	bool zeroObject(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Zero object", DiagramScene::Axiom);
		rule.some("0", 0, 0);
		rule.note("There is a zero object: one arrow into it from anywhere, and one out of it "
		          "to anywhere.");
		return rule.write(cat + "/zero-object.axiom.totopos", error);
	}

	bool zeroMap(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Zero map", DiagramScene::Axiom);
		Object* A = rule.obj("A", -150, 0);
		Object* B = rule.obj("B", 150, 0);
		rule.someArr("0", A, B);
		rule.note("Between any two objects there is the zero arrow, which factors through the "
		          "zero object.");
		return rule.write(cat + "/zero-map.axiom.totopos", error);
	}

	bool kernel(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Kernel", DiagramScene::Definition);
		Object* A = rule.obj("A", -150, 0);
		Object* B = rule.obj("B", 150, 0);
		Arrow* f = rule.arr("f", A, B);
		Object* K = rule.some("Ker f", -150, -200);
		rule.derived(K, "Ker %1", { f });
		rule.someArr("k", K, A);
		rule.defines("kernel");
		rule.note("Every arrow has a kernel: the largest thing that f sends to zero, with its "
		          "inclusion into A. fk = 0, and anything else killed by f factors through k.");
		return rule.write(cat + "/kernel.definition.totopos", error);
	}

	bool cokernel(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Cokernel", DiagramScene::Definition);
		Object* A = rule.obj("A", -150, 0);
		Object* B = rule.obj("B", 150, 0);
		Arrow* f = rule.arr("f", A, B);
		Object* C = rule.some("Coker f", 150, 200);
		rule.derived(C, "Coker %1", { f });
		rule.someArr("c", B, C);
		rule.defines("cokernel");
		rule.note("Every arrow has a cokernel: B with the image of f collapsed to zero, and the "
		          "projection onto it. cf = 0, and anything else killing f factors through c.");
		return rule.write(cat + "/cokernel.definition.totopos", error);
	}

	bool image(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Image", DiagramScene::Definition);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 200, 0);
		Arrow* f = rule.arr("f", A, B);
		Object* I = rule.some("Im f", 0, 180);
		rule.derived(I, "Im %1", { f });
		rule.someArr("e", A, I);
		rule.someArr("m", I, B);
		rule.commutes(true);
		rule.defines("image");
		rule.note("Every arrow factors through its image: f = me, with e onto and m into.");
		return rule.write(cat + "/image.definition.totopos", error);
	}

	// The same factorisation, used as a MOVE: f is put away and the two halves
	// it factors into are drawn in its place.
	bool imageFactorisation(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Factor through the image", DiagramScene::Theorem);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 200, 0);
		Arrow* f = rule.arr("f", A, B);
		Object* I = rule.some("Im f", 0, 180);
		rule.derived(I, "Im %1", { f });
		rule.someArr("e", A, I);
		rule.someArr("m", I, B);
		rule.gone(f);
		rule.commutes(true);
		rule.note("f is replaced by the two halves it factors into: onto its image, then into B.");
		return rule.write(cat + "/image-factorisation.theorem.totopos", error);
	}

	bool biproduct(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Biproduct", DiagramScene::Definition);
		Object* A = rule.obj("A", -250, 0);
		Object* B = rule.obj("B", 250, 0);
		Object* S = rule.some("A ⊕ B", 0, -180);
		rule.derived(S, "%1 ⊕ %2", { A, B });
		Arrow* i1 = rule.someArr("i₁", A, S);
		Arrow* i2 = rule.someArr("i₂", B, S);
		Arrow* p1 = rule.someArr("p₁", S, A);
		Arrow* p2 = rule.someArr("p₂", S, B);
		rule.bend(i1, -160, -60);
		rule.bend(p1, -90, -120);
		rule.bend(i2, 160, -60);
		rule.bend(p2, 90, -120);
		rule.defines("biproduct");
		rule.note("In an additive category the product and the coproduct are the same object: "
		          "p₁i₁ = 1, p₂i₂ = 1, p₂i₁ = 0, p₁i₂ = 0 and i₁p₁ + i₂p₂ = 1.");
		return rule.write(cat + "/biproduct.definition.totopos", error);
	}

	// A picture with nothing dotted and nothing crossed out: it says this scene
	// is there. Citing it is what a step of a proof is made of.
	bool shortExactSequence(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Short exact sequence", DiagramScene::Definition);
		Object* z1 = rule.obj("0", -400, 0);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 0, 0);
		Object* C = rule.obj("C", 200, 0);
		Object* z2 = rule.obj("0", 400, 0);
		rule.arr("", z1, A);
		rule.arr("f", A, B);
		rule.arr("g", B, C);
		rule.arr("", C, z2);
		rule.exactRows(true);
		rule.defines("short exact sequence");
		rule.note("0 → A → B → C → 0 exact: f is into, g is onto, and the image of f is "
		          "exactly what g kills.");
		return rule.write(cat + "/short-exact-sequence.definition.totopos", error);
	}

	bool exactRow(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Exact at B", DiagramScene::Definition);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 0, 0);
		Object* C = rule.obj("C", 200, 0);
		rule.arr("f", A, B);
		rule.arr("g", B, C);
		rule.exactRows(true);
		rule.defines("exact");
		rule.note("Exact at B: the image of f is exactly the kernel of g.");
		return rule.write(cat + "/exact-row.definition.totopos", error);
	}

	// A sequence that splits: B is put away and the biproduct drawn in its place.
	bool splitSequence(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Split short exact sequence", DiagramScene::Theorem);
		Object* z1 = rule.obj("0", -400, 0);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 0, 0);
		Object* C = rule.obj("C", 200, 0);
		Object* z2 = rule.obj("0", 400, 0);
		rule.arr("", z1, A);
		rule.arr("f", A, B);
		rule.arr("g", B, C);
		rule.arr("", C, z2);
		Arrow* s = rule.arr("s", C, B);
		rule.bend(s, 100, 120);

		Object* S = rule.some("A ⊕ C", 0, -220);
		rule.derived(S, "%1 ⊕ %2", { A, C });
		rule.someArr("i", A, S);
		rule.someArr("p", S, C);
		rule.gone(B);
		rule.exactRows(true);
		rule.note("gs = 1, so the sequence splits and B is the biproduct of A and C. B and the "
		          "arrows on it are put away, and A ⊕ C is drawn in their place.");
		return rule.write(cat + "/split-sequence.theorem.totopos", error);
	}

	// The five lemma, as a picture to be cited: two exact rows, five arrows
	// between them, the squares commuting.
	bool fiveLemma(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Five lemma", DiagramScene::Theorem);
		const QStringList top = { "A", "B", "C", "D", "E" };
		const QStringList bottom = { "A'", "B'", "C'", "D'", "E'" };
		const QStringList across = { "f", "g", "h", "k" };
		const QStringList down = { "a", "b", "c", "d", "e" };

		QList<Node*> upper, lower;
		for (int i = 0; i < 5; ++i)
		{
			upper << rule.obj(top.at(i), -400 + 200 * i, -120);
			lower << rule.obj(bottom.at(i), -400 + 200 * i, 120);
		}
		for (int i = 0; i < 4; ++i)
		{
			rule.arr(across.at(i), upper.at(i), upper.at(i + 1));
			rule.arr(across.at(i) + "'", lower.at(i), lower.at(i + 1));
		}
		for (int i = 0; i < 5; ++i)
			rule.arr(down.at(i), upper.at(i), lower.at(i));

		rule.commutes(true);
		rule.exactRows(true);
		rule.note("Two exact rows and five arrows between them, every square commuting.");
		rule.note("If b and d are isomorphisms, a is onto and e is into, then c is an "
		          "isomorphism as well.");
		return rule.write(cat + "/five-lemma.theorem.totopos", error);
	}

	// ------------------------------------------------------------ modules

	bool quotient(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Quotient", DiagramScene::Definition);
		Object* A = rule.obj("A", -150, 0);
		Object* B = rule.obj("B", 150, 0);
		rule.arr("i", A, B);
		Object* Q = rule.some("B/A", 150, 200);
		rule.derived(Q, "%2/%1", { A, B });
		rule.someArr("q", B, Q);
		rule.defines("quotient");
		rule.note("A submodule of B gives the quotient B/A and the map onto it. "
		          "qi = 0, and anything killing A factors through q.");
		return rule.write(cat + "/quotient.definition.totopos", error);
	}

	bool hom(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Hom", DiagramScene::Definition);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 200, 0);
		Object* H = rule.some("Hom(A, B)", 0, 200);
		rule.derived(H, "Hom(%1, %2)", { A, B });
		rule.defines("hom");
		rule.note("The arrows from A to B are themselves an object here: they add, and the "
		          "ring acts on them.");
		return rule.write(cat + "/hom.definition.totopos", error);
	}

	bool projectiveLifting(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Lifting through a projective", DiagramScene::Definition);
		Object* P = rule.obj("P", 0, -200);
		Object* B = rule.obj("B", -200, 0);
		Object* C = rule.obj("C", 200, 0);
		rule.arr("g", B, C);
		rule.arr("f", P, C);
		rule.someArr("h", P, B);
		rule.commutes(true);
		rule.defines("projective");
		rule.note("P is projective and g is onto: then f lifts, gh = f.");
		rule.note("This is what being projective MEANS - do not cite it unless P is projective "
		          "and g is onto.");
		return rule.write(cat + "/projective-lifting.definition.totopos", error);
	}

	bool injectiveExtension(const QString& cat, QString* error)
	{
		RuleBuilder rule(cat, "Extending through an injective", DiagramScene::Definition);
		Object* I = rule.obj("I", 0, 200);
		Object* A = rule.obj("A", -200, 0);
		Object* B = rule.obj("B", 200, 0);
		rule.arr("m", A, B);
		rule.arr("f", A, I);
		rule.someArr("h", B, I);
		rule.commutes(true);
		rule.defines("injective");
		rule.note("I is injective and m is into: then f extends, hm = f.");
		rule.note("This is what being injective MEANS - do not cite it unless I is injective "
		          "and m is into.");
		return rule.write(cat + "/injective-extension.definition.totopos", error);
	}
}

int Library::writeStandardRules(QString* error)
{
	const QString base = ensureRoot();
	if (base.isEmpty())
	{
		if (error != nullptr)
			*error = QStringLiteral("There is nowhere to put the library.");
		return 0;
	}

	int written = 0;
	QString whyNot;
	// only what is not already there: a rule the user has edited stays edited
	auto put = [&](const QString& relativePath, const std::function<bool(QString*)>& make) {
		if (QFileInfo::exists(base + "/" + relativePath))
			return;
		QString one;
		if (make(&one))
			++written;
		else if (whyNot.isEmpty())
			whyNot = one;
	};

	// what holds in any category at all
	for (const QString& cat : Category::builtInNames())
	{
		put(cat + "/identity.axiom.totopos", [&](QString* e) { return identity(cat, e); });
		put(cat + "/composite.axiom.totopos", [&](QString* e) { return composite(cat, e); });
		put(cat + "/compose-pair.theorem.totopos", [&](QString* e) { return composePair(cat, e); });
		put(cat + "/erase-loop.theorem.totopos", [&](QString* e) { return eraseLoop(cat, e); });
	}

	// products and coproducts, where there are any
	struct WithProducts { const char* cat; const char* coproductSign; };
	const QList<WithProducts> withProducts = {
		{ "Set", "⊔" }, { "Top", "⊔" }, { "Cat", "⊔" },
		{ "Grp", "∗" },
		{ "Ab", "⊕" }, { "Vect", "⊕" }, { "R-Mod", "⊕" }, { "Mod-R", "⊕" },
	};
	for (const WithProducts& where : withProducts)
	{
		const QString cat = QString::fromUtf8(where.cat);
		const QString sign = QString::fromUtf8(where.coproductSign);
		put(cat + "/product.definition.totopos", [&](QString* e) { return product(cat, e); });
		put(cat + "/coproduct.definition.totopos", [&](QString* e) { return coproduct(cat, sign, e); });
	}

	// zeros, kernels, cokernels, exactness: the abelian categories
	const QStringList abelian = { "Ab", "Vect", "R-Mod", "Mod-R" };
	for (const QString& cat : abelian)
	{
		put(cat + "/zero-object.axiom.totopos", [&](QString* e) { return zeroObject(cat, e); });
		put(cat + "/zero-map.axiom.totopos", [&](QString* e) { return zeroMap(cat, e); });
		put(cat + "/kernel.definition.totopos", [&](QString* e) { return kernel(cat, e); });
		put(cat + "/cokernel.definition.totopos", [&](QString* e) { return cokernel(cat, e); });
		put(cat + "/image.definition.totopos", [&](QString* e) { return image(cat, e); });
		put(cat + "/image-factorisation.theorem.totopos", [&](QString* e) { return imageFactorisation(cat, e); });
		put(cat + "/biproduct.definition.totopos", [&](QString* e) { return biproduct(cat, e); });
		put(cat + "/exact-row.definition.totopos", [&](QString* e) { return exactRow(cat, e); });
		put(cat + "/short-exact-sequence.definition.totopos", [&](QString* e) { return shortExactSequence(cat, e); });
		put(cat + "/split-sequence.theorem.totopos", [&](QString* e) { return splitSequence(cat, e); });
		put(cat + "/five-lemma.theorem.totopos", [&](QString* e) { return fiveLemma(cat, e); });
	}

	// and what belongs to modules over a ring
	const QStringList modules = { "R-Mod", "Mod-R" };
	for (const QString& cat : modules)
	{
		put(cat + "/quotient.definition.totopos", [&](QString* e) { return quotient(cat, e); });
		put(cat + "/hom.definition.totopos", [&](QString* e) { return hom(cat, e); });
		put(cat + "/projective-lifting.definition.totopos", [&](QString* e) { return projectiveLifting(cat, e); });
		put(cat + "/injective-extension.definition.totopos", [&](QString* e) { return injectiveExtension(cat, e); });
	}

	if (error != nullptr && written == 0 && !whyNot.isEmpty())
		*error = whyNot;
	return written;
}
