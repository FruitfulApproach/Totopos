namespace AbCatDC.Kernel

/// C#-friendly result for the frontend test screen.
type RunResult =
    { Success: bool
      Formatted: string
      Canonical: string
      /// "canonical ← original" lines for bound and free variables
      VariableMap: string
      /// inferred typing judgments for the expression's leaves
      Inference: string
      /// the expression after elaboration (powers expanded to products);
      /// empty when elaboration changes nothing
      Elaborated: string
      Error: string }

module Api =

    /// Parse the input, format it, canonicalize it, and render the
    /// canonical-name -> original-name mapping — everything as plain strings.
    let Run (input: string) : RunResult =
        match Parser.parse input with
        | Error e ->
            { Success = false; Formatted = ""; Canonical = ""; VariableMap = ""; Inference = ""; Elaborated = ""; Error = e }
        | Ok ty ->
            // elaborate first (A^2 ⇒ A × A), so that A^2 and A × A share a
            // canonical form — they are definitionally equal
            let formatted = Ty.format ty
            let elaborated = Ty.expandPowers ty
            let elaboratedText = Ty.format elaborated
            let c = Canonical.canonicalize elaborated
            let mapLines =
                [ for canon, orig in c.BoundVars -> $"{canon} ← {orig}  (bound)"
                  for KeyValue (canon, orig) in c.FreeVars -> $"{canon} ← {orig}  (free)" ]
                |> String.concat "\n"
            let inferLines =
                Infer.infer ty
                |> List.map (fun j ->
                    let rel = if j.Type.StartsWith "⊆" then j.Type else ": " + j.Type
                    $"{j.Subject} {rel}  — {j.Note}")
                |> String.concat "\n"
            { Success = true
              Formatted = formatted
              Canonical = Ty.format c.Ty
              VariableMap = mapLines
              Inference = inferLines
              Elaborated = (if elaboratedText = formatted then "" else elaboratedText)
              Error = "" }
