namespace AbCatDC.Kernel

/// A variable, member, or type-variable name (x, α, ...)
type Name = string

/// The types σ, τ of the calculus — one case per row of the table.
///
/// Binding conventions:
///  - Polymorphic/Existential/Recursive bind a type variable α in their body.
///  - The Dependent* and Familial* cases bind a term variable x (of the first
///    type) in the second type.
[<RequireQualifiedAccess>]
type Ty =
    /// A base type constant (leaf; not in the table but needed to build anything)
    | Atom of Name
    /// A type variable α, bound by Polymorphic / Existential / Recursive
    | Var of Name
    /// An integer literal, e.g. 3 — an unregistered numeral is a *constant* of
    /// type ℕ whose computational behavior is the integer value in ℤ. Never
    /// renamed by canonicalization.
    | Lit of bigint
    /// σ^n — the n-fold diagonal power: A^2 ≡ A × A = {(a,b) : a ∈ A, b ∈ A}
    | Power of Ty * bigint
    /// [D] — a reference to a named quiver sketch (a diagram literal)
    | Sketch of Name
    /// τ commutes — the assertion that the referenced diagram commutes
    | Commutes of Ty
    /// [D] in C — the sketch's objects and morphisms all live in category C;
    /// equivalent to the textual group of judgments X : C, …, f : X → Y, …
    | InCategory of Ty * Ty
    /// Γ, x : σ, τ ⊢ ρ — a sequent: the goal ρ is derivable from the context
    | Entails of context: CtxItem list * goal: Ty
    /// M : σ — a typing judgment: the term M inhabits the type σ
    | HasType of subject: Ty * Ty
    /// M = N — an equation; symmetric, so canonicalization may orient it
    | Eq of Ty * Ty
    /// σ → τ
    | Function of domain: Ty * codomain: Ty
    /// σ × τ
    | Product of Ty * Ty
    /// σ + τ
    | Sum of Ty * Ty
    /// σ ∩ τ
    | Intersection of Ty * Ty
    /// σ ∪ τ
    | Union of Ty * Ty
    /// ⟨x : τ⟩ — generalized to any number of members
    | Record of members: (Name * Ty) list
    /// ∀α. τ
    | Polymorphic of alpha: Name * body: Ty
    /// ∃α. τ
    | Existential of alpha: Name * body: Ty
    /// μα. τ
    | Recursive of alpha: Name * body: Ty
    /// (x : σ) → τ
    | DependentFunction of x: Name * domain: Ty * codomain: Ty
    /// (x : σ) × τ
    | DependentPair of x: Name * Ty * Ty
    /// (x : σ) ∩ τ
    | DependentIntersection of x: Name * Ty * Ty
    /// ⋂(x:σ) τ
    | FamilialIntersection of x: Name * Ty * Ty
    /// ⋃(x:σ) τ
    | FamilialUnion of x: Name * Ty * Ty

/// One entry of a sequent context.
and CtxItem =
    /// Γ — a metavariable standing for a whole list of hypotheses
    | CtxVar of Name
    /// x : σ — a named hypothesis (binds x in later entries and the goal)
    | Hyp of Name * Ty
    /// σ — an anonymous hypothesis
    | Anon of Ty

/// The terms M, N, O appearing in the "Meaning" column.
[<RequireQualifiedAccess>]
type Term =
    /// A term variable x
    | Var of Name
    /// λx:σ. M — inhabitant of Function / DependentFunction
    | Lambda of x: Name * annot: Ty * body: Term
    /// M(N)
    | App of Term * Term
    /// (N, O) — inhabitant of Product / DependentPair
    | Pair of Term * Term
    /// ι₁(N) / ι₂(N) — the injections into a Sum
    | Inj1 of Term
    | Inj2 of Term
    /// ⟨x = M, ...⟩ — inhabitant of Record
    | Record of (Name * Term) list
    /// M.x — member access
    | Member of Term * Name

