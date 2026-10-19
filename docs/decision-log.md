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


## Decision 002: Separate zone assessment and alert lifecycle handling

**Date:** 13.10.26
**Status:** In progress

### Context

Varde needs to detect whether an aircraft is inside or approaching an active zone. It also needs to maintain alerts across processing cycles, including the required delay before lowering a warning level.

### Decision

Implement `VardeEngine` as the component that projects an aircraft's movement and checks its path against the supplied zone polygons. It returns the computed warning level and first entry time for each aircraft–zone pair.

Implement alert lifecycle handling in a separate `AlertManager`. It maintains the published warning level and alert state for each pair, producing events when an alert opens, changes level, or closes.

The application supplies tracks and zones and handles publication of the resulting events.

### Reason

Separating zone assessment from alert state allows the geometry calculations and alert transitions to be tested independently. Simulated and live aircraft can use the same assessment and alert logic.

Keeping state for each aircraft–zone pair supports independent alerts when an aircraft approaches several zones.

### Consequences

`ProcessorApp` now evaluates current aircraft through `VardeEngine`, passes the results to `AlertManager`, and publishes the resulting alert events.

The demonstration polygon was removed from the application. Zone loading remains pending, so the processor currently publishes aircraft snapshots but produces no zone alerts.

## Decision 003: Publish alert transitions through Redis Streams

**Date:** 14.10.26
**Status:** Complete

### Decision

Map `AlertEvent` to the agreed `ZoneAlertEvent` Protobuf message and append
it to the `varde:alerts` Redis Stream. Store the serialized message in the
`data` field and approximately cap the stream at 10,000 entries.

Publish events when an alert opens, changes level, or closes. Keep the
same alert ID throughout its lifetime.

### Reason

This follows the shared event contract and allows consumers to read alert
transitions from the stream. Publishing transitions avoids repeating
unchanged alerts every processing cycle.

### Verification

A manual run with the demonstration zone produced OPENED, LEVEL_CHANGED,
and CLOSED events in Redis. The messages carried contract version 3 and
the same alert ID throughout the alert's lifetime.



## Decision 004: Support API and local file zone sources

**Date:** 15.10.26
**Status:** Implemented

### Decision

Load zones from either the Geo Service API or a local GeoJSON file,
selected in configuration. Both sources use the same parser.

Fetch API updates in the background every 30 seconds by default.
Keep cached zones if fetching fails, using the local file at startup.

### Reason

This allows development without the Geo Service and lets aircraft
processing continue during requests or temporary API failures.

### Consequences

A valid local zone file is currently required even in API mode.
Cached zones may become outdated while the service is unavailable.

### Verification

Parsing and file-loading tests pass. Automated HTTP and refresh-failure
tests remain pending.


## Decision 005: Buffer alerts during Redis outages

**Date:** 19.10.26
**Status:** Implemented

### Decision

Queue alert events in memory until Redis publication succeeds.
Limit the pending queue to 10,000 alerts and publish at most 100
queued alerts per processing cycle.

If the queue is full, drop the oldest alert to make room for the
newest and log the loss.

### Reason

Allow aircraft processing and warning calculations to continue
during Redis outages. Bound memory usage during prolonged outages
and limit the backlog processed in each cycle.

### Consequences

Queued alerts are retried after Redis recovers. The queue is not
persistent, so pending alerts are lost if the processor exits.

Alerts can also be lost when the queue reaches its limit.
A serialization failure causes the affected alert to be logged
and discarded.