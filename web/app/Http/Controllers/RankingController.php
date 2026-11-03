<?php

namespace App\Http\Controllers;

use App\Level;
use Illuminate\Support\Facades\DB;

class RankingController extends Controller
{
    public function __invoke()
    {
        // Each character, by its own name: an account can have several.
        $players = DB::table('players')
            ->orderByDesc('experience')
            ->orderBy('name')
            ->limit(100)
            ->get(['account_id', 'name', 'world', 'experience']);

        foreach ($players as $player) {
            $player->level = Level::for($player->experience);
        }

        return view('rankings', ['players' => $players]);
    }
}
