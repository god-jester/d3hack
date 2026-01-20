#include "symbols/refstring.hpp"

SETUP_CRS_PTR(CRefString_ctor_default);
SETUP_CRS_PTR(CRefString_ctor_int);
SETUP_CRS_PTR(CRefString_ctor_cref);
SETUP_CRS_PTR(CRefString_ctor_lpcstr);
SETUP_CRS_PTR(CRefString_dtor);

SETUP_CRS_PTR(CRefString_op_eq_cref);
SETUP_CRS_PTR(CRefString_op_eq_cref_assign);
SETUP_CRS_PTR(CRefString_op_eq_lpcstr);
SETUP_CRS_PTR(CRefString_op_add_eq_lpcstr);
SETUP_CRS_PTR(CRefString_CommonCtorBody);
SETUP_CRS_PTR(CRefString_ReAllocate);
SETUP_CRS_PTR(CRefString_Free);
SETUP_CRS_PTR(CRefString_Allocate);
SETUP_CRS_PTR(CRefString_Append);

CRefString::CRefString() {
    auto *nil                        = GetRefStringDataBufferNil();
    this->m_pData                    = reinterpret_cast<CRefStringData *>(nil);
    this->m_pszString                = nil->m_szString;
    this->m_bForceAppLevelMemoryPool = 0;
}

CRefString::CRefString(int bForceAppLevelMemoryPool) {
    auto *nil                        = GetRefStringDataBufferNil();
    this->m_pData                    = reinterpret_cast<CRefStringData *>(nil);
    this->m_pszString                = nil->m_szString;
    this->m_bForceAppLevelMemoryPool = bForceAppLevelMemoryPool;
}

CRefString::CRefString(const CRefString &szString) noexcept {
    //   EXL_ASSERT(*this != nullptr, "Pointer \"*this\" should not be null");
    // PRINT("[crefstr] Before ctor")
    // PRINT("[crefstr] Before ctor: this=%p, *m_pszString=%p, *o.szString=%p", this, this->m_pszString, (szString).str())
    // // PRINT("[crefstr] Before ctor: m_pszString=%s, o.szString=%s", this->m_pszString, (szString).str())
    // PRINT("[crefstr] Before ctor: *m_nReferenceCount=%p, *o.m_nReferenceCount=%p", this->m_pData->m_nReferenceCount, (szString).m_pData->m_nReferenceCount)
    // PRINT("[crefstr] Before ctor: m_nReferenceCount=%x, o.m_nReferenceCount=%x", this->m_pData->m_nReferenceCount, (szString).m_pData->m_nReferenceCount)
    CRefString_ctor_cref(this, &szString);
    // PRINT("[crefstr] After ctor: this=%p, *m_pszString=%p, *szString=%p", this, this->m_pszString, (szString).str())
    // PRINT("[crefstr] After ctor: m_pszString=%s, szString=%s", this->m_pszString, (szString).str())
    // PRINT("[crefstr] After ctor: *m_nReferenceCount=%p, *o.m_nReferenceCount=%p", this->m_pData->m_nReferenceCount, (szString).m_pData->m_nReferenceCount)
    // PRINT("[crefstr] After ctor: m_nReferenceCount=%x, o.m_nReferenceCount=%x", this->m_pData->m_nReferenceCount, (szString).m_pData->m_nReferenceCount)
}

CRefString::CRefString(const char *szString) {
    // PRINT("[char* ctor] Before ctor")
    // PRINT("[char* ctor] Before ctor: this=%p, this->m_pszString=%p, szString=%p", this, this->m_pszString, szString)
    CRefString_ctor_lpcstr(this, szString);
}

CRefString::CRefString(const char *szString, int nLength) { CRefString_CommonCtorBody(this, szString, nLength); }
CRefString::CRefString(const char *szString, uint nLength) { CRefString_CommonCtorBody(this, szString, static_cast<int>(nLength)); }
CRefString::CRefString(const char *szString, long nLength) { CRefString_CommonCtorBody(this, szString, static_cast<int>(nLength)); }

CRefString::~CRefString() {
    if (this->m_pData != static_cast<CRefStringData *>(nullptr))
        CRefString_dtor(this);
}

auto CRefString::operator=(char const *szString) -> CRefString & {
    CRefString_op_eq_lpcstr(this, szString);
    // PRINT("After op= lpcstr: this=%p, m_pszString=%p", this, this->m_pszString)
    return *this;
}

auto CRefString::operator=(CRefString *const &szString) -> CRefString & {
    CRefString_op_eq_cref_assign(this, szString);
    // PRINT("After op= crefstring: this=%p, m_pszString=%s", this, this->m_pszString)
    return *this;
}

auto CRefString::operator+=(char const *szString) -> CRefString & {
    CRefString_op_add_eq_lpcstr(this, szString);
    // PRINT("After op+= lpcstr: this=%p, m_pszString=%p", this, this->m_pszString)
    return *this;
}

auto CRefString::operator==(CRefString *const &szString) const -> bool {
    // PRINT("[crefstr] Before op==")
    if (this->m_pData == szString->m_pData)
        return true;
    if (this->m_pData->m_nDataLength == szString->m_pData->m_nDataLength)
        return strcmp(this->m_pszString, szString->m_pszString) == 0;
    return false;
}

auto CRefString::str() const -> LPCSTR {
    // PRINT("str()")
    return this->m_pszString;
}

CRefString::operator char const *() const {
    // PRINT("op char const")
    return this->m_pszString;
}

void CRefString::_Free(CRefString *) { CRefString_Free(this); }
