# Varde Processor

Varde Processor checks whether aircraft are inside or approaching active airspace zones. It receives aircraft positions from a simulator or OpenSky, projects each aircraft's movement 120 seconds ahead using its current ground speed and ground track, and checks the projected straight path against zone polygons. Zone checks use horizontal position only; altitude is not considered.

The processor publishes aircraft snapshots and zone-alert events through Redis. It currently provides these warning levels:

| Level | Meaning |
|---|---|
| `NONE` | No projected entry within 120 seconds. |
| `PREDICTED` | Projected entry after 30 seconds and within 120 seconds. |
| `IMMINENT` | Projected entry within 30 seconds. |
| `INFRINGEMENT` | The aircraft is already inside the zone, including its boundary. |

Alert levels rise immediately. A decrease requires three consecutive checks below the published level; the new level is the highest computed during those three checks. Alerts close immediately when an aircraft disappears from the snapshot or a restricted zone is removed from the active API collection. Alert events are published when an alert opens, changes level, or closes.

## Requirements

- A C++17 compiler.
- CMake 3.16 or newer.
- Qt 6 with Core and Network. CI uses Qt 6.8.3.
- Protobuf libraries and the `protoc` compiler.
- Git and internet access for the initial CMake configuration, which downloads Catch2, hiredis, and redis-plus-plus.
- A running Redis server. Docker can be used to run Redis locally.
- The Geo Service API when using API zone mode. File mode works without the Geo Service.

## Redis setup

The shared Varde Docker Compose project manages the `redis` service.
From this repository, create or start it with:

```sh
docker compose -f ../Varde/docker-compose.yml up -d --no-deps redis
```

Docker must be running. Check the connection:

```sh
docker compose -f ../Varde/docker-compose.yml exec -T redis redis-cli ping
```

The response should be `PONG`. The processor's local connection is configured in `configuration.cfg`:

```ini
[redis]
redisUrl=tcp://localhost:6379?connect_timeout=1000ms&socket_timeout=2000ms
redisChannel=flightdata
```

## Configuration overrides

The processor reads `configuration.cfg` from its working directory. Environment variables override individual settings. Variable names use the uppercase section and key joined by an underscore, without adding underscores inside camel-case names.

For example:

```sh
REDIS_REDISURL='tcp://localhost:6379?connect_timeout=1000ms&socket_timeout=2000ms' \
ZONES_SOURCE=file \
EXECUTION_LOOPINTERVALMS=500 \
./build/backend
```

For API zones inside Docker, set `ZONES_SOURCE=api`, `ZONES_APIURL`, and `ZONES_AVOIDANCEAPIURL` to addresses reachable from the processor container.

The configuration file must still exist when using environment overrides. Unknown sections, unknown keys, duplicate keys, and invalid effective values cause startup to fail.

## Zone sources

Choose the zone source in `configuration.cfg`. The `[zones] source` setting controls both restricted zones and avoidance areas. This is independent of `[dataSource] type`, which selects simulated aircraft or OpenSky. Restart the processor after changing either setting.

### Geo Service API

Start the Geo Service separately, then configure:

```ini
[zones]
source=api
apiUrl=http://localhost:5040/collections/restricted-areas/items
avoidanceApiUrl=http://localhost:5040/collections/avoidance-areas/items
refreshSeconds=30
```

Check that both endpoints respond:

```sh
curl --fail --show-error http://localhost:5040/collections/restricted-areas/items
curl --fail --show-error http://localhost:5040/collections/avoidance-areas/items
```

Each endpoint returns a GeoJSON `FeatureCollection`. The processor uses features whose `properties.active` value is `true`, so the response may contain both active and inactive zones. It follows pagination links and currently supports `Polygon` geometries with one closed ring.

In API mode, the processor fetches restricted zones and avoidance areas before aircraft processing starts. Both initial requests must succeed; otherwise startup fails. Local zone files are not read, and there is no automatic fallback to files.

After startup, both collections refresh in the background every `refreshSeconds` seconds, as processing cycles allow. If a refresh fails, the processor logs the failure and keeps that collection's last successful API data in memory. A successful empty collection replaces the previous data. The cache is not saved to disk.

### Local file without the Geo Service

To run without the API, change the zone source to `file`:

```ini
[zones]
source=file
file=data/zones.geojson
avoidanceFile=data/avoidance-areas.geojson
```

