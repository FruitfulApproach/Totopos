module AbCatDC.Kernel.Parser

open System
open AbCatDC.Kernel

/// Tokens of the surface syntax. ASCII fallbacks: -> for →, * for ×.
type Token =
    | TIdent of string
    | TInt of bigint
    | TCaret
    | TArrow | TTimes | TPlus | TCap | TCup | TBigCap | TBigCup
    | TForall | TExists | TMu
    | TLParen | TRParen | TLAngle | TRAngle
    | TLBracket | TRBracket
    | TCommutes
    | TIn
    | TTurnstile
    | TEquals
    | TDot | TColon | TComma
    | TEnd

let private describe tok =
    match tok with
    | TIdent n -> $"identifier '{n}'"
    | TInt n -> $"integer '{n}'"
    | TCaret -> "'^'"
    | TArrow -> "'→'"
    | TTimes -> "'×'"
    | TPlus -> "'+'"
    | TCap -> "'∩'"
    | TCup -> "'∪'"
    | TBigCap -> "'⋂'"
    | TBigCup -> "'⋃'"
    | TForall -> "'∀'"
    | TExists -> "'∃'"
    | TMu -> "'μ'"
    | TLParen -> "'('"
    | TRParen -> "')'"
    | TLAngle -> "'⟨'"
    | TRAngle -> "'⟩'"
    | TLBracket -> "'['"
    | TRBracket -> "']'"
    | TCommutes -> "'commutes'"
    | TIn -> "'in'"
    | TTurnstile -> "'⊢'"
    | TEquals -> "'='"
    | TDot -> "'.'"
    | TColon -> "':'"
    | TComma -> "','"
    | TEnd -> "end of input"

exception private ParseError of string

let private lex (s: string) : Token list =
    let toks = ResizeArray<Token>()
    let mutable i = 0
    while i < s.Length do
        let c = s.[i]
        if Char.IsWhiteSpace c then i <- i + 1
        elif c = '→' then toks.Add TArrow; i <- i + 1
        elif c = '-' && i + 1 < s.Length && s.[i + 1] = '>' then toks.Add TArrow; i <- i + 2
        elif c = '×' || c = '*' then toks.Add TTimes; i <- i + 1
        elif c = '+' then toks.Add TPlus; i <- i + 1
        elif c = '∩' then toks.Add TCap; i <- i + 1
        elif c = '∪' then toks.Add TCup; i <- i + 1
        elif c = '⋂' then toks.Add TBigCap; i <- i + 1
        elif c = '⋃' then toks.Add TBigCup; i <- i + 1
        elif c = '∀' then toks.Add TForall; i <- i + 1
        elif c = '∃' then toks.Add TExists; i <- i + 1
        elif c = 'μ' then toks.Add TMu; i <- i + 1
        elif c = '⟨' then toks.Add TLAngle; i <- i + 1
        elif c = '⟩' then toks.Add TRAngle; i <- i + 1
        elif c = '(' then toks.Add TLParen; i <- i + 1
        elif c = ')' then toks.Add TRParen; i <- i + 1
        elif c = '[' then toks.Add TLBracket; i <- i + 1
        elif c = ']' then toks.Add TRBracket; i <- i + 1
        elif c = '⊢' then toks.Add TTurnstile; i <- i + 1
        elif c = '|' && i + 1 < s.Length && s.[i + 1] = '-' then toks.Add TTurnstile; i <- i + 2
        elif c = '=' then toks.Add TEquals; i <- i + 1
        elif c = '.' then toks.Add TDot; i <- i + 1
        elif c = ':' then toks.Add TColon; i <- i + 1
        elif c = ',' then toks.Add TComma; i <- i + 1
        elif c = '^' then toks.Add TCaret; i <- i + 1
        elif "⁰¹²³⁴⁵⁶⁷⁸⁹".IndexOf c >= 0 then
            // a superscript run is sugar for ^n, so formatted output re-parses
            let digits = "⁰¹²³⁴⁵⁶⁷⁸⁹"
            let sb = System.Text.StringBuilder()
            while i < s.Length && digits.IndexOf s.[i] >= 0 do
                sb.Append(char (int '0' + digits.IndexOf s.[i])) |> ignore
                i <- i + 1
            toks.Add TCaret
            toks.Add (TInt (bigint.Parse(sb.ToString())))
        elif Char.IsDigit c then
            let start = i
            while i < s.Length && Char.IsDigit s.[i] do i <- i + 1
            // digits followed by a subscript continue as an identifier, so the
            // identity morphism 1ₓ is one name rather than a numeral
            let isSubTail (ch: char) = "₀₁₂₃₄₅₆₇₈₉₊₋₌ₐₑₕᵢⱼₖₗₘₙₒₚᵣₛₜᵤᵥₓ".IndexOf ch >= 0
            if i < s.Length && isSubTail s.[i] then
                while i < s.Length && s.[i] <> 'μ'
                      && (Char.IsLetterOrDigit s.[i] || s.[i] = '_' || s.[i] = '\'' || isSubTail s.[i]) do
                    i <- i + 1
                toks.Add (TIdent (s.Substring(start, i - start)))
            else
                toks.Add (TInt (bigint.Parse(s.Substring(start, i - start))))
        elif Char.IsLetter c || c = '_' then
            // μ is a letter but always lexes as the binder, so identifiers exclude it.
            // Subscript letters are Unicode letters already; subscript digits and
            // signs (₀-₉, ₊₋₌) are admitted explicitly so idₓ and x₁ stay one token.
            let isSubscript (ch: char) = "₀₁₂₃₄₅₆₇₈₉₊₋₌".IndexOf ch >= 0
            // a '-' continues the identifier (right-identity) unless it starts
            // an arrow '->' — lookahead keeps X->Y lexing as an arrow
            let isHyphenJoin j =
                s.[j] = '-' && j + 1 < s.Length && s.[j + 1] <> '>'
                && (Char.IsLetterOrDigit s.[j + 1] || isSubscript s.[j + 1] || s.[j + 1] = '_')
            let start = i
            while i < s.Length && s.[i] <> 'μ'
                  && (Char.IsLetterOrDigit s.[i] || s.[i] = '_' || s.[i] = '\'' || isSubscript s.[i] || isHyphenJoin i) do
                i <- i + 1
            let word = s.Substring(start, i - start)
            toks.Add (
                match word with
                | "commutes" -> TCommutes
                | "in" -> TIn
                | _ -> TIdent word)
        else
            raise (ParseError $"Unexpected character '{c}' at position {i}.")
    toks.Add TEnd
    List.ofSeq toks

