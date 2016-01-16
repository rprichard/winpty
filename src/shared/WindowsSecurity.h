// Copyright (c) 2016 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef WINPTY_WINDOWS_SECURITY_H
#define WINPTY_WINDOWS_SECURITY_H

#include <windows.h>
#include <aclapi.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace winpty_shared {

struct LocalFreer {
    void operator()(void *ptr) {
        if (ptr != nullptr) {
            LocalFree(reinterpret_cast<HLOCAL>(ptr));
        }
    }
};

struct SidFreer {
    void operator()(PSID ptr) {
        if (ptr != nullptr) {
            FreeSid(ptr);
        }
    }
};

// An arbitrary value of copyable type V with an associated underlying object
// of movable type A.
template <typename V, typename A>
class AssocValue {
    V m_v;
    A m_a;

public:
    V get() const { return m_v; }
    operator bool() const { return m_v; }
    AssocValue() : m_v {}, m_a {} {}
    AssocValue(V v, A &&a) : m_v(v), m_a(std::move(a)) {}
    AssocValue(AssocValue &&o) : m_v(o.m_v), m_a(std::move(o.m_a)) {
        o.m_v = V {};
    }
    AssocValue &operator=(AssocValue &&o) {
        m_v = o.m_v;
        m_a = std::move(o.m_a);
        o.m_v = V {};
        return *this;
    }
    AssocValue(const AssocValue &other) = delete;
    AssocValue &operator=(const AssocValue &other) = delete;
};

class DynamicAssoc {
public:
    virtual ~DynamicAssoc() {}
};

typedef std::unique_ptr<void, LocalFreer> PointerLocal;
typedef std::unique_ptr<std::remove_pointer<PACL>::type, LocalFreer> AclLocal;
typedef std::unique_ptr<std::remove_pointer<PSID>::type, SidFreer> SidDefault;
typedef std::unique_ptr<std::remove_pointer<PSID>::type, LocalFreer> SidLocal;
typedef AssocValue<PSID, std::unique_ptr<DynamicAssoc>> SidDynamic;
typedef AssocValue<PSECURITY_DESCRIPTOR, std::unique_ptr<DynamicAssoc>>
    SecurityDescriptorDynamic;
typedef std::unique_ptr<std::remove_pointer<PSECURITY_DESCRIPTOR>::type, LocalFreer>
    SecurityDescriptorLocal;

SidDynamic getOwnerSid();
SidDefault wellKnownSid(const char *debuggingName,
                      SID_IDENTIFIER_AUTHORITY authority,
                      BYTE authorityCount,
                      DWORD subAuthority0=0,
                      DWORD subAuthority1=0);
SidDefault builtinAdminsSid();
SidDefault localSystemSid();
SidDefault everyoneSid();
SecurityDescriptorDynamic createPipeSecurityDescriptorOwnerFullControl();
SecurityDescriptorDynamic createPipeSecurityDescriptorOwnerFullControlEveryoneWrite();
SecurityDescriptorLocal getObjectSecurityDescriptor(HANDLE handle);
std::wstring sidToString(PSID sid);
SidLocal stringToSid(const std::wstring &str);
SecurityDescriptorLocal stringToSd(const std::wstring &str);
std::wstring sdToString(PSECURITY_DESCRIPTOR sd);
DWORD rejectRemoteClientsPipeFlag();

} // winpty_shared namespace

#endif // WINPTY_WINDOWS_SECURITY_H
