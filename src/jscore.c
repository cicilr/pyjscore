#include <Python.h>
#include <structmember.h>
#include <signal.h>

#include "jscore.h"
#include "jsobj.h"
#include "conversions.h"

PyObject *PyJSError;
PyJSObject *PyJSNull;

static PyObject *PyJSContext_getGlobalObject(PyJSContext *);
static PyObject *PyJSObject_repr(PyJSObject *self);

PyObject *
PyJSObject_new(JSObjectRef object, PyJSObject *thisObject, PyJSContext *context)
{
    PyJSObject *self = (PyJSObject *)jscore_PyJSObjectType.tp_alloc(
        &jscore_PyJSObjectType, 0);
    if (!self)
        return NULL;
    self->object = object;
    self->thisObject = thisObject;
    self->context = context;
    
    if (object && context) 
        JSValueProtect(context->context, object);
    Py_XINCREF(thisObject);
    Py_XINCREF(context);
#ifdef TRACE_MALLOC
    printf("ALLOC ");
    PyObject *repr = PyJSObject_repr(self);
    if (!repr)
        return NULL;
    PyObject_Print(repr, stdout, Py_PRINT_RAW);
    Py_DECREF(repr);
    printf("\n");
#endif
    return (PyObject *)self;
}


static int
PyJSObject_contains(PyJSObject *self, PyObject *key)
{
    JSStringRef jsstr;
    int value;
    
    if ((jsstr = PyObject_to_JSString(key))) {
        value = JSObjectHasProperty(self->context->context, self->object, jsstr);
        JSStringRelease(jsstr);
        return (value ? 1 : 0);
    } else {
        return -1;
    }
}


static PyObject *
PyJSObject_getitem(PyJSObject *self, PyObject *key)
{
    JSStringRef jsstr;
    JSValueRef value = NULL;
    JSValueRef exception = NULL;

    if (PyInt_Check(key)) {
        long ikey = PyInt_AsLong(key);
        if (ikey == -1 && PyErr_Occurred()) return NULL;
        if (ikey >= 0 && ikey < UINT_MAX) {
            value = JSObjectGetPropertyAtIndex(self->context->context, self->object,
                ikey, &exception);
            if (!value) {
                return JSException_to_PyErr(self->context->context, exception);
            }
        }
    }
    
    if (!value) {
        if ((jsstr = PyObject_to_JSString(key))) {
            value = JSObjectGetProperty(self->context->context, self->object,
                jsstr, &exception);
            JSStringRelease(jsstr);
            if (!value) {
                return JSException_to_PyErr(self->context->context, exception);
            }
        } else {
            return NULL;
        }
    }
    return JSValue_to_PyJSObject(value, self);
}

static int
PyJSObject_setitem(PyJSObject *self, PyObject *key, PyObject *value)
{
    JSValueRef jsvalue = NULL;
    JSStringRef jsstr = NULL;
    JSValueRef exception = NULL;

    if (value) {
        jsvalue = PyObject_to_JSValue(value, self->context);
        if (!jsvalue) {
            return -1;
        }
    }

    if (value && PyInt_Check(key)) {
        long ikey = PyInt_AsLong(key);
        if (ikey == -1 && PyErr_Occurred()) return -1;
        if (ikey >= 0 && ikey < UINT_MAX) {
            JSObjectSetPropertyAtIndex(self->context->context, self->object,
                ikey, jsvalue, &exception);
            if (exception) {
                JSException_to_PyErr(self->context->context, exception);
                return -1;
            }
            return 0;
        }
    }
    
    if ((jsstr = PyObject_to_JSString(key))) {
        int rv = 0;
        if (value) {
            JSObjectSetProperty(self->context->context, self->object, 
                jsstr, jsvalue, kJSPropertyAttributeNone, &exception);
        } else {
            rv = JSObjectDeleteProperty(self->context->context, self->object,
                jsstr, &exception);
        }
        JSStringRelease(jsstr);
        if (!rv && exception) {
            JSException_to_PyErr(self->context->context, exception);
            return -1;
        }
        return 0;
    } else {
        return -1;
    }
}

static PyObject *
PyJSObject_getattro(PyJSObject *self, PyObject *key)
{
    PyObject *result = NULL;
    JSStringRef jsstr = NULL;
    
    /* See if we can get the attribute by traditional means */
    if ((result = PyBaseObject_Type.tp_getattro((PyObject *)self, key))) {
        return result;
    }
    PyErr_Clear();
    
    
    
    if ((jsstr = PyObject_to_JSString(key))) {
        if (JSObjectHasProperty(self->context->context, self->object, jsstr)) {
            JSValueRef exception = NULL;
            JSValueRef value = JSObjectGetProperty(self->context->context,
                 self->object, jsstr, &exception);
            JSStringRelease(jsstr);
            if (!value) {
                return JSException_to_PyErr(self->context->context, exception);
            }
            return JSValue_to_PyJSObject(value, self);
        } else {
            JSStringRelease(jsstr);
            PyErr_Format(PyExc_AttributeError,
                "JSObject has no property '%.400s'", PyString_AsString(key));
        }
    }
    return NULL;
}



