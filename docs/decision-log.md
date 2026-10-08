# Decision Log — Varde Processor

## Decision 001: Remove inherited weather and sector-capacity functionality

**Date:** 08.10.26
Status: Complete - code removal. Protobuf migration deferred

### Context

The inherited CapOps processor contains weather simulation, risk-event generation, and sector-capacity functionality. These features are not required for Varde, which instead focuses on detecting potential restricted-airspace infringements and generating avoidance proposals.

### Decision

Remove the inherited weather, risk-event, and sector-capacity functionality while preserving the processor's required track-processing and Redis-publishing capabilities. This includes removing obsolete source code, dependencies, configuration, and tests that exclusively cover the deleted functionality.

### Rationale

Varde detects potential infringements of restricted airspace rather than congestion caused by sector capacity. Weather-dependent capacity calculations therefore do not support the required behaviour.

The inherited risk events describe changes in sector congestion, not restricted-airspace infringements. They were removed together with the capacity calculations. Varde's zone alerts will be implemented separately.

Aircraft acquisition, snapshot processing, grid filtering, and Redis publication were retained because they support the existing aircraft pipeline. The random simulator remains temporarily until scripted scenario simulation replaces it.

The Protobuf schema was left unchanged during this removal to separate internal code cleanup from the coordinated contract migration. Obsolete fields will be removed and reserved when the agreed schema is incorporated.

### Changes Made

- Removed weather ticking, processing, initialization, and ownership from `ProcessorApp`.
- Removed weather access from `IngestService`, leaving it responsible for aircraft retrieval.
- Deleted `WeatherSourceOpenMeteo`, `WeatherSourceSimulated`, and `IWeatherSource`.
- Deleted `WeatherSimulator`, `WeatherPatternUtils`, `WeatherCell`, and `WeatherSeverity`.
- Removed weather handling, sector-risk evaluation, and risk-event generation from `ComputeData`.
- Deleted `RiskEvent` and `SectorState`.
- Removed weather, risk-status, and capacity fields and methods from `SectorSummary`, retaining aircraft counts and sector membership.
- Removed weather and capacity configuration from `Configuration`, `configuration.cfg`, and both test configuration helpers.
- Removed weather, risk-event, and capacity mapping from `ProtoMapper`.
- Deleted tests and assertions specific to the removed functionality, retaining aircraft-processing and publication tests.
- Updated the aircraft pipeline integration test to use the revised ingest constructor.
- The existing Protobuf schema remains unchanged pending the agreed contract migration and reservation of obsolete fields.

### Verification

- The processor builds successfully after weather, risk-event, and capacity removal.
- All tests in `backend_tests` pass after these changes.
- The processor was run after risk-event generation was removed, and a Redis subscriber received aircraft snapshots and sector counts.

### Consequences

Removing the inherited functionality simplifies the processor and prepares it for implementing Varde's restricted-airspace warning and avoidance functionality. The processor will no longer provide weather simulation, inherited risk-event calculations, or sector-capacity management.
