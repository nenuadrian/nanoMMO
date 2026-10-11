<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Attributes\Fillable;
use Illuminate\Database\Eloquent\Attributes\Hidden;
use Illuminate\Database\Eloquent\Attributes\WithoutTimestamps;
use Illuminate\Foundation\Auth\User as Authenticatable;

#[Fillable(['username', 'password_hash'])]
#[Hidden(['password_hash'])]
#[WithoutTimestamps]
class Account extends Authenticatable
{
    protected $authPasswordName = 'password_hash';

    protected $rememberTokenName = '';

    protected function casts(): array
    {
        return [
            'password_hash' => 'hashed',
        ];
    }
}
