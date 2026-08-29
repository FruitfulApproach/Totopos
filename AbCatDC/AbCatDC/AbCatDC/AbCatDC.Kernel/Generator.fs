module AbCatDC.Kernel.Generator

open System

let private letters = [| "α"; "β"; "γ"; "ρ"; "σ"; "τ"; "a"; "b"; "x"; "y" |]
let private glyphs = [| "→"; "×"; "+"; "∩"; "∪"; "∀"; "∃"; "μ"; "⋂"; "⋃"; "⟨"; "⟩"; "("; ")"; "."; ":"; ","; " " |]

/// Random token soup over the language's alphabet — usually not well-formed.
let private soup (rng: Random) =
    let n = rng.Next(4, 14)
    String.concat "" [ for _ in 1 .. n -> if rng.Next 3 = 0 then letters.[rng.Next letters.Length] else glyphs.[rng.Next glyphs.Length] ]

/// Grammar-driven random expression — always well-formed.
let rec private expr (rng: Random) depth : string =
    let leaf () = letters.[rng.Next letters.Length]
    // operand leaves may be integer literals or constant atoms; binder names
    // below always come from leaf() since binders need identifiers
    if depth <= 0 then
        match rng.Next 5 with
        | 0 -> string (rng.Next 100)
        | 1 -> (if rng.Next 2 = 0 then "ℕ" else "ℤ")
        | _ -> leaf ()
    else
        let sub () = expr rng (depth - 1)
        match rng.Next 12 with
        | 0 -> $"∀{leaf ()}. {sub ()}"
        | 1 -> $"∃{leaf ()}. {sub ()}"
        | 2 -> $"μ{leaf ()}. {sub ()}"
        | 3 -> $"{sub ()} → {sub ()}"
        | 4 -> $"{sub ()} × {sub ()}"
        | 5 -> $"{sub ()} + {sub ()}"
        | 6 -> $"{sub ()} ∩ {sub ()}"
        | 7 -> $"{sub ()} ∪ {sub ()}"
        | 8 -> $"⟨{leaf ()} : {sub ()}, {leaf ()} : {sub ()}⟩"
        | 9 -> $"({leaf ()} : {sub ()}) → {sub ()}"
        | 10 -> $"⋂({leaf ()}:{sub ()}) {sub ()}"
        | _ -> $"⋃({leaf ()}:{sub ()}) {sub ()}"

/// A mixed batch over the language's alphabet: slightly more than half are
/// grammar-generated (well-formed), the rest are random soup (mostly not).
let samples (seed: int) (count: int) : string[] =
    let rng = Random(seed)
    [| for _ in 1 .. count ->
        if rng.Next 100 < 55 then expr rng (rng.Next(1, 4)) else soup rng |]
