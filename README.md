# TCG Card Presenter

TCG Card Presenter is a third-party native OBS Studio plugin for searching and presenting trading card images during livestreams and recordings.

The current version supports **Magic: The Gathering** card lookup through the Scryfall API. Other TCGs are visible in the dock as in-development options, but are not active yet.

## Features

- Native OBS source: `TCG Card Presenter`
- Native OBS dock panel
- Magic: The Gathering card search via Scryfall
- Card image preview
- Local card image cache
- Manual local image selection fallback
- Show and hide controls
- Configurable enter, visible, and exit durations
- Basic transition animations: fade, slide, zoom, cut
- TCG selector with non-Magic games marked as in development

## Status

This is an early release. Magic: The Gathering support is functional; other TCG integrations are planned.

## Installation

Download a release ZIP when available, then copy the plugin files into your OBS Studio installation.

For local Windows builds from source:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
powershell -ExecutionPolicy Bypass -File .\scripts\install-windows.ps1
```

The installer copies:

- `tcg-card-presenter.dll` to `C:\Program Files\obs-studio\obs-plugins\64bit`
- plugin resources to `C:\Program Files\obs-studio\data\obs-plugins\tcg-card-presenter`
- Qt TLS backend plugins needed for HTTPS Scryfall lookups

Restart OBS after installing.

## Usage

1. Open OBS Studio.
2. Open `Docks > TCG Card Presenter`.
3. Create or select a `TCG Card Presenter` source.
4. Search a Magic card by English name, or choose a local image.
5. Adjust timing and animation settings.
6. Press `Show`.

## Building

This repository is based on the official `obs-plugintemplate` structure.

Requirements:

- CMake
- Visual Studio 2022 with Desktop development with C++
- OBS plugin template dependencies, downloaded automatically during CMake configure

Windows:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

## Third-Party Notice

This plugin is not affiliated with or endorsed by OBS Studio, the OBS Project, Wizards of the Coast, Magic: The Gathering, or Scryfall.

Card data and images are provided through the Scryfall API. Please respect Scryfall's API guidelines and terms.

## AI-Assisted Development Disclosure

This plugin was developed by Adriaabad with AI-assisted coding support. The code has been built and manually tested before release.

## License

Licensed under GPL-2.0-or-later.
