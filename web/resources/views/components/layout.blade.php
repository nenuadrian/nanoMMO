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
                @auth
                    <li>{{ auth()->user()->username }}</li>
                    <li>
                        <form method="post" action="/logout">
                            <button>Log out</button>
                        </form>
                    </li>
                @else
                    <li><a href="/login">Log in</a></li>
                    <li><a href="/register">Create account</a></li>
                @endauth
            </ul>
        </nav>
    </header>
    <main>
        {{ $slot }}
    </main>
</body>
</html>
