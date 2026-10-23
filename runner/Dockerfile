# A self-hosted GitHub Actions runner, for the server: GitHub's runner image,
# with Docker Compose, which a deploy runs on the server's Docker.
FROM ghcr.io/actions/actions-runner:2.338.0
COPY --from=docker/compose-bin:v5.6.0 /docker-compose \
  /usr/local/lib/docker/cli-plugins/docker-compose
# The runner says what it does, without its diagnostic log.
ENV ACTIONS_RUNNER_PRINT_LOG_TO_STDOUT=
COPY entrypoint.sh /usr/local/bin/
ENTRYPOINT ["entrypoint.sh"]
