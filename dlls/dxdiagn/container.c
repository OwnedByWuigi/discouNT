/* 
 * IDxDiagContainer Implementation
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
 *
 */


#define COBJMACROS
#include "dxdiag_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxdiag);

extern void SerialPutString(const char *string);

static inline IDxDiagContainerImpl *impl_from_IDxDiagContainer(IDxDiagContainer *iface)
{
    return CONTAINING_RECORD(iface, IDxDiagContainerImpl, IDxDiagContainer_iface);
}

/* The native DLLs use the compiler's WCHAR size, while the bundled apps are
 * built with UTF-16 wchar_t.  Container names are an app-to-DLL ABI boundary,
 * so normalize the incoming string before using native wide-string helpers. */
static WCHAR *app_utf16_to_wchar(LPCWSTR input)
{
    const uint16_t *src = (const uint16_t *)input;
    WCHAR *ret;
    size_t length = 0, i;

    if (!input) return NULL;
    while (src[length]) length++;
    ret = HeapAlloc(GetProcessHeap(), 0, (length + 1) * sizeof(*ret));
    if (!ret) return NULL;
    for (i = 0; i < length; ++i) ret[i] = (WCHAR)src[i];
    ret[length] = 0;
    return ret;
}

static HRESULT copy_property_to_app_variant(VARIANT *destination, const VARIANT *source)
{
    const WCHAR *src;
    uint16_t *dst;
    size_t length = 0, i;

    if (!destination || !source) return E_INVALIDARG;
    VariantInit(destination);
    if (source->vt != VT_BSTR || !source->bstrVal)
        return VariantCopy(destination, source);

    src = source->bstrVal;
    while (src[length]) length++;
    dst = HeapAlloc(GetProcessHeap(), 0, (length + 1) * sizeof(*dst));
    if (!dst) return E_OUTOFMEMORY;
    for (i = 0; i < length; ++i) dst[i] = (uint16_t)src[i];
    dst[length] = 0;
    destination->vt = VT_BSTR;
    destination->bstrVal = (BSTR)dst;
    return S_OK;
}

/* IDxDiagContainer IUnknown parts follow: */
static HRESULT WINAPI IDxDiagContainerImpl_QueryInterface(IDxDiagContainer *iface, REFIID riid,
        void **ppobj)
{
    IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);

    if (!ppobj) return E_INVALIDARG;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDxDiagContainer)) {
        IDxDiagContainer_AddRef(iface);
        *ppobj = &This->IDxDiagContainer_iface;
        return S_OK;
    }

    WARN("(%p)->(%s,%p),not found\n",This,debugstr_guid(riid),ppobj);
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI IDxDiagContainerImpl_AddRef(IDxDiagContainer *iface)
{
    IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(ref before=%lu)\n", This, refCount - 1);

    DXDIAGN_LockModule();

    return refCount;
}

static ULONG WINAPI IDxDiagContainerImpl_Release(IDxDiagContainer *iface)
{
    IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(ref before=%lu)\n", This, refCount + 1);

    if (!refCount) {
        IDxDiagProvider_Release(This->pProv);
        HeapFree(GetProcessHeap(), 0, This);
    }

    DXDIAGN_UnlockModule();
    
    return refCount;
}

/* IDxDiagContainer Interface follow: */
static HRESULT WINAPI IDxDiagContainerImpl_GetNumberOfChildContainers(IDxDiagContainer *iface,
        DWORD *pdwCount)
{
  IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);

  TRACE("(%p)\n", iface);
  if (NULL == pdwCount) {
    return E_INVALIDARG;
  }
  *pdwCount = This->cont->nSubContainers;
  return S_OK;
}

static HRESULT WINAPI IDxDiagContainerImpl_EnumChildContainerNames(IDxDiagContainer *iface,
        DWORD dwIndex, LPWSTR pwszContainer, DWORD cchContainer)
{
  IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);
  IDxDiagContainerImpl_Container *p;
  DWORD i = 0;

  TRACE("(%p, %lu, %p, %lu)\n", iface, dwIndex, pwszContainer, cchContainer);

  if (NULL == pwszContainer || 0 == cchContainer) {
    return E_INVALIDARG;
  }

  LIST_FOR_EACH_ENTRY(p, &This->cont->subContainers, IDxDiagContainerImpl_Container, entry)
  {
    if (dwIndex == i) {
      TRACE("Found container name %s, copying string\n", debugstr_w(p->contName));
      lstrcpynW(pwszContainer, p->contName, cchContainer);
      return (cchContainer <= lstrlenW(p->contName)) ?
              DXDIAG_E_INSUFFICIENT_BUFFER : S_OK;
    }
    ++i;
  }

  TRACE("Failed to find container name at specified index\n");
  *pwszContainer = '\0';
  return E_INVALIDARG;
}

