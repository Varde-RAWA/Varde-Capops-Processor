# Decision Log — Varde Processor

## Decision 001: Remove inherited weather and sector-capacity functionality

**Date:** 08.10.26
Status: In progress

### Context

The inherited CapOps processor contains weather simulation, risk-event generation, and sector-capacity functionality. These features are not required for Varde, which instead focuses on detecting potential restricted-airspace infringements and generating avoidance proposals.

### Decision

Remove the inherited weather, risk-event, and sector-capacity functionality while preserving the processor's required track-processing and Redis-publishing capabilities. This includes removing obsolete source code, dependencies, configuration, and tests that exclusively cover the deleted functionality.

### Changes Made

- Removed weather ticking, processing, initialization, and ownership from `ProcessorApp`.
- Removed weather access from `IngestService`, leaving it responsible for aircraft retrieval.
- Deleted `WeatherSourceOpenMeteo`, `WeatherSourceSimulated`, and `IWeatherSource`.
- Deleted `WeatherSimulator` and `WeatherPatternUtils`.
- Removed weather handling from `ComputeData`.
- Deleted `WeatherCell` and `WeatherSeverity`.
- Removed weather configuration from `Configuration`, `configuration.cfg`, and both test configuration helpers.
- Removed weather fields and update methods from `SectorSummary`. Effective sector capacity now equals base capacity.
- Removed weather mapping from `ProtoMapper`; the existing Protobuf schema remains unchanged.
- Deleted weather-only tests and removed weather setup and assertions from the remaining tests.
- Updated the aircraft pipeline integration test to use the revised ingest constructor.
- Sector-capacity logic and risk-event functionality remain temporarily.

### Verification

- The processor builds successfully after the changes.
- All tests in `backend_tests` pass after weather removal.
- The processor has been run during the removal process and continues publishing aircraft snapshots to Redis.

### Consequences

Removing the inherited functionality simplifies the processor and prepares it for implementing Varde's restricted-airspace warning and avoidance functionality. The processor will no longer provide weather simulation, inherited risk-event calculations, or sector-capacity management.

### Related Requirements

- **VD-LEG-003:** Characterisation tests before modifying inherited modules.
- **VD-LEG-004:** Safe removal of weather and sector-capacity functionality.
- **VD-TST-008:** Backwards compatibility between Protobuf contract versions 2 and 3.