static PyObject *
PyJSObject_getiter(PyJSObject *self)
{
    PyJSObjectIter *iter = JSALLOC(PyJSObjectIter);
#ifdef TRACE_MALLOC
    printf("ALLOC <JSObjectIter>\n");
#endif

    if (!iter) return NULL;
    Py_INCREF(self);
    iter->object = self;
    iter->names = JSObjectCopyPropertyNames(self->context->context,
        self->object);
    iter->index = 0;
    iter->size = JSPropertyNameArrayGetCount(iter->names);
    return (PyObject *)iter;
}

static PyObject *
PyJSObject_repr(PyJSObject *self)
{
    if (!self->object) {
        return PyString_FromString("<JSObject [null]>");
    } else {
        return PyString_FromFormat("<JSObject [%s] at %p>",
            JSObjectIsFunction(self->context->context, self->object) ?
                "function" : "object",
                self->object);
    }
}

static void
PyJSObject_dealloc(PyJSObject *self)
{
#ifdef TRACE_MALLOC
    printf("FREE  ");
    PyObject *repr = PyJSObject_repr(self);
    if (!repr) return;
    PyObject_Print(repr, stdout, Py_PRINT_RAW);
    Py_DECREF(repr);
    printf("\n");
#endif
    if (self->object) {
        JSValueUnprotect(self->context->context, self->object);
    }
    Py_XDECREF(self->thisObject);
    Py_XDECREF(self->context);
    self->ob_type->tp_free((PyObject*)self);
}


static PyObject *
PyJSObject_call(PyJSObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *result = NULL;
    JSValueRef value = NULL;
    JSValueRef exception = NULL;
    int i;

    if (kwargs) {
        PyErr_SetString(PyExc_TypeError, "Keyword arguments are not supported");
        return NULL;
    }
    if (!JSObjectIsFunction(self->context->context, self->object)) {
        PyErr_SetString(PyExc_TypeError, "JSObject not callable");
        return NULL;
    }

    Py_ssize_t argCount = PyTuple_Size(args);
    JSValueRef *valueList = malloc(argCount * sizeof(JSValueRef));
    for (i = 0; i < argCount; i++) {
        PyObject *pyValue = PyTuple_GET_ITEM(args, i);
        valueList[i] = PyObject_to_JSValue(pyValue, self->context);
        if (!valueList[i]) goto argErr;
    }

    value = JSObjectCallAsFunction(self->context->context, self->object, 
        self->thisObject ? self->thisObject->object : NULL,
        argCount, valueList, &exception);
    if (value) {
        result = JSValue_to_PyJSObject(value, &self->context->dummy);
        return result;
    } else {
        JSException_to_PyErr(self->context->context, exception);
    }
    return result;
  argErr:
    free(valueList);
    return NULL;
}

static PySequenceMethods PyJSObject_as_sequence = {
	(lenfunc)0,                             /* sq_length */
	(binaryfunc)0,                          /* sq_concat */
	(ssizeargfunc)0,                        /* sq_repeat */
	(ssizeargfunc)0,                        /* sq_item */
	(ssizessizeargfunc)0,                   /* sq_slice */
	(ssizeobjargproc)0,                     /* sq_ass_item */
	(ssizessizeobjargproc)0,                /* sq_ass_slice */
	(objobjproc)PyJSObject_contains,        /* sq_contains */
	(binaryfunc)0,                          /* sq_inplace_concat */
	(ssizeargfunc)0,                        /* sq_inplace_repeat */
};

static PyMappingMethods PyJSObject_as_mapping = {
    (lenfunc)0,                             /* mp_length */
	(binaryfunc)PyJSObject_getitem,         /* mp_subscript */
	(objobjargproc)PyJSObject_setitem,      /* mp_ass_subscript */
};

PyTypeObject jscore_PyJSObjectType = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "pyjscore.JSObject",            /* tp_name */
    sizeof(PyJSObject),             /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)PyJSObject_dealloc, /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc)PyJSObject_repr,      /* tp_repr */
    0,                              /* tp_as_number */
    &PyJSObject_as_sequence,        /* tp_as_sequence */
    &PyJSObject_as_mapping,         /* tp_as_mapping */
    0,                              /* tp_hash */
    (ternaryfunc)PyJSObject_call,   /* tp_call */
    0,                              /* tp_str */
    (getattrofunc)PyJSObject_getattro, /* tp_getattro */
    (setattrofunc)PyJSObject_setitem, /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    "A wrapper for a JavaScript object.", /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    (getiterfunc)PyJSObject_getiter,/* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    0,                              /* tp_new */
};

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

static PyObject *
PyJSObjectIter_next(PyJSObjectIter *self)
{
    JSStringRef jsstr;
    
    if (self->index >= self->size) {
        return NULL;
    }
    jsstr = JSPropertyNameArrayGetNameAtIndex(self->names, self->index);
    self->index++;
    return JSString_to_PyString(jsstr);
}

