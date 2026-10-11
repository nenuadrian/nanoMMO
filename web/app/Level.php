<?php

namespace App;

class Level
{
    public static function for(int $experience): int
    {
        $level = 1;
        while ($experience >= 100 * $level) {
            $experience -= 100 * $level;
            $level++;
        }

        return $level;
    }
}