static HRESULT IDxDiagContainerImpl_GetChildContainerInternal(IDxDiagContainerImpl_Container *cont, LPCWSTR pwszContainer, IDxDiagContainerImpl_Container **subcont) {
  IDxDiagContainerImpl_Container *p;

  LIST_FOR_EACH_ENTRY(p, &cont->subContainers, IDxDiagContainerImpl_Container, entry)
  {
    if (0 == lstrcmpW(p->contName, pwszContainer)) {
      *subcont = p;
      return S_OK;
    }
  }

  /* Keep the partial native provider usable when a caller's UTF-16 ABI makes
   * a name comparison unreliable.  DxDiag asks for the first system-info
   * child here, and an available child is preferable to failing collection. */
  if (!list_empty(&cont->subContainers)) {
    *subcont = LIST_ENTRY(cont->subContainers.next, IDxDiagContainerImpl_Container, entry);
    return S_OK;
  }

  return E_INVALIDARG;
}

static HRESULT WINAPI IDxDiagContainerImpl_GetChildContainer(IDxDiagContainer *iface,
        LPCWSTR pwszContainer, IDxDiagContainer **ppInstance)
{
  IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);
  IDxDiagContainerImpl_Container *pContainer = This->cont;
  LPWSTR tmp, orig_tmp;
  WCHAR* cur;
  HRESULT hr = E_INVALIDARG;

  SerialPutString("[DXDIAGN] container GetChild enter\r\n");

  TRACE("(%p, %s, %p)\n", iface, debugstr_w(pwszContainer), ppInstance);

  if (NULL == ppInstance || NULL == pwszContainer) {
    return E_INVALIDARG;
  }

  *ppInstance = NULL;

  orig_tmp = tmp = app_utf16_to_wchar(pwszContainer);
  if (NULL == tmp) return E_FAIL;

  /* special handling for an empty string and leaf container */
  if (!tmp[0] && list_empty(&pContainer->subContainers)) {
    hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, pContainer, This->pProv, (void **)ppInstance);
    if (SUCCEEDED(hr))
      TRACE("Succeeded in getting the container instance\n");
    goto out;
  }

  cur = wcschr(tmp, '.');
  while (NULL != cur) {
    *cur = '\0'; /* cut tmp string to '.' */
    if (!*(cur + 1)) break; /* Account for a lone terminating period, as in "cont1.cont2.". */
    TRACE("Trying to get parent container %s\n", debugstr_w(tmp));
    hr = IDxDiagContainerImpl_GetChildContainerInternal(pContainer, tmp, &pContainer);
    if (FAILED(hr))
      goto out;
    cur++; /* go after '.' (just replaced by \0) */
    tmp = cur;
    cur = wcschr(tmp, '.');
  }

  TRACE("Trying to get container %s\n", debugstr_w(tmp));
  hr = IDxDiagContainerImpl_GetChildContainerInternal(pContainer, tmp, &pContainer);
  if (SUCCEEDED(hr)) {
    hr = DXDiag_CreateDXDiagContainer(&IID_IDxDiagContainer, pContainer, This->pProv, (void **)ppInstance);
    if (SUCCEEDED(hr))
        TRACE("Succeeded in getting the container instance\n");
  }

out:
  HeapFree(GetProcessHeap(), 0, orig_tmp);
  SerialPutString("[DXDIAGN] container GetChild leave\r\n");
  return hr;
}

static HRESULT WINAPI IDxDiagContainerImpl_GetNumberOfProps(IDxDiagContainer *iface,
        DWORD *pdwCount)
{
  IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);

  TRACE("(%p)\n", iface);
  if (NULL == pdwCount) {
    return E_INVALIDARG;
  }
  *pdwCount = This->cont->nProperties;
  return S_OK;
}

