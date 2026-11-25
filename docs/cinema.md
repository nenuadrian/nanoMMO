# Cinema

`nanommo_cinema` films the game. It makes shots of the lands, drawn through the client's own
renderer a frame at a time rather than as fast as the screen goes, so each frame takes as long as it
needs, at any size, and the game's clock can run slower than the film's. Each shot goes into a video
with the game's sounds. The trailer is made with it, and the quiet shots of the weather.

```sh
./build/bin/nanommo_cinema tools/cinema/trailer.txtpb out
./build/bin/nanommo_cinema tools/cinema/trailer.txtpb out shots=earthsplitter size=3840x2160
./build/bin/nanommo_cinema tools/cinema/trailer.txtpb out shots=earthsplitter tall=1   # 1080x1920
./build/bin/nanommo_cinema tools/cinema/trailer.txtpb out still=2.5                    # one frame, as a PNG
./build/bin/nanommo_cinema tools/cinema/weather.txtpb out cel=1                        # with cel shading
./build/bin/nanommo_cinema scout Meadows 9                                             # open ground
./build/bin/nanommo_cinema peaks Meadows                                               # high places
./build/bin/nanommo_cinema shores Ashlands                                             # by the sea
```

It needs ffmpeg on the path, which takes the frames as raw pixels on its input and adds the sound.

## Shots

A film is a text file of shots (`tools/cinema/film.proto`), written like the content files. A shot
names its land, how long it is, and where the camera goes, as keys at moments of the film, each with
where the camera is, what it looks at and its lens, which it passes through on a smooth curve
(Catmull-Rom), or as an orbit round a point. `ease` sets off and stops gently. Heights are above the
ground under a point, or above the sea with `sea`, which keeps a camera flying over hills level.
`tall` and `tall_orbit` give a short's vertical frame a camera of its own, and `sky` changes the
land's sky for the shot, setting the fog, where the sun is, how cloudy, and how much light.

A film looks as the game does when it starts, without cel shading, unless the film turns it on
(`cel_shading: true`), as the trailer does for its cartoon look, or `cel=1` or `cel=0` says
otherwise on the command line.

- A `sky` can have a time of day and weather, as the server sends them (see
  [Day and night](graphics.md#day-and-night)). The time of day is `day`, with `days` to let the day
  go by, a day a minute at 1/60, and the weather is `weather`, by its name, RAIN or FIRE, with its
  `strength`, and `strength_to` for weather that comes or goes in the shot. `wetness` starts the
  ground wet, after the rain. The fog, sun, light and clouds the shot sets go over them, each frame.
  The lightning strikes where it did the last time the shot was filmed, from the shot's name.
- `shoulder` is a camera behind a player, over their shoulder, at a height above their feet and to
  their side, looking past them at something far off, and drifting in from `back` to `back_end`.
  The player turns to face what it looks at. `nanommo_cinema peaks <land>` and `shores <land>` list
  the highest places and the places by the sea, a dozen apart, for someone to stand and look out
  from. `tools/cinema/weather.txtpb` has the weather's shots.

## Slow motion

`pace` says how fast the game runs from a moment of the film, easing from one pace to the next. A
pace of 0.2 is five times slower. The game's clock moves on by the film's frame times the pace, in
small steps, as the pace can change within a frame, and the stage, the animations, the fx, the sea
and the clouds all go by it, while the camera keeps to the film's own time.

## The stage

Players and monsters are written into the shot, and the stage plays them out on the game's clock
(`tools/cinema/film.h`). Players walk or run their legs at the server's speeds and use their skills
when the shot says, and the stage hands the client's animator the world as the server would send it.
A skill's blow lands at its `hit_ms`, with its reach, its arc and its push, as the server's do, so
monsters in reach fall at their last hit, and a push knocks them back. Monsters can charge a player
and swing once they're in reach, and a pack spreads many of a kind round a place, like the seeds of
a sunflower. `nanommo_cinema scout <land> <radius>` lists the flattest open places, with no tree or
water within the radius, to stage a fight on.

## Sound

Each shot's sound is mixed as it's filmed, from what the client would play, which is each cast's
sound and each foot put down, heard from where the camera looks, as the game hears from where you
stand. A sound made while the game is slowed plays slower and lower, by less than the picture slows.

A shot with a time of day or weather has the [weather's sounds](graphics.md#the-weathers-sounds)
under the rest, as the client would play them where the camera looks, with each bolt's thunder
after it, and `music` lays the game's own music under it all, as loud as that, from `music_from`
seconds into the track, fading in and out with the shot.
