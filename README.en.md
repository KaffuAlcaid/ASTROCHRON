<p align="center">
  <img src="assets/app-icon/astrochron-256.png" alt="ASTROCHRON icon" width="112" height="112">
</p>

<h1 align="center">ASTROCHRON · 星纪</h1>

<p align="center">A desktop application for viewing satellite tracks, predicting passes, and planning observations</p>

<p align="center"><a href="README.md">简体中文</a> | English</p>

![ASTROCHRON main window: satellite map, observation information, and pass predictions](docs/images/overview.png)

## Features

- **Satellite map**: View satellite positions and ground tracks; zoom, pan, or expand the map, and choose layers such as tracks, day/night, cities, and footprints
- **Target following**: Click the follow button or press F to follow the current satellite at 9× zoom; repeat to restore the previous view. Following remains active while zooming
- **24 h time navigation**: Browse positions within 12 h before and after the current time, with the map, sky chart, and observation information updating together
- **Pass predictions**: View start, culmination, and end times, along with maximum elevation, duration, and optical conditions; click a pass to jump to its culmination
- **Observation plans**: Export passes for the 12 h following the selected time as CSV or ICS, including UTC time, local time, observing location, and orbital epoch
- **Watchlist**: Save satellites as needed and view their next pass, peak elevation, and optical conditions within the next 24 h; sort by watchlist order or next pass. Browse the catalog by constellation and search by name, NORAD ID, or international designator
- **Observation information**: View azimuth, elevation, range, estimated apparent magnitude, and weather, and calculate Doppler shift for a receiving frequency
- **Orbital data**: Inspect epochs, orbital elements, data sources, and dates; import TLE / OMM files and use locally stored orbital data
- **Historical snapshots**: Pin important orbital data, preview records scheduled for cleanup, and set the number of online historical snapshots retained per group; local imports are retained indefinitely
- **Observing locations**: Search for a city, enter coordinates, or select a point on the map; set altitude, minimum elevation, and time zone, and save frequently used locations
- **Interface language**: Choose Simplified Chinese, English, or the system default on first launch or in settings; changes take effect immediately
- **Refresh settings**: Set map rendering and orbit calculation rates separately; map markers move smoothly between successive sampled positions
- **Software updates**: Manually check for releases in settings, view release notes, and open the download page; optionally include pre-release versions
- **Keyboard controls**: Quickly search, import, and export; zoom with the main keyboard or numeric keypad, and press Space to expand or collapse the map

## Download

Current version: [v0.1.0](https://github.com/KaffuAlcaid/ASTROCHRON/releases/tag/v0.1.0), for Windows 11 x64

| Download | Usage |
| --- | --- |
| [Windows installer](https://github.com/KaffuAlcaid/ASTROCHRON/releases/download/v0.1.0/ASTROCHRON-v0.1.0-windows-x64-setup.exe) | Choose an installation directory in the setup wizard |
| [Portable ZIP](https://github.com/KaffuAlcaid/ASTROCHRON/releases/download/v0.1.0/ASTROCHRON-v0.1.0-windows-x64.zip) | Extract the entire archive before running |

The installer can install over an existing installation in the same directory. Observing locations, the watchlist, and local orbital data are stored under the current Windows user account and are preserved when installing over an existing installation; they are not automatically migrated when switching accounts

## Quick Start

1. Install and launch the application, or extract the ZIP and run `ASTROCHRON.exe`
2. On first launch, choose a language and observing city, confirm the location, and leave the minimum elevation at its default of `10°`
3. On first use, connect to the internet to retrieve CelesTrak orbital data, or import a TLE/OMM file in "Satellite catalog"; if online retrieval fails, check the network connection and try again. Once the catalog has loaded, select a target in the watchlist on the left, such as the International Space Station
4. Click a pass below the map to move the map, sky chart, and observation information to that pass's culmination
5. Drag the bottom timeline to view positions before and after the pass; click "Back to now" to resume live tracking

See the [User Guide](docs/user-guide.en.md) for more operations

## Documentation

| Document | Contents |
| --- | --- |
| [User Guide](docs/user-guide.en.md) | Selecting locations, managing the watchlist, viewing passes, and configuring observation parameters |
| [Data Sources](docs/data-sources.en.md) | Data providers, source dates, online queries, and local storage |
| [Calculation Notes](docs/calculations.en.md) | Orbit propagation, coordinates, passes, apparent magnitude, and Doppler calculations |
| [Principles of SGP4 Orbit Propagation](docs/orbit-model.en.md) | Calculating TEME states and local observing directions from mean elements |

## Data and Calculations

Satellite orbital data comes from [CelesTrak](https://celestrak.org/NORAD/elements/). Orbit propagation uses the Vallado SGP4 implementation, including deep-space orbit handling. Maps and cities come from [Natural Earth](https://www.naturalearthdata.com/), and weather and terrain elevation queries are provided by [Open-Meteo](https://open-meteo.com/)

Apparent magnitude uses the McCants / QuickSat magnitude catalog and historical photometric observations of the Chinese Space Station to estimate brightness above the atmosphere from range and phase angle, with support for manual parameters and magnitude catalog imports

The bundled QuickSat magnitude catalog has a file date of **2020-09-14**, and the Chinese Space Station reference has a report date of **2022-08-03**. Public references were checked through **2026-09-22**; see [Data Sources](docs/data-sources.en.md) for individual versions and dates

The application's "Sources" tab displays the sources and dates currently in use. Orbit predictions are affected by element epochs and satellite maneuvers; observed brightness is also affected by attitude, configuration, and the atmosphere. Past positions on the timeline are likewise calculated using the orbital model

Reference project: [ShenMian/tracker](https://github.com/ShenMian/tracker)

Inspired by: *仰望夜空的星辰* (見上げてごらん、夜空の星を)

## License

ASTROCHRON source code is licensed under [Apache-2.0](LICENSE)

Third-party software and data are subject to their respective licenses and terms of use. See [Third-Party Notices](THIRD_PARTY_NOTICES.en.md) for attribution, sources, and license file locations
