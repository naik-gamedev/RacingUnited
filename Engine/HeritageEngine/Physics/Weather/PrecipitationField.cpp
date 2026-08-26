#include "PrecipitationField.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::physics::weather {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;

std::uint64_t mix64(std::uint64_t value)
{
    value ^= value >> 30;
    value *= 0xBF58476D1CE4E5B9ull;
    value ^= value >> 27;
    value *= 0x94D049BB133111EBull;
    value ^= value >> 31;
    return value;
}

std::uint64_t hashCell(
    std::int64_t x,
    std::int64_t y,
    std::int64_t z,
    std::uint32_t lane,
    std::uint64_t seed)
{
    std::uint64_t value = seed;
    value ^= mix64(static_cast<std::uint64_t>(x) + 0x9E3779B97F4A7C15ull);
    value ^= mix64(static_cast<std::uint64_t>(y) + 0xD1B54A32D192ED03ull);
    value ^= mix64(static_cast<std::uint64_t>(z) + 0x94D049BB133111EBull);
    value ^= mix64(static_cast<std::uint64_t>(lane) + 0xBF58476D1CE4E5B9ull);
    return mix64(value);
}

double unitRandom(std::uint64_t value)
{
    // 53 high-quality bits map exactly into a double mantissa interval.
    return static_cast<double>(value >> 11)
        * (1.0 / static_cast<double>(std::uint64_t{ 1 } << 53));
}

double fract(double value)
{
    return value - std::floor(value);
}

double normalizeDegrees(double degrees)
{
    if (!std::isfinite(degrees))
        return 0.0;
    double result = std::fmod(degrees, 360.0);
    if (result < 0.0)
        result += 360.0;
    return result;
}

double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double smoothStep(double edge0, double edge1, double value)
{
    if (edge1 <= edge0)
        return value >= edge1 ? 1.0 : 0.0;
    const double t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

double lerp(double a, double b, double t)
{
    return a + (b - a) * clamp01(t);
}

// CLOUDURP15H6: convert authored cloud fraction into a threshold for the
// coherent weather-value-noise selector. The thresholds are an empirical CDF
// calibration of weatherValueNoise, so authored 1%, 50%, 80% and so on map
// approximately to the same fraction of the regional sky rather than to a
// front-loaded density knob.
double cloudCoverageSelectorThreshold(double authoredFraction)
{
    const double q = 1.0 - clamp01(authoredFraction);
    constexpr double kProbability[] = {
        0.00, 0.01, 0.02, 0.05, 0.10, 0.20, 0.30, 0.40, 0.50,
        0.60, 0.70, 0.80, 0.90, 0.95, 0.98, 0.99, 1.00 };
    constexpr double kValue[] = {
        0.0012, 0.0673, 0.0943, 0.1500, 0.2119, 0.3009, 0.3744,
        0.4399, 0.5028, 0.5652, 0.6305, 0.7028, 0.7915, 0.8528,
        0.9068, 0.9331, 0.9973 };
    for (std::size_t i = 1; i < std::size(kProbability); ++i)
    {
        if (q <= kProbability[i])
        {
            const double t = (q - kProbability[i - 1])
                / (kProbability[i] - kProbability[i - 1]);
            return lerp(kValue[i - 1], kValue[i], t);
        }
    }
    return kValue[std::size(kValue) - 1];
}

double weatherLatticeValue(
    std::int64_t x,
    std::int64_t z,
    std::uint64_t seed)
{
    std::uint64_t value = seed;
    value ^= mix64(static_cast<std::uint64_t>(x) + 0x9E3779B97F4A7C15ull);
    value ^= mix64(static_cast<std::uint64_t>(z) + 0xD1B54A32D192ED03ull);
    return unitRandom(mix64(value));
}

double weatherValueNoise(double x, double z, std::uint64_t seed)
{
    const std::int64_t ix = static_cast<std::int64_t>(std::floor(x));
    const std::int64_t iz = static_cast<std::int64_t>(std::floor(z));
    double fx = x - std::floor(x);
    double fz = z - std::floor(z);
    fx = fx * fx * (3.0 - 2.0 * fx);
    fz = fz * fz * (3.0 - 2.0 * fz);
    const double a = weatherLatticeValue(ix, iz, seed);
    const double b = weatherLatticeValue(ix + 1, iz, seed);
    const double c = weatherLatticeValue(ix, iz + 1, seed);
    const double d = weatherLatticeValue(ix + 1, iz + 1, seed);
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fz);
}

