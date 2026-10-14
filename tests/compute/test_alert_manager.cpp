#include "compute/AlertManager.hpp"
#include "../test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

// Reproduces the worked example in SRS requirement VD-FUN-023.
TEST_CASE("Alert manager follows the SRS hysteresis example")
{
    AlertManager alerts = createTestAlertManager();

    const auto predicted = makeAlertCheck(WarningLevel::PREDICTED);
    const auto imminent = makeAlertCheck(WarningLevel::IMMINENT);
    const auto none = makeAlertCheck(WarningLevel::NONE);

    auto event = alerts.update(predicted); // Cycle 1: open.
    REQUIRE(event.has_value());
    REQUIRE(event->type == AlertEventType::OPENED);
    REQUIRE(event->level == WarningLevel::PREDICTED);
    REQUIRE_FALSE(event->alertId.empty());

    const std::string alertId = event->alertId;

    event = alerts.update(imminent);       // Cycle 2: rise immediately.
    REQUIRE(event.has_value());
    REQUIRE(event->type == AlertEventType::LEVEL_CHANGED);
    REQUIRE(event->level == WarningLevel::IMMINENT);
    REQUIRE(event->alertId == alertId);

    REQUIRE_FALSE(alerts.update(predicted)); // Cycle 3: lower.
    REQUIRE_FALSE(alerts.update(none));      // Cycle 4: lower.

    event = alerts.update(predicted);        // Cycle 5: third lower result.
    REQUIRE(event.has_value());
    REQUIRE(event->type == AlertEventType::LEVEL_CHANGED);
    REQUIRE(event->level == WarningLevel::PREDICTED);
    REQUIRE(event->alertId == alertId);

    REQUIRE_FALSE(alerts.update(none));      // Cycle 6.
    REQUIRE_FALSE(alerts.update(none));      // Cycle 7.

    event = alerts.update(none);             // Cycle 8: close.
    REQUIRE(event.has_value());
    REQUIRE(event->type == AlertEventType::CLOSED);
    REQUIRE(event->level == WarningLevel::NONE);
    REQUIRE(event->alertId == alertId);
    REQUIRE_FALSE(event->timeToEntrySeconds.has_value());
}

TEST_CASE("No warning or an unchanged level produces no new event")
{
    AlertManager alerts = createTestAlertManager();

    const auto none = makeAlertCheck(WarningLevel::NONE);
    const auto predicted = makeAlertCheck(WarningLevel::PREDICTED);

    REQUIRE_FALSE(alerts.update(none));

    const auto opened = alerts.update(predicted);
    REQUIRE(opened.has_value());
    REQUIRE(opened->type == AlertEventType::OPENED);

    REQUIRE_FALSE(alerts.update(predicted));
}

TEST_CASE("Returning to the published level resets the lower-level count")
{
    AlertManager alerts = createTestAlertManager();

    const auto imminent = makeAlertCheck(WarningLevel::IMMINENT);
    const auto predicted = makeAlertCheck(WarningLevel::PREDICTED);
    const auto none = makeAlertCheck(WarningLevel::NONE);

    REQUIRE(alerts.update(imminent).has_value());

    REQUIRE_FALSE(alerts.update(predicted)); // First lower result.
    REQUIRE_FALSE(alerts.update(predicted)); // Second lower result.
    REQUIRE_FALSE(alerts.update(imminent));  // Reset the count.

    REQUIRE_FALSE(alerts.update(none));      // First new lower result.
    REQUIRE_FALSE(alerts.update(none));      // Second new lower result.

    const auto closed = alerts.update(none); // Third: close.
    REQUIRE(closed.has_value());
    REQUIRE(closed->type == AlertEventType::CLOSED);
    REQUIRE(closed->level == WarningLevel::NONE);
}

TEST_CASE("A higher level rises immediately and resets the lower-level count")
{
    AlertManager alerts = createTestAlertManager();

    const auto predicted = makeAlertCheck(WarningLevel::PREDICTED);
    const auto imminent = makeAlertCheck(WarningLevel::IMMINENT);
    const auto none = makeAlertCheck(WarningLevel::NONE);

    const auto opened = alerts.update(predicted);
    REQUIRE(opened.has_value());

    REQUIRE_FALSE(alerts.update(none));
    REQUIRE_FALSE(alerts.update(none));

    const auto raised = alerts.update(imminent);
    REQUIRE(raised.has_value());
    REQUIRE(raised->type == AlertEventType::LEVEL_CHANGED);
    REQUIRE(raised->level == WarningLevel::IMMINENT);
    REQUIRE(raised->alertId == opened->alertId);

    REQUIRE_FALSE(alerts.update(none));
    REQUIRE_FALSE(alerts.update(none));

    const auto closed = alerts.update(none);
    REQUIRE(closed.has_value());
    REQUIRE(closed->type == AlertEventType::CLOSED);
}

