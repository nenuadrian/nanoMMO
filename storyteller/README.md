# The storyteller

mvGPT's completions API, from [mvGPT](https://github.com/nenuadrian/mvGPT) at
[`ep014/end`](https://github.com/nenuadrian/mvGPT/tree/ep014/end): `server.py`, `tokenizer.py` and
`requirements-server.txt` as they are there, and the `server` stage of its Dockerfile. mvGPT is
under the MIT licence, in `LICENSE`.

It serves the model `nenuadrian/mvGPT`, at its `ep003-sft` tag on the Hugging Face Hub, which writes
short stories from a few words, as an OpenAI-style `POST /v1/completions` on port 8000. It writes
the requests that wait at the same time together, up to 16 at once.
