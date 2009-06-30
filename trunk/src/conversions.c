#include "jscore.h"
#include "jsobj.h"
#include "conversions.h"

#include <assert.h>

PyObject *
JSException_to_PyErr(PyJSContext *context, JSValueRef exception)
{
    PyObject *message = NULL;
    PyObject *exc, *val, *tb;
    PyJSError *pyjs_val;

    if (JSValueIsObjectOfClass(context->context, exception, JSPyErrClass)) {
        JSObjectRef exception_object = JSValueToObject(context->context, exception, NULL);
        assert(exception_object);
        JSPyErrPrivateData *data = JSObjectGetPrivate(exception_object);
        exc = data->exc_type;
        val = data->exc_value;
        tb = data->exc_tb;
        Py_INCREF(exc);
        Py_INCREF(val);
        Py_XINCREF(tb);
        PyErr_Restore(exc, val, tb);
    } else {
        message = JSValue_to_PyString(context, exception);
        assert(message); /* TODO: use a backup message if it fails */
        
        pyjs_val = (PyJSError *)PyObject_CallFunctionObjArgs(
            (PyObject *)&jscore_PyJSErrorType,
            message,
            NULL);
        assert(pyjs_val);
        Py_DECREF(message);
        JSValueProtect(context->context, exception);
        pyjs_val->object = exception;
        Py_INCREF(context);
        pyjs_val->context = context;
        PyErr_SetObject((PyObject *)&jscore_PyJSErrorType, (PyObject *)pyjs_val);
        Py_DECREF((PyObject *)pyjs_val);
    }
    return NULL;
}

void
set_JSException(PyJSContext *context, JSValueRef *exception)
{
    PyObject *exc, *val, *tb;
    PyErr_Fetch(&exc, &val, &tb);
    PyErr_NormalizeException(&exc, &val, &tb);
    assert(val);
    if (PyObject_TypeCheck(val, &jscore_PyJSErrorType)) {
        PyJSError *pyjs_val = (PyJSError *)val;
        assert(pyjs_val && pyjs_val->context == context);
        *exception = pyjs_val->object;
    } else {
        *exception = PyJSPyErr_new(context, val, exc, tb);
    }
    Py_DECREF(exc);
    Py_DECREF(val);
    Py_XDECREF(tb);
}

/* returns a new PyObject or NULL */
PyObject *
JSString_to_PyString(JSStringRef jsstr)
{
    size_t bufferlen = JSStringGetMaximumUTF8CStringSize(jsstr);
    char *buffer = (char *)malloc(bufferlen);
    if (buffer == NULL) {
        return PyErr_NoMemory();
    }
    size_t len = JSStringGetUTF8CString(jsstr, buffer, bufferlen);
    PyObject *pystr = PyUnicode_DecodeUTF8(buffer, len-1, "strict");
    free(buffer);
    return pystr;
}

/* returns a new PyObject or NULL */
PyObject *
JSValue_to_PyString(PyJSContext *context, JSValueRef value)
{
    JSStringRef jsstr = NULL;
    JSValueRef exception = NULL;
    PyObject *pystr = NULL;
    jsstr = JSValueToStringCopy(context->context, value, &exception);
    if (!jsstr) {
        return JSException_to_PyErr(context, exception);
    }
    pystr = JSString_to_PyString(jsstr);
    JSStringRelease(jsstr);
    return pystr;
}

/* returns a new JSStringRef or NULL */
JSStringRef
PyUnicode_to_JSString(PyObject *obj)
{
    JSStringRef value = NULL;
    PyObject *pystr = PyUnicode_AsUTF8String(obj);
    if (pystr) {
        char *str = PyString_AsString(pystr);
        value = JSStringCreateWithUTF8CString(str);
        Py_DECREF(pystr);
    }
    return value;
}

/* returns a new JSStringRef or NULL */
JSStringRef
PyString_to_JSString(PyObject *obj)
{
    PyObject *unicode;
    if (PyUnicode_Check(obj)) {
        return PyUnicode_to_JSString(obj);
    } else if (PyString_Check(obj)) {
        if ((unicode = PyObject_Unicode(obj))) {
            JSStringRef jsstr = PyUnicode_to_JSString(unicode);
            Py_DECREF(unicode);
            return jsstr;
        }
    }
    // TODO: else raise typeerror
    return NULL;
}

/* returns a new JSStringRef or NULL */
JSStringRef
PyObject_to_JSString(PyObject *obj)
{
    PyObject *unicode = NULL;
    
    if (PyUnicode_Check(obj)) {
        return PyUnicode_to_JSString(obj);
    } else if ((unicode = PyObject_Unicode(obj))) {
        JSStringRef jsstr = PyUnicode_to_JSString(unicode);
        Py_DECREF(unicode);
        return jsstr;
    }
    return NULL;
}


PyObject *
JSValue_to_PyJSObject(JSValueRef value, PyJSObject *thisObject)
{
    JSValueRef exception = NULL;
    JSObjectRef jsobj = NULL;
    JSGlobalContextRef context = thisObject->context->context;

    switch (JSValueGetType(context, value)) {
        case kJSTypeBoolean:
            return PyBool_FromLong(JSValueToBoolean(context, value));
        case kJSTypeNumber:
            /* TODO check exception */
            return PyFloat_FromDouble(JSValueToNumber(context, value, NULL));
        case kJSTypeUndefined:
            Py_RETURN_NONE;
        case kJSTypeNull:
            Py_INCREF(PyJSNull);
            return (PyObject *)PyJSNull;
        case kJSTypeString:
            /* TODO check exception */
            return JSValue_to_PyString(thisObject->context, value);
        default:
            jsobj = JSValueToObject(context, value, &exception);
            if (!jsobj) {
                return JSException_to_PyErr(thisObject->context, exception);
            }
            if (JSValueIsObjectOfClass(context, jsobj, JSPyClass)) {
                JSPrivateData *data = JSObjectGetPrivate(jsobj);
                Py_INCREF(data->obj);
                return data->obj;
            }
            return PyJSObject_new(jsobj, 
                thisObject->object ? thisObject : NULL,
                thisObject->context);
    }
}

JSValueRef
PyObject_to_JSValue(PyObject *obj, PyJSContext *context)
{
    if (obj == Py_None) {
        return JSValueMakeUndefined(context->context);
    }
    if (PyObject_TypeCheck(obj, &jscore_PyJSObjectType)) {
        assert(((PyJSObject *)obj)->context == context);
        JSObjectRef jsobj = ((PyJSObject *)obj)->object;
        if (jsobj) {
            return jsobj;
        }
        return JSValueMakeNull(context->context);
    }
    if (PyBool_Check(obj)) {
        return JSValueMakeBoolean(context->context, obj == Py_True);
    }
    if (PyNumber_Check(obj)) {
        double floatVal = PyFloat_AsDouble(obj);
        if (!PyErr_Occurred()) {
            return JSValueMakeNumber(context->context, floatVal);
        } else {
            PyErr_Clear();
        }
    }
    {
        JSStringRef jsstr = PyString_to_JSString(obj);
        if (jsstr) {
            JSValueRef value = JSValueMakeString(context->context, jsstr);
            JSStringRelease(jsstr);
            return value;
        } else if (PyErr_Occurred()) {
            return NULL;
        } else {
            return PyJS_new(context, obj);
        }
    }
}
