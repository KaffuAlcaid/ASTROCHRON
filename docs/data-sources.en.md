# Data Sources

[简体中文](data-sources.md) | English

[Home](../README.en.md) · [Calculation Notes](calculations.en.md) · [Third-Party Notices](../THIRD_PARTY_NOTICES.en.md)

Applies to: `v0.1.0`. Public references were checked through **2026-09-22**

Orbital elements are dated by each satellite's epoch, and photometric data by the original observation date or source date. Web reference checks, file updates, and local acquisition times are recorded separately; data updates may be delayed

## Data Used by the Application

| Data | Source | Versions and dates |
| --- | --- | --- |
| Satellite orbital elements | [CelesTrak GP](https://celestrak.org/NORAD/elements/) or imported OMM / TLE files | Each satellite's epoch is shown in the "Orbit" tab, and local acquisition time in the "Sources" tab |
| Base reference magnitudes | [Mike McCants / QuickSat](https://www.mmccants.org/programs/qsmag.zip) | Bundled `qs.mag` file dated 2020-09-14 |
| Chinese Space Station reference magnitude | [Jay Respler / SeeSat-L](https://www.satobs.org/seesat/Aug-2022/0030.html) | Report dated 2022-08-03, corresponding to the configuration at that time |
| Weather | [Open-Meteo Weather Forecast API](https://open-meteo.com/en/docs) | Hourly forecasts; the interface shows forecast times and acquisition time |
| World map and cities | [Natural Earth](https://www.naturalearthdata.com/), 1:50 million | Land 4.1.0, lakes 5.0.0, borders 5.1.0, cities 5.1.2 |
| Terrain elevation | [Open-Meteo Elevation API](https://open-meteo.com/en/docs/elevation-api) / Copernicus DEM | 2021 GLO-90, approximately 90 m resolution, EGM2008 vertical datum |
| Geoid | [NGA / GeographicLib EGM2008](https://geographiclib.sourceforge.io/C++/doc/geoid.html) | 5′ grid, file dated 2009-08-29 |

The map, cities, geoid, and default magnitude catalog are bundled with the application. Orbital data is retrieved from CelesTrak or imported by the user; weather and terrain elevation are obtained through online queries

### Orbital Catalog

CelesTrak provides GP JSON by data group. The application retains NORAD IDs, COSPAR international designators, epochs, and orbital parameters, and saves retrieved data as local snapshots

When a file or snapshot contains multiple valid element sets with the same NORAD ID, the one with the latest epoch is used. If epochs are equal, the first record in the file is retained. The original file content is saved with the snapshot

OMM uses UTC, SGP4, and the TEME reference frame. When `TIME_SYSTEM`, `MEAN_ELEMENT_THEORY`, and `REF_FRAME` are omitted, these conventions are assumed; explicit values must match them. Loading messages state the reason for invalid records. When some records are usable, the number of invalid records skipped is also listed; if multiple records are invalid, only the last reason is shown

The local catalog collects objects already retrieved, the watchlist stores targets selected by the user, and a source group's record count corresponds to a particular download. These counts have different meanings; actual navigation service status should be checked against official constellation status information

Docked space station modules are grouped as an assembly. The Chinese Space Station is represented by the Tianhe core module, `TIANHE / CSS`; spacecraft with independent orbits remain separate objects

After a successful retrieval, downloads from the same source are spaced at least 2 h apart. A manual retry after a failure usually requires a wait of at least 60 s, or longer if specified by the server's `Retry-After`

### Magnitude Data

QuickSat's reference conditions are maximum brightness at a range of 1000 km and full phase. The bundled catalog matches records using both NORAD ID and COSPAR international designator

For the Chinese Space Station, the application uses the mean QuickSat intrinsic magnitude of **0.87 mag** from three observations in the report dated 2022-08-03. This corresponds to the station's configuration at that time in 2022; changes in modules, docked spacecraft, and attitude all affect brightness

Manual parameters take precedence over imported data, which takes precedence over bundled data. Importing a magnitude catalog preserves existing manual parameters. Source dates and local save times are displayed separately; see [Apparent Magnitude](calculations.en.md#apparent-magnitude) for the calculation method

### Map and Cities

The application uses Natural Earth's `land`, `lakes`, `admin_0_boundary_lines_land`, and full `populated_places` layers. City attributes include multilingual names

The map uses an equirectangular projection, displaying longitude and latitude as rectangular coordinates. Natural Earth cities are used for map labels and location searches; the specific observing position can be refined through coordinates or map selection

### Weather and Altitude

Weather query range parameters request the past 1 day and a 2-day forecast, with hourly values and a periodic refresh interval of 1 h. The actual period available for viewing depends on the returned forecast data

Terrain elevation uses Copernicus DEM. For observations inside buildings or on rooftops, the height can be entered manually. The application combines it with EGM2008 geoid undulation to calculate the observer's ellipsoidal height

## GNSS and Photometric References

The following references are used to look up identities, PRN assignment history, orbital planes, service status, and brightness observations. The scope of data automatically retrieved in this version is listed in the table above; reference web pages can be consulted using the satellite identifiers shown in the application

| Reference | Source | Date information |
| --- | --- | --- |
| GNSS identifiers, orbital planes, slots, and assignment history | [IGS satellite metadata](https://files.igs.org/pub/station/general/igs_satellite_metadata.snx) | File version dated 2026-09-02 |
| Galileo operational status | [GSC constellation status](https://www.gsc-europa.eu/system-service-status/constellation-information) | Checked on 2026-09-22 |
| Galileo orbital planes and slots | [GSC orbital and technical parameters](https://www.gsc-europa.eu/system-service-status/orbital-and-technical-parameters) | Checked on 2026-09-22 |
| Galileo almanac | [GSC Almanac](https://www.gsc-europa.eu/gsc-products/almanac) | Sample published on 2026-09-18 |
| BeiDou identities, operational status, and health status | [BeiDou Test and Assessment Research Center](https://www.csno-tarc.cn/status/constellation) | Status table published on 2026-09-22 |
| Combined magnitude and radar cross-section catalog | [Stellarium satellite data](https://github.com/Stellarium/stellarium-data/tree/master/satellites) | File updated on 2026-09-11, including historical photometric records |
| Measured satellite brightness | [SCORE](https://score.cps.iau.org/) | At the time of checking, the database contained observations through 2026-09-22; each observation has its own timestamp, CC BY 4.0 |

A brightness observation corresponds to a specific time, range, phase, and attitude. Using measured brightness as a reference magnitude also requires the conversion conditions to be specified

## Network Access and Local Storage

| Operation | Information sent | Local storage |
| --- | --- | --- |
| Retrieve orbital data | CelesTrak group name | Orbital data, source, and acquisition time |
| Query weather | Observing location coordinates, forecast variables, and time range | Forecast used in the current session |
| Query altitude | Observing location coordinates | Observing location altitude and source |
| Import orbital or magnitude data | Selected file read locally | Imported data and dates |

The orbital and magnitude database is `%LOCALAPPDATA%\ASTROCHRON\ASTROCHRON\orbits.sqlite`. Observing locations, the watchlist, and interface settings are stored in the Windows user configuration

The application folder can be moved independently. With local orbital data already available, the map, calculated tracks, and passes can be viewed offline; weather and online catalog queries require an internet connection

See [Third-Party Notices](../THIRD_PARTY_NOTICES.en.md) for data attribution, original distribution notes, and license file locations
