# TCG Card Presenter

**TCG Card Presenter** is a native OBS Studio plugin for searching, previewing, and presenting trading card images during livestreams, recordings, deck techs, tournament coverage, and tabletop content.

The first public version focuses on **Magic: The Gathering** through the Scryfall API. Other trading card games are already listed in the dock as in-development targets and will be added progressively.

## Current Status

TCG Card Presenter is in early development. The plugin is usable today for Magic: The Gathering, but the interface, packaging, cache behavior, and supported games may evolve between releases.

| Area | Status |
| --- | --- |
| Native OBS source | Available |
| Native OBS dock | Available |
| Magic: The Gathering search | Available |
| Online card image loading | Available |
| Local image fallback | Available |
| Transition controls | Available |
| Pokemon TCG support | In development |
| Yu-Gi-Oh! support | In development |
| Disney Lorcana support | In development |

## Features

- Native OBS source: `TCG Card Presenter`
- Native OBS dock: `Docks > TCG Card Presenter`
- Magic: The Gathering card search by English card name
- Card data and images from Scryfall
- Card preview before showing it on stream
- Local image cache for downloaded cards
- Manual local image selection fallback
- Show and hide controls
- Configurable enter, visible, and exit durations
- Basic transition animations: fade, slide, zoom, and cut
- TCG selector with future games marked as in development

## Release Roadmap

This roadmap is intentionally listed without dates. Priorities may change as the plugin is tested in real OBS workflows.

| Release Track | Focus | Planned Work |
| --- | --- | --- |
| v0.1.x | Magic foundation | Stabilize the OBS dock, source rendering, Scryfall lookup, HTTPS loading, and Windows ZIP packaging. |
| v0.2.x | Streamer controls | Improve cache handling, source selection, repeated show/hide reliability, and user-facing settings. |
| v0.3.x | Pokemon TCG | Add Pokemon card search, image selection, and game-specific result handling. |
| v0.4.x | Yu-Gi-Oh! | Add Yu-Gi-Oh! card search, image support, and game-specific metadata handling. |
| v0.5.x | Disney Lorcana | Add Disney Lorcana card lookup and presentation support. |
| v0.6.x | Presentation polish | Add more animation presets, layout options, card sizing controls, and improved preview behavior. |
| v1.0 | Stable release | Prepare a stable OBS Forum release with documentation, packaged downloads, and tested install flow. |

## Installation

Download the latest Windows release ZIP from the GitHub releases page.

To install manually:

1. Close OBS Studio.
2. Extract the ZIP into your OBS Studio installation folder, usually:

   ```text
   C:\Program Files\obs-studio
   ```

3. Reopen OBS Studio.
4. Open `Docks > TCG Card Presenter`.
5. Add or create a `TCG Card Presenter` source.

The release ZIP is structured so it can be extracted directly over the OBS Studio folder:

```text
obs-plugins/64bit/tcg-card-presenter.dll
data/obs-plugins/tcg-card-presenter/
bin/64bit/tls/
```

The `bin/64bit/tls` files are included so Qt can perform HTTPS requests to Scryfall on Windows.

## Usage

1. Open OBS Studio.
2. Open `Docks > TCG Card Presenter`.
3. Create or select a `TCG Card Presenter` source.
4. Select `Magic: The Gathering`.
5. Search for a card using its English name.
6. Confirm the preview image.
7. Choose enter, visible, and exit timing.
8. Press `Show`.

You can also use a local image if online lookup is unavailable or if you want to present a custom card image.

## Building From Source

This repository follows the official `obs-plugintemplate` project structure.

Requirements:

- CMake
- Visual Studio 2022
- Desktop development with C++
- OBS plugin template dependencies

Windows build:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

Local install after building:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-windows.ps1
```

The install script copies:

- `tcg-card-presenter.dll` to `C:\Program Files\obs-studio\obs-plugins\64bit`
- plugin resources to `C:\Program Files\obs-studio\data\obs-plugins\tcg-card-presenter`
- Qt TLS backend plugins required for HTTPS card lookups

Restart OBS after installing or updating the plugin.

## Notes For Streamers

- Magic card search currently expects English card names.
- Downloaded card images are stored in the plugin cache.
- Non-Magic TCG options are visible in the dock but are not active yet.
- This is an early plugin, so testing before using it in a live production scene is recommended.

## Third-Party Notice

This plugin is not affiliated with or endorsed by OBS Studio, the OBS Project, Wizards of the Coast, Magic: The Gathering, Scryfall, The Pokemon Company, Konami, Disney, Ravensburger, or any other TCG rights holder.

Card data and images are provided by their respective services and rights holders. Please respect the terms, guidelines, and intellectual property policies of each data provider and game publisher.

Magic: The Gathering card data and images are currently retrieved through the Scryfall API.

## AI-Assisted Development Disclosure

This plugin was developed by Adriaabad with AI-assisted coding support. The code has been built and manually tested before release.

## License

Licensed under GPL-2.0-or-later.
