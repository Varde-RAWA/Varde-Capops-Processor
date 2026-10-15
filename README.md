# Varde Processor

Varde Processor checks whether aircraft are inside or approaching active airspace zones. It receives aircraft positions from a simulator or OpenSky, projects each aircraft's movement 120 seconds ahead, and checks the projected path against zone polygons.

The processor publishes aircraft snapshots and zone-alert events through Redis. It currently provides these warning levels:

| Level | Meaning |
|---|---|
| `NONE` | No projected entry within 120 seconds. |
| `PREDICTED` | Projected entry after 30 seconds and within 120 seconds. |
| `IMMINENT` | Projected entry within 30 seconds. |
| `INFRINGEMENT` | The aircraft is already inside the zone, including its boundary. |

Alert levels rise immediately. A decrease requires three consecutive checks below the published level; the new level is the highest computed during those three checks. Alert events are published when an alert opens, changes level, or closes.

## Requirements

- A C++17 compiler.
- CMake 3.16 or newer.
- Qt 6 with Core and Network. CI uses Qt 6.8.3.
- Protobuf libraries and the `protoc` compiler.
- Git and internet access for the initial CMake configuration, which downloads Catch2, hiredis, and redis-plus-plus.
- A running Redis server. Docker can be used to run Redis locally.
- The Geo Service API when using API zone mode. File mode works without the Geo Service.

## Redis setup

If the Redis container already exists, start it:

```sh
docker start varde-redis
```

Otherwise, create it once:

```sh
docker run -d --name varde-redis -p 127.0.0.1:6379:6379 redis:7
```

Docker must be running before either command. Check the connection:

```sh
docker exec varde-redis redis-cli ping
```

The response should be `PONG`. The processor's local connection is configured in `configuration.cfg`:

```ini
[redis]
redisUrl=tcp://localhost:6379
redisChannel=flightdata
```

## Zone sources

Choose the zone source in `configuration.cfg`. This is independent of the aircraft source: simulated aircraft can use API zones or local zones.

### Geo Service API

Start the Geo Service separately, then configure:

```ini
[zones]
source=api
apiUrl=http://localhost:5040/collections/restricted-areas/items
file=data/zones.geojson
refreshSeconds=30
```

Check that the endpoint responds:

```sh
curl --fail --show-error http://localhost:5040/collections/restricted-areas/items
```

The endpoint returns a GeoJSON `FeatureCollection`. The processor uses features whose `properties.active` value is `true`, so the response may contain both active and inactive zones. It follows pagination links and currently supports `Polygon` geometries with one closed ring.

The processor requests zones at startup and refreshes them every 30 seconds by default. Requests run in the background while aircraft processing continues.

The configured local zone file must exist and contain valid GeoJSON **even in API mode**. It supplies the initial zones until the first successful API response. If a later request fails, the processor logs the failure and keeps the last successfully loaded zones in memory. The cache is not saved to disk.

### Local file without the Geo Service

To run without the API, change the zone source to `file`:

```ini
[zones]
source=file
apiUrl=http://localhost:5040/collections/restricted-areas/items
file=data/zones.geojson
refreshSeconds=30
```

The processor loads `data/zones.geojson` once at startup and makes no zone API requests. Restart it after changing the file. File mode uses the stored `active` flags; it does not recalculate activation windows.

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

With Redis running and the desired zone source configured, start the processor from the repository root so it can find `configuration.cfg` and `data/zones.geojson`:

```sh
./build/backend
```

Press **Ctrl+C** to terminate the processor.

The default aircraft source is simulation:

```ini
[dataSource]
type=sim

[simulation]
timestepSize=0.5

[execution]
loopIntervalMs=500
```

The current application initializes one predefined eastward flight in `ProcessorApp::initializeSources`. It does not yet load the scenario GeoJSON file or use `numFlights` to select the number of aircraft in this setup. Set `type=api` to use the OpenSky source instead.

`loopIntervalMs=500` adds a half-second pause between processing cycles. Zone fetching has its own refresh interval; it does not make an HTTP request every cycle.

If the processor runs inside Docker, configure Redis and Geo Service hostnames reachable from that container. `localhost` then refers to the processor container itself.

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
| `varde:alerts` | Redis Stream | `ZoneAlertEvent` on alert transitions, stored in the `data` field. |

To watch aircraft snapshots:

```sh
docker exec -it varde-redis redis-cli SUBSCRIBE flightdata
```

To inspect stored alert events:

```sh
docker exec varde-redis redis-cli XRANGE varde:alerts - + COUNT 10
```

The payloads are binary Protobuf, so `redis-cli` displays escaped bytes rather than readable JSON.

## Background and licence

This project builds on the [CapOps processor](https://github.com/magnuax/capops-processor) by the original CapOps team. The inherited project remains credited, and its GPL-3.0 licence is retained in [`LICENSE`](LICENSE).

The inherited architecture is documented in [`docs/as-found.md`](docs/as-found.md).
