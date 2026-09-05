namespace AbCatDC

open Microsoft.Extensions.DependencyInjection
open Microsoft.Maui.Accessibility
open Microsoft.Maui.Hosting
open Fabulous.Maui
open CommunityToolkit.Maui
open CommunityToolkit.Maui.Markup
open Microsoft.Maui.Foldable
open AbCatDC.Extensions
open AbCatDC.RazorLib.Data
open AbCatDC.RazorLib.Services

type MauiProgram =
    static member CreateMauiApp() =
        MauiApp
            .CreateBuilder()
            .UseFabulousApp(App.program)
            .UseMauiCommunityToolkit()
            .UseMauiCommunityToolkitMarkup()
            .UseFoldable()
            .ConfigureBlazorWebView()
            .ConfigureFonts(fun fonts ->
                fonts
                    .AddFont("OpenSans-Regular.ttf", "OpenSansRegular")
                    .AddFont("OpenSans-Semibold.ttf", "OpenSansSemibold")
                |> ignore)
            .ConfigureServices(fun services ->
                services
                    .AddSingleton<WeatherForecastService>()
                    .AddSingleton<ISnapshotService, SnapshotService>()
                    .AddScoped<AbCatDC.RazorLib.Services.AppSettingsService>()
#if DEBUG
                    .AddBlazorWebViewDeveloperTools()
#endif
                |> ignore)
#if DEBUG
            .AddDebugLog()
#endif
            .Build()
