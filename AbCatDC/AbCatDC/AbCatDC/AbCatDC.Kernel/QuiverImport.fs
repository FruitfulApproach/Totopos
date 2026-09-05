module AbCatDC.Kernel.QuiverImport

open System
open System.Collections.Generic
open System.Text.Json
open System.Threading

/// quiver's arrow-body style. Dashed and Dotted are interchangeable and both
/// mark the arrow EXISTENTIAL; None_ is quiver's invisible body.
type EdgeStyle =
    | Solid
    | Dashed
    | Dotted
    | None_

/// A quiver diagram reduced to its 1-skeleton.
type Graph =
    { VertexCount: int
      VertexLabels: string[]
      /// grid positions (x, y) per vertex, as drawn in quiver
      Positions: (int * int)[]
      /// (source, target) pairs indexing vertices; higher cells are filtered out upstream
      Edges: (int * int)[]
      /// labels parallel to Edges
      EdgeLabels: string[]
      /// parallel to Edges; Solid when the cell carries no style object
      EdgeStyles: EdgeStyle[]
      SkippedHigherCells: int }

/// The canonicalized diagram in the internal language.
type CanonicalDiagram =
    { /// one line per edge: {@e}:{@s}\to{@t}
      Lines: string[]
      /// the ∀-prefix binding the unmentioned vertices, e.g. "∀ {@1} {@2}"
      ForallPrefix: string
      /// canonical free token -> original vertex label (mentioned in the title)
      FreeMap: (string * string)[]
      /// canonical bound vertex token -> original vertex label
      BoundMap: (string * string)[]
      SkippedHigherCells: int
      /// how many candidate orderings were examined
      Candidates: int }

type CanonicalOutcome =
    { Ok: bool
      Diagram: CanonicalDiagram
      Error: string }

let private emptyDiagram =
    { Lines = [||]; ForallPrefix = ""; FreeMap = [||]; BoundMap = [||]; SkippedHigherCells = 0; Candidates = 0 }

/// quiver keeps an arrow's style in the cell's 5th element:
/// [src, tgt, "label", alignment, {"style":{"body":{"name":"dashed"}}, "curve":-1}]
let private styleOf (c: JsonElement) : EdgeStyle =
    if c.GetArrayLength() < 5 || c.[4].ValueKind <> JsonValueKind.Object then Solid
    else
        match c.[4].TryGetProperty "style" with
        | true, st when st.ValueKind = JsonValueKind.Object ->
            match st.TryGetProperty "body" with
            | true, b when b.ValueKind = JsonValueKind.Object ->
                match b.TryGetProperty "name" with
                | true, nm when nm.ValueKind = JsonValueKind.String ->
                    match nm.GetString() with
                    | "dashed" -> Dashed
                    | "dotted" -> Dotted
                    | "none" -> None_
                    | _ -> Solid
                | _ -> Solid
            | _ -> Solid
        | _ -> Solid

/// Parse quiver's export format: [version, vertexCount, ...cells], vertices
/// first as [x, y, label?, ...], then edges as [source, target, ...] indexing
/// the cells array. Accepts raw JSON or the base64 form used in share links.
let rec parse (input: string) : Result<Graph, string> =
    let text = input.Trim()
    if text.Length = 0 then Error "Empty input." else
    if not (text.StartsWith "[") then
        try
            let decoded = Text.Encoding.UTF8.GetString(Convert.FromBase64String text)
            if decoded.TrimStart().StartsWith "[" then parse decoded
            else Error "Input is neither a JSON array nor base64-encoded quiver data."
        with _ -> Error "Input is neither a JSON array nor valid base64."
    else
    try
        use doc = JsonDocument.Parse text
        let root = doc.RootElement
        if root.ValueKind <> JsonValueKind.Array then Error "Expected a JSON array (quiver export format)." else
        let items = [| for e in root.EnumerateArray() -> e.Clone() |]
        if items.Length < 2 then Error "Expected [version, vertexCount, ...cells]." else
        let version = items.[0].GetInt32()
        if version <> 0 then Error $"Unsupported quiver format version {version}." else
        let n = items.[1].GetInt32()
        if n < 0 || items.Length - 2 < n then Error "Vertex count does not match the cell list." else
        let labels =
            [| for i in 0 .. n - 1 ->
                let c = items.[2 + i]
                if c.GetArrayLength() >= 3 && c.[2].ValueKind = JsonValueKind.String
                then c.[2].GetString()
                else "" |]
        let positions =
            [| for i in 0 .. n - 1 ->
                let c = items.[2 + i]
                int (c.[0].GetDouble()), int (c.[1].GetDouble()) |]
        let all = ResizeArray()
        for idx in 2 + n .. items.Length - 1 do
            let c = items.[idx]
            let label =
                if c.GetArrayLength() >= 3 && c.[2].ValueKind = JsonValueKind.String
                then c.[2].GetString()
                else ""
            all.Add (c.[0].GetInt32(), c.[1].GetInt32(), label, styleOf c)
        let oneCells = all |> Seq.filter (fun (s, t, _, _) -> 0 <= s && s < n && 0 <= t && t < n) |> Array.ofSeq
        Ok { VertexCount = n
             VertexLabels = labels
             Positions = positions
             Edges = oneCells |> Array.map (fun (s, t, _, _) -> s, t)
             EdgeLabels = oneCells |> Array.map (fun (_, _, l, _) -> l)
             EdgeStyles = oneCells |> Array.map (fun (_, _, _, st) -> st)
             SkippedHigherCells = all.Count - oneCells.Length }
    with ex -> Error $"Could not parse quiver JSON: {ex.Message}"

