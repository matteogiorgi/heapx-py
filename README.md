# heapx-py

Python bindings for the [`heapx`](https://github.com/matteogiorgi/heapx) C
heap library.

This repository keeps the Python package separate from the C library. The C
sources are consumed through `extern/heapx`, which is pinned as a git submodule.

The package exposes a small Pythonic API backed by a CPython extension module.
Python objects are stored by reference while they are inside the native heap,
and `Handle` objects model the handles returned by the C library.

## Status

This package is experimental. The current API is intentionally small:

- `Heap(implementation="binary", key=None)`
- `Heap.push(item) -> Handle`
- `Heap.peek() -> item`
- `Heap.pop() -> item`
- `Heap.remove(handle) -> item`
- `Heap.decrease_key(handle) -> None`
- `len(heap)`
- `handle.live`

Supported heap implementations are `"binary"`, `"fibonacci"`, and `"kaplan"`.

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

Build source and wheel distributions:

```bash
python -m build
```

The wheel produced locally is specific to the current Python version, operating
system, and CPU architecture. Use the `Wheels` GitHub Actions workflow to build
binary wheels for multiple platforms.

## Binary wheels

Binary wheels are built with
[`cibuildwheel`](https://github.com/pypa/cibuildwheel). The workflow in
`.github/workflows/wheels.yml` currently targets:

- Linux x86_64 and aarch64
- macOS x86_64 and arm64
- Windows x64
- CPython 3.9 through 3.14

Run the workflow manually from the GitHub Actions tab, or create and push a
version tag:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The built wheels and source distribution are uploaded as workflow artifacts.

## Submodule workflow

`extern/heapx` is a git submodule. The main `heapx-py` repository stores only
the submodule URL and the exact `heapx` commit to use.

Update the submodule checkout after cloning:

```bash
git submodule update --init --recursive
```

Move the binding to a newer `heapx` commit:

```bash
cd extern/heapx
git fetch origin
git checkout main
git pull
cd ../..
git add extern/heapx
git commit -m "Update heapx submodule"
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

## API notes

The public Python API owns Python object references while they are stored in the
native heap. Handles are Python objects and become stale after their item is
removed or extracted.

If `key` is omitted, items are compared directly. If `key` is provided, it is
called when an item is pushed and again when `decrease_key()` is called.

`decrease_key()` should only be used after mutating an item so that its key
becomes smaller according to the heap ordering. Increasing a key and then
calling `decrease_key()` does not restore heap order.

## License

The bundled `heapx` C library is licensed under GPLv3. This package declares
`GPL-3.0-only` and includes the upstream license file in source and wheel
distributions.