type private State = { toks: Token[]; mutable pos: int }

let private peekAt (st: State) k = st.toks.[min (st.pos + k) (st.toks.Length - 1)]
let private peek st = peekAt st 0
let private advance (st: State) = st.pos <- st.pos + 1

let private expect st tok =
    if peek st = tok then advance st
    else raise (ParseError $"Expected {describe tok} but found {describe (peek st)}.")

let private ident st =
    match peek st with
    | TIdent n -> advance st; n
    | t -> raise (ParseError $"Expected an identifier but found {describe t}.")

let private startsDependent st =
    peek st = TLParen
    && (match peekAt st 1 with TIdent _ -> true | _ -> false)
    && peekAt st 2 = TColon

// Grammar (binders extend as far right as possible; → is right-associative,
// the other binary operators are left-associative, tightest first: × + ∩ ∪ →):
//
//   type    := '∀' x '.' type | '∃' x '.' type | 'μ' x '.' type
//            | '⋂' '(' x ':' type ')' type | '⋃' '(' x ':' type ')' type
//            | '(' x ':' type ')' ('→' | '×' | '∩') type
//            | arrow
//   arrow   := union ('→' type)?
//   union   := inter ('∪' inter)*
//   inter   := sum ('∩' sum)*
//   sum     := product ('+' product)*
//   product := atom ('×' atom)*
//   atom    := identifier | '(' type ')' | '⟨' x ':' type (',' x ':' type)* '⟩' | type

let rec private parseTypedBinder st (plain: Name * Ty -> Ty) (dep: Name * Ty * Ty -> Ty) : Ty =
    // quantifier body forms:  x . body  |  x : annot . body  |  x : annot
    let a = ident st
    match peek st with
    | TDot ->
        advance st
        plain (a, parseBody st)
    | TColon ->
        advance st
        let ann = parseType st
        if peek st = TDot then
            advance st
            dep (a, ann, parseBody st)
        else
            // bare "∃x : T" — existence of a typed witness
            plain (a, Ty.HasType (Ty.Var a, ann))
    | t -> raise (ParseError $"Expected '.' or ':' after the bound variable but found {describe t}.")

/// A binder body: a type, optionally refined to a typing judgment M : σ.
/// (Equations are already part of parseType.)
and private parseBody st =
    let t = parseType st
    if peek st = TColon then
        advance st
        Ty.HasType (t, parseType st)
    else t

and private parseType (st: State) : Ty =
    let mutable t = parseTypeCore st
    // postfix assertions: 'commutes' and 'in <category>'
    let mutable looping = true
    while looping do
        match peek st with
        | TCommutes ->
            advance st
            t <- Ty.Commutes t
            if peek st = TArrow then
                advance st
                t <- Ty.Function (t, parseType st)
        | TIn ->
            advance st
            t <- Ty.InCategory (t, parseAtom st)
        | _ -> looping <- false
    // '=' binds loosest of all: a = b → c reads a = (b → c)
    if peek st = TEquals then
        advance st
        t <- Ty.Eq (t, parseType st)
    t

