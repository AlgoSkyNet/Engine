#!/bin/sh

docker-compose --env-file Docker/.env -f Docker/docker-compose-boost.yml build || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-quantlib.yml build || exit 1
