<?php

namespace Tests\Feature;

use Illuminate\Support\Facades\Hash;
use Tests\TestCase;

class PasswordTest extends TestCase
{
    public function test_the_website_and_the_login_server_read_each_others_hashes(): void
    {
        $theirs = sodium_crypto_pwhash_str(
            'correct horse',
            SODIUM_CRYPTO_PWHASH_OPSLIMIT_INTERACTIVE,
            SODIUM_CRYPTO_PWHASH_MEMLIMIT_INTERACTIVE,
        );
        $this->assertTrue(Hash::check('correct horse', $theirs));
        $this->assertFalse(Hash::check('wrong horse', $theirs));
        $this->assertFalse(Hash::needsRehash($theirs));

        $ours = Hash::make('correct horse');
        $this->assertTrue(sodium_crypto_pwhash_str_verify($ours, 'correct horse'));
    }
}