double weatherFbm(double x, double z, std::uint64_t seed)
{
    double value = 0.0;
    double amplitude = 0.56;
    double px = x;
    double pz = z;
    for (int octave = 0; octave < 4; ++octave)
    {
        value += weatherValueNoise(px, pz, seed + static_cast<std::uint64_t>(octave) * 0x9E3779B97F4A7C15ull)
            * amplitude;
        const double nx = px * 0.80 - pz * 0.60;
        const double nz = px * 0.60 + pz * 0.80;
        px = nx * 2.03 + 13.7;
        pz = nz * 2.03 + 7.9;
        amplitude *= 0.48;
    }
    return clamp01(value);
}

} // namespace

bool PrecipitationField::setDescription(
    const PrecipitationFieldDescription& description)
{
    if (!std::isfinite(description.horizontalCellSizeM)
        || !std::isfinite(description.verticalCellSizeM)
        || description.horizontalCellSizeM < 0.25
        || description.horizontalCellSizeM > 100.0
        || description.verticalCellSizeM < 0.50
        || description.verticalCellSizeM > 200.0)
    {
        return false;
    }
    m_description = description;
    return true;
}

void PrecipitationField::clear()
{
    m_population = {};
    m_authoredRainfallRateMmPerHour = 0.0;
    m_authoredRelativeHumidity = 0.55;
    m_authoredCloudCover = 0.20;
    m_windSpeedMps = 0.0;
    m_windDirectionDegrees = 45.0;
    m_windVelocityMps = { 0.0f, 0.0f, 0.0f };
    m_elapsedSeconds = 0.0;
}

void PrecipitationField::configureRain(
    double rainfallRateMmPerHour,
    double windSpeedMps,
    double windDirectionDegrees)
{
    configureWeather(
        rainfallRateMmPerHour,
        m_authoredRelativeHumidity,
        m_authoredCloudCover,
        windSpeedMps,
        windDirectionDegrees);
}

void PrecipitationField::configureWeather(
    double rainfallRateMmPerHour,
    double relativeHumidity,
    double cloudCover,
    double windSpeedMps,
    double windDirectionDegrees)
{
    m_authoredRainfallRateMmPerHour = std::clamp(
        std::isfinite(rainfallRateMmPerHour) ? rainfallRateMmPerHour : 0.0,
        0.0, 500.0);
    m_authoredRelativeHumidity = std::clamp(
        std::isfinite(relativeHumidity) ? relativeHumidity : 0.55,
        0.0, 1.0);
    m_authoredCloudCover = std::clamp(
        std::isfinite(cloudCover) ? cloudCover : 0.20,
        0.0, 1.0);
    // Build the representative population from the authored storm peak. The
    // regional field controls local visibility/flux strength without changing
    // the physical drop-size distribution itself.
    m_population = buildRainDropPopulation(m_authoredRainfallRateMmPerHour);
    m_windSpeedMps = std::max(
        std::isfinite(windSpeedMps) ? windSpeedMps : 0.0,
        0.0);
    m_windDirectionDegrees = normalizeDegrees(windDirectionDegrees);

    // Heritage heading convention for weather: 0 deg travels toward world +Z,
    // 90 deg toward +X. This is a velocity-to direction, not meteorological
    // "wind from" notation.
    const double radians = m_windDirectionDegrees * (kPi / 180.0);
    m_windVelocityMps = {
        static_cast<float>(std::sin(radians) * m_windSpeedMps),
        0.0f,
        static_cast<float>(std::cos(radians) * m_windSpeedMps) };
}

void PrecipitationField::setElapsedSeconds(double elapsedSeconds)
{
    m_elapsedSeconds = std::isfinite(elapsedSeconds)
        ? std::max(elapsedSeconds, 0.0)
        : 0.0;
}

void PrecipitationField::advance(double deltaTimeSeconds)
{
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0)
        return;
    m_elapsedSeconds += deltaTimeSeconds;
}

heritage::math::Vec3 PrecipitationField::atmosphericWindVelocityMps(
    double heightAboveSurfaceM) const
{
    // A bounded boundary-layer/free-atmosphere profile: wind strengthens and
    // veers gradually with height. This is intentionally deterministic and
    // lightweight rather than a full CFD atmosphere, but it gives clouds and
    // storm cells coherent vertical shear while preserving the authored
    // near-surface wind used by tires/rain close to the road.
    const double height = std::clamp(
        std::isfinite(heightAboveSurfaceM) ? heightAboveSurfaceM : 0.0,
        0.0,
        6000.0);
    const double t = smoothStep(0.0, 6000.0, height);
    const double speed = m_windSpeedMps * (1.0 + 0.70 * t);
    const double veerDegrees = 22.0 * t;
    const double direction = normalizeDegrees(m_windDirectionDegrees + veerDegrees);
    const double radians = direction * (kPi / 180.0);
    return {
        static_cast<float>(std::sin(radians) * speed),
        0.0f,
        static_cast<float>(std::cos(radians) * speed) };
}

