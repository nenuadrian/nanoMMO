<?php

namespace Tests\Unit;

use App\Level;
use PHPUnit\Framework\TestCase;

class LevelTest extends TestCase
{
    public function test_each_level_takes_a_hundred_more_than_the_last(): void
    {
        $this->assertSame(1, Level::for(0));
        $this->assertSame(1, Level::for(99));
        $this->assertSame(2, Level::for(100));
        $this->assertSame(3, Level::for(450));
    }
}
