# DIY AI: Word Meaning Sketch

This tiny demo tries to extract "meaning hints" from text using WordNet.
It looks up each content word in WordNet, grabs the first sense per POS,
and builds a simple frequency table of related terms from synset words
and gloss text.

## Build

```bash
cmake -S . -B build
cmake --build build --target wn-meaning
cmake --build build --target wn-chat
```

If you prefer building from inside `diy-ai/`:

```bash
cmake -S . -B .
cmake --build . --target wn-meaning
cmake --build . --target wn-chat
```

## Run

```bash
./build/diy-ai/wn-meaning "the sun warmed the cold stone"
```

Set `WNHOME` if you want a non-default WordNet data path:

```bash
WNHOME=/path/to/WordNet-3.0 ./build/diy-ai/wn-meaning "river of time"
```

## CLI

```bash
./build/diy-ai/wn-meaning --help
./build/diy-ai/wn-meaning --top 8 --no-gloss "retry failed requests"
./build/diy-ai/wn-meaning --no-hypernyms "database latency cache"
```

## Chat

```bash
./build/diy-ai/wn-chat
```

The chat uses WordNet to expand terms with synonyms/hypernyms, ranks concepts,
and returns a short, natural response with a simple confidence score and
grammar guardrails.

Inside the chat:

```
build a CLI that parses log files
add retries and backoff for failed requests
/exit
```
