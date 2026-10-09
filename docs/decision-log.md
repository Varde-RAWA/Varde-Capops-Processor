# Decision Log — Varde Processor

## Decision 001: Remove inherited weather, sector-capacity, and sector-summary functionality

**Date:** 08.10.26
Status: Complete - code removal. Protobuf migration deferred

### Context

The inherited CapOps processor contains weather simulation, risk-event generation, and sector-capacity functionality. These features are not required for Varde, which instead focuses on detecting potential restricted-airspace infringements and generating avoidance proposals.

### Decision

Remove the inherited weather, risk-event, and sector-capacity functionality while preserving the processor's required track-processing and Redis-publishing capabilities. This includes removing obsolete source code, dependencies, configuration, and tests that exclusively cover the deleted functionality.

Sector summaries and local aircraft counts were also removed. They grouped aircraft into rectangular grid sectors, while Varde will evaluate each aircraft's position and predicted path against restricted-zone polygons. Individual aircraft snapshots already contain the position and movement information needed for those checks. Grid-based region filtering was retained.

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
- Removed weather, risk-status, capacity, local aircraft counts, and sector-membership functionality from the domain model; `SectorSummary` was subsequently deleted entirely.
- Removed weather and capacity configuration from `Configuration`, `configuration.cfg`, and both test configuration helpers.
- Removed weather, risk-event, and capacity mapping from `ProtoMapper`.
- Deleted tests and assertions specific to the removed functionality, retaining aircraft-processing and publication tests.
- Updated the aircraft pipeline integration test to use the revised ingest constructor.
- The existing Protobuf schema remains unchanged pending the agreed contract migration and reservation of obsolete fields.
- Removed local aircraft counts and their update methods, mapping, and test assertions.
- Removed sector-summary initialization, ICAO membership tracking, and result collection from `ComputeData`.
- Removed sector summaries from `ProcessingResult` and `ProtoMapper`.
- Deleted `SectorSummary` and the `findSectorSummary()` test helper.
- Removed sector-summary tests and assertions while retaining aircraft update, timestamp, movement, removal, re-entry, and serialization checks.
- Removed unused mapper parameters, members, and local grid objects.
- Retained `Grid` for operating-region filtering and simulation support.

### Verification

- The processor builds successfully after weather, risk-event, and capacity removal.
- All tests in `backend_tests` pass after these changes.
- A subsequent manual run confirmed that a Redis subscriber received metadata and 15 aircraft snapshots, without sector summaries or inherited risk events.

### Consequences

The processor now collects and publishes individual aircraft snapshots without weather, capacity, sector-risk, or sector-summary data. Aircraft storage, timestamp handling, operating-region filtering, and Redis publication remain.

Restricted-zone checks and Varde alerts will be implemented separately. The Protobuf schema migration and replacement of the random simulator with scripted scenarios remain deferred.
