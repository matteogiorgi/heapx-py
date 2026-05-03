#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>

#include "heapx/heap.h"

typedef struct HeapObject HeapObject;
typedef struct HandleObject HandleObject;

typedef struct {
    PyObject *item;
    PyObject *key;
    HandleObject *handle;
} HeapEntry;

struct HeapObject {
    PyObject_HEAD
        struct heapx_heap *heap;
    PyObject *key_func;
};

struct HandleObject {
    PyObject_HEAD
        HeapObject *heap_owner;
    HeapEntry *entry;
    struct heapx_handle handle;
    int live;
};

static PyTypeObject HeapType;
static PyTypeObject HandleType;

static int
entry_cmp(const void *lhs, const void *rhs)
{
    const HeapEntry *left = (const HeapEntry *)lhs;
    const HeapEntry *right = (const HeapEntry *)rhs;
    int result;

    result = PyObject_RichCompareBool(left->key, right->key, Py_LT);
    if (result < 0)
        return 0;
    if (result)
        return -1;

    result = PyObject_RichCompareBool(left->key, right->key, Py_GT);
    if (result < 0)
        return 0;
    if (result)
        return 1;

    return 0;
}

static PyObject *
compute_key(HeapObject *self, PyObject *item)
{
    if (self->key_func == Py_None) {
        Py_INCREF(item);
        return item;
    }

    return PyObject_CallFunctionObjArgs(self->key_func, item, NULL);
}

static void
entry_dispose(HeapEntry *entry)
{
    HandleObject *handle;

    if (entry == NULL)
        return;

    handle = entry->handle;
    if (handle != NULL) {
        entry->handle = NULL;
        handle->live = 0;
        handle->entry = NULL;
        handle->heap_owner = NULL;
        Py_DECREF(handle);
    }

    Py_XDECREF(entry->item);
    Py_XDECREF(entry->key);
    PyMem_Free(entry);
}

static int
parse_implementation(PyObject *value, enum heapx_implementation *out)
{
    const char *name;

    if (value == NULL || value == Py_None) {
        *out = HEAPX_BINARY_HEAP;
        return 0;
    }

    if (PyLong_Check(value)) {
        long implementation = PyLong_AsLong(value);
        if (implementation == -1 && PyErr_Occurred())
            return -1;
        if (
            implementation == HEAPX_BINARY_HEAP ||
            implementation == HEAPX_FIBONACCI_HEAP ||
            implementation == HEAPX_KAPLAN_HEAP
            ) {
            *out = (enum heapx_implementation)implementation;
            return 0;
        }
    }
    else if (PyUnicode_Check(value)) {
        name = PyUnicode_AsUTF8(value);
        if (name == NULL)
            return -1;
        if (strcmp(name, "binary") == 0) {
            *out = HEAPX_BINARY_HEAP;
            return 0;
        }
        if (strcmp(name, "fibonacci") == 0) {
            *out = HEAPX_FIBONACCI_HEAP;
            return 0;
        }
        if (strcmp(name, "kaplan") == 0) {
            *out = HEAPX_KAPLAN_HEAP;
            return 0;
        }
    }

    PyErr_SetString(
        PyExc_ValueError,
        "implementation must be 'binary', 'fibonacci', 'kaplan', or a heapx constant"
    );
    return -1;
}

static int
ensure_heap_open(HeapObject *self)
{
    if (self->heap == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "heap is closed");
        return -1;
    }
    return 0;
}

static int
ensure_live_handle(HeapObject *self, HandleObject *handle)
{
    if (!PyObject_TypeCheck((PyObject *)handle, &HandleType)) {
        PyErr_SetString(PyExc_TypeError, "expected a heapx.Handle");
        return -1;
    }

    if (!handle->live || handle->entry == NULL || handle->heap_owner != self) {
        PyErr_SetString(PyExc_ValueError, "handle is stale or belongs to another heap");
        return -1;
    }

    return 0;
}

static Py_ssize_t
Heap_len(PyObject *object)
{
    HeapObject *self = (HeapObject *)object;
    size_t size;

    if (self->heap == NULL)
        return 0;

    size = heapx_size(self->heap);
    if (size > (size_t)PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "heap size exceeds Py_ssize_t");
        return -1;
    }

    return (Py_ssize_t)size;
}

