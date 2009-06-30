#pragma once

#include <Python.h>
#ifdef __APPLE__
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

/* sets an exception, returns null (calls JSValue_to_PyString) */
PyObject *JSException_to_PyErr(PyJSContext *, JSValueRef);

/* sets a JS exception based on the current Python exception;
   clears the Python exception */
void set_JSException(PyJSContext *, JSValueRef *exception);

/* returns a new PyObject;
   if an error occurs, sets a Python exception and returns NULL */
PyObject *JSString_to_PyString(JSStringRef);

/* returns a new PyObject;
   if an error occurs, sets a Python exception and returns NULL */
PyObject *JSValue_to_PyString(PyJSContext *, JSValueRef);

/* returns a new PyObject;
   if an error occurs, sets a Python exception and returns NULL */
PyObject *JSValue_to_PyJSObject(JSValueRef, PyJSObject *thisObject);

/* given a unicode object, return the corresponding JSString;
   if an error occurs (a non-unicode object was passed int),
   sets a Python exception and returns NULL */
JSStringRef PyUnicode_to_JSString(PyObject *);

/* given a unicode or str object, return the corresponding JSString;
   if an error occurs (if the str cannot be coerced to unicode),
   sets a Python exception and returns NULL */
JSStringRef PyString_to_JSString(PyObject *);

/* gets the unicode representation of any Python object, and
   returns the corresponding JSString. 
   if an error occurs, sets a Python exception and returns NULL */
JSStringRef PyObject_to_JSString(PyObject *);

/* returns a JSValueRef (NOT protected/retained) */
JSValueRef PyObject_to_JSValue(PyObject *, PyJSContext *);
