/*
 * DXDiag private include file
 *
 * Copyright 2004 Raphael Junqueira
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_DXDIAG_PRIVATE_H
#define __WINE_DXDIAG_PRIVATE_H

#include <stdarg.h>

#include "windows.h"
#include "string.h"
#include "ddraw.h"
#include "wbemcli.h"

#ifndef CO_E_NOTINITIALIZED
#define CO_E_NOTINITIALIZED ((HRESULT)0x800401F0L)
#endif
#ifndef VARIANT_TRUE
#define VARIANT_TRUE ((VARIANT_BOOL)-1)
#define VARIANT_FALSE ((VARIANT_BOOL)0)
#endif
#ifndef LOCALE_NEUTRAL
#define LOCALE_NEUTRAL 0
#define LOCALE_SENGLANGUAGE 0x1001
#define LOCALE_SNATIVELANGNAME 4
#endif
#ifndef DATE_LONGDATE
#define DATE_LONGDATE 2
#endif

typedef struct { DWORD dwLength; DWORD dwMemoryLoad; unsigned long long ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile, ullTotalVirtual, ullAvailVirtual; } MEMORYSTATUSEX;
#include "uuids.h"
#include "wine/list.h"
#include "dxdiag.h"
#include "resource.h"

#ifndef DXDIAG_E_INSUFFICIENT_BUFFER
#define DXDIAG_E_INSUFFICIENT_BUFFER ((HRESULT)0x8004A001L)
#endif

/* DXDiag Interfaces: */
typedef struct IDxDiagProviderImpl  IDxDiagProviderImpl;
typedef struct IDxDiagContainerImpl IDxDiagContainerImpl;
typedef struct IDxDiagContainerImpl_Container IDxDiagContainerImpl_Container;

/* ---------------- */
/* IDxDiagContainer  */
/* ---------------- */

struct IDxDiagContainerImpl_Container {
  struct list entry;
  WCHAR *contName;

  struct list subContainers;
  DWORD nSubContainers;
  struct list properties;
  DWORD nProperties;
};

typedef struct IDxDiagContainerImpl_Property {
  struct list entry;
  WCHAR *propName;
  VARIANT vProp;
} IDxDiagContainerImpl_Property;


/*****************************************************************************
 * IDxDiagContainer implementation structure
 */
struct IDxDiagContainerImpl {
  IDxDiagContainer IDxDiagContainer_iface;
  LONG ref;
  IDxDiagContainerImpl_Container *cont;
  IDxDiagProvider *pProv;
};

/**
 * factories
 */
extern HRESULT DXDiag_CreateDXDiagProvider(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj);

/** internal factory */
extern HRESULT DXDiag_CreateDXDiagContainer(REFIID riid, IDxDiagContainerImpl_Container *cont, IDxDiagProvider *pProv, LPVOID *ppobj);

/**********************************************************************
 * Dll lifetime tracking declaration for dxdiagn.dll
 */
extern LONG DXDIAGN_refCount;
static inline void DXDIAGN_LockModule(void) { InterlockedIncrement( &DXDIAGN_refCount ); }
static inline void DXDIAGN_UnlockModule(void) { InterlockedDecrement( &DXDIAGN_refCount ); }

extern HINSTANCE dxdiagn_instance;

#endif
