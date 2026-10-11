<?php

namespace App\Http\Controllers;

use App\Models\Account;
use Illuminate\Database\UniqueConstraintViolationException;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Auth;

class RegisterController extends Controller
{
    public function create()
    {
        return view('register');
    }

    public function store(Request $request)
    {
        // The login server's rules, in CheckUsername() and CheckPassword().
        $fields = $request->validate([
            'username' => ['required', 'string', 'between:3,16', 'regex:/^[A-Za-z0-9_]+$/'],
            'password' => ['required', 'string', 'between:8,128', 'confirmed'],
        ], [
            'username.regex' => 'Names are letters, digits and underscores.',
        ], [
            'username' => 'name',
        ]);

        // The unique key decides. Checking first and inserting after would let
        // two people take the same name in between.
        try {
            $account = Account::create([
                'username' => $fields['username'],
                'password_hash' => $fields['password'],
            ]);
        } catch (UniqueConstraintViolationException) {
            return back()
                ->withErrors(['username' => 'That name is taken.'])
                ->onlyInput('username');
        }

        Auth::login($account);
        $request->session()->regenerate();

        return redirect('/');
    }
}
