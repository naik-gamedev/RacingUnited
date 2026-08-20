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
    m_population = buildRainDropPopulation(rainfallRateMmPerHour);
    m_windSpeedMps = std::max(
        std::isfinite(windSpeedMps) ? windSpeedMps : 0.0,
        0.0);
    m_windDirectionDegrees = normalizeDegrees(windDirectionDegrees);

    // Heritage heading convention for weather: 0 deg travels toward world +Z,
    // 90 deg toward +X. This is a velocity-to direction, not meteorological
    // "wind from" notation; a later atmosphere authoring layer may expose both.
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
