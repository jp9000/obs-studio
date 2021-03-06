/******************************************************************************
    Copyright (C) 2017 by Hugh Bailey <jim@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <util/c99defs.h>

#if defined(_WIN32) || defined(__APPLE__)
#define RUNTIME_LINK 1
#define Py_NO_ENABLE_SHARED
#else
#define RUNTIME_LINK 0
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4115)
#endif

#if defined(_WIN32) && defined(_DEBUG)
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

#if defined(HAVE_ATTRIBUTE_UNUSED) || defined(__MINGW32__)
#if !defined(UNUSED)
#define UNUSED __attribute__((unused))
#endif
#else
#define UNUSED
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if RUNTIME_LINK

#ifdef NO_REDEFS
#define PY_EXTERN
#else
#define PY_EXTERN extern
#endif

PY_EXTERN int (*Import_PyType_Ready)(PyTypeObject *);
PY_EXTERN PyObject *(*Import_PyObject_GenericGetAttr)(PyObject *, PyObject *);
PY_EXTERN int (*Import_PyObject_IsTrue)(PyObject *);
PY_EXTERN void (*Import_Py_DecRef)(PyObject *);
PY_EXTERN void *(*Import_PyObject_Malloc)(size_t size);
PY_EXTERN void (*Import_PyObject_Free)(void *ptr);
PY_EXTERN PyObject *(*Import_PyObject_Init)(PyObject *, PyTypeObject *);
PY_EXTERN PyObject *(*Import_PyUnicode_FromFormat)(const char *format, ...);
PY_EXTERN PyObject *(*Import_PyUnicode_Concat)(PyObject *left, PyObject *right);
PY_EXTERN PyObject *(*Import_PyLong_FromVoidPtr)(void *);
PY_EXTERN PyObject *(*Import_PyBool_FromLong)(long);
PY_EXTERN PyGILState_STATE (*Import_PyGILState_Ensure)(void);
PY_EXTERN PyThreadState *(*Import_PyGILState_GetThisThreadState)(void);
PY_EXTERN void (*Import_PyErr_SetString)(PyObject *exception,
					 const char *string);
PY_EXTERN PyObject *(*Import_PyErr_Occurred)(void);
PY_EXTERN void (*Import_PyErr_Fetch)(PyObject **, PyObject **, PyObject **);
PY_EXTERN void (*Import_PyErr_Restore)(PyObject *, PyObject *, PyObject *);
PY_EXTERN void (*Import_PyErr_WriteUnraisable)(PyObject *);
PY_EXTERN int (*Import_PyArg_UnpackTuple)(PyObject *, const char *, Py_ssize_t,
					  Py_ssize_t, ...);
PY_EXTERN PyObject *(*Import_Py_BuildValue)(const char *, ...);
PY_EXTERN int (*Import_PyRun_SimpleStringFlags)(const char *,
						PyCompilerFlags *);
PY_EXTERN void (*Import_PyErr_Print)(void);
PY_EXTERN void (*Import_Py_SetPythonHome)(wchar_t *);
PY_EXTERN void (*Import_Py_Initialize)(void);
PY_EXTERN void (*Import_Py_Finalize)(void);
PY_EXTERN int (*Import_Py_IsInitialized)(void);
PY_EXTERN void (*Import_PyEval_InitThreads)(void);
PY_EXTERN int (*Import_PyEval_ThreadsInitialized)(void);
PY_EXTERN void (*Import_PyEval_ReleaseThread)(PyThreadState *tstate);
PY_EXTERN void (*Import_PySys_SetArgv)(int, wchar_t **);
PY_EXTERN PyObject *(*Import_PyImport_ImportModule)(const char *name);
PY_EXTERN PyObject *(*Import_PyObject_CallFunctionObjArgs)(PyObject *callable,
							   ...);
PY_EXTERN PyObject(*Import__Py_NotImplementedStruct);
PY_EXTERN PyObject *(*Import_PyExc_TypeError);
PY_EXTERN PyObject *(*Import_PyExc_RuntimeError);
PY_EXTERN PyObject *(*Import_PyObject_GetAttr)(PyObject *, PyObject *);
PY_EXTERN PyObject *(*Import_PyUnicode_FromString)(const char *u);
PY_EXTERN PyObject *(*Import_PyDict_New)(void);
PY_EXTERN PyObject *(*Import_PyDict_GetItemString)(PyObject *dp,
						   const char *key);
PY_EXTERN int (*Import_PyDict_SetItemString)(PyObject *dp, const char *key,
					     PyObject *item);
PY_EXTERN PyObject *(*Import_PyCFunction_NewEx)(PyMethodDef *, PyObject *,
						PyObject *);
PY_EXTERN PyObject *(*Import_PyModule_GetDict)(PyObject *);
PY_EXTERN PyObject *(*Import_PyModule_GetNameObject)(PyObject *);
PY_EXTERN int (*Import_PyModule_AddObject)(PyObject *, const char *,
					   PyObject *);
PY_EXTERN int (*Import_PyModule_AddStringConstant)(PyObject *, const char *,
						   const char *);
PY_EXTERN PyObject *(*Import_PyImport_Import)(PyObject *name);
PY_EXTERN PyObject *(*Import_PyObject_CallObject)(PyObject *callable_object,
						  PyObject *args);
PY_EXTERN struct _longobject(*Import__Py_FalseStruct);
PY_EXTERN struct _longobject(*Import__Py_TrueStruct);
PY_EXTERN void (*Import_PyGILState_Release)(PyGILState_STATE);
PY_EXTERN int (*Import_PyList_Append)(PyObject *, PyObject *);
PY_EXTERN PyObject *(*Import_PySys_GetObject)(const char *);
PY_EXTERN PyObject *(*Import_PyImport_ReloadModule)(PyObject *m);
PY_EXTERN PyObject *(*Import_PyObject_GetAttrString)(PyObject *, const char *);
PY_EXTERN PyObject *(*Import_PyCapsule_New)(void *pointer, const char *name,
					    PyCapsule_Destructor destructor);
PY_EXTERN void *(*Import_PyCapsule_GetPointer)(PyObject *capsule,
					       const char *name);
PY_EXTERN int (*Import_PyArg_ParseTuple)(PyObject *, const char *, ...);
PY_EXTERN PyTypeObject(*Import_PyFunction_Type);
PY_EXTERN int (*Import_PyObject_SetAttr)(PyObject *, PyObject *, PyObject *);
PY_EXTERN PyObject *(*Import__PyObject_New)(PyTypeObject *);
PY_EXTERN void *(*Import_PyCapsule_Import)(const char *name, int no_block);
PY_EXTERN void (*Import_PyErr_Clear)(void);
PY_EXTERN PyObject *(*Import_PyObject_Call)(PyObject *callable_object,
					    PyObject *args, PyObject *kwargs);
PY_EXTERN PyObject *(*Import_PyList_New)(Py_ssize_t size);
PY_EXTERN Py_ssize_t (*Import_PyList_Size)(PyObject *);
PY_EXTERN PyObject *(*Import_PyList_GetItem)(PyObject *, Py_ssize_t);
PY_EXTERN PyObject *(*Import_PyUnicode_AsUTF8String)(PyObject *unicode);
PY_EXTERN PyObject *(*Import_PyLong_FromUnsignedLongLong)(unsigned long long);
PY_EXTERN int (*Import_PyArg_VaParse)(PyObject *, const char *, va_list);
PY_EXTERN PyObject(*Import__Py_NoneStruct);
PY_EXTERN PyObject *(*Import_PyTuple_New)(Py_ssize_t size);
#if PY_VERSION_HEX >= 0x030900b0
PY_EXTERN int (*Import_PyType_GetFlags)(PyTypeObject *o);
#endif
#if defined(Py_DEBUG) || PY_VERSION_HEX >= 0x030900b0
PY_EXTERN void (*Import__Py_Dealloc)(PyObject *obj);
#endif

extern bool import_python(const char *python_path);

#ifndef NO_REDEFS
#define PyType_Ready Import_PyType_Ready
#if PY_VERSION_HEX >= 0x030900b0
#define PyType_GetFlags Import_PyType_GetFlags
#endif
#if defined(Py_DEBUG) || PY_VERSION_HEX >= 0x030900b0
#define _Py_Dealloc Import__Py_Dealloc
#endif
#define PyObject_GenericGetAttr Import_PyObject_GenericGetAttr
#define PyObject_IsTrue Import_PyObject_IsTrue
#define Py_DecRef Import_Py_DecRef
#define PyObject_Malloc Import_PyObject_Malloc
#define PyObject_Free Import_PyObject_Free
#define PyObject_Init Import_PyObject_Init
#define PyUnicode_FromFormat Import_PyUnicode_FromFormat
#define PyUnicode_Concat Import_PyUnicode_Concat
#define PyLong_FromVoidPtr Import_PyLong_FromVoidPtr
#define PyBool_FromLong Import_PyBool_FromLong
#define PyGILState_Ensure Import_PyGILState_Ensure
#define PyGILState_GetThisThreadState Import_PyGILState_GetThisThreadState
#define PyErr_SetString Import_PyErr_SetString
#define PyErr_Occurred Import_PyErr_Occurred
#define PyErr_Fetch Import_PyErr_Fetch
#define PyErr_Restore Import_PyErr_Restore
#define PyErr_WriteUnraisable Import_PyErr_WriteUnraisable
#define PyArg_UnpackTuple Import_PyArg_UnpackTuple
#define Py_BuildValue Import_Py_BuildValue
#define PyRun_SimpleStringFlags Import_PyRun_SimpleStringFlags
#define PyErr_Print Import_PyErr_Print
#define Py_SetPythonHome Import_Py_SetPythonHome
#define Py_Initialize Import_Py_Initialize
#define Py_Finalize Import_Py_Finalize
#define Py_IsInitialized Import_Py_IsInitialized
#define PyEval_InitThreads Import_PyEval_InitThreads
#define PyEval_ThreadsInitialized Import_PyEval_ThreadsInitialized
#define PyEval_ReleaseThread Import_PyEval_ReleaseThread
#define PySys_SetArgv Import_PySys_SetArgv
#define PyImport_ImportModule Import_PyImport_ImportModule
#define PyObject_CallFunctionObjArgs Import_PyObject_CallFunctionObjArgs
#define _Py_NotImplementedStruct (*Import__Py_NotImplementedStruct)
#define PyExc_TypeError (*Import_PyExc_TypeError)
#define PyExc_RuntimeError (*Import_PyExc_RuntimeError)
#define PyObject_GetAttr Import_PyObject_GetAttr
#define PyUnicode_FromString Import_PyUnicode_FromString
#define PyDict_New Import_PyDict_New
#define PyDict_GetItemString Import_PyDict_GetItemString
#define PyDict_SetItemString Import_PyDict_SetItemString
#define PyCFunction_NewEx Import_PyCFunction_NewEx
#define PyModule_GetDict Import_PyModule_GetDict
#define PyModule_GetNameObject Import_PyModule_GetNameObject
#define PyModule_AddObject Import_PyModule_AddObject
#define PyModule_AddStringConstant Import_PyModule_AddStringConstant
#define PyImport_Import Import_PyImport_Import
#define PyObject_CallObject Import_PyObject_CallObject
#define _Py_FalseStruct (*Import__Py_FalseStruct)
#define _Py_TrueStruct (*Import__Py_TrueStruct)
#define PyGILState_Release Import_PyGILState_Release
#define PyList_Append Import_PyList_Append
#define PySys_GetObject Import_PySys_GetObject
#define PyImport_ReloadModule Import_PyImport_ReloadModule
#define PyObject_GetAttrString Import_PyObject_GetAttrString
#define PyCapsule_New Import_PyCapsule_New
#define PyCapsule_GetPointer Import_PyCapsule_GetPointer
#define PyArg_ParseTuple Import_PyArg_ParseTuple
#define PyFunction_Type (*Import_PyFunction_Type)
#define PyObject_SetAttr Import_PyObject_SetAttr
#define _PyObject_New Import__PyObject_New
#define PyCapsule_Import Import_PyCapsule_Import
#define PyErr_Clear Import_PyErr_Clear
#define PyObject_Call Import_PyObject_Call
#define PyList_New Import_PyList_New
#define PyList_Size Import_PyList_Size
#define PyList_GetItem Import_PyList_GetItem
#define PyUnicode_AsUTF8String Import_PyUnicode_AsUTF8String
#define PyLong_FromUnsignedLongLong Import_PyLong_FromUnsignedLongLong
#define PyArg_VaParse Import_PyArg_VaParse
#define _Py_NoneStruct (*Import__Py_NoneStruct)
#define PyTuple_New Import_PyTuple_New
#if PY_VERSION_HEX >= 0x030800f0
static inline void Import__Py_DECREF(const char *filename UNUSED,
				     int lineno UNUSED, PyObject *op)
{
	if (--op->ob_refcnt != 0) {
#ifdef Py_REF_DEBUG
		if (op->ob_refcnt < 0) {
			_Py_NegativeRefcount(filename, lineno, op);
		}
#endif
	} else {
		_Py_Dealloc(op);
	}
}

#undef Py_DECREF
#define Py_DECREF(op) Import__Py_DECREF(__FILE__, __LINE__, _PyObject_CAST(op))

static inline void Import__Py_XDECREF(PyObject *op)
{
	if (op != NULL) {
		Py_DECREF(op);
	}
}

#undef Py_XDECREF
#define Py_XDECREF(op) Import__Py_XDECREF(_PyObject_CAST(op))
#endif

#if PY_VERSION_HEX >= 0x030900b0
static inline int Import_PyType_HasFeature(PyTypeObject *type,
					   unsigned long feature)
{
	return ((PyType_GetFlags(type) & feature) != 0);
}
#define PyType_HasFeature(t, f) Import_PyType_HasFeature(t, f)
#endif

#endif

#endif
