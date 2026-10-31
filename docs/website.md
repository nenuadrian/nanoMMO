# The website

`web/` is the game's website, a [Laravel](https://laravel.com) app. It ranks the players by
experience, and you can create an account on it, log in and log out. It reads and writes the same
MySQL tables as the servers, so an account made on the website logs into the game, and the other way
round. It hashes passwords with Argon2id, with the login server's settings, in
`web/config/hashing.php`, so each reads the other's hashes. It keeps its sessions in Redis.

It runs in Docker, on [FrankenPHP](https://frankenphp.dev), a web server with PHP built in:

```bash
docker compose up -d --build web
```

Then open http://localhost:8080. [Pico CSS](https://picocss.com), from a CDN, styles the plain HTML,
so there's no CSS of our own, and nothing to build with npm.

- The rankings are the hundred players with the most experience. An account that has never played
  isn't on them. The saver brings them up to date every five seconds.
- Each address gets ten tries a minute at creating an account or logging in, since each try hashes
  a password, which takes 64 MiB and tens of milliseconds.
- The tables come from the migrations in `db/`, which Flyway runs (see [Migrations](saving.md#migrations)).
  The website has no migrations of its own, so the tables are described in one place.
- `APP_KEY` in `compose.yaml` encrypts the website's cookies. Like the MySQL password next to it,
  it's for your own machine. On a server, `deploy.sh` makes a new one (see [Deployment](deployment.md)).
  To make one yourself, run `docker compose run --rm --no-deps web php artisan key:generate --show`,
  and keep it out of git.

The website's tests, and Pint, Laravel's formatter, which fails on code it would change:

```bash
docker build --target test web
```
