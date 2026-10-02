# Third-Party Data and Software

[简体中文](THIRD_PARTY_NOTICES.md) | English

## CelesTrak

Satellite orbital elements come from [CelesTrak](https://celestrak.org/NORAD/elements/) and are retrieved through the [GP data service](https://celestrak.org/NORAD/documentation/gp-data-formats.php)

Locally stored orbital data retains its source, acquisition time, and the epoch of each object. Data use is subject to the provider's terms; the project's Apache-2.0 source code license applies to ASTROCHRON's own code

## Magnitude Data

McCants / QuickSat magnitude catalog: Mike McCants; the `qs.mag` file is dated 2020-09-14.

Source: <https://www.mmccants.org/programs/qsmag.zip>. The reference conditions are maximum brightness at a range of 1000 km and full phase.

Chinese Space Station reference magnitude: Jay Respler, 2022-08-03. The mean QuickSat intrinsic magnitude from three observations is 0.87 mag, corresponding to the configuration at that time.

Source: <https://www.satobs.org/seesat/Aug-2022/0030.html>.

See `assets/photometry/NOTICE` and `assets/photometry/QUICKSAT.txt` for the original author's distribution notes and data attribution; copies provided with the application are located in `licenses/data/photometry/`.

## Natural Earth

Source: <https://www.naturalearthdata.com/>

Natural Earth vector and raster map data is in the public domain. The original archives contain descriptions and version files for each layer.

Terms of use: <https://www.naturalearthdata.com/about/terms-of-use/>

## Astronomy Engine

Source: <https://github.com/cosinekitty/astronomy>

Version: `865d3da7d8112bbc7911238052c6af4aaf877181`. License: MIT.

See `third_party/astronomy-engine/LICENSE` for the full license.

## Lucide

Source: <https://lucide.dev/>. Version: 1.47.0. License: ISC.

See `assets/icons/LICENSE` for the full license.

## SGP4

Source: <https://github.com/brandon-rhodes/python-sgp4>, specifically its Vallado C++ implementation.

Version: `bf25b00ccf8cf0770a8e5ba458134156f1c70812`. License: MIT.

See `third_party/sgp4/LICENSE` for the full license.

## Elevation and Geoid

Observing weather is provided by the Open-Meteo Weather Forecast API. Source and attribution: <https://open-meteo.com/en/docs>. The data is used under CC BY 4.0.

Terrain elevation is provided by the Open-Meteo Elevation API, using Copernicus DEM 2021 GLO-90 with EGM2008 as its vertical datum.

Sources and attribution: <https://open-meteo.com/en/docs/elevation-api>, <https://doi.org/10.5270/ESA-c5d3d65>. The data is used under CC BY 4.0; Copernicus DEM is data provided by the European Union and the European Space Agency.

Geoid grid: NGA EGM2008 distributed by GeographicLib, at 5 arc-minute resolution, using bilinear interpolation.

Data and terms of use: <https://geographiclib.sourceforge.io/C++/doc/geoid.html>. The EGM2008 grid is in the public domain.

## Qt

Source: <https://www.qt.io/>. Qt 6.11 runtime libraries are dynamically linked and used under LGPL-3.0.

License terms: <https://www.qt.io/licensing/open-source-lgpl-obligations>
