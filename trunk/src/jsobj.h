#pragma once

#include <Python.h>
#include <JavaScriptCore/JavaScriptCore.h>

#define ALLOW_PRIVATE_ATTR  1
#define ALLOW_MODIFY_ATTR   2


extern JSClassRef JSPyClass;

typedef struct JSPrivateData {
    PyJSContext     *context;
    PyObject        *obj;
} JSPrivateData;

void init_jsobj(void);
JSObjectRef PyJS_new(PyJSContext *context, PyObject *pyobj);
