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

typedef struct JSPrivateData {
    PyJSContext     *context;
    PyObject        *obj;
} JSPrivateData;

void init_jsobj(void);
JSObjectRef PyJS_new(PyJSContext *context, PyObject *pyobj);
