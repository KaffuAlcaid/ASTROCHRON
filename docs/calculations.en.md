# Calculation Notes

[简体中文](calculations.md) | English

[Home](../README.en.md) · [Data Sources](data-sources.en.md) · [SGP4 Principles](orbit-model.en.md)

Applies to: `v0.1.0`

## Orbit Propagation and Coordinates

Orbit propagation uses the Vallado SGP4 C++ implementation, including near-Earth and deep-space orbit handling. Both TLE and OMM are initialized with WGS-72 constants

SGP4 outputs position and velocity in the TEME frame. The application rotates them into Earth-fixed coordinates using Greenwich mean sidereal time, then uses the WGS84 ellipsoid to calculate the subsatellite point, satellite altitude, and observer position

Earth rotation uses a sidereal-time approximation. Earth orientation parameters, polar motion, and atmospheric refraction are effects beyond the scope of the current calculation accuracy

| Displayed value | Definition |
| --- | --- |
| Satellite altitude | Height above the WGS84 ellipsoid |
| Speed | Magnitude of velocity in the TEME frame |
| Subsatellite point | Latitude and longitude of the satellite's position on Earth's surface |
| Azimuth | Angle measured eastward from true north, in the range 0–360° |
| Elevation | Angle relative to the observer's geometric horizon |
| Range | Straight-line distance from the satellite to the observer |
| Range rate | Relative velocity along the line of sight; positive when receding and negative when approaching |

The observing location uses altitude $H$ relative to mean sea level, converted to ellipsoidal height using EGM2008 geoid undulation $N$

$$h = H + N$$

The EGM2008 grid has a resolution of 5′ and uses bilinear interpolation

## Time and Tracks

Interface times are displayed in the observing location's time zone, and orbital epochs are expressed in UTC. Tracks cover 12 h before and after the reference time; internal samples align with 30 s UTC intervals, and the two ends of the time window are sampled at their actual times

Both past and future positions are calculated from the selected orbital elements. Using a historical element snapshot changes the basis of the calculation. Elements closer to the target time are generally more useful; satellite maneuvers and changes in drag affect predictions

The orbit calculation rate controls updates to positions and observation values, while the map refresh rate controls map animation and time display. When the rendering rate is higher, map markers are interpolated between successive SGP4 sampled positions; observation values still use propagation results

As the time window advances, existing track samples are retained and samples are added at both ends, while passes and eclipse events continue to be refined. Changing the target, orbital elements, or observing location triggers recalculation using the corresponding data

## Passes and Optical Conditions

Pass start and end are determined by the minimum elevation set for the observing location, which defaults to 10°. Culmination corresponds to the maximum elevation during the pass

Numerical searches for the start, end, and culmination are refined to 0.5 s. This value is the search resolution; actual prediction accuracy still depends on orbital elements and observing conditions

When both adjacent samples are below the minimum elevation and the elevation rate changes from positive to negative, the application searches for the peak within that interval at 0.01 s resolution. If the elevation criterion is met, it continues searching for the start and end times. The sky chart includes these key pass points

This search depends on elevation changes between adjacent valid samples. Gaps caused by propagation failures and marginal passes shorter than the search resolution may still affect the completeness of predictions

Optical-condition intervals are sampled every 5 s. Within the pass's elevation limits, both of the following conditions must be met

- The satellite is fully illuminated by the Sun
- The Sun's elevation at the observing location is −6° or lower

Cloud cover, apparent magnitude, terrain, and building obstructions must be assessed separately using the observation information. The twilight condition provides only a rough criterion for sky background brightness

## Sun, Eclipse, and Footprint

The Sun's position is calculated by Astronomy Engine. The apparent disks of the Sun and Earth as seen from the satellite determine sunlight, penumbra, and umbra conditions; Earth's occultation uses a spherical approximation

The map's day/night boundary is drawn from the angle between the Sun's direction and the surface normal. The footprint is calculated using spherical geometry from satellite altitude and minimum elevation

## Apparent Magnitude

The calculation uses the phase function of a diffusely reflecting sphere, with reference magnitude $m_{ref}$, reference range 1000 km, reference phase angle $\alpha_{ref}$, actual range $r$, and actual phase angle $\alpha$

$$\Phi(\alpha) = \frac{\sin\alpha + (\pi-\alpha)\cos\alpha}{\pi}$$

$$m = m_{ref} + 5\log_{10}\left(\frac{r}{1000\,\mathrm{km}}\right) - 2.5\log_{10}\left(\frac{\Phi(\alpha)}{\Phi(\alpha_{ref})}\right)$$

Phase angles in the formulas use rad, while the interface displays °. The manual reference phase can be set to 0° or 90°; the QuickSat catalog uses full-phase conditions at 0°

A result is displayed when the satellite is above the geometric horizon and fully illuminated. The calculated value is an estimate of apparent magnitude above the atmosphere, in mag; smaller values indicate greater brightness

Bundled data is matched by satellite number and international designator. Manual parameters and imported data can supply the corresponding reference values. Satellite attitude, configuration, specular reflection, atmospheric extinction, and clouds determine differences between estimated and measured values

## Doppler Shift

The first-order Doppler shift is calculated from the nominal receiving frequency $f_0$, line-of-sight range rate $\dot r$, and speed of light $c$

$$\Delta f = -\frac{\dot r}{c} f_0, \qquad f_{receive} = f_0 + \Delta f$$

Here, $c = 299792.458\,\mathrm{km/s}$. When the satellite is receding, $\dot r > 0$ and the receiving frequency decreases

The interface accepts input in MHz, displays the shift in kHz, and displays the corrected receiving frequency in MHz

## Units

Units use international symbols. Common distances and speeds are expressed in m, km, and km/s; angles use °, times use s, min, and h, orbital mean motion uses rev/d, and magnitude uses mag

BSTAR retains the inverse Earth-radius unit `R_E^-1` used in the orbital format. Mean-motion derivative terms retain the numerical values and conventions of the source format