/// C#-friendly parse wrapper for the preview renderer.
type ParseOutcome =
    { Ok: bool
      Graph: Graph
      Error: string }

let private emptyGraph =
    { VertexCount = 0; VertexLabels = [||]; Positions = [||]; Edges = [||]; EdgeLabels = [||]
      EdgeStyles = [||]; SkippedHigherCells = 0 }

let TryParse (input: string) : ParseOutcome =
    match parse input with
    | Ok g -> { Ok = true; Graph = g; Error = "" }
    | Error e -> { Ok = false; Graph = emptyGraph; Error = e }

/// Emit unicode labels (an existential mark, a subscripted witness) literally
/// rather than as escapes, so stored sketches stay readable.
let private jsonOptions =
    JsonSerializerOptions(Encoder = Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping)

/// Compact a diagram's grid layout: distinct x/y coordinates are rank-
/// compressed to 0, 1, 2, … so dragging cells far apart in the editor never
/// bloats the bounding box (embeds auto-fit, so sprawl = unreadable zoom-out).
/// Labels, edges, curves, and styles are preserved; base64 input is accepted
/// and plain JSON returned. On any error the input is returned unchanged.
let NormalizePositions (input: string) : string =
    try
        let text =
            let t = (input : string).Trim()
            if t.StartsWith "[" then t
            else Text.Encoding.UTF8.GetString(Convert.FromBase64String t)
        let node = Nodes.JsonNode.Parse text
        let arr = node.AsArray()
        let n = arr.[1].GetValue<int>()
        if n <= 0 then text else
        let coords (d: int) =
            [ for i in 2 .. 1 + n -> int (arr.[i].AsArray().[d].GetValue<double>()) ]
        let rank (xs: int list) =
            xs |> List.distinct |> List.sort |> List.mapi (fun i v -> v, i) |> Map.ofList
        let rx = rank (coords 0)
        let ry = rank (coords 1)
        for i in 2 .. 1 + n do
            let cell = arr.[i].AsArray()
            let x = int (cell.[0].GetValue<double>())
            let y = int (cell.[1].GetValue<double>())
            cell.[0] <- Nodes.JsonValue.Create(rx.[x])
            cell.[1] <- Nodes.JsonValue.Create(ry.[y])
        node.ToJsonString(jsonOptions)
    with _ -> input

// ---- labels ----------------------------------------------------------------

let private subMap =
    dict [ '0','₀'; '1','₁'; '2','₂'; '3','₃'; '4','₄'; '5','₅'; '6','₆'; '7','₇'; '8','₈'; '9','₉'
           'a','ₐ'; 'e','ₑ'; 'h','ₕ'; 'i','ᵢ'; 'j','ⱼ'; 'k','ₖ'; 'l','ₗ'; 'm','ₘ'; 'n','ₙ'; 'o','ₒ'
           'p','ₚ'; 'r','ᵣ'; 's','ₛ'; 't','ₜ'; 'u','ᵤ'; 'v','ᵥ'; 'x','ₓ'
           'A','ₐ'; 'E','ₑ'; 'H','ₕ'; 'I','ᵢ'; 'J','ⱼ'; 'K','ₖ'; 'L','ₗ'; 'M','ₘ'; 'N','ₙ'; 'O','ₒ'
           'P','ₚ'; 'R','ᵣ'; 'S','ₛ'; 'T','ₜ'; 'U','ᵤ'; 'V','ᵥ'; 'X','ₓ' ]

/// Normalize a quiver cell label towards a kernel identifier: LaTeX-style
/// subscripts (_X, _{ab}) become unicode subscripts; whitespace is trimmed.
let private normalizeLabel (label: string) =
    let l = label.Trim()
    let l = Text.RegularExpressions.Regex.Replace(l, @"_\{([^}]*)\}|_(.)", fun m ->
        let payload = if m.Groups.[1].Success then m.Groups.[1].Value else m.Groups.[2].Value
        let sb = Text.StringBuilder()
        let mutable ok = true
        for ch in payload do
            match subMap.TryGetValue ch with
            | true, s -> sb.Append s |> ignore
            | _ -> ok <- false
        if ok then sb.ToString() else m.Value)
    l

/// True when the normalized label lexes as one plain identifier or literal.
let private isUsableName (l: string) =
    match Parser.parse l with
    | Ok (Ty.Var _) | Ok (Ty.Atom _) | Ok (Ty.Lit _) -> true
    | _ -> false

/// The quantifier marks a label may carry, and its kernel-safe bare name.
///   objects: "∃X" / "\exists X"             → existential object
///   arrows:  "!u", "∃!u", "\exists! u"      → UNIQUE existential
///            "∃u"                             → existential (same as dashed)
/// Marks are stripped here only; the picture keeps them.
type LabelMark =
    { Bare: string
      Raw: string
      Existential: bool
      Unique: bool }

