#pragma once

#include <lib.hpp>
#include "d3/types/common.hpp"

struct CRefString;
namespace mod = exl::util::modules;

#define CRS_PTR(offset, name, ...)                                 \
    namespace crefstring_ptrs {                                    \
        using APPEND(name, _t)                      = __VA_ARGS__; \
        static constexpr size_t APPEND(name, _addr) = offset;      \
    }

#define SETUP_CRS_PTR(name)                              \
    crefstring_ptrs::APPEND(name, _t) CRefString::name = \
        reinterpret_cast<crefstring_ptrs::APPEND(name, _t)>(GameOffset(crefstring_ptrs::APPEND(name, _addr)))

CRS_PTR(0xA48690, CRefString_ctor_default,          void (*)(CRefString * const ));
CRS_PTR(0xA486B0, CRefString_ctor_int,              void (*)(CRefString *, int));
CRS_PTR(0xA486D0, CRefString_ctor_cref,             void (*)(CRefString *, const CRefString *szString));
CRS_PTR(0xA48700, CRefString_ctor_lpcstr,           void (*)(CRefString *, const char* szString));

CRS_PTR(0xA48790, CRefString_CommonCtorBody,        void (*)(CRefString *, const char*, int));
CRS_PTR(0xA488B0, CRefString_ReAllocate,            void (*)(CRefString *, unsigned long, int));
CRS_PTR(0xA48AB0, CRefString_Free,                  void (*)(CRefString *));
CRS_PTR(0xA48C80, CRefString_Allocate,              void (*)(CRefString *, unsigned long));

CRS_PTR(0xA48E10, CRefString_dtor,                  void (*)(CRefString *));

// CRS_PTR(0xA48E70, CRefString_op_char_const_ptr,     const char* (*)(CRefString*));
// CRS_PTR(0xA48E80, CRefString_str,                   const char* (*)(CRefString*));
// CRS_PTR(0xA48EA0, CRefString_Format,                void (*)(CRefString*, const char*, ...));  // Note: Variadic functions may need special handling

CRS_PTR(0xA490A0, CRefString_op_eq_cref,            bool (*)(CRefString *, const CRefString&));
CRS_PTR(0xA49100, CRefString_op_eq_cref_assign,     void (*)(CRefString *, const CRefString* szString));
CRS_PTR(0xA491D0, CRefString_op_eq_lpcstr,          CRefString* (*)(CRefString *, const char*));
CRS_PTR(0xA492C0, CRefString_op_add_eq_lpcstr,      CRefString* (*)(CRefString *, const char*));
CRS_PTR(0xA49370, CRefString_Append,                void (*)(CRefString*, const char*));

#undef CRS_PTR

static TRefStringDataBuffer *g_RefStringDataBufferNil = reinterpret_cast<TRefStringDataBuffer *>(GameOffset(0x1A5F108));

struct ALIGNED(8) CRefString {
    CRefStringData *m_pData                    = reinterpret_cast<CRefStringData *>(g_RefStringDataBufferNil);
    CHAR           *m_pszString                = g_RefStringDataBufferNil->m_szString;
    BOOL            m_bForceAppLevelMemoryPool = 0;

    CRefString();
    explicit CRefString(int);
    explicit CRefString(char const *);
    CRefString(char const *, int);
    CRefString(char const *, uint);
    CRefString(char const *, long);
    CRefString(CRefString const &) noexcept;
    ~CRefString();
    auto               operator=(char const *) -> CRefString &;
    auto               operator=(CRefString *const &) -> CRefString &;
    auto               operator+=(char const *) -> CRefString &;
    auto               operator==(CRefString *const &) const -> bool;
    explicit           operator char const *() const;
    [[nodiscard]] auto str() const -> LPCSTR;
    void               _Free(CRefString *);

    static crefstring_ptrs::CRefString_ctor_default_t CRefString_ctor_default;
    static crefstring_ptrs::CRefString_ctor_int_t     CRefString_ctor_int;
    static crefstring_ptrs::CRefString_ctor_cref_t    CRefString_ctor_cref;
    static crefstring_ptrs::CRefString_ctor_lpcstr_t  CRefString_ctor_lpcstr;
    static crefstring_ptrs::CRefString_dtor_t         CRefString_dtor;

    static crefstring_ptrs::CRefString_op_eq_cref_t        CRefString_op_eq_cref;
    static crefstring_ptrs::CRefString_op_eq_cref_assign_t CRefString_op_eq_cref_assign;
    static crefstring_ptrs::CRefString_op_eq_lpcstr_t      CRefString_op_eq_lpcstr;
    static crefstring_ptrs::CRefString_op_add_eq_lpcstr_t  CRefString_op_add_eq_lpcstr;
    static crefstring_ptrs::CRefString_CommonCtorBody_t    CRefString_CommonCtorBody;
    static crefstring_ptrs::CRefString_ReAllocate_t        CRefString_ReAllocate;
    static crefstring_ptrs::CRefString_Free_t              CRefString_Free;
    static crefstring_ptrs::CRefString_Allocate_t          CRefString_Allocate;
    static crefstring_ptrs::CRefString_Append_t            CRefString_Append;
};

struct ALIGNED(8) TextCreationParams {
    WindowCreationParams baseclass_0;
    UIJustifyHorizontal  eJustifyHorizontal;
    UIJustifyVertical    eJustifyVertical;
    float                flAdvanceScalar;
    BOOL                 fAllCaps;
    RGBAColor            rgbaText;
    RGBAColor            rgbaTextDisabled;
    RGBAColor            rgbaDropShadow;
    RGBAColor            rgbaDropShadowDisabled;
    SNO                  snoFont;
    int32                nFontSize;
    int32                nFontSizeFallback;
    BOOL                 fWordWrap;
    BOOL                 fShrinkToFit;
    ResizeToFitValue     eResizeToFit;
    BOOL                 fHyperlinkAware;
    BOOL                 fValidateHyperlinks;
    BOOL                 fIsPassword;
    BOOL                 fSelectable;
    BOOL                 fUseDropShadow;
    int32                nLinesBeforeTruncation;
    Vector2D             vecTextButtonPressOffsetUIC;
    CRefString           sDecoratedLabel;
    Vector2D             vecTextInsetUIC;
    Rect2D               rectTextPaddingUIC;
    CRefString           sPlaceholderTextLabel;
    RGBAColor            rgbaPlaceholderText;
    RGBAColor            rgbaPlaceholderTextDropShadow;
};
