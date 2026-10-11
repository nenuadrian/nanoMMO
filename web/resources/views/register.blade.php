<x-layout>
    <h1>Create an account</h1>
    <form method="post" action="/register">
        @csrf
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
            <input type="password" name="password" autocomplete="new-password" required
                @error('password') aria-invalid="true" @enderror>
            @error('password')
                <small>{{ $message }}</small>
            @enderror
        </label>
        <label>
            Password again
            <input type="password" name="password_confirmation" autocomplete="new-password" required>
        </label>
        <button>Create account</button>
    </form>
</x-layout>
