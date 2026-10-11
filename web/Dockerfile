FROM dunglas/frankenphp:1.13.1-php8.5 AS php
RUN install-php-extensions pdo_mysql redis zip
COPY --from=composer:2.10.3 /usr/bin/composer /usr/local/bin/
WORKDIR /app

FROM php AS web
COPY composer.json composer.lock ./
RUN composer install --no-dev --no-scripts --no-autoloader
COPY . .
RUN composer dump-autoload --optimize --no-dev \
 && chown -R www-data:www-data storage bootstrap/cache /config/caddy /data/caddy
ENV SERVER_NAME=:8080
USER www-data
EXPOSE 8080
