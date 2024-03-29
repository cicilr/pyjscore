#pragma once

#include <Python.h>
#ifdef __APPLE__
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

typedef struct PyJSContext PyJSContext;
typedef struct PyJSObject PyJSObject;
typedef struct PyJSObjectIter PyJSObjectIter;
typedef struct PyJSError PyJSError;

struct PyJSObject {
    PyObject_HEAD
    JSObjectRef         object;         /* retain */
    PyJSObject          *thisObject;    /* retain */
    PyJSContext         *context;       /* retain */
};

struct PyJSContext {
    PyObject_HEAD
	JSGlobalContextRef  context;
	PyJSObject          dummy;
	/* TODO: weak reference dictionary from JSObjectRef to live JSObjects */
	/* TODO: dict from id(PyObject) to JSPyObjects 
	        (which remove themselves from dict on finalization), in order
	        to allow multiply-inserted PyObjects to appear as the same 
	        JSObjectRef */
};

struct PyJSObjectIter {
    PyObject_HEAD
    PyJSObject              *object;    /* retain */
    JSPropertyNameArrayRef  names;      /* retain */
    size_t                  size;
    size_t                  index;
};

struct PyJSError {
    PyBaseExceptionObject   exception;
    JSValueRef              object;         /* retain */
    PyJSContext             *context;       /* retain */
};

extern PyJSObject *PyJSNull;

extern PyTypeObject jscore_PyJSObjectType;
extern PyTypeObject jscore_PyJSObjectIterType;
extern PyTypeObject jscore_PyJSErrorType;

PyObject *PyJSObject_new(JSObjectRef object, PyJSObject *thisObject, PyJSContext *context);

#define JSALLOC(T) \
    ((T *)jscore_ ## T ## Type.tp_alloc(&jscore_ ## T ## Type, 0))
