# heapx-py

Python bindings for the [`heapx`](https://github.com/matteogiorgi/heapx) C
heap library.

This repository keeps the Python package separate from the C library. The C
sources are consumed through `extern/heapx`, which is pinned as a git submodule.

## Development

Clone with submodules:

```bash
git clone --recurse-submodules <heapx-py-url>
cd heapx-py
```

For an existing checkout:

```bash
git submodule update --init --recursive
```

Create an editable install:

```bash
python -m venv .venv
. .venv/bin/activate
python -m pip install -U pip
python -m pip install -e ".[dev]"
pytest
```

## Example

```python
from heapx import Heap


heap = Heap("kaplan", key=lambda item: item.priority)
handle = heap.push(item)

item.priority = 1
heap.decrease_key(handle)

minimum = heap.pop()
```

The public Python API owns Python object references while they are stored in the
native heap. Handles are Python objects and become stale after their item is
removed or extracted.
