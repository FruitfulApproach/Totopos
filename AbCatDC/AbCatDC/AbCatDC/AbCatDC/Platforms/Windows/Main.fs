namespace AbCatDC.WinUI

open System

module Program =
    [<EntryPoint; STAThread>]
    let main args =
        do FSharp.Maui.WinUICompat.Program.Main(args, typeof<AbCatDC.WinUI.App>)
        0
