# Sounds

A few of the client's sounds are recordings, from [Pixabay](https://pixabay.com/):

| File | Sound | By | Pixabay id |
|---|---|---|---|
| `rain.ogg` | "Relaxing Rain", a minute of it | [DRAGON-STUDIO](https://pixabay.com/users/dragon-studio-38165424/) | 444802 |
| `fire.ogg` | "Fire Crackling Sound", a minute of it | [Jurij](https://pixabay.com/users/soundreality-31074404/) | 499636 |
| `gust.ogg` | "Wind Gust" | [DRAGON-STUDIO](https://pixabay.com/users/dragon-studio-38165424/) | 386158 |
| `teleport.ogg` | "Transform" | [freesound_community](https://pixabay.com/users/freesound_community-46691455/) | 103819 |

Pixabay's licence lets us use them in the game, but not hand out the files on their own, so they
stay out of git, and you make your own copies. Find each on Pixabay by its id, download it, and make
the file the game plays into this folder:

```sh
ffmpeg -ss 450 -t 61 -i dragon-studio-relaxing-rain-444802.mp3 -ac 2 -ar 44100 \
  -c:a libvorbis -q:a 5 assets/sounds/rain.ogg
ffmpeg -ss 10 -t 61 -i soundreality-fire-crackling-sound-499636.mp3 -ac 2 -ar 44100 \
  -c:a libvorbis -q:a 5 assets/sounds/fire.ogg
ffmpeg -i dragon-studio-wind-gust-386158.mp3 -ac 2 -ar 44100 \
  -c:a libvorbis -q:a 5 assets/sounds/gust.ogg
ffmpeg -i teleport-freesound_community-transform-103819.mp3 -ac 2 -ar 44100 \
  -c:a libvorbis -q:a 5 assets/sounds/teleport.ogg
```

The rain and the fire are a steady minute of each, from seven and a half minutes into the rain and
ten seconds into the fire, where it's started. The build copies this folder next to the client.
The client loops the rain while it rains and the fire while it falls in the Ashlands, blows the gust
now and then in the wind, and plays the teleport as you go through a gate. Without a file, as in a
browser, it makes that sound from numbers instead (`client/ambience.h`, `client/foley.h`).