RegionalWeatherSample PrecipitationField::regionalWeatherSample(
    double globalX,
    double globalZ) const
{
    RegionalWeatherSample result;
    if (!std::isfinite(globalX) || !std::isfinite(globalZ))
        return result;

    // WEATHER10: one deterministic, world-scale weather field owns the broad
    // cloud/rain envelope. It is stateless and wind-advected, so a 200 km2 map
    // does not require millions of resident weather cells. Renderers/radar and
    // local hydrology all sample this same function.
    const double advectSeconds = m_elapsedSeconds * 0.38;
    // Synoptic cloud/rain cells are steered by the representative ~2 km wind,
    // while near-ground rain keeps the authored surface wind. This lets the
    // weather front and low-level precipitation differ naturally under shear.
    const auto steeringWind = weatherSteeringWindVelocityMps();
    const double x = globalX - static_cast<double>(steeringWind.x) * advectSeconds;
    const double z = globalZ - static_cast<double>(steeringWind.z) * advectSeconds;

    const std::uint64_t seed = m_description.seed;
    const double synoptic = weatherFbm(x / 18000.0, z / 18000.0, seed ^ 0xA24BAED4963EE407ull);
    const double macro = weatherFbm(x / 6800.0, z / 6800.0, seed ^ 0x9FB21C651E98DF25ull);
    const double cellular = weatherFbm(x / 2400.0, z / 2400.0, seed ^ 0xC13FA9A902A6328Full);
    const double moisture = clamp01(0.48 * synoptic + 0.37 * macro + 0.15 * cellular);

    // CLOUDURP15H6: authored cloud cover is now a genuine approximately linear
    // sky-fraction control. A separate coherent selector decides WHERE cloud
    // regions exist; inside a selected region the local cloud authority is
    // deliberately strong enough to form visible volumetric bodies. This
    // removes the H5 50..85% saturation plateau and gives 1% real sparse cloud.
    const double authoredCover = clamp01(m_authoredCloudCover);
    double cloudPresence = 0.0;
    if (authoredCover >= 1.0)
    {
        cloudPresence = 1.0;
    }
    else if (authoredCover > 0.0)
    {
        const double selector = weatherValueNoise(
            x / 5200.0, z / 5200.0, seed ^ 0x6A09E667F3BCC909ull);
        const double threshold = cloudCoverageSelectorThreshold(authoredCover);
        // A narrow spatial feather keeps cloud-front boundaries organic while
        // preserving the requested authored sky fraction very closely.
        cloudPresence = smoothStep(threshold - 0.012, threshold + 0.012, selector);
    }
    // CLOUDURP15H7: the selected regional fraction is itself the cloud-cover
    // authority. H6 multiplied selected cells back down by 0.72..1.0, which
    // meant a nominal 98% regional occupancy still formed only a small amount
    // of visible cloud after the shader threshold. Moisture remains available
    // independently for humidity/storm shaping; it must not silently rescale
    // the user's cloud-cover percentage.
    result.cloudCover = clamp01(cloudPresence);
    result.relativeHumidity = clamp01(
        m_authoredRelativeHumidity * 0.78 + (moisture - 0.45) * 0.42);

    const double stormDriver = clamp01(
        result.cloudCover * 0.55
        + result.relativeHumidity * 0.27
        + cellular * 0.26
        + std::min(m_authoredRainfallRateMmPerHour / 160.0, 1.0) * 0.18);
    result.stormIntensity = smoothStep(0.48, 0.88, stormDriver);

    if (m_authoredRainfallRateMmPerHour > 0.0)
    {
        // Authored rain is the storm-cell peak rather than a scene-wide uniform
        // value. Dense humid cloud receives the full rate; cell edges taper to
        // dry weather and therefore match what the radar/clouds show.
        const double rainMask = smoothStep(
            0.42, 0.78,
            result.cloudCover * 0.52
                + result.stormIntensity * 0.40
                + result.relativeHumidity * 0.18);
        result.currentRateMmPerHour = m_authoredRainfallRateMmPerHour * rainMask;
    }
    result.valid = true;
    return result;
}

