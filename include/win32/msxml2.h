#ifndef DISCOUNT_MSXML2_H
#define DISCOUNT_MSXML2_H

#include "oaidl.h"

typedef struct IXMLDOMNode IXMLDOMNode;
typedef struct IXMLDOMDocument IXMLDOMDocument;
typedef struct IXMLDOMElement IXMLDOMElement;

typedef struct IXMLDOMNodeVtbl {
    HRESULT (WINAPI *QueryInterface)(IXMLDOMNode *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IXMLDOMNode *);
    ULONG (WINAPI *Release)(IXMLDOMNode *);
} IXMLDOMNodeVtbl;
struct IXMLDOMNode { const IXMLDOMNodeVtbl *lpVtbl; };

typedef struct IXMLDOMElementVtbl {
    HRESULT (WINAPI *QueryInterface)(IXMLDOMElement *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IXMLDOMElement *);
    ULONG (WINAPI *Release)(IXMLDOMElement *);
    HRESULT (WINAPI *put_text)(IXMLDOMElement *, BSTR);
    HRESULT (WINAPI *appendChild)(IXMLDOMElement *, IXMLDOMNode *, IXMLDOMNode **);
} IXMLDOMElementVtbl;
struct IXMLDOMElement { const IXMLDOMElementVtbl *lpVtbl; };

typedef struct IXMLDOMDocumentVtbl {
    HRESULT (WINAPI *QueryInterface)(IXMLDOMDocument *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IXMLDOMDocument *);
    ULONG (WINAPI *Release)(IXMLDOMDocument *);
    HRESULT (WINAPI *createElement)(IXMLDOMDocument *, BSTR, IXMLDOMElement **);
    HRESULT (WINAPI *appendChild)(IXMLDOMDocument *, IXMLDOMNode *, IXMLDOMNode **);
    HRESULT (WINAPI *save)(IXMLDOMDocument *, VARIANT);
} IXMLDOMDocumentVtbl;
struct IXMLDOMDocument { const IXMLDOMDocumentVtbl *lpVtbl; };

#define IXMLDOMDocument_createElement(p,a,b) ((p)->lpVtbl->createElement((p),(a),(b)))
#define IXMLDOMDocument_appendChild(p,a,b) ((p)->lpVtbl->appendChild((p),(a),(b)))
#define IXMLDOMDocument_save(p,a) ((p)->lpVtbl->save((p),(a)))
#define IXMLDOMDocument_Release(p) ((p)->lpVtbl->Release((p)))
#define IXMLDOMElement_put_text(p,a) ((p)->lpVtbl->put_text((p),(a)))
#define IXMLDOMElement_appendChild(p,a,b) ((p)->lpVtbl->appendChild((p),(a),(b)))
#define IXMLDOMElement_Release(p) ((p)->lpVtbl->Release((p)))
#ifdef INITGUID
const CLSID CLSID_DOMDocument =
    {0x2933BF90, 0x7B36, 0x11D2, {0xB2, 0x0E, 0x00, 0xC0, 0x4F, 0x98, 0x3E, 0x60}};
const IID IID_IXMLDOMDocument =
    {0x2933BF81, 0x7B36, 0x11D2, {0xB2, 0x0E, 0x00, 0xC0, 0x4F, 0x98, 0x3E, 0x60}};
#else
extern const CLSID CLSID_DOMDocument;
extern const IID IID_IXMLDOMDocument;
#endif

#endif
