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

The core components are intentionally separated:

- `BackendRegistry` stores registered backends and their persisted health data.
- `BackendHealthMonitor` performs scheduled health requests through the shared
  connection-pool code in `requests`.
- `BackendProxy` filters matching healthy backends and relays HTTP traffic.
- `LoadBalancingStrategy` defines the selection interface.
- `RoundRobinStrategy` is the current strategy implementation.

To implement another policy, add a class that implements
`LoadBalancingStrategy::select` and pass it to `BackendProxy` in `src/proxy.cpp`.
