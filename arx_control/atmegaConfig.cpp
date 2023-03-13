#include <Python.h>

#include "aspCommon.hpp"


static PyMethodDef ATmegaMethods[] = { {NULL, NULL, 0, NULL} };


PyDoc_STRVAR(atmegaconfig_doc, "Compile time configuration values used by the ATmega interface.");


PyMODINIT_FUNC PyInit_atmegaConfig(void) {
  PyObject *m, *all, *value0, *value1, *value2, *value3;
  
  // Module definitions and functions
  static struct PyModuleDef moduledef = {
     PyModuleDef_HEAD_INIT, "atmegaConfig", atmegaconfig_doc, -1, ATmegaMethods
  };
  m = PyModule_Create(&moduledef);
  if( m == NULL ) {
      return NULL;
  }
  
  // Constants
  value0 = PyLong_FromLong(MAX_BOARDS);
  PyModule_AddObject(m, "MAX_BOARDS", value0);
  value1 = PyLong_FromLong(STANDS_PER_BOARD);
  PyModule_AddObject(m, "STANDS_PER_BOARD", value1);
  #ifdef __USE_INPUT_CURRENT__
    value2 = Py_True;
  #else
    value2 = Py_False;
  #endif
  PyModule_AddObject(m, "USE_INPUT_CURRENT", value2);
  #ifdef __INCLUDE_MODULE_TEMPS__
    value3 = Py_True;
  #else
    value3 = Py_False;
  #endif
  PyModule_AddObject(m, "INCLUDE_MODULE_TEMPS", value3);
  
  // Module listings
  all = PyList_New(0);
  PyList_Append(all, PyUnicode_FromString("MAX_BOARDS"));
  PyList_Append(all, PyUnicode_FromString("STANDS_PER_BOARD"));
  PyList_Append(all, PyUnicode_FromString("USE_INPUT_CURRENT"));
  PyList_Append(all, PyUnicode_FromString("INCLUDE_MODULE_TEMPS"));
  PyModule_AddObject(m, "__all__", all);
  
  return m;
}
  