let private readLabel (raw: string) : LabelMark =
    let tryStrip (p: string) (s: string) =
        if s.StartsWith p then Some (s.Substring(p.Length).TrimStart()) else None
    let l0 = raw.Trim()
    // order matters: the ∃! forms before the ∃ forms
    let l1, ex1, uq1 =
        match tryStrip "∃!" l0 with
        | Some r -> r, true, true
        | None ->
        match tryStrip "\\exists!" l0 with
        | Some r -> r, true, true
        | None ->
        match tryStrip "\\exists !" l0 with
        | Some r -> r, true, true
        | None ->
        match tryStrip "∃" l0 with
        | Some r -> r, true, false
        | None ->
        match tryStrip "\\exists" l0 with
        | Some r -> r, true, false
        | None -> l0, false, false
    let l2, ex2, uq2 =
        match tryStrip "!" l1 with
        | Some r -> r, true, true
        | None -> l1, ex1, uq1
    // KaTeX thin spaces users type after a marker
    let l3 =
        [ "\\;"; "\\,"; "\\:"; "\\ " ]
        |> List.fold (fun (acc: string) sp -> match tryStrip sp acc with Some r -> r | None -> acc) l2
    { Bare = normalizeLabel l3; Raw = raw; Existential = ex2; Unique = uq2 }

/// Public view of readLabel for the UI (save-time warnings).
let ReadLabel (raw: string) : LabelMark = readLabel raw

let private subscriptDigits (k: int) =
    string k |> String.map (fun ch -> match subMap.TryGetValue ch with | true, s -> s | _ -> ch)

/// 12 → "₁₂" — the suffix used to freshen a name or a sketch.
let SubscriptDigits (k: int) : string = subscriptDigits k

// ---- the meaning of a diagram ---------------------------------------------

/// How an item is quantified relative to the surrounding rule/context.
type Quant =
    /// the label is already bound outside — it refers to that thing
    | Bound
    /// ∀ : an unbound label; a metavariable of the rule
    | Universal
    /// ∃ : a dashed/dotted arrow, or a label marked ∃
    | Existential
    /// ∃! : an arrow labelled ! or ∃!
    | UniqueExistential

type DiagramItem =
    { Name: string
      Raw: string
      IsObject: bool
      Quant: Quant
      /// (source, target) object names; ("", "") for an object
      Endpoints: string * string
      /// index into VertexLabels or Edges
      Index: int
      /// the label was blank/unusable and the name was generated — never a metavariable
      Generated: bool
      Style: EdgeStyle }

/// How the active system spells an arrow typing.
type HomStyle =
    /// f : A → B
    | ArrowStyle
    /// f : Hom(A, B)
    | HomTag of string

/// THE meaning of a diagram. Everything else in the system is a view of this.
/// Judgments are formatted text, ready to parse.
type Elaborated =
    { Items: DiagramItem[]
      /// X : C, one per distinct object name; empty when no category was given
      ObjectTypings: string[]
      /// f : A → B (or f : Hom(A, B)), one per edge
      ArrowTypings: string[]
      /// every pair of parallel directed paths, parallel single arrows included
      Equations: string[]
      /// for each ∃! arrow: any other arrow satisfying its equations equals it
      UniquenessObligations: string[]
      Universals: string[]
      /// name, isUnique
      Existentials: (string * bool)[]
      Notes: string[]
      Truncated: bool }

/// Longest path read as a composition: bounded by Ty.maxCompositionUnits,
/// since a 5-edge composite would read as an opaque word.
let maxPathLength = 4
let maxEquations = 256

/// The active system's way of spelling an arrow typing: Hom(A, B) when it
/// declared Hom as a rigid constant, else A → B.
let HomStyleForActiveSystem () : HomStyle =
    if Set.contains "Hom" Ty.userConstants then HomTag "Hom" else ArrowStyle

