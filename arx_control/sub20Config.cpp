#include <Python.h>

#include "aspCommon.hpp"


static PyMethodDef sub20config_methods[] = { {NULL, NULL, 0, NULL} };


PyDoc_STRVAR(sub20config_doc, "Compile time configuration values used by the SUB-20 interface.");


static int sub20config_exec(PyObject *module) {
    PyObject* value0 = PyLong_FromLong(MAX_BOARDS);
    PyModule_AddObject(module, "MAX_BOARDS", value0);
    PyObject* value1 = PyLong_FromLong(STANDS_PER_BOARD);
    PyModule_AddObject(module, "STANDS_PER_BOARD", value1);
    #ifdef __USE_INPUT_CURRENT__
        PyObject* value2 = Py_True;
    #else
        PyObject* value2 = Py_False;
    #endif
    PyModule_AddObject(module, "USE_INPUT_CURRENT", value2);
    #ifdef __INCLUDE_MODULE_TEMPS__
        PyObject* value3 = Py_True;
    #else
        PyObject* value3 = Py_False;
    #endif
    PyModule_AddObject(module, "INCLUDE_MODULE_TEMPS", value3);
    
    // Module listings
    PyObject* all = PyList_New(0);
    PyList_Append(all, PyUnicode_FromString("MAX_BOARDS"));
    PyList_Append(all, PyUnicode_FromString("STANDS_PER_BOARD"));
    PyList_Append(all, PyUnicode_FromString("USE_INPUT_CURRENT"));
    PyList_Append(all, PyUnicode_FromString("INCLUDE_MODULE_TEMPS"));
    PyModule_AddObject(module, "__all__", all);
    return 0;
}

static PyModuleDef_Slot sub20config_slots[] = {
    {Py_mod_exec, (void *)&sub20config_exec},
    {0,           NULL}
};

static PyModuleDef sub20config_def = {
    PyModuleDef_HEAD_INIT,    /* m_base */
    "sub20Config",                 /* m_name */
    sub20config_doc,               /* m_doc */
    0,                        /* m_size */
    sub20config_methods,           /* m_methods */
    sub20config_slots,             /* m_slots */
    NULL,                     /* m_traverse */
    NULL,                     /* m_clear */
    NULL,                     /* m_free */
};

PyMODINIT_FUNC PyInit_sub20Config(void) {
  return PyModuleDef_Init(&sub20config_def);
}
