#include <Python.h>

#include "aspCommon.hpp"


static PyMethodDef atmegaconfig_methods[] = { {NULL, NULL, 0, NULL} };


PyDoc_STRVAR(atmegaconfig_doc, "Compile time configuration values used by the ATmega interface.");


static int atmegaconfig_exec(PyObject *module) {
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

static PyModuleDef_Slot atmegaconfig_slots[] = {
    {Py_mod_exec, (void *)&atmegaconfig_exec},
    {0,           NULL}
};

static PyModuleDef atmegaconfig_def = {
    PyModuleDef_HEAD_INIT,    /* m_base */
    "atmegaConfig",           /* m_name */
    atmegaconfig_doc,         /* m_doc */
    0,                        /* m_size */
    atmegaconfig_methods,     /* m_methods */
    atmegaconfig_slots,       /* m_slots */
    NULL,                     /* m_traverse */
    NULL,                     /* m_clear */
    NULL,                     /* m_free */
};

PyMODINIT_FUNC PyInit_atmegaConfig(void) {
  return PyModuleDef_Init(&atmegaconfig_def);
}