void PrecipitationField::buildRainRadarSnapshot(
    double centerGlobalX,
    double centerGlobalZ,
    double halfRangeM,
    std::uint32_t resolution,
    RainRadarSnapshot& out) const
{
    out = {};
    if (!std::isfinite(centerGlobalX) || !std::isfinite(centerGlobalZ)
        || !std::isfinite(halfRangeM) || halfRangeM <= 0.0
        || resolution < 2u || resolution > 512u)
    {
        return;
    }

    out.resolution = resolution;
    out.centerGlobalX = centerGlobalX;
    out.centerGlobalZ = centerGlobalZ;
    out.halfRangeM = halfRangeM;
    const std::size_t count = static_cast<std::size_t>(resolution) * resolution;
    out.currentRateMmPerHour.assign(count, 0.0f);
    out.cumulativePrecipitationMm.assign(count, 0.0f);
    const double diameter = halfRangeM * 2.0;
    const double cell = diameter / static_cast<double>(resolution);

    for (std::uint32_t iz = 0; iz < resolution; ++iz)
    {
        for (std::uint32_t ix = 0; ix < resolution; ++ix)
        {
            const double gx = centerGlobalX - halfRangeM
                + (static_cast<double>(ix) + 0.5) * cell;
            const double gz = centerGlobalZ - halfRangeM
                + (static_cast<double>(iz) + 0.5) * cell;
            const RegionalWeatherSample sample = regionalWeatherSample(gx, gz);
            const std::size_t index = static_cast<std::size_t>(iz) * resolution + ix;
            const float rate = static_cast<float>(std::max(sample.currentRateMmPerHour, 0.0));
            out.currentRateMmPerHour[index] = rate;
            out.maximumCurrentRateMmPerHour = std::max(
                out.maximumCurrentRateMmPerHour, static_cast<double>(rate));

            // A bounded deterministic history estimate keeps the radar useful
            // without storing every distant weather cell. Twelve temporal
            // samples reconstruct the advected storm exposure over session time.
            double accumulationMm = 0.0;
            if (m_elapsedSeconds > 0.0 && m_authoredRainfallRateMmPerHour > 0.0)
            {
                constexpr int kHistorySamples = 12;
                const double duration = std::min(m_elapsedSeconds, 6.0 * 3600.0);
                const double dt = duration / static_cast<double>(kHistorySamples);
                for (int history = 0; history < kHistorySamples; ++history)
                {
                    const double age = duration - (static_cast<double>(history) + 0.5) * dt;
                    const auto steeringWind = weatherSteeringWindVelocityMps();
                    const double advectedX = gx + static_cast<double>(steeringWind.x) * age * 0.38;
                    const double advectedZ = gz + static_cast<double>(steeringWind.z) * age * 0.38;
                    const RegionalWeatherSample historical = regionalWeatherSample(advectedX, advectedZ);
                    accumulationMm += historical.currentRateMmPerHour * (dt / 3600.0);
                }
            }
            out.cumulativePrecipitationMm[index] = static_cast<float>(accumulationMm);
            out.maximumCumulativePrecipitationMm = std::max(
                out.maximumCumulativePrecipitationMm, accumulationMm);
        }
    }
    out.valid = true;
}

RainRepresentative PrecipitationField::sampleRainRepresentative(
    std::int64_t cellX,
    std::int64_t cellY,
    std::int64_t cellZ,
    std::uint32_t lane) const
{
    RainRepresentative result;
    if (!m_population.valid)
        return result;

    const std::uint64_t identity = hashCell(
        cellX, cellY, cellZ, lane, m_description.seed);
    const double uDiameter = unitRandom(mix64(identity ^ 0xA24BAED4963EE407ull));
    const double uX = unitRandom(mix64(identity ^ 0x9FB21C651E98DF25ull));
    const double uY = unitRandom(mix64(identity ^ 0xC13FA9A902A6328Full));
    const double uZ = unitRandom(mix64(identity ^ 0x91E10DA5C79E7B1Dull));
    const RainDropSample drop = sampleRainDrop(m_population, uDiameter);
    if (!drop.valid || drop.terminalVelocityMps <= 0.0)
        return result;

    const double cellXZ = m_description.horizontalCellSizeM;
    const double cellHeightM = m_description.verticalCellSizeM;
    const double phase = fract(
        uY + m_elapsedSeconds * drop.terminalVelocityMps / cellHeightM);
    const double ageSinceCellTopSeconds =
        phase * cellHeightM / drop.terminalVelocityMps;

    const double horizontalVelocityX =
        static_cast<double>(m_windVelocityMps.x) * drop.horizontalWindCoupling;
    const double horizontalVelocityZ =
        static_cast<double>(m_windVelocityMps.z) * drop.horizontalWindCoupling;

    result.identity = identity;
    result.globalPosition = {
        (static_cast<double>(cellX) + uX) * cellXZ
            + horizontalVelocityX * ageSinceCellTopSeconds,
        (static_cast<double>(cellY) + 1.0) * cellHeightM - phase * cellHeightM,
        (static_cast<double>(cellZ) + uZ) * cellXZ
            + horizontalVelocityZ * ageSinceCellTopSeconds };
    result.velocityMps = {
        static_cast<float>(horizontalVelocityX),
        static_cast<float>(-drop.terminalVelocityMps),
        static_cast<float>(horizontalVelocityZ) };
    result.diameterMm = drop.diameterMm;
    result.massKg = drop.massKg;
    result.terminalVelocityMps = drop.terminalVelocityMps;
    result.phase01 = phase;
    result.valid = true;
    return result;
}

} // namespace heritage::physics::weather
