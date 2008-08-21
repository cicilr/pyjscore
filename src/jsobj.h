#pragma once

#include <Python.h>
#include <JavaScriptCore/JavaScriptCore.h>

extern JSClassRef JSPyClass;

typedef struct JSPrivateData {
    PyJSContext     *context;
    PyObject        *obj;
} JSPrivateData;

void init_jsobj(void);
JSObjectRef PyJS_new(PyJSContext *context, PyObject *pyobj);