In file mode, the processor loads both configured files once at startup and makes no zone API requests. Both files must exist and contain valid GeoJSON; a load failure stops startup. Restart the processor after changing either file. File mode uses the stored `active` flags; it does not recalculate activation windows.

At startup, the processor logs the selected zone source and the number of restricted zones and avoidance areas loaded.

Redis is required in both modes.

## Build and run on Linux / macOS

Run these commands from the repository root after installing the build requirements:

```sh
cmake -S . -B build
cmake --build build
```

If CMake cannot find Qt, supply the path to your Qt installation when configuring:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/platform
```

Replace the example path with your installed Qt directory, such as its `gcc_64` or `macos` directory.

With Redis running and the desired zone source configured, start the processor from the repository root so it can find `configuration.cfg` and resolve any configured relative file paths:

```sh
./build/backend
```

Press **Ctrl+C** to terminate the processor.

The checked-in configuration uses simulated aircraft with these settings:

```ini
[dataSource]
type=sim

[simulation]
scenarioFiles=scenarios/S6.geojson
timestepSize=0.5

[execution]
loopIntervalMs=1000
```

### Simulation scenarios

When `[dataSource]` has `type=sim`, the processor loads aircraft from the GeoJSON files specified in `configuration.cfg`:

```ini
[simulation]
scenarioFiles=scenarios/S6.geojson
timestepSize=0.5

[execution]
loopIntervalMs=1000
```

To load multiple scenarios together, separate their paths with commas:

```ini
scenarioFiles=scenarios/S1.geojson,scenarios/S2-near-miss.geojson
```

All aircraft in the selected files are loaded; `numFlights` does not control their count. Paths are relative to the working directory, so start the processor from the repository root:

```sh
./build/backend
```

Restart the processor after changing configuration or scenario files. No rebuild is needed for those edits.

Each aircraft must be a GeoJSON Point feature with:

- `properties.icao24`: a six-character hexadecimal identifier, unique across all selected files. Identifiers are normalized to lowercase.
- `geometry.coordinates`: starting position as `[longitude, latitude]`.
- `properties.destination`: destination as `[longitude, latitude]`.
- `properties.groundSpeedKt`: positive ground speed in knots.
- `properties.startDelaySeconds`: currently must be `0` or omitted.

Aircraft fly toward their destinations and are removed from subsequent snapshots when they arrive. Any open alerts for those aircraft close immediately. Approved Vektor detours can change their routes.

Vektor proposals include an extra waypoint after the detour's last turning point, placed toward the destination. It represents about 12 seconds at the reported ground speed, capped at the destination. Simulated ground speed includes the simulator's 5× multiplier, so this corresponds to about 60 seconds at the scenario's configured speed. The point splits the final leg and adds no distance or delay.

`timestepSize` controls simulated seconds advanced per cycle. `loopIntervalMs` controls the pause after each processing cycle, in milliseconds. Processing time adds to that pause. Values of `0.5` and `500` give approximately real-time simulation before any speed multiplier. The checked-in interval is `1000`, so each cycle advances 0.5 simulated seconds followed by a one-second pause, plus processing time.

The current simulator applies a hardcoded 5× speed multiplier in `RadarSimulator.cpp`. This affects both movement and reported ground speed. Changing this multiplier requires rebuilding.

Altitudes are generated rather than read from `altitudeM`. Timestamps use current time, and headings are calculated from the flight route.

Available scenarios:

| File | Aircraft |
|---|---:|
| `S1.geojson` | 1 |
| `S2-near-miss.geojson` | 1 |
| `S3-detour.geojson` | 1 |
| `S4-load-150.geojson` | 150 |
| `S4-random-30.geojson` | 30 |
| `S4-random-150.geojson` | 150 |
| `S5-zone-switched-on.geojson` | 1 |
| `S6.geojson` | 2 |

The random scenarios have varied starting positions, destinations near the map boundary, and configured speeds within 200–450 knots before the simulator multiplier. Scenario files configure aircraft; restricted and avoidance zones are configured separately under `[zones]`.

### OpenSky aircraft

OpenSky API mode is not yet verified end to end. Use `type=sim` for the tested scenario-based simulation. To select OpenSky for a short live test, configure:

```ini
[dataSource]
type=api