static int
Heap_init(HeapObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "implementation", "key", NULL };
    PyObject *implementation_arg = NULL;
    PyObject *key_func = Py_None;
    enum heapx_implementation implementation;

    if (!PyArg_ParseTupleAndKeywords(
        args,
        kwargs,
        "|OO:Heap",
        kwlist,
        &implementation_arg,
        &key_func
    ))
        return -1;

    if (self->heap != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Heap objects cannot be reinitialised");
        return -1;
    }

    if (key_func != Py_None && !PyCallable_Check(key_func)) {
        PyErr_SetString(PyExc_TypeError, "key must be callable or None");
        return -1;
    }

    if (parse_implementation(implementation_arg, &implementation) != 0)
        return -1;

    self->heap = heapx_create(implementation, entry_cmp);
    if (self->heap == NULL) {
        PyErr_SetString(PyExc_MemoryError, "failed to create heap");
        return -1;
    }

    Py_INCREF(key_func);
    self->key_func = key_func;
    return 0;
}

static void
Heap_dealloc(HeapObject *self)
{
    HeapEntry *entry;

    if (self->heap != NULL) {
        while (!heapx_empty(self->heap)) {
            entry = (HeapEntry *)heapx_extract_min(self->heap);
            entry_dispose(entry);
        }
        heapx_destroy(self->heap);
        self->heap = NULL;
    }

    Py_XDECREF(self->key_func);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Heap_push(HeapObject *self, PyObject *item)
{
    HeapEntry *entry;
    HandleObject *handle;
    struct heapx_handle c_handle;

    if (ensure_heap_open(self) != 0)
        return NULL;

    entry = PyMem_Calloc(1, sizeof(*entry));
    if (entry == NULL)
        return PyErr_NoMemory();

    Py_INCREF(item);
    entry->item = item;
    entry->key = compute_key(self, item);
    if (entry->key == NULL) {
        entry_dispose(entry);
        return NULL;
    }

    if (heapx_insert_handle(self->heap, entry, &c_handle) != 0) {
        entry_dispose(entry);
        return PyErr_NoMemory();
    }

    handle = PyObject_New(HandleObject, &HandleType);
    if (handle == NULL) {
        entry = (HeapEntry *)heapx_remove(self->heap, c_handle);
        entry_dispose(entry);
        return NULL;
    }

    handle->heap_owner = self;
    handle->entry = entry;
    handle->handle = c_handle;
    handle->live = 1;

    entry->handle = handle;
    Py_INCREF(handle);
    return (PyObject *)handle;
}

static PyObject *
Heap_peek(HeapObject *self, PyObject *Py_UNUSED(ignored))
{
    HeapEntry *entry;

    if (ensure_heap_open(self) != 0)
        return NULL;

    entry = (HeapEntry *)heapx_peek_min(self->heap);
    if (entry == NULL) {
        PyErr_SetString(PyExc_IndexError, "peek from empty heap");
        return NULL;
    }

    Py_INCREF(entry->item);
    return entry->item;
}

static PyObject *
Heap_pop(HeapObject *self, PyObject *Py_UNUSED(ignored))
{
    HeapEntry *entry;
    PyObject *item;

    if (ensure_heap_open(self) != 0)
        return NULL;

    entry = (HeapEntry *)heapx_extract_min(self->heap);
    if (entry == NULL) {
        PyErr_SetString(PyExc_IndexError, "pop from empty heap");
        return NULL;
    }

    item = entry->item;
    Py_INCREF(item);
    entry_dispose(entry);
    return item;
}

static PyObject *
Heap_remove(HeapObject *self, PyObject *arg)
{
    HandleObject *handle = (HandleObject *)arg;
    HeapEntry *entry;
    PyObject *item;

    if (ensure_heap_open(self) != 0)
        return NULL;
    if (ensure_live_handle(self, handle) != 0)
        return NULL;

    entry = (HeapEntry *)heapx_remove(self->heap, handle->handle);
    if (entry == NULL) {
        PyErr_SetString(PyExc_ValueError, "handle is stale");
        return NULL;
    }

    item = entry->item;
    Py_INCREF(item);
    entry_dispose(entry);
    return item;
}

static PyObject *
Heap_decrease_key(HeapObject *self, PyObject *arg)
{
    HandleObject *handle = (HandleObject *)arg;
    HeapEntry *entry;
    PyObject *old_key;
    PyObject *new_key;

    if (ensure_heap_open(self) != 0)
        return NULL;
    if (ensure_live_handle(self, handle) != 0)
        return NULL;

    entry = handle->entry;
    new_key = compute_key(self, entry->item);
    if (new_key == NULL)
        return NULL;

    old_key = entry->key;
    entry->key = new_key;
    if (heapx_decrease_key(self->heap, handle->handle) != 0) {
        entry->key = old_key;
        Py_DECREF(new_key);
        PyErr_SetString(PyExc_ValueError, "handle is stale");
        return NULL;
    }

    Py_DECREF(old_key);
    Py_RETURN_NONE;
}

static PyObject *
Heap_repr(HeapObject *self)
{
    Py_ssize_t size = Heap_len((PyObject *)self);

    if (size < 0)
        return NULL;

    return PyUnicode_FromFormat("<heapx.Heap size=%zd>", size);
}

static PyMethodDef Heap_methods[] = {
    {"push", (PyCFunction)Heap_push, METH_O, "Push an item and return its handle."},
    {"peek", (PyCFunction)Heap_peek, METH_NOARGS, "Return the minimum item."},
    {"pop", (PyCFunction)Heap_pop, METH_NOARGS, "Remove and return the minimum item."},
    {"remove", (PyCFunction)Heap_remove, METH_O, "Remove an item by handle."},
    {"decrease_key", (PyCFunction)Heap_decrease_key, METH_O, "Repair order after an item's key decreased."},
    {NULL, NULL, 0, NULL}
};

static PySequenceMethods Heap_sequence = {
    .sq_length = Heap_len,
};

static PyTypeObject HeapType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "heapx.Heap",
    .tp_basicsize = sizeof(HeapObject),
    .tp_dealloc = (destructor)Heap_dealloc,
    .tp_repr = (reprfunc)Heap_repr,
    .tp_as_sequence = &Heap_sequence,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Native heap backed by the heapx C library.",
    .tp_methods = Heap_methods,
    .tp_init = (initproc)Heap_init,
    .tp_new = PyType_GenericNew,
};

