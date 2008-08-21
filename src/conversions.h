#pragma once

#include <Python.h>
#include <JavaScriptCore/JavaScriptCore.h>

/* sets an exception, returns null (calls JSValue_to_PyString) */
PyObject *JSException_to_PyErr(JSGlobalContextRef, JSValueRef);

/* sets an exception based on the current Python exception */
void set_JSException(JSValueRef *exception);

/* returns a new PyObject or NULL */
PyObject *JSString_to_PyString(JSStringRef);

/* returns a new PyObject or NULL */
PyObject *JSValue_to_PyString(JSGlobalContextRef, JSValueRef);

/* returns a new PyObject or NULL */
PyObject *JSValue_to_PyJSObject(JSValueRef, PyJSObject *thisObject);

/* returns a new JSStringRef or NULL */
JSStringRef PyUnicode_to_JSString(PyObject *);

/* returns a new JSStringRef or NULL */
JSStringRef PyString_to_JSString(PyObject *);

/* returns a new JSStringRef or NULL */
JSStringRef PyObject_to_JSString(PyObject *);

/* returns a JSValueRef (NOT retained) */
JSValueRef PyObject_to_JSValue(PyObject *, PyJSContext *);
