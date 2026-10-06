# As-found

## Purpose and scope

The inherited CapOps processor monitors how many aircraft are in each sector of a geographic region and checks whether that number exceeds the sector’s capacity. Weather reduces the available capacity, so a sector can become at risk even when its aircraft count stays the same.

The processor divides the region into a grid, receives aircraft positions and weather updates, and continuously recalculates each sector’s status. Its inputs come from either local simulations or the OpenSky and Open-Meteo APIs. It publishes aircraft snapshots, sector summaries, and events describing changes in risk through Redis.

Its scope ends at publishing these results. It has no operator interface, historical database, or aircraft-control functionality.

## Main modules
The source code is organised into eight main modules: `app`, `compute`, `config`, `domain`, `ingest`, `publish`, `sources`, and `utils`. The `include/` and `src/` directories largely mirror this structure. Header files (`.hpp`) in `include/` declare classes, data types, and interfaces, while implementation files (`.cpp`) in `src/` define their behaviour. Some simple types and interfaces only require a header file.

- **`app`** coordinates the processor. `ProcessorApp` loads the configuration, creates the sources and processing components, and runs the continuous processing loop. Each iteration advances the simulations when enabled, retrieves aircraft and weather updates, passes them to the computation module, and publishes the results. The root-level `main.cpp` creates the Qt application and starts `ProcessorApp`.

- **`config`** manages settings loaded from `configuration.cfg`. These include geographic boundaries, desired grid-cell size, base sector capacity, weather factors, source selection, simulation settings, Redis connection details, and loop timing. `GridConfig` holds the geographic grid settings and calculates its number of rows and columns.

- **`domain`** defines the core data objects used throughout the processor. `Track` represents an aircraft’s state at a particular timestamp, including position, altitude, speed, and direction. `WeatherCell` describes weather severity in a sector. `SectorSummary` stores a sector’s traffic and capacity information and calculates its effective capacity and status (`NORMAL`, `AT_RISK`, or `CONGESTED`). `RiskEvent` describes changes in sector risk. The `types/` subfolder contains supporting structures and enumerations, such as `Position`, `WeatherSeverity`, `SectorState`, and `SourceType`. It also contains `ProcessingResult`, which groups the results collected for publication.

- **`sources`** provides aircraft and weather data. `TrackSourceOpenSky` retrieves aircraft observations from OpenSky, while `WeatherSourceOpenMeteo` retrieves weather information from Open-Meteo. The simulated source classes provide equivalent access to locally generated data. The `interfaces/` subfolder defines `ITrackSource` and `IWeatherSource`, allowing the processor to use either simulated or API sources through a common interface. The `simulations/` subfolder contains `RadarSimulator`, which generates and advances simulated flights, and `WeatherSimulator`, which generates changing weather patterns.

- **`ingest`** is a layer between `ProcessorApp` and the sources, which retrieve data from external APIs or local simulators. `IngestService` provides methods for retrieving individual aircraft, all aircraft, and weather severity at a position. It delegates these requests to the selected sources, giving the application a central access point for incoming data.

- **`compute`** contains the grid and sector-processing logic. `Grid` defines the geographic boundaries and arrangement of sectors in rows and columns. It checks whether an aircraft’s position is inside the grid and identifies its sector. It also calculates each sector’s centre position for weather sampling. `ComputeData` handles aircraft and weather updates, maintains the information needed for sector calculations, and collects aircraft snapshots, sector summaries, and risk events into a `ProcessingResult`.

- **`publish`** prepares and transmits processing results. `ProtoMapper` converts domain objects into Protobuf messages according to the schema in `proto/FlightData.proto`. `RedisPublisher` serializes the resulting message into binary data and publishes it on the configured Redis channel. This separates the internal data model from its transport representation.

- **`utils`** contains shared helper functions. `WeatherPatternUtils` supports weather-pattern calculations, while the `time/` subfolder contains `IsoTimestamp`, which generates timestamps used in snapshots and events.

## Data flow

- **Input:** Aircraft and weather data come from external APIs or local simulators.