static void
Handle_dealloc(HandleObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Handle_repr(HandleObject *self)
{
    if (self->live)
        return PyUnicode_FromString("<heapx.Handle live=True>");
    return PyUnicode_FromString("<heapx.Handle live=False>");
}

static PyObject *
Handle_live(HandleObject *self, void *Py_UNUSED(closure))
{
    if (self->live)
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyGetSetDef Handle_getset[] = {
    {"live", (getter)Handle_live, NULL, "Whether this handle still points to a live heap item.", NULL},
    {NULL}
};

static PyTypeObject HandleType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "heapx.Handle",
    .tp_basicsize = sizeof(HandleObject),
    .tp_dealloc = (destructor)Handle_dealloc,
    .tp_repr = (reprfunc)Handle_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Handle returned by Heap.push().",
    .tp_getset = Handle_getset,
};

static PyModuleDef heapx_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_heapx",
    .m_doc = "CPython bindings for the heapx C library.",
    .m_size = -1,
};

PyMODINIT_FUNC
PyInit__heapx(void)
{
    PyObject *module;

    if (PyType_Ready(&HeapType) < 0)
        return NULL;
    if (PyType_Ready(&HandleType) < 0)
        return NULL;

    module = PyModule_Create(&heapx_module);
    if (module == NULL)
        return NULL;

    Py_INCREF(&HeapType);
    if (PyModule_AddObject(module, "Heap", (PyObject *)&HeapType) < 0) {
        Py_DECREF(&HeapType);
        Py_DECREF(module);
        return NULL;
    }

    Py_INCREF(&HandleType);
    if (PyModule_AddObject(module, "Handle", (PyObject *)&HandleType) < 0) {
        Py_DECREF(&HandleType);
        Py_DECREF(module);
        return NULL;
    }

    if (PyModule_AddIntConstant(module, "BINARY", HEAPX_BINARY_HEAP) < 0) {
        Py_DECREF(module);
        return NULL;
    }
    if (PyModule_AddIntConstant(module, "FIBONACCI", HEAPX_FIBONACCI_HEAP) < 0) {
        Py_DECREF(module);
        return NULL;
    }
    if (PyModule_AddIntConstant(module, "KAPLAN", HEAPX_KAPLAN_HEAP) < 0) {
        Py_DECREF(module);
        return NULL;
    }

    return module;
}
