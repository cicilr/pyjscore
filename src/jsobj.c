#include "jscore.h"
#include "jsobj.h"
#include "conversions.h"

static JSClassDefinition JSPyClassDef;

JSObjectRef
PyJS_new(PyJSContext *context, PyObject *pyobj)
{
    JSPrivateData *data = malloc(sizeof(JSPrivateData));
    Py_INCREF(pyobj);
    data->obj = pyobj;
    Py_INCREF(context);
    data->context = context;
    return JSObjectMake(context->context, JSPyClass, data);
}

static void
PyJS_finalize(JSObjectRef object)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    Py_DECREF(data->obj);
    Py_DECREF(data->context);
    free(data);
}

static long
PyJS_GetFlags(PyObject *object)
{
    PyObject *flagsobj;
    long flags;
    flagsobj = PyObject_GetAttrString(object, "__jsflags__");
    
    if (flagsobj == NULL) {
        PyErr_Clear();
        return 0;
    }
    if ((flags = PyInt_AsLong(flagsobj)) == -1) {
        PyErr_Clear();
        flags = 0;
    }
    Py_DECREF(flagsobj);
    return flags;
}

static JSValueRef
CallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[],
               JSValueRef *exception)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    PyObject *pyargs = NULL, *result = NULL;
    JSValueRef jsresult;
    int i;
    
    pyargs = PyTuple_New(argumentCount);
    if (!pyargs) return NULL;
    for (i = 0; i < argumentCount; i++) {
        PyObject *arg = JSValue_to_PyJSObject(arguments[i], &data->context->dummy);
        if (arg == NULL) {
            Py_DECREF(pyargs);
            set_JSException(data->context, exception);
            return NULL;
        }
        PyTuple_SetItem(pyargs, i, arg);
    }
    result = PyObject_CallObject(data->obj, pyargs);
    Py_DECREF(pyargs);
    if (result == NULL) {
        set_JSException(data->context, exception);
        return NULL;
    }
    jsresult = PyObject_to_JSValue(result, data->context);
    Py_DECREF(result);
    return jsresult;
}

static bool
HasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    PyObject *pyprop = NULL;
    int result;
    
    if (JSStringGetCharactersPtr(propertyName)[0] == '_' &&
        !(PyJS_GetFlags(data->obj) & ALLOW_PRIVATE_ATTR)) {
        return NULL;
    }
    pyprop = JSString_to_PyString(propertyName);
    if (pyprop == NULL) {
        PyErr_PrintEx(1);
        return 0;
    }
    result = PyObject_HasAttr(data->obj, pyprop);
    Py_DECREF(pyprop);
    return result;
}

static JSValueRef
GetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    PyObject *pyprop = NULL, *pyval = NULL;
    JSValueRef result;
    
    if (JSStringGetCharactersPtr(propertyName)[0] == '_' &&
        !(PyJS_GetFlags(data->obj) & ALLOW_PRIVATE_ATTR)) {
        return NULL;
    }
    pyprop = JSString_to_PyString(propertyName);
    if (pyprop == NULL) {
        set_JSException(data->context, exception);
        return NULL;
    }
    pyval = PyObject_GetAttr(data->obj, pyprop);
    Py_DECREF(pyprop);
    if (pyval == NULL) {
        if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
            return JSValueMakeUndefined(ctx);
        } else {
            set_JSException(data->context, exception);
            return NULL;
        }
    }
    result = PyObject_to_JSValue(pyval, data->context);
    Py_DECREF(pyval);
    return result;
}

/* This function always returns true so that the set request is not forwarded */
static bool
SetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    PyObject *pyprop = NULL, *pyval = NULL;
    long flags = PyJS_GetFlags(data->obj);
    int rv;
    
    if (!(flags & ALLOW_MODIFY_ATTR)) {
        return true;
    }
    if (JSStringGetCharactersPtr(propertyName)[0] == '_' &&
        !(flags & ALLOW_PRIVATE_ATTR)) {
        return true;
    }
    pyprop = JSString_to_PyString(propertyName);
    if (pyprop == NULL) {
        set_JSException(data->context, exception);
        return true;
    }
    pyval = JSValue_to_PyJSObject(value, &data->context->dummy);
    if (pyval == NULL) {
        Py_DECREF(pyprop);
        set_JSException(data->context, exception);
        return true;
    }
    rv = PyObject_SetAttr(data->obj, pyprop, pyval);
    Py_DECREF(pyprop);
    Py_DECREF(pyval);
    if (rv == -1) {
        if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
        } else {
            set_JSException(data->context, exception);
        }
    }
    return true;
}


static bool
DeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    PyObject *pyprop = NULL;
    long flags = PyJS_GetFlags(data->obj);
    int rv;
    
    if (!(flags & ALLOW_MODIFY_ATTR)) {
        return true;
    }
    if (JSStringGetCharactersPtr(propertyName)[0] == '_' &&
        !(flags & ALLOW_PRIVATE_ATTR)) {
        return true;
    }
    pyprop = JSString_to_PyString(propertyName);
    if (pyprop == NULL) {
        set_JSException(data->context, exception);
        return true;
    }
    rv = PyObject_DelAttr(data->obj, pyprop);
    Py_DECREF(pyprop);
    if (rv == -1) {
        if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
        } else {
            set_JSException(data->context, exception);
        }
    }
    return true;
}


void
init_jsobj(void)
{
    JSPyClass = JSClassCreate(&JSPyClassDef);
}

JSClassRef JSPyClass = NULL;

static JSClassDefinition JSPyClassDef = {
    0,                              /* version */
    kJSClassAttributeNone,          /* attributes */
    "PythonObject",                 /* className */
    NULL,                           /* parentClass */
    NULL,                           /* staticValues */
    NULL,                           /* staticFunctions */
    NULL, /* TODO implement*/       /* initialize */
    PyJS_finalize,                  /* finalize */
    HasProperty,                    /* hasProperty */
    GetProperty,                    /* getProperty */
    SetProperty,                    /* setProperty */
    DeleteProperty,                 /* deleteProperty */
    NULL, /* TODO implement*/       /* getPropertyNames */
    CallAsFunction,                 /* callAsFunction */
    NULL,                           /* hasInstance */
    NULL, /* TODO implement*/       /* callAsConstructor */
    NULL, /* TODO implement*/       /* convertToType */
};

