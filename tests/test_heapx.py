import pytest

from heapx import BINARY, FIBONACCI, KAPLAN, Heap


class Item:
    def __init__(self, priority, value):
        self.priority = priority
        self.value = value


@pytest.mark.parametrize("implementation", ["binary", "fibonacci", "kaplan", BINARY, FIBONACCI, KAPLAN])
def test_push_pop_orders_items(implementation):
    heap = Heap(implementation, key=lambda item: item.priority)
    items = [Item(5, "e"), Item(1, "a"), Item(3, "c")]

    handles = [heap.push(item) for item in items]

    assert len(heap) == 3
    assert all(handle.live for handle in handles)
    assert heap.peek().value == "a"
    assert [heap.pop().value, heap.pop().value, heap.pop().value] == ["a", "c", "e"]
    assert len(heap) == 0
    assert not any(handle.live for handle in handles)


def test_decrease_key_reorders_existing_item():
    heap = Heap("kaplan", key=lambda item: item.priority)
    first = Item(10, "first")
    second = Item(20, "second")

    heap.push(first)
    second_handle = heap.push(second)
    second.priority = 1
    heap.decrease_key(second_handle)

    assert heap.pop() is second
    assert heap.pop() is first


def test_remove_invalidates_handle_and_returns_item():
    heap = Heap("binary", key=lambda item: item.priority)
    item = Item(4, "item")
    handle = heap.push(item)

    assert heap.remove(handle) is item
    assert not handle.live
    with pytest.raises(ValueError):
        heap.remove(handle)


def test_empty_heap_errors_are_pythonic():
    heap = Heap("binary")

    with pytest.raises(IndexError):
        heap.peek()
    with pytest.raises(IndexError):
        heap.pop()


def test_default_heap_orders_items_directly():
    heap = Heap()
    heap.push(3)
    heap.push(1)
    heap.push(2)

    assert [heap.pop(), heap.pop(), heap.pop()] == [1, 2, 3]
