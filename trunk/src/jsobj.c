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

static JSValueRef
CallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject,
               size_t argumentCount, const JSValueRef arguments[],
               JSValueRef *exception)
{
    JSPrivateData *data = JSObjectGetPrivate(object);
    PyObject *pyargs = NULL, *result = NULL;
    int i;
    
    pyargs = PyTuple_New(argumentCount);
    if (!pyargs) return NULL;
    for (i = 0; i < argumentCount; i++) {
        PyObject *arg = JSValue_to_PyJSObject(arguments[i], &data->context->dummy);
        if (arg == NULL) {
            Py_DECREF(pyargs);
            set_JSException(exception);
            
        }
        PyTuple_SetItem(pyargs, i, arg);
    }
    result = PyObject_CallObject(data->obj, pyargs);
    // TODO: leaks memory like crazy!
    return PyObject_to_JSValue(result, data->context);
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
    NULL,                           /* hasProperty */
    NULL, /* TODO implement*/       /* getProperty */
    NULL, /* TODO implement*/       /* setProperty */
    NULL, /* TODO implement*/       /* deleteProperty */
    NULL, /* TODO implement*/       /* getPropertyNames */
    CallAsFunction,                 /* callAsFunction */
    NULL, /* TODO implement*/       /* hasInstance */
    NULL, /* TODO implement*/       /* callAsConstructor */
    NULL, /* TODO implement*/       /* convertToType */
};