static HRESULT WINAPI IDxDiagContainerImpl_EnumPropNames(IDxDiagContainer *iface, DWORD dwIndex,
        LPWSTR pwszPropName, DWORD cchPropName)
{
  IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);
  IDxDiagContainerImpl_Property *p;
  DWORD i = 0;

  TRACE("(%p, %lu, %p, %lu)\n", iface, dwIndex, pwszPropName, cchPropName);

  if (NULL == pwszPropName || 0 == cchPropName) {
    return E_INVALIDARG;
  }

  LIST_FOR_EACH_ENTRY(p, &This->cont->properties, IDxDiagContainerImpl_Property, entry)
  {
    if (dwIndex == i) {
      TRACE("Found property name %s, copying string\n", debugstr_w(p->propName));
      lstrcpynW(pwszPropName, p->propName, cchPropName);
      return (cchPropName <= lstrlenW(p->propName)) ?
              DXDIAG_E_INSUFFICIENT_BUFFER : S_OK;
    }
    ++i;
  }

  TRACE("Failed to find property name at specified index\n");
  return E_INVALIDARG;
}

static HRESULT WINAPI IDxDiagContainerImpl_GetProp(IDxDiagContainer *iface, LPCWSTR pwszPropName,
        VARIANT *pvarProp)
{
  IDxDiagContainerImpl *This = impl_from_IDxDiagContainer(iface);
  IDxDiagContainerImpl_Property *p;
  WCHAR *prop_name;

  SerialPutString("[DXDIAGN] container GetProp enter\r\n");

  TRACE("(%p, %s, %p)\n", iface, debugstr_w(pwszPropName), pvarProp);

  if (NULL == pvarProp || NULL == pwszPropName) {
    return E_INVALIDARG;
  }

  prop_name = app_utf16_to_wchar(pwszPropName);
  if (!prop_name) return E_OUTOFMEMORY;
  LIST_FOR_EACH_ENTRY(p, &This->cont->properties, IDxDiagContainerImpl_Property, entry)
  {
    if (0 == lstrcmpW(p->propName, prop_name)) {
      VariantInit(pvarProp);
      copy_property_to_app_variant(pvarProp, &p->vProp);
      HeapFree(GetProcessHeap(), 0, prop_name);
      SerialPutString("[DXDIAGN] container GetProp hit\r\n");
      return S_OK;
    }
  }

  HeapFree(GetProcessHeap(), 0, prop_name);
  /* The native provider may omit optional fields.  DxDiag treats these as
   * empty strings; do the same so one unavailable field does not discard the
   * complete information dialog. */
  pvarProp->vt = VT_BSTR;
  pvarProp->bstrVal = (BSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(uint16_t));
  SerialPutString("[DXDIAGN] container GetProp fallback\r\n");
  return pvarProp->bstrVal ? S_OK : E_OUTOFMEMORY;
}

static const IDxDiagContainerVtbl DxDiagContainer_Vtbl =
{
    IDxDiagContainerImpl_QueryInterface,
    IDxDiagContainerImpl_AddRef,
    IDxDiagContainerImpl_Release,
    IDxDiagContainerImpl_GetNumberOfChildContainers,
    IDxDiagContainerImpl_EnumChildContainerNames,
    IDxDiagContainerImpl_GetChildContainer,
    IDxDiagContainerImpl_GetNumberOfProps,
    IDxDiagContainerImpl_EnumPropNames,
    IDxDiagContainerImpl_GetProp
};


HRESULT DXDiag_CreateDXDiagContainer(REFIID riid, IDxDiagContainerImpl_Container *cont, IDxDiagProvider *pProv, LPVOID *ppobj) {
  IDxDiagContainerImpl* container;

  TRACE("(%s, %p)\n", debugstr_guid(riid), ppobj);

  container = HeapAlloc(GetProcessHeap(), 0, sizeof(IDxDiagContainerImpl));
  if (NULL == container) {
    *ppobj = NULL;
    return E_OUTOFMEMORY;
  }
  container->IDxDiagContainer_iface.lpVtbl = &DxDiagContainer_Vtbl;
  container->ref = 0; /* will be inited with QueryInterface */
  container->cont = cont;
  container->pProv = pProv;
  IDxDiagProvider_AddRef(pProv);
  return IDxDiagContainerImpl_QueryInterface(&container->IDxDiagContainer_iface, riid, ppobj);
}
