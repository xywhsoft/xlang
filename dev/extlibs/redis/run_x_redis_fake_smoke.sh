#!/usr/bin/env sh
set -eu

node "$(dirname "$0")/fake_redis_smoke.js"
