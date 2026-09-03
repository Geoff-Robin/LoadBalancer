# Load Balancer

A C++17 HTTP load balancer that routes requests to registered backends. Backend
selection is pluggable; the default strategy is round robin.

## Features

- Register one or more backends for specific request paths.
- Forward requests to healthy matching backends.
- Run health checks against each backend's `GET /health` endpoint at startup
  and every five minutes.
- Persist backend definitions, health state, and the last health-check time in
  SQLite.
- Exclude unhealthy backends; a forwarding failure also marks a backend
  unhealthy immediately.
- Replace the round-robin strategy without changing proxying or health-check
  code.

## Run locally

Install dependencies and configure the project with Conan, then build and run
the executable:

```sh
make configure
make dev-build
make dev-run
```

The server listens on `http://localhost:3000` and creates `load_balancer.db`
in its working directory.

## Register a backend

Register a backend by sending its address and the paths it serves. Addresses
may use `host`, `host:port`, or `[ipv6]:port` format.

```sh
curl -X POST http://localhost:3000/backends/register \
  -H "Content-Type: application/json" \
  -d '{"host":"localhost:8081","urls":["/api/users"]}'
```

Registering the same host again replaces its path list. Each backend must
provide a `GET /health` endpoint that returns a `2xx` response when it is
ready to receive traffic.

After registration, requests to a registered path are forwarded to the healthy
backends serving that path:

```sh
curl http://localhost:3000/api/users
```

Requests with no registered backend return `404`. Requests that cannot be sent
to a selected backend return `502` and mark that backend unhealthy.

## Docker

```sh
docker build -t load-balancer .
docker run -d --name load-balancer -p 3000:3000 -v load-balancer-data:/data load-balancer
```

The Docker volume preserves the SQLite database, including backend and health
state, between container restarts.

## Architecture

`src/proxy.cpp` composes the server, backend registry, health monitor, proxy,
and round-robin strategy. Incoming connections follow this path:

```text
client
  | HTTP/1.1
  v
slam::Server --> bounded inbound queue --> worker thread
  |                                      |
  | exact management-route match         v
  +--> routes::Router                  BackendProxy fallback
  |     +-- GET /                      +-- load matching backends from SQLite
  |     +-- POST /backends/register    +-- exclude unhealthy entries
  |                                    +-- select one with a strategy
  |                                    +-- forward the HTTP request
  v
response
```

### Server and routes

The `slam` library provides a synchronous HTTP/1.1 server built on Boost.Asio
and Boost.Beast. One accept loop puts sockets into a bounded queue, and four
worker threads consume them by default. Workers support keep-alive connections
and dispatch exact method/path routes. A path with a registered route but the
wrong HTTP method produces `405`; an unmatched request reaches the proxy
fallback.

The `routes` library contains the management API:

- `GET /` returns a JSON service response.
- `POST /backends/register` validates and stores a backend host plus its exact
  served paths. Re-registering a host replaces its paths.

### Backend storage and health

`BackendRegistry` owns the SQLite schema and migrations. It persists backend
hosts, their registered paths, `health_status` (`unknown`, `healthy`, or
`unhealthy`), and `health_checked_at`.

`BackendHealthMonitor` runs once when the process starts and then every five
minutes. It sends `GET /health` to every registered backend; a `2xx` response
marks the backend healthy, otherwise it is unhealthy. It keeps one
`requests::BackendConnectionPool` (maximum one connection) per backend for
these checks. A failed proxied request also immediately records `unhealthy` in
SQLite. New registrations are `unknown` and remain eligible until their first
health check completes.

### Proxy and selection strategy

`BackendProxy` matches the request path without its query string against the
paths stored for each backend. It removes unhealthy candidates, gives the
remaining list to a `LoadBalancingStrategy`, and returns `404` when no eligible
backend exists.

`RoundRobinStrategy` is the current strategy. Its atomic counter advances for
each selection and chooses the next eligible backend. To add another policy,
implement `LoadBalancingStrategy::select` and pass it to `BackendProxy` in
`src/proxy.cpp`.

The selected request is currently relayed over a fresh synchronous TCP
connection using HTTP/1.1. The `requests` connection-pool library is reusable
in this path, but is presently used only by health checks.

### Build and deployment

CMake builds four libraries: `slam`, `requests`, `core`, and `routes`, then
links the `load_balancer` executable. Conan supplies Boost, spdlog,
nlohmann_json, and SQLite. The Docker image builds a Release executable on
Ubuntu and runs it as a non-root user with `/data` as a persistent volume for
the SQLite database.
