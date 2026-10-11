<x-layout>
    <h1>Rankings</h1>
    <table>
        <thead>
            <tr>
                <th>#</th>
                <th>Name</th>
                <th>Level</th>
                <th>Experience</th>
            </tr>
        </thead>
        <tbody>
            @forelse ($players as $player)
                <tr>
                    <td>{{ $loop->iteration }}</td>
                    <td>{{ $player->username }}</td>
                    <td>{{ $player->level }}</td>
                    <td>{{ $player->experience }}</td>
                </tr>
            @empty
                <tr>
                    <td colspan="4">No one has played yet.</td>
                </tr>
            @endforelse
        </tbody>
    </table>
</x-layout>
