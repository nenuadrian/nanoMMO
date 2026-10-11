<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="color-scheme" content="light dark">
    <title>{{ config('app.name') }}</title>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2.1.1/css/pico.classless.min.css">
</head>
<body>
    <header>
        <nav>
            <ul>
                <li><strong>{{ config('app.name') }}</strong></li>
            </ul>
            <ul>
                <li><a href="/">Rankings</a></li>
            </ul>
        </nav>
    </header>
    <main>
        {{ $slot }}
    </main>
</body>
</html>