static void
PyJSObjectIter_dealloc(PyJSObjectIter *self)
{
    JSPropertyNameArrayRelease(self->names);
    Py_DECREF(self->object);
#ifdef TRACE_MALLOC
    printf("FREE  <JSObjectIter>\n");
#endif
    self->ob_type->tp_free((PyObject*)self);
}


PyTypeObject jscore_PyJSObjectIterType = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "pyjscore.JSObjectIterator",    /* tp_name */
    sizeof(PyJSObjectIter),         /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)PyJSObjectIter_dealloc,/* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    "Iterator over JSObject properties.", /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    (iternextfunc)PyJSObjectIter_next, /* tp_iternext */
    0,                              /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    0,                              /* tp_new */
};

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

static PyObject *
PyJSContext_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyJSContext *self;
    self = (PyJSContext *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->context = JSGlobalContextCreate(NULL);
        if (self->context == NULL) {
            PyErr_SetString(PyJSError, "Context creation failed!");
            self = NULL;
        }
        self->dummy.object = NULL;
        self->dummy.thisObject = NULL;
        self->dummy.context = self;
    }
#ifdef TRACE_MALLOC
    printf("ALLOC <Context>\n");
#endif
    return (PyObject *)self;
}

static void
PyJSContext_dealloc(PyJSContext *self)
{
#ifdef TRACE_MALLOC
    printf("FREE  <Context>\n");
#endif
    JSGlobalContextRelease(self->context);
    JSGarbageCollect(self->context);
    self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
PyJSContext_getGlobalObject(PyJSContext *self)
{
    return PyJSObject_new(
        JSContextGetGlobalObject(self->context), NULL, self);
}

static PyObject *
PyJSContext_evaluate(PyJSContext *self, PyObject *arg)
{
    JSStringRef source;
    JSValueRef value;
    JSValueRef exception;
    
    source = PyString_to_JSString(arg);
    if (!source) {
        /* it could be a file.... */
        return NULL;
    }
    value = JSEvaluateScript(self->context, source, NULL, NULL, 1, &exception);
    if (value) {
        return JSValue_to_PyJSObject(value, &self->dummy);
    } else {
        return JSException_to_PyErr(self->context, exception);
    }
}

static PyObject *
PyJSContext_garbageCollect(PyJSContext *self)
{
    JSGarbageCollect(self->context);
    Py_RETURN_NONE;
}

static PyMethodDef PyJSContext_methods[] = {
    {"eval", (PyCFunction)PyJSContext_evaluate, METH_O,
     "Evaluate the specified string."},
    {"gc", (PyCFunction)PyJSContext_garbageCollect, METH_NOARGS,
     "garbage collect the context"},
    {NULL},
};

static PyGetSetDef PyJSContext_getsetters[] = {
    {"globalObject", (getter)PyJSContext_getGlobalObject},
    {NULL},
};

static PyMemberDef PyJSContext_members[] = {
    {NULL},
};

PyTypeObject jscore_PyJSContextType = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "pyjscore.Context",             /* tp_name */
    sizeof(PyJSContext),            /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)PyJSContext_dealloc,/* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    "A context for JavaScript objects.", /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    PyJSContext_methods,            /* tp_methods */
    PyJSContext_members,            /* tp_members */
    PyJSContext_getsetters,         /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyJSContext_new,                /* tp_new */
};

PyMODINIT_FUNC
initjscore(void)
{
    PyObject* m;
    
    init_jsobj();
    
    m = Py_InitModule3("jscore", NULL,
        "PyJSCore embeds a JavaScript interpreter into Python, and allows "
        "objects to be passed between the two environments.");
    
    if (PyType_Ready(&jscore_PyJSContextType) < 0)
        return;
    
    if (PyType_Ready(&jscore_PyJSObjectType) < 0)
        return;
    
    if (PyType_Ready(&jscore_PyJSObjectIterType) < 0)
        return;
    
    PyJSError = PyErr_NewException("jscore.error", NULL, NULL);
    if (PyJSError == NULL)
        return;
    
    PyJSNull = (PyJSObject *)PyJSObject_new(NULL, NULL, NULL);
    if (PyJSNull == NULL)
        return;
    
    
    Py_INCREF(&jscore_PyJSContextType);
    if (PyModule_AddObject(m, "Context", (PyObject *)&jscore_PyJSContextType) < 0)
        return;
    Py_INCREF(PyJSError);
    if (PyModule_AddObject(m, "error", PyJSError) < 0)
        return;
    Py_INCREF(PyJSNull);
    if (PyModule_AddObject(m, "null", (PyObject *)PyJSNull) < 0)
        return;
    if (PyModule_AddObject(m, "ALLOW_PRIVATE_ATTR", PyInt_FromLong(ALLOW_PRIVATE_ATTR)) < 0)
        return;
    if (PyModule_AddObject(m, "ALLOW_MODIFY_ATTR", PyInt_FromLong(ALLOW_MODIFY_ATTR)) < 0)
        return;
}
