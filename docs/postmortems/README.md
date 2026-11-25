# Postmortems

A postmortem is written after something goes wrong for players, or nearly does, be it an outage,
data that's lost or wrong, or a chaos experiment that bit harder than we thought it would. It says
what happened, what it cost, why, how we found out, and what we'll do so it can't happen again, or
hurts less, or we hear of it sooner, each with who and when.

They're blameless, as the SRE book's chapter 15, "Postmortem Culture: Learning from Failure", has
it. A postmortem is about what the system let happen, not who did it. "The code
assumed MySQL always answers" says something we can change; "someone forgot a timeout" doesn't.

Each is a page, named by its date and what happened, from [the template](template.md), which
follows the book's Appendix D, "Example Postmortem". An action item names the step that does it, or
says it's open. A postmortem isn't edited once it's written, apart from an action item's status
and its wording, which 0.97.7 changed without changing what it says.

| Date | What happened | Found by |
|---|---|---|
| [2026-11-18](2026-11-18-a-redis-pause-dropped-everyone-in-ember.md) | A Redis frozen for twelve seconds dropped every player in Ember, and the tick SLO said one tick was missed | Chaos experiment, episode 88 |
