<?php

namespace App\Http\Controllers;

use App\Level;
use Illuminate\Support\Facades\DB;

class RankingController extends Controller
{
    public function __invoke()
    {
        $players = DB::table('players')
            ->join('accounts', 'accounts.id', '=', 'players.account_id')
            ->orderByDesc('players.experience')
            ->orderBy('accounts.username')
            ->limit(100)
            ->get(['players.account_id', 'accounts.username', 'players.experience']);

        foreach ($players as $player) {
            $player->level = Level::for($player->experience);
        }

        return view('rankings', ['players' => $players]);
    }
}
