module AbCatDC.Kernel.QuiverImport

open System
open System.Collections.Generic
open System.Text.Json
open System.Threading

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
            all.Add (c.[0].GetInt32(), c.[1].GetInt32(), label)
        let oneCells = all |> Seq.filter (fun (s, t, _) -> 0 <= s && s < n && 0 <= t && t < n) |> Array.ofSeq
        Ok { VertexCount = n
             VertexLabels = labels
             Positions = positions
             Edges = oneCells |> Array.map (fun (s, t, _) -> s, t)
             EdgeLabels = oneCells |> Array.map (fun (_, _, l) -> l)
             SkippedHigherCells = all.Count - oneCells.Length }
    with ex -> Error $"Could not parse quiver JSON: {ex.Message}"

/// C#-friendly parse wrapper for the preview renderer.
type ParseOutcome =
    { Ok: bool
      Graph: Graph
      Error: string }

let private emptyGraph =
    { VertexCount = 0; VertexLabels = [||]; Positions = [||]; Edges = [||]; EdgeLabels = [||]; SkippedHigherCells = 0 }

let TryParse (input: string) : ParseOutcome =
    match parse input with
    | Ok g -> { Ok = true; Graph = g; Error = "" }
    | Error e -> { Ok = false; Graph = emptyGraph; Error = e }

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
        node.ToJsonString()
    with _ -> input

// ---- diagram → textual judgment group -------------------------------------

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

/// The textual judgment group equivalent to "[sketch] in <category>":
/// one object judgment per DISTINCT label (two vertices labeled X are separate
/// diagram nodes, but their label — and hence their value as an object — is
/// the same, so they yield a single X : C), then one morphism judgment per
/// edge, endpoints referred to by label. Unusable/empty labels get generated
/// names (v1…, e1…).
type ExpandOutcome = { Ok: bool; Text: string; Error: string }

let TryExpand (input: string) (category: string) : ExpandOutcome =
    match parse input with
    | Error e -> { Ok = false; Text = ""; Error = e }
    | Ok g ->
        let objName i =
            let l = normalizeLabel g.VertexLabels.[i]
            if l <> "" && isUsableName l then l else $"v{i + 1}"
        let objects =
            [ for i in 0 .. g.VertexCount - 1 -> objName i ]
            |> List.distinct
            |> List.map (fun o -> $"{o} : {category}")
        let morphisms =
            [ for k in 0 .. g.Edges.Length - 1 ->
                let (s, t) = g.Edges.[k]
                let l = normalizeLabel g.EdgeLabels.[k]
                let name = if l <> "" && isUsableName l then l else $"e{k + 1}"
                $"{name} : {objName s} → {objName t}" ]
            // same label + same endpoints = the same morphism drawn twice
            |> List.distinct
        { Ok = true; Text = String.concat ", " (objects @ morphisms); Error = "" }

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