[execution]
loopIntervalMs=10000
```

**Take care with OpenSky request frequency. Switching to `type=api` while keeping the simulation interval can send unnecessary requests and quickly exhaust the API allowance. Multiple processors or other clients sharing the same public IP consume the same anonymous allowance. Set a suitable interval before starting API mode and avoid leaving live tests running unattended.**

Each processing cycle requests aircraft inside the configured `[grid]` bounds. `loopIntervalMs` controls the pause after the entire cycle, so increasing it also slows publishing, alert checks, and decision handling. It is not a separate OpenSky polling timer. In simulation mode, no OpenSky requests are sent.

The current OpenSky client makes unauthenticated requests. Anonymous data has 10-second time resolution, so polling faster generally does not provide fresher data. A 30-second interval is not required and does not guarantee avoiding rate limits. OpenSky currently documents 400 anonymous credits per day, shared by IP address; the example region in `configuration.cfg` costs one credit per request. From a full allowance, polling every 10 seconds consumes that budget in approximately 67 minutes, or every 30 seconds in approximately 3 hours 20 minutes. Other clients sharing the IP also consume the allowance. See the [OpenSky limits and API credits](https://openskynetwork.github.io/opensky-api/rest.html#limitations) for current policies.

On HTTP `429 Too Many Requests`, OpenSky's `X-Rate-Limit-Retry-After-Seconds` response header indicates how long to wait. Restarting the processor or increasing the interval does not clear an exhausted allowance. The current client throws on request errors and stops the processor; it does not automatically wait and retry.

Geo Service zone requests use the separate `[zones] refreshSeconds` setting. Changing it does not change OpenSky request frequency.

If the processor runs inside Docker, connect it to the same Docker network as Redis
and use `redisUrl=tcp://redis:6379?connect_timeout=200ms&socket_timeout=200ms`.
In API zone mode, configure both Geo Service URLs to use a hostname reachable from that container too.
`localhost` inside Docker refers to the processor container itself.

## Tests

Build first, then run:

```sh
ctest --test-dir build --output-on-failure
```

Alternatively, run the Catch2 executable directly:

```sh
./build/backend_tests
```

Keep Redis running to exercise Redis integration tests. Those tests are skipped when Redis is unavailable.

## Redis output

Messages use the schema in [`proto/FlightData.proto`](proto/FlightData.proto), with contract version 3.

| Destination | Transport | Message |
|---|---|---|
| `flightdata` by default, configurable through `redisChannel` | Redis Pub/Sub | `FlightDataProto` aircraft snapshots each cycle. |
| `varde:tracks` | Redis Stream | The same `FlightDataProto` snapshots, retained as history. |
| `varde:alerts` | Redis Stream | `ZoneAlertEvent` on alert transitions. |
| `varde:vektor-proposals` | Redis Stream | `VektorProposalEvent` containing proposed detour waypoints and estimated extra distance and time. |
| `varde:processor-runs` | Redis Stream | `ProcessorRunStarted` once at processor startup. |

Stream payloads are stored in the `data` field. Each processor startup generates a new UUID, included as `metadata.runId` in all outgoing messages. The startup event also carries it as `ProcessorRunStarted.runId`. This identifies a processing run and lets consumers distinguish events from different runs after a restart.

The track, alert, and proposal streams use approximate trimming to about 10,000 entries each. The processor-run stream is not trimmed by the processor.

Vektor proposals are generated only for simulated aircraft with `PREDICTED` or `IMMINENT` alerts, a known destination, and an active avoidance area whose ID matches the restricted zone ID. A proposal is published only after its alert's opening event has been published successfully.

The processor also consumes `VektorDecisionEvent` messages from `varde:vektor-decisions`, using consumer group `processor-vektor`. Approving a pending proposal applies its waypoints to the simulated aircraft. Rejected or expired proposals do not change the route.

To watch aircraft snapshots:

```sh
docker compose -f ../Varde/docker-compose.yml exec redis redis-cli SUBSCRIBE flightdata
```

To inspect stored aircraft snapshots:

```sh
docker compose -f ../Varde/docker-compose.yml exec -T redis redis-cli XRANGE varde:tracks - + COUNT 10
```

To inspect stored alert events:

```sh
docker compose -f ../Varde/docker-compose.yml exec -T redis redis-cli XRANGE varde:alerts - + COUNT 10
```

The payloads are binary Protobuf, so `redis-cli` displays escaped bytes rather than readable JSON.

## Background and licence

This project builds on the [CapOps processor](https://github.com/magnuax/capops-processor) by the original CapOps team. The inherited project remains credited, and its GPL-3.0 licence is retained in [`LICENSE`](LICENSE).

The inherited architecture is documented in [`docs/as-found.md`](docs/as-found.md).
