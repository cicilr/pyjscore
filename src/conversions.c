#include "jscore.h"
#include "jsobj.h"
#include "conversions.h"


PyObject *
JSException_to_PyErr(JSGlobalContextRef context, JSValueRef exception)
{
    PyObject *message = JSValue_to_PyString(context, exception);
    /* XXX infinite loop oh noooooooooo */
    PyErr_SetObject(PyJSError, message);
    Py_DECREF(message);
    return NULL;
}

void
set_JSException(PyJSContext *context, JSValueRef *exception)
{
    PyErr_Clear();
    JSStringRef str = JSStringCreateWithUTF8CString("a python error occurred");
    *exception = JSValueMakeString(context->context, str);
    JSStringRelease(str);
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
JSValue_to_PyString(JSGlobalContextRef context, JSValueRef value)
{
    JSStringRef jsstr = NULL;
    JSValueRef exception = NULL;
    PyObject *pystr = NULL;
    jsstr = JSValueToStringCopy(context, value, &exception);
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
            return JSValue_to_PyString(context, value);
        default:
            jsobj = JSValueToObject(context, value, &exception);
            if (!jsobj) {
                return JSException_to_PyErr(context, exception);
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