/// Rebuild a composite name with one unit renamed (ku[u:=u'] = ku').
let private renameUnit (name: string) (from: string) (to': string) =
    let units = Ty.nameUnits name
    if units |> List.contains from then units |> List.map (fun u -> if u = from then to' else u) |> String.concat ""
    else name

let elaborate (g: Graph) (category: string) (hom: HomStyle) (bound: Set<string>) : Elaborated =
    let notes = ResizeArray<string>()
    let n = g.VertexCount
    // ---- objects: one per raw vertex; equal labels share a NAME but stay
    //      distinct vertices for path enumeration
    let vmarks = g.VertexLabels |> Array.map readLabel
    let objNames =
        Array.init n (fun i ->
            let m = vmarks.[i]
            if m.Bare <> "" && isUsableName m.Bare then m.Bare, false
            else $"v{subscriptDigits (i + 1)}", true)
    for (d, c) in objNames |> Array.map fst |> Array.countBy id do
        if c > 1 then notes.Add $"the label '{d}' is drawn on {c} objects — they stay distinct objects that share a name"
    // ---- arrows
    let emarks = g.EdgeLabels |> Array.map readLabel
    let arrNames =
        Array.init g.Edges.Length (fun k ->
            let m = emarks.[k]
            if m.Bare <> "" && isUsableName m.Bare then m.Bare, false
            else $"e{subscriptDigits (k + 1)}", true)
    // a label that parses as a literal or a rigid constant (0, 1ₐ, Ker, a
    // declared name) is never quantified — it is what it says
    let isRigid (name: string) =
        match Parser.parse name with
        | Ok (Ty.Lit _) | Ok (Ty.Atom _) -> true
        | _ -> false
    let quantOf (name: string) (generated: bool) (ex: bool) (uq: bool) =
        if uq then UniqueExistential
        elif ex then Existential
        elif generated then Universal
        elif isRigid name || Set.contains name bound then Bound
        else Universal
    let objItems =
        [| for i in 0 .. n - 1 ->
            let name, gen = objNames.[i]
            let m = vmarks.[i]
            { Name = name; Raw = m.Raw; IsObject = true
              Quant = quantOf name gen m.Existential m.Unique
              Endpoints = "", ""; Index = i; Generated = gen; Style = Solid } |]
    let arrItems =
        [| for k in 0 .. g.Edges.Length - 1 ->
            let name, gen = arrNames.[k]
            let m = emarks.[k]
            let (s, t) = g.Edges.[k]
            let st = g.EdgeStyles.[k]
            let ex = m.Existential || st = Dashed || st = Dotted
            { Name = name; Raw = m.Raw; IsObject = false
              Quant = quantOf name gen ex m.Unique
              Endpoints = fst objNames.[s], fst objNames.[t]; Index = k; Generated = gen; Style = st } |]
    let arrowType (s: string) (t: string) =
        match hom with
        | ArrowStyle -> $"{s} → {t}"
        | HomTag tag -> $"{tag}({s}, {t})"
    // ---- typings
    let objectTypings =
        if category = "" then [||]
        else objItems |> Array.map (fun o -> o.Name) |> Array.distinct |> Array.map (fun o -> $"{o} : {category}")
    let arrowTypings =
        arrItems |> Array.map (fun a -> let (s, t) = a.Endpoints in $"{a.Name} : {arrowType s t}") |> Array.distinct
    // ---- simple directed paths of length 1..maxPathLength, over raw vertices.
    //      A path may close back on its own source (two parallel loops are
    //      compared) but never revisits any other vertex; the identity (length
    //      0) is excluded — a diagram that does not draw 1ₐ does not mention it.
    let outEdges = Array.init n (fun _ -> ResizeArray<int>())
    for k in 0 .. g.Edges.Length - 1 do
        let (s, _) = g.Edges.[k]
        outEdges.[s].Add k
    let paths = ResizeArray<int * int * int list>()
    let mutable cutOff = 0
    let rec walk (src: int) (cur: int) (visited: Set<int>) (acc: int list) (len: int) =
        if len > 0 then paths.Add (src, cur, List.rev acc)
        if len < maxPathLength then
            for k in outEdges.[cur] do
                let (_, t) = g.Edges.[k]
                if t = src then paths.Add (src, src, List.rev (k :: acc))
                elif not (visited.Contains t) then walk src t (Set.add t visited) (k :: acc) (len + 1)
        elif outEdges.[cur] |> Seq.exists (fun k -> not (visited.Contains (snd g.Edges.[k]))) then
            cutOff <- cutOff + 1
    for v in 0 .. n - 1 do walk v v (Set.singleton v) [] 0
    if cutOff > 0 then notes.Add $"paths longer than {maxPathLength} arrows are not compared — name an intermediate composite to reason about them"
    // ---- composites: edge names concatenated in REVERSE traversal order
    let byEnds = Dictionary<int * int, ResizeArray<string>>()
    let mutable skippedGenerated = 0
    let mutable skippedComposite = 0
    for (s, t, es) in paths do
        let names = es |> List.map (fun k -> arrNames.[k])
        if names |> List.exists snd then skippedGenerated <- skippedGenerated + 1
        else
            let composite = names |> List.rev |> List.map fst |> String.concat ""
            if es.Length >= 2 && not (Ty.isCompositeName composite) then skippedComposite <- skippedComposite + 1
            else
                let key = (s, t)
                if not (byEnds.ContainsKey key) then byEnds.[key] <- ResizeArray()
                if not (byEnds.[key].Contains composite) then byEnds.[key].Add composite
    if skippedGenerated > 0 then notes.Add $"{skippedGenerated} path(s) through an unlabelled arrow contribute no equation — label the arrow to include it"
    if skippedComposite > 0 then notes.Add $"{skippedComposite} path(s) form a composite the kernel cannot read as a composition (a hyphenated label, a path through a literal 0 arrow, or a chain longer than {maxPathLength})"
    let equations = ResizeArray<string>()
    let mutable truncated = false
    for KeyValue (_, comps) in byEnds |> Seq.sortBy (fun kv -> kv.Key) do
        let cs = List.ofSeq comps
        if cs.Length >= 2 then
            let pairs =
                if cs.Length <= 5 then
                    [ for i in 0 .. cs.Length - 1 do for j in i + 1 .. cs.Length - 1 -> cs.[i], cs.[j] ]
                else
                    notes.Add $"{cs.Length} parallel paths share one pair of endpoints — only a chain of equations is emitted"
                    List.pairwise cs
            for (a, b) in pairs do
                if equations.Count < maxEquations then equations.Add $"{a} = {b}"
                else truncated <- true
    if truncated then notes.Add $"more than {maxEquations} commuting equations — the diagram is too dense; some were dropped"
    // ---- uniqueness: for !u, any u' satisfying u's equations equals u
    let uniqueness =
        [| for a in arrItems do
            if a.Quant = UniqueExistential then
                let u = a.Name
                let u' = u + "'"
                let mine =
                    equations
                    |> Seq.filter (fun e ->
                        let sides = e.Split([| " = " |], StringSplitOptions.None)
                        sides |> Array.exists (fun side -> Ty.nameUnits side |> List.contains u))
                    |> List.ofSeq
                let (s, t) = a.Endpoints
                let body =
                    (List.foldBack (fun (e: string) acc ->
                        let e' =
                            let sides = e.Split([| " = " |], StringSplitOptions.None)
                            sides |> Array.map (fun side -> renameUnit side u u') |> String.concat " = "
                        $"({e'}) → {acc}") mine $"{u'} = {u}")
                yield $"⋂({u'}:{arrowType s t}) ({body})" |]
    let universals =
        Array.append objItems arrItems
        |> Array.filter (fun it -> it.Quant = Universal && not it.Generated)
        |> Array.map (fun it -> it.Name)
        |> Array.distinct
    let existentials =
        Array.append objItems arrItems
        |> Array.filter (fun it -> it.Quant = Existential || it.Quant = UniqueExistential)
        |> Array.map (fun it -> it.Name, (it.Quant = UniqueExistential))
        |> Array.distinct
    { Items = Array.append objItems arrItems
      ObjectTypings = objectTypings
      ArrowTypings = arrowTypings
      Equations = equations.ToArray()
      UniquenessObligations = uniqueness
      Universals = universals
      Existentials = existentials
      Notes = notes.ToArray()
      Truncated = truncated }

// ---- registry access -------------------------------------------------------

let private graphCache = Dictionary<string, Result<Graph, string>>()

/// Parse with a cache keyed by the JSON text (the registry is re-set on every
/// UI batch, so caching by text rather than by name is what stays correct).
let GraphOfJson (json: string) : Result<Graph, string> =
    match graphCache.TryGetValue json with
    | true, r -> r
    | _ ->
        let r = parse json
        if graphCache.Count > 512 then graphCache.Clear()
        graphCache.[json] <- r
        r

/// The graph behind a sketch name of the active system, if registered and valid.
let LookupSketch (name: string) : Graph option =
    match Map.tryFind name Ty.userSketches with
    | Some json ->
        match GraphOfJson json with
        | Ok g -> Some g
        | Error _ -> None
    | None -> None

// ---- textual views ---------------------------------------------------------

/// The textual judgment group equivalent to "[sketch] in <category>": one
/// object judgment per distinct object name, then one morphism judgment per
/// edge. Unusable/empty labels get generated names (v₁…, e₁…).
type ExpandOutcome = { Ok: bool; Text: string; Error: string }

let TryExpand (input: string) (category: string) : ExpandOutcome =
    match parse input with
    | Error e -> { Ok = false; Text = ""; Error = e }
    | Ok g ->
        let el = elaborate g category ArrowStyle Set.empty
        { Ok = true; Text = String.concat ", " (Array.append el.ObjectTypings el.ArrowTypings); Error = "" }

/// The meaning of "[sketch] commutes" (optionally "in <category>"), for display:
/// its quantifier prefix, the judgments it supplies, and any caveats.
type MeaningOutcome =
    { Ok: bool
      /// "∀ p q … . ∃ u . " — empty when nothing is quantified
      Prefix: string
      Typings: string[]
      Equations: string[]
      Uniqueness: string[]
      Notes: string[]
      Error: string }

let TryExpandCommutes (input: string) (category: string) (bound: string[]) : MeaningOutcome =
    match parse input with
    | Error e -> { Ok = false; Prefix = ""; Typings = [||]; Equations = [||]; Uniqueness = [||]; Notes = [||]; Error = e }
    | Ok g ->
        let el = elaborate g category (HomStyleForActiveSystem ()) (Set.ofArray bound)
        let forall = if el.Universals.Length = 0 then "" else "∀ " + String.concat " " el.Universals + " . "
        let exists =
            if el.Existentials.Length = 0 then ""
            else "∃ " + (el.Existentials |> Array.map (fun (nm, uq) -> if uq then "!" + nm else nm) |> String.concat " ") + " . "
        { Ok = true
          Prefix = forall + exists
          Typings = Array.append el.ObjectTypings el.ArrowTypings
          Equations = el.Equations
          Uniqueness = el.UniquenessObligations
          Notes = el.Notes
          Error = "" }

// ---- JSON rewriting for living diagrams ------------------------------------

let private withJson (input: string) (f: Nodes.JsonArray -> int -> unit) : string =
    try
        let text =
            let t = (input : string).Trim()
            if t.StartsWith "[" then t
            else Text.Encoding.UTF8.GetString(Convert.FromBase64String t)
        let node = Nodes.JsonNode.Parse text
        let arr = node.AsArray()
        let n = arr.[1].GetValue<int>()
        f arr n
        node.ToJsonString(jsonOptions)
    with _ -> input

/// Rewrite labels through a renaming, keeping the ∃ / ! marks: every vertex or
/// edge whose bare label is a key becomes mark + value. This is M(H) — the
/// rule's diagram instantiated by the match.
let Relabel (input: string) (rename: (string * string)[]) : string =
    let map = dict rename
    withJson input (fun arr _ ->
        for i in 2 .. arr.Count - 1 do
            let cell = arr.[i].AsArray()
            if cell.Count >= 3 && not (isNull cell.[2]) then
                let raw = cell.[2].GetValue<string>()
                let m = readLabel raw
                match map.TryGetValue m.Bare with
                | true, fresh ->
                    // an arrow carries its existence in its style, an object
                    // only in its ∃ mark; uniqueness is always a ! mark
                    let isObject = i < 2 + arr.[1].GetValue<int>()
                    let mark = if m.Unique then "!" elif isObject && m.Existential then "∃" else ""
                    cell.[2] <- Nodes.JsonValue.Create(mark + fresh)
                | _ -> ())

/// Set or clear the dashed style of every arrow whose bare label is `label`
/// — a witness turns solid once a later step uses it.
let SetEdgeStyle (input: string) (label: string) (solid: bool) : string =
    withJson input (fun arr n ->
        for i in 2 + n .. arr.Count - 1 do
            let cell = arr.[i].AsArray()
            if cell.Count >= 3 && not (isNull cell.[2]) && (readLabel (cell.[2].GetValue<string>())).Bare = label then
                if solid then
                    if cell.Count >= 5 && not (isNull cell.[4]) then
                        let opts = cell.[4].AsObject()
                        opts.Remove "style" |> ignore
                else
                    while cell.Count < 4 do cell.Add(Nodes.JsonValue.Create 0)
                    if cell.Count < 5 then cell.Add(Nodes.JsonObject())
                    let opts = cell.[4].AsObject()
                    let body = Nodes.JsonObject()
                    body.["name"] <- Nodes.JsonValue.Create "dashed"
                    let style = Nodes.JsonObject()
                    style.["body"] <- body
                    opts.["style"] <- style)

/// Every dashed/dotted arrow's bare label — the witnesses not yet used.
let DashedLabels (input: string) : string[] =
    match parse input with
    | Ok g ->
        [| for k in 0 .. g.Edges.Length - 1 do
            if g.EdgeStyles.[k] = Dashed || g.EdgeStyles.[k] = Dotted then
                yield (readLabel g.EdgeLabels.[k]).Bare |]
    | Error _ -> [||]

// ---- exact rows / columns ----------------------------------------------------

/// The exactness judgments a diagram's rows (or columns) assert: for each line
/// of objects sharing a y (x) coordinate, taken in x (y) order, and each
/// interior object with an arrow in from its predecessor and one out to its
/// successor, "exact(in, out)". A line broken by a missing arrow asserts
/// nothing there; an arrow without a usable label contributes nothing (noted) —
/// label a zero arrow 0 to state exactness at the ends of a sequence.
let private exactAlong (g: Graph) (rows: bool) : string[] * string[] =
    let lineOf v = if rows then snd g.Positions.[v] else fst g.Positions.[v]
    let along v = if rows then fst g.Positions.[v] else snd g.Positions.[v]
    let edgeName k =
        let b = (readLabel g.EdgeLabels.[k]).Bare
        if isUsableName b then Some b else None
    let vertexName v =
        let b = (readLabel g.VertexLabels.[v]).Bare
        if isUsableName b then b else "v" + subscriptDigits (v + 1)
    let stmts = ResizeArray<string>()
    let notes = ResizeArray<string>()
    let lines =
        [ 0 .. g.VertexCount - 1 ]
        |> List.groupBy lineOf
        |> List.sortBy fst
        |> List.map (fun (_, vs) -> vs |> List.sortBy along |> Array.ofList)
        |> List.filter (fun vs -> vs.Length >= 3)
    for vs in lines do
        // the arrow from vs.[i] to vs.[i+1], preferring a labelled one
        let between i =
            let cands = [ for k in 0 .. g.Edges.Length - 1 do if g.Edges.[k] = (vs.[i], vs.[i + 1]) then yield k ]
            match cands |> List.tryFind (fun k -> (edgeName k).IsSome) with
            | Some k -> Some k
            | None -> List.tryHead cands
        for i in 1 .. vs.Length - 2 do
            match between (i - 1), between i with
            | Some a, Some b ->
                match edgeName a, edgeName b with
                | Some na, Some nb -> stmts.Add (sprintf "exact(%s, %s)" na nb)
                | _ -> notes.Add (sprintf "exactness at '%s' is not stated: an adjacent arrow has no usable label" (vertexName vs.[i]))
            | _ -> ()
    stmts.ToArray(), notes.ToArray()

/// (exact(f, g) judgments, notes) of a sketch's rows (rows = true) or columns.
let ExactStatements (input: string) (rows: bool) : string[] * string[] =
    match GraphOfJson input with
    | Ok g -> exactAlong g rows
    | Error e -> [||], [| e |]

/// Display form of "[D] has exact rows/columns".
let TryExpandExact (input: string) (rows: bool) : MeaningOutcome =
    match GraphOfJson input with
    | Error e -> { Ok = false; Prefix = ""; Typings = [||]; Equations = [||]; Uniqueness = [||]; Notes = [||]; Error = e }
    | Ok g ->
        let stmts, notes = exactAlong g rows
        let notes =
            if stmts.Length = 0 then
                Array.append notes [| sprintf "no %s has an interior object — nothing is asserted" (if rows then "row" else "column") |]
            else notes
        { Ok = true; Prefix = ""; Typings = [||]; Equations = stmts; Uniqueness = [||]; Notes = notes; Error = "" }

/// Save-time advice for a sketch under the ∀∃ reading: labels that will not
/// behave as the author expects (anonymous items, duplicated names, declared
/// constants that can never be metavariables).
let LabelWarnings (input: string) : string[] =
    match parse input with
    | Error e -> [| e |]
    | Ok g ->
        let el = elaborate g "" ArrowStyle Set.empty
        [| yield! el.Notes
           for it in el.Items do
               if it.Generated && not (System.String.IsNullOrWhiteSpace it.Raw) then
                   yield sprintf "'%s' is not a usable name — the %s is anonymous and matches by shape only" it.Raw (if it.IsObject then "object" else "arrow")
           let rigid =
               el.Items
               |> Array.filter (fun it -> not it.Generated && it.Quant = Bound)
               |> Array.map (fun it -> it.Name)
               |> Array.distinct
           for nm in rigid do
               if Ty.userConstants.Contains nm || Ty.constantAtoms.Contains nm then
                   yield sprintf "'%s' is a declared constant — it matches only itself, never a metavariable" nm |]

// ---- shape matching --------------------------------------------------------

let maxSearchNodes = 20000

/// Every label-correspondence induced by a subgraph MONOMORPHISM of `pat`
/// into `tgt` (objects injective, arrows injective with matching endpoints;
/// extra target arrows are allowed — a subquiver of a commuting diagram
/// commutes). Each element pairs the pattern's non-generated object and
/// arrow names with the target names they land on; the caller decides
/// whether those pairs are consistent with its substitution. Existential
/// pattern arrows may land on a target arrow of any style — that is the
/// auto-instantiation, the witness IS the target's arrow — while a ! arrow
/// needs a ! target arrow or the only arrow between those two objects.
/// Lazy, degree-ordered, and cut off after maxSearchNodes.
let Embeddings (pat: Graph) (tgt: Graph) : seq<(string * string) list> =
    let pe = elaborate pat "" ArrowStyle Set.empty
    let te = elaborate tgt "" ArrowStyle Set.empty
    let pObj = pe.Items |> Array.filter (fun i -> i.IsObject)
    let pArr = pe.Items |> Array.filter (fun i -> not i.IsObject)
    let tObj = te.Items |> Array.filter (fun i -> i.IsObject)
    let tArr = te.Items |> Array.filter (fun i -> not i.IsObject)
    let np, nt = pat.VertexCount, tgt.VertexCount
    if np > nt || pat.Edges.Length > tgt.Edges.Length then Seq.empty else
    let degrees (g: Graph) =
        let o = Array.zeroCreate g.VertexCount
        let i = Array.zeroCreate g.VertexCount
        for (s, t) in g.Edges do
            o.[s] <- o.[s] + 1
            i.[t] <- i.[t] + 1
        o, i
    let outP, inP = degrees pat
    let outT, inT = degrees tgt
    let order = [ 0 .. np - 1 ] |> List.sortByDescending (fun v -> outP.[v] + inP.[v])
    let budget = ref 0
    let rec assignVertices (vs: int list) (vmap: Map<int, int>) (usedT: Set<int>) : seq<Map<int, int>> =
        seq {
            match vs with
            | [] -> yield vmap
            | v :: rest ->
                for w in 0 .. nt - 1 do
                    if not (usedT.Contains w) && outT.[w] >= outP.[v] && inT.[w] >= inP.[v] then
                        budget.Value <- budget.Value + 1
                        if budget.Value < maxSearchNodes then
                            yield! assignVertices rest (Map.add v w vmap) (Set.add w usedT) }
    let rec assignEdges (ks: int list) (vmap: Map<int, int>) (emap: Map<int, int>) (usedE: Set<int>) : seq<Map<int, int>> =
        seq {
            match ks with
            | [] -> yield emap
            | k :: rest ->
                let (s, t) = pat.Edges.[k]
                let ends = vmap.[s], vmap.[t]
                let parallelCount = tgt.Edges |> Array.filter (fun e -> e = ends) |> Array.length
                for j in 0 .. tgt.Edges.Length - 1 do
                    if not (usedE.Contains j) && tgt.Edges.[j] = ends then
                        let uniqueOk =
                            pArr.[k].Quant <> UniqueExistential
                            || tArr.[j].Quant = UniqueExistential
                            || parallelCount = 1
                        if uniqueOk then
                            yield! assignEdges rest vmap (Map.add k j emap) (Set.add j usedE) }
    seq {
        for vmap in assignVertices order Map.empty Set.empty do
            for emap in assignEdges [ 0 .. pat.Edges.Length - 1 ] vmap Map.empty Set.empty do
                yield [ for KeyValue (v, w) in vmap do
                            if not pObj.[v].Generated then yield pObj.[v].Name, tObj.[w].Name
                        for KeyValue (k, j) in emap do
                            if not pArr.[k].Generated then yield pArr.[k].Name, tArr.[j].Name ] }

let rec private permutations (xs: 'a list) : 'a list list =
    match xs with
    | [] | [ _ ] -> [ xs ]
    | _ ->
        [ for i in 0 .. xs.Length - 1 do
            let x = xs.[i]
            let rest = xs |> List.mapi (fun j y -> j, y) |> List.choose (fun (j, y) -> if j = i then None else Some y)
            for p in permutations rest -> x :: p ]

let private maxCandidates = 20000

/// Canonicalize: choose the ordering of edges (permuting invariant-tied edges
/// in the worst case) whose de Bruijn rendering is lexicographically least.
/// Vertices mentioned in `title` stay free (#k); all others are ∀-bound (@k).
/// `report` receives (candidatesDone, candidatesTotal); `ct` cancels.
let run (json: string) (title: string) (report: Action<int, int>) (ct: CancellationToken) : CanonicalOutcome =
    match parse json with
    | Error e -> { Ok = false; Diagram = emptyDiagram; Error = e }
    | Ok g ->
        try
            let n = g.VertexCount
            let isFree v =
                let l = g.VertexLabels.[v]
                l <> "" && not (String.IsNullOrWhiteSpace title) && title.Contains l
            let outd = Array.zeroCreate n
            let ind = Array.zeroCreate n
            for (s, t) in g.Edges do
                outd.[s] <- outd.[s] + 1
                ind.[t] <- ind.[t] + 1

            // invariant used to pre-sort edges; only tied edges get permuted
            let key (s, t) =
                (not (isFree s), not (isFree t),
                 -outd.[s], -ind.[s], -outd.[t], -ind.[t], (s = t))
            let sorted = g.Edges |> Array.sortBy key |> List.ofArray
            let groups = sorted |> List.groupBy key |> List.map snd
            let choices = groups |> List.map (fun grp -> if grp.Length <= 5 then permutations grp else [ grp ])
            let total = choices |> List.fold (fun acc c -> min (acc * c.Length) maxCandidates) 1

            // render one candidate ordering; edge tokens are allocated before
            // their endpoints, matching {@0}:{@1}\to{@2}
            let render (ordering: (int * int) list) =
                let tokens = Dictionary<int, string>()
                let mutable nextBound = 0
                let mutable nextFree = 0
                let boundLog = ResizeArray()
                let freeLog = ResizeArray()
                let tok v =
                    match tokens.TryGetValue v with
                    | true, t -> t
                    | _ ->
                        let t =
                            if isFree v then
                                let t = $"#{nextFree}"
                                nextFree <- nextFree + 1
                                freeLog.Add(t, g.VertexLabels.[v])
                                t
                            else
                                let t = $"@{nextBound}"
                                nextBound <- nextBound + 1
                                boundLog.Add(t, (if g.VertexLabels.[v] = "" then "·" else g.VertexLabels.[v]))
                                t
                        tokens.[v] <- t
                        t
                let lines =
                    [| for (s, t) in ordering ->
                        let e = $"@{nextBound}"
                        nextBound <- nextBound + 1
                        let st = tok s
                        let tt = tok t
                        $"{{{e}}}:{{{st}}}→{{{tt}}}" |]
                // isolated vertices still need binders / free names
                for v in 0 .. n - 1 do
                    if not (tokens.ContainsKey v) then tok v |> ignore
                lines, boundLog.ToArray(), freeLog.ToArray()

            let cartesian (lists: (int * int) list list list) =
                lists |> List.fold (fun acc xs -> [ for a in acc do for x in xs -> a @ [ x ] ] |> List.truncate maxCandidates) [ [] ]

            let mutable best : (string * ((int * int) list)) option = None
            let mutable count = 0
            for combo in cartesian choices do
                ct.ThrowIfCancellationRequested()
                let ordering = List.concat combo
                let lines, _, _ = render ordering
                let k = String.Join("\n", lines)
                match best with
                | Some (bk, _) when String.CompareOrdinal(k, bk) >= 0 -> ()
                | _ -> best <- Some (k, ordering)
                count <- count + 1
                if count % 32 = 0 || count = total then report.Invoke(count, total)
            report.Invoke(total, total)

            let ordering = match best with Some (_, o) -> o | None -> []
            let lines, boundLog, freeLog = render ordering
            let prefix =
                if boundLog.Length = 0 then ""
                else "∀ " + String.Join(" ", boundLog |> Array.map (fun (t, _) -> $"{{{t}}}"))
            { Ok = true
              Diagram =
                { Lines = lines
                  ForallPrefix = prefix
                  FreeMap = freeLog
                  BoundMap = boundLog
                  SkippedHigherCells = g.SkippedHigherCells
                  Candidates = count }
              Error = "" }
        with
        | :? OperationCanceledException ->
            { Ok = false; Diagram = emptyDiagram; Error = "Cancelled." }
        | ex ->
            { Ok = false; Diagram = emptyDiagram; Error = ex.Message }