- **Retrieval:** `ProcessorApp` requests data through `IngestService`, which forwards the requests to the selected sources.

- **Processing:** `ComputeData` stores the latest aircraft snapshots, updates sector counts and weather conditions, and triggers sector-status calculations. Changes in sector status generate risk events.

- **Collection:** Current aircraft snapshots, sector summaries, and pending risk events are gathered into a `ProcessingResult`.

- **Output:** `ProtoMapper` converts the results into Protobuf messages. `RedisPublisher` serializes the completed message and publishes it to Redis.

This sequence repeats during each processing cycle.

## Build and run

Building requires CMake 3.16 or newer, a C++17 compiler, Qt 6 (Core and Network), and Protobuf. CMake downloads Catch2, hiredis, and redis-plus-plus during configuration, so the initial build requires internet access.

Run the following commands from the repository root:

```sh
cmake -S . -B build
cmake --build build --config Release
```

### Redis

The processor requires a running Redis server at the address specified by `redisUrl` in `configuration.cfg`. Keep one address active and comment out the other with `#`:

```ini
[redis]
# When running the processor directly on your machine:
redisUrl=tcp://localhost:6379

# When running the processor in Docker, with Redis reachable
# as redis-server on the same Docker network:
#redisUrl=tcp://redis-server:6379

redisChannel=flightdata
```

The Docker hostname must match the Redis service name or network alias.

With Docker Desktop running, start the existing Redis container:

```sh
docker start varde-redis
```

Check that Redis responds:

```sh
docker exec varde-redis redis-cli ping
```

A successful check returns `PONG`.

### macOS / Linux

Run the processor from the repository root so it can find `configuration.cfg`:

```sh
./build/backend
```

Press Ctrl+C to terminate the processor.

Run the tests:

```sh
./build/backend_tests
```

### Windows

With a Visual Studio build, run the processor from the repository root:

```powershell
.\build\Release\backend.exe
```

Press Ctrl+C to terminate the processor.

Run the tests:

```powershell
.\build\Release\backend_tests.exe
```

Depending on the build tool, the executables may instead be directly under `build`, without the `Release` subfolder. Ninja is one such build tool used by CMake.

Windows is not included in the current CI build matrix; these commands have not been verified on Windows.

Tests can also be run through CTest on all platforms:

```sh
ctest --test-dir build -C Release --output-on-failure
```

## Existing tests

The project uses Catch2, with tests grouped by component:

| Test file | Coverage |
|---|---|
| `tests/domain/test_domain.cpp` | Construction and getters for tracks, weather cells, and risk events; sector aircraft-count changes and weather updates. |
| `tests/compute/test_grid.cpp` | Grid dimensions, sector counts, inside/outside positions, row and column calculations, and mapping sector centres back to their sector IDs. |
| `tests/compute/test_compute_data.cpp` | Initial state, adding and updating aircraft, movement between sectors, removal when leaving the grid, weather updates, and risk escalation and de-escalation. |
| `tests/weather/test_weather.cpp` | Weather simulator initialization, constant and time-dependent patterns, severity bounds, simulated weather-source output, and valid/invalid Open-Meteo height settings. |
| `tests/proto/test_proto.cpp` | Protobuf mapping and serialization/deserialization, checking metadata, aircraft fields, risk events, sector summaries, and empty results. |
| `tests/redis/test_redis.cpp` | Attempts to construct the Redis publisher and publish a sample result. |
| `tests/integration/test_integration.cpp` | Combined aircraft and weather processing across sectors, followed by Protobuf serialization/deserialization and an optional Redis publishing attempt. |
| `tests/test_config_utils.cpp` | Loading values from a test configuration, sorting weather thresholds, and retrieving weather factors. |

Coverage limitations: Redis-related tests accept exceptions and therefore can pass without successful publication. No subscriber verifies receipt of a message. The suite does not directly test OpenSky retrieval, live Open-Meteo responses, `RadarSimulator`, or `IngestService`.

Verification: The complete `backend_tests` executable was run locally, and all tests passed.

