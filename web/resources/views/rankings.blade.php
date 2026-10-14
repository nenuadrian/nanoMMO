<x-layout>
    <h1>Rankings</h1>
    <table>
        <thead>
            <tr>
                <th>#</th>
                <th>Name</th>
                <th>World</th>
                <th>Level</th>
                <th>Experience</th>
            </tr>
        </thead>
        <tbody>
            @forelse ($players as $player)
                <tr>
                    <td>{{ $loop->iteration }}</td>
                    <td>
                        @if ($player->account_id === auth()->id())
                            <mark>{{ $player->username }}</mark>
                        @else
                            {{ $player->username }}
                        @endif
                    </td>
                    <td>{{ $player->world }}</td>
                    <td>{{ $player->level }}</td>
                    <td>{{ $player->experience }}</td>
                </tr>
            @empty
                <tr>
                    <td colspan="5">No one has played yet.</td>
                </tr>
            @endforelse
        </tbody>
    </table>
</x-layout>
