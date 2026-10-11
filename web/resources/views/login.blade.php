<x-layout>
    <h1>Log in</h1>
    <form method="post" action="/login">
        <label>
            Name
            <input name="username" value="{{ old('username') }}" autocomplete="username" required
                @error('username') aria-invalid="true" @enderror>
            @error('username')
                <small>{{ $message }}</small>
            @enderror
        </label>
        <label>
            Password
            <input type="password" name="password" autocomplete="current-password" required>
        </label>
        <button>Log in</button>
    </form>
</x-layout>