TEST_CASE("A warning after closure opens a new alert")
{
    AlertManager alerts = createTestAlertManager();

    const auto predicted = makeAlertCheck(WarningLevel::PREDICTED);
    const auto none = makeAlertCheck(WarningLevel::NONE);

    const auto original = alerts.update(predicted);
    REQUIRE(original.has_value());

    REQUIRE_FALSE(alerts.update(none));
    REQUIRE_FALSE(alerts.update(none));

    const auto closed = alerts.update(none);
    REQUIRE(closed.has_value());
    REQUIRE(closed->type == AlertEventType::CLOSED);

    REQUIRE_FALSE(alerts.update(none));

    const auto reopened = alerts.update(predicted);
    REQUIRE(reopened.has_value());
    REQUIRE(reopened->type == AlertEventType::OPENED);
    REQUIRE(reopened->alertId != original->alertId);
}

TEST_CASE("A disappearing aircraft closes its alerts and leaves others open")
{
    AlertManager alerts = createTestAlertManager();

    const std::string missingAircraft = "SIM-0";
    const std::string remainingAircraft = "SIM-1";
    const std::string dangerZone = "D-101";
    const std::string restrictedZone = "R-201";
    constexpr std::size_t expectedClosedAlerts = 2;

    const auto firstAlert = makeAlertCheck(
        WarningLevel::PREDICTED, missingAircraft, dangerZone);
    const auto secondAlert = makeAlertCheck(
        WarningLevel::PREDICTED, missingAircraft, restrictedZone);
    const auto unaffectedAlert = makeAlertCheck(
        WarningLevel::PREDICTED, remainingAircraft, dangerZone);

    REQUIRE(alerts.update(firstAlert).has_value());
    REQUIRE(alerts.update(secondAlert).has_value());
    REQUIRE(alerts.update(unaffectedAlert).has_value());

    const auto closed =
        alerts.handleTrackDisappearance(missingAircraft);

    REQUIRE(closed.size() == expectedClosedAlerts);

    for (const auto &event : closed)
    {
        REQUIRE(event.icao == missingAircraft);
        REQUIRE(event.type == AlertEventType::CLOSED);
        REQUIRE(event.level == WarningLevel::NONE);
        REQUIRE_FALSE(event.timeToEntrySeconds.has_value());
    }

    REQUIRE(alerts.handleTrackDisappearance(missingAircraft).empty());

    // If this alert had been removed, the update would open a new one.
    REQUIRE_FALSE(alerts.update(unaffectedAlert));
}

TEST_CASE("A deactivated zone closes its alerts and leaves other zones open")
{
    AlertManager alerts = createTestAlertManager();

    const std::string firstAircraft = "SIM-0";
    const std::string secondAircraft = "SIM-1";
    const std::string deactivatedZone = "D-101";
    const std::string remainingZone = "R-201";
    constexpr std::size_t expectedClosedAlerts = 2;

    const auto firstAlert = makeAlertCheck(
        WarningLevel::PREDICTED, firstAircraft, deactivatedZone);
    const auto secondAlert = makeAlertCheck(
        WarningLevel::PREDICTED, secondAircraft, deactivatedZone);
    const auto unaffectedAlert = makeAlertCheck(
        WarningLevel::PREDICTED, firstAircraft, remainingZone);

    REQUIRE(alerts.update(firstAlert).has_value());
    REQUIRE(alerts.update(secondAlert).has_value());
    REQUIRE(alerts.update(unaffectedAlert).has_value());

    const auto closed =
        alerts.handleZoneDeactivation(deactivatedZone);

    REQUIRE(closed.size() == expectedClosedAlerts);

    for (const auto &event : closed)
    {
        REQUIRE(event.zoneId == deactivatedZone);
        REQUIRE(event.type == AlertEventType::CLOSED);
        REQUIRE(event.level == WarningLevel::NONE);
        REQUIRE_FALSE(event.timeToEntrySeconds.has_value());
    }

    REQUIRE(alerts.handleZoneDeactivation(deactivatedZone).empty());
    REQUIRE_FALSE(alerts.update(unaffectedAlert));
}