and private parseTypeCore (st: State) : Ty =
    match peek st with
    | TForall -> advance st; parseTypedBinder st Ty.Polymorphic Ty.DependentFunction
    | TExists -> advance st; parseTypedBinder st Ty.Existential Ty.DependentPair
    | TMu -> advance st; let a = ident st in expect st TDot; Ty.Recursive (a, parseType st)
    | TBigCap -> advance st; parseFamilial st Ty.FamilialIntersection
    | TBigCup -> advance st; parseFamilial st Ty.FamilialUnion
    | _ when startsDependent st -> parseDependent st
    | _ -> parseArrow st

and private parseFamilial st ctor =
    expect st TLParen
    let x = ident st
    expect st TColon
    let dom = parseType st
    expect st TRParen
    ctor (x, dom, parseType st)

and private parseDependent st =
    expect st TLParen
    let x = ident st
    expect st TColon
    let dom = parseType st
    expect st TRParen
    match peek st with
    | TArrow -> advance st; Ty.DependentFunction (x, dom, parseType st)
    | TTimes -> advance st; Ty.DependentPair (x, dom, parseType st)
    | TCap -> advance st; Ty.DependentIntersection (x, dom, parseType st)
    | t -> raise (ParseError $"Expected '→', '×', or '∩' after the dependent binder but found {describe t}.")

and private parseArrow st =
    let l = parseUnion st
    if peek st = TArrow then
        advance st
        Ty.Function (l, parseType st)
    else l

and private parseChain sub op ctor st =
    let mutable acc = sub st
    while peek st = op do
        advance st
        acc <- ctor (acc, sub st)
    acc

and private parseUnion st = parseChain parseInter TCup Ty.Union st
and private parseInter st = parseChain parseSum TCap Ty.Intersection st
and private parseSum st = parseChain parseProduct TPlus Ty.Sum st
and private parseProduct st = parseChain parsePower TTimes Ty.Product st

and private parsePower st =
    let mutable acc = parseAtom st
    while peek st = TCaret do
        advance st
        match peek st with
        | TInt n -> advance st; acc <- Ty.Power (acc, n)
        | t -> raise (ParseError $"Expected an integer exponent after '^' but found {describe t}.")
    acc

and private parseAtom st =
    match peek st with
    | TIdent n -> advance st; (if Set.contains n Ty.constantAtoms then Ty.Atom n else Ty.Var n)
    | TInt n -> advance st; Ty.Lit n
    | TLBracket ->
        advance st
        let name = ident st
        expect st TRBracket
        Ty.Sketch name
    | TForall | TExists | TMu | TBigCap | TBigCup -> parseType st
    | TLParen when startsDependent st -> parseDependent st
    | TLParen ->
        advance st
        let t = parseType st
        expect st TRParen
        t
    | TLAngle ->
        advance st
        let members = ResizeArray()
        let parseMember () =
            let n = ident st
            expect st TColon
            members.Add (n, parseType st)
        parseMember ()
        while peek st = TComma do
            advance st
            parseMember ()
        expect st TRAngle
        Ty.Record (List.ofSeq members)
    | t -> raise (ParseError $"Unexpected {describe t}.")

/// goal := type (':' type)?
/// — a bare formula (equations included via parseType) or a typing judgment
let private parseGoal st =
    let t = parseType st
    if peek st = TColon then
        advance st
        Ty.HasType (t, parseType st)
    else t

/// Top-level form: an optional sequent context and turnstile, then a goal.
///   top := (ctxitem (',' ctxitem)* )? '⊢' goal | goal
///   ctxitem := ident ':' type | Γ/Δ/Θ/Ξ | type
let private parseTop (st: State) : Ty =
    let start = st.pos
    let attemptSequent () =
        try
            let items = ResizeArray<CtxItem>()
            if peek st <> TTurnstile then
                let parseItem () =
                    match peek st, peekAt st 1 with
                    | TIdent x, TColon ->
                        advance st
                        advance st
                        items.Add (Hyp (x, parseType st))
                    | _ ->
                        match parseType st with
                        | Ty.Var n when Set.contains n Ty.contextVars -> items.Add (CtxVar n)
                        | t -> items.Add (Anon t)
                parseItem ()
                while peek st = TComma do
                    advance st
                    parseItem ()
            if peek st = TTurnstile then
                advance st
                Some (Ty.Entails (List.ofSeq items, parseGoal st))
            else None
        with ParseError _ -> None
    match attemptSequent () with
    | Some s -> s
    | None ->
        st.pos <- start
        parseGoal st

/// Parse an expression in the notation of the table, e.g.
/// "∀α. (x : α) → ⋃(y : α) β ∩ α", a typing judgment "M : σ", or a sequent
/// "Γ, x : σ ⊢ M : τ".
let parse (input: string) : Result<Ty, string> =
    try
        let st = { toks = Array.ofList (lex input); pos = 0 }
        if peek st = TEnd then Error "Empty input." else
        let t = parseTop st
        match peek st with
        | TEnd -> Ok t
        | tok -> Error $"Unexpected {describe tok} after a complete expression."
    with ParseError m -> Error m