module Ty =

    /// Well-known constant atoms — identifiers the parser treats as constants
    /// rather than variables (so canonicalization never renames them).
    let constantAtoms = Set.ofList [ "ℕ"; "ℤ"; "ℚ"; "ℝ"; "ℂ" ]

    /// Identifiers that denote whole-context metavariables in sequents.
    let contextVars = Set.ofList [ "Γ"; "Δ"; "Θ"; "Ξ" ]

    /// Simultaneous substitution of types for free type variables: τ[m].
    /// Capture-naive on the replacement side, but shadow-correct on the
    /// pattern side: every binder (∀/∃/μ, dependent and familial binders, and
    /// sequent hypotheses) removes its bound name from the substitution for
    /// the scope it governs.
    let rec substTyVars (m: Map<Name, Ty>) (ty: Ty) : Ty =
        if Map.isEmpty m then ty else
        let s = substTyVars m
        let under (x: Name) = substTyVars (Map.remove x m)
        match ty with
        | Ty.Atom _ | Ty.Lit _ | Ty.Sketch _ -> ty
        | Ty.Var a -> (match Map.tryFind a m with Some rep -> rep | None -> ty)
        | Ty.Power (b, n) -> Ty.Power (s b, n)
        | Ty.Commutes b -> Ty.Commutes (s b)
        | Ty.InCategory (d, c) -> Ty.InCategory (s d, s c)
        | Ty.Entails (ctx, goal) ->
            // hypotheses bind their name for everything to the right
            let mutable cur = m
            let items =
                [ for it in ctx ->
                    match it with
                    | CtxVar n -> CtxVar n
                    | Hyp (x, t) ->
                        let t' = substTyVars cur t
                        cur <- Map.remove x cur
                        Hyp (x, t')
                    | Anon t -> Anon (substTyVars cur t) ]
            Ty.Entails (items, substTyVars cur goal)
        | Ty.HasType (subj, t) -> Ty.HasType (s subj, s t)
        | Ty.Eq (a, b) -> Ty.Eq (s a, s b)
        | Ty.Function (d, c) -> Ty.Function (s d, s c)
        | Ty.Product (a, b) -> Ty.Product (s a, s b)
        | Ty.Sum (a, b) -> Ty.Sum (s a, s b)
        | Ty.Intersection (a, b) -> Ty.Intersection (s a, s b)
        | Ty.Union (a, b) -> Ty.Union (s a, s b)
        | Ty.Record ms -> Ty.Record (ms |> List.map (fun (n, t) -> n, s t))
        | Ty.Polymorphic (a, body) -> Ty.Polymorphic (a, under a body)
        | Ty.Existential (a, body) -> Ty.Existential (a, under a body)
        | Ty.Recursive (a, body) -> Ty.Recursive (a, under a body)
        | Ty.DependentFunction (x, d, c) -> Ty.DependentFunction (x, s d, under x c)
        | Ty.DependentPair (x, a, b) -> Ty.DependentPair (x, s a, under x b)
        | Ty.DependentIntersection (x, a, b) -> Ty.DependentIntersection (x, s a, under x b)
        | Ty.FamilialIntersection (x, a, b) -> Ty.FamilialIntersection (x, s a, under x b)
        | Ty.FamilialUnion (x, a, b) -> Ty.FamilialUnion (x, s a, under x b)

    /// Single-variable substitution τ[α := σ], via substTyVars.
    let substTyVar (alpha: Name) (sigma: Ty) (ty: Ty) : Ty =
        substTyVars (Map.ofList [ alpha, sigma ]) ty

    let private superscript (n: bigint) =
        string n
        |> String.map (function
            | '0' -> '⁰' | '1' -> '¹' | '2' -> '²' | '3' -> '³' | '4' -> '⁴'
            | '5' -> '⁵' | '6' -> '⁶' | '7' -> '⁷' | '8' -> '⁸' | '9' -> '⁹'
            | c -> c)

    /// Expand diagonal powers into explicit products: A^3 ⇒ (A × A) × A,
    /// A^1 ⇒ A. Exponents outside 2..16 (and 0) are left as Power nodes.
    let rec expandPowers (ty: Ty) : Ty =
        match ty with
        | Ty.Atom _ | Ty.Var _ | Ty.Lit _ | Ty.Sketch _ -> ty
        | Ty.Commutes b -> Ty.Commutes (expandPowers b)
        | Ty.InCategory (d, c) -> Ty.InCategory (expandPowers d, expandPowers c)
        | Ty.Entails (ctx, goal) ->
            let item = function
                | CtxVar n -> CtxVar n
                | Hyp (x, t) -> Hyp (x, expandPowers t)
                | Anon t -> Anon (expandPowers t)
            Ty.Entails (List.map item ctx, expandPowers goal)
        | Ty.HasType (subj, t) -> Ty.HasType (expandPowers subj, expandPowers t)
        | Ty.Eq (a, b) -> Ty.Eq (expandPowers a, expandPowers b)
        | Ty.Power (b, n) ->
            let b' = expandPowers b
            if n = bigint 1 then b'
            elif n >= bigint 2 && n <= bigint 16 then
                let mutable acc = b'
                for _ in 2 .. int n do acc <- Ty.Product (acc, b')
                acc
            else Ty.Power (b', n)
        | Ty.Function (a, b) -> Ty.Function (expandPowers a, expandPowers b)
        | Ty.Product (a, b) -> Ty.Product (expandPowers a, expandPowers b)
        | Ty.Sum (a, b) -> Ty.Sum (expandPowers a, expandPowers b)
        | Ty.Intersection (a, b) -> Ty.Intersection (expandPowers a, expandPowers b)
        | Ty.Union (a, b) -> Ty.Union (expandPowers a, expandPowers b)
        | Ty.Record ms -> Ty.Record [ for n', t in ms -> n', expandPowers t ]
        | Ty.Polymorphic (a, b) -> Ty.Polymorphic (a, expandPowers b)
        | Ty.Existential (a, b) -> Ty.Existential (a, expandPowers b)
        | Ty.Recursive (a, b) -> Ty.Recursive (a, expandPowers b)
        | Ty.DependentFunction (x, a, b) -> Ty.DependentFunction (x, expandPowers a, expandPowers b)
        | Ty.DependentPair (x, a, b) -> Ty.DependentPair (x, expandPowers a, expandPowers b)
        | Ty.DependentIntersection (x, a, b) -> Ty.DependentIntersection (x, expandPowers a, expandPowers b)
        | Ty.FamilialIntersection (x, a, b) -> Ty.FamilialIntersection (x, expandPowers a, expandPowers b)
        | Ty.FamilialUnion (x, a, b) -> Ty.FamilialUnion (x, expandPowers a, expandPowers b)

    /// One unfolding of a recursive type: μα. τ  ⇒  τ[α := μα. τ]
    let unfold ty =
        match ty with
        | Ty.Recursive (alpha, body) -> substTyVar alpha ty body
        | _ -> ty

    /// Render a type in the notation of the table.
    let rec format (ty: Ty) : string =
        let atom t =
            match t with
            | Ty.Atom _ | Ty.Var _ | Ty.Lit _ | Ty.Power _ | Ty.Sketch _ -> format t
            | _ -> $"({format t})"
        match ty with
        | Ty.Atom n | Ty.Var n -> n
        | Ty.Lit n -> string n
        | Ty.Power (b, n) -> $"{atom b}{superscript n}"
        | Ty.Sketch n -> $"[{n}]"
        | Ty.Commutes b -> $"{atom b} commutes"
        | Ty.InCategory (d, c) -> $"{atom d} in {atom c}"
        | Ty.Entails (ctx, goal) ->
            let item = function
                | CtxVar n -> n
                | Hyp (x, t) -> $"{x} : {format t}"
                | Anon t -> format t
            let left = ctx |> List.map item |> String.concat ", "
            (if left = "" then "⊢ " else left + " ⊢ ") + format goal
        | Ty.HasType (subj, t) -> $"{format subj} : {format t}"
        | Ty.Eq (a, b) -> $"{format a} = {format b}"
        | Ty.Function (d, c) -> $"{atom d} → {atom c}"
        | Ty.Product (a, b) -> $"{atom a} × {atom b}"
        | Ty.Sum (a, b) -> $"{atom a} + {atom b}"
        | Ty.Intersection (a, b) -> $"{atom a} ∩ {atom b}"
        | Ty.Union (a, b) -> $"{atom a} ∪ {atom b}"
        | Ty.Record ms ->
            let inner = ms |> List.map (fun (n, t) -> $"{n} : {format t}") |> String.concat ", "
            $"⟨{inner}⟩"
        | Ty.Polymorphic (a, body) -> $"∀{a}. {format body}"
        | Ty.Existential (a, body) -> $"∃{a}. {format body}"
        | Ty.Recursive (a, body) -> $"μ{a}. {format body}"
        | Ty.DependentFunction (x, d, c) -> $"({x} : {format d}) → {atom c}"
        | Ty.DependentPair (x, a, b) -> $"({x} : {format a}) × {atom b}"
        | Ty.DependentIntersection (x, a, b) -> $"({x} : {format a}) ∩ {atom b}"
        | Ty.FamilialIntersection (x, a, b) -> $"⋂({x}:{format a}) {atom b}"
        | Ty.FamilialUnion (x, a, b) -> $"⋃({x}:{format a}) {atom b}"
