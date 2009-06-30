#pragma once

#include <Python.h>
#ifdef __APPLE__
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif

#define ALLOW_PRIVATE_ATTR  1
#define ALLOW_MODIFY_ATTR   2

extern JSClassRef JSPyClass;
extern JSClassRef JSPyErrClass;

typedef struct JSPrivateData {
    PyJSContext     *context;
    PyObject        *obj;
} JSPrivateData;

typedef struct JSPyErrPrivateData {
    PyJSContext     *context;
    PyObject        *exc_value;
    PyObject        *exc_type;
    PyObject        *exc_tb;
} JSPyErrPrivateData;

void init_jsobj(void);
JSObjectRef PyJS_new(PyJSContext *context, PyObject *pyobj);
JSObjectRef PyJSPyErr_new(PyJSContext *context, PyObject *val, PyObject *type, PyObject *tb);
