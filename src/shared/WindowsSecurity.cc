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

#include "WindowsSecurity.h"

#include <sddl.h>
#include <string.h>

#include <array>
#include <sstream>

#include "DebugClient.h"
#include "OsModule.h"

namespace winpty_shared {

SidDynamic getOwnerSid() {
    class AutoCloseHandle {
        HANDLE m_h;
    public:
        AutoCloseHandle(HANDLE h) : m_h(h) {}
        ~AutoCloseHandle() { CloseHandle(m_h); }
    };

    struct OwnerSidImpl : public DynamicAssoc {
        std::unique_ptr<char[]> buffer;
    };

    HANDLE token = nullptr;
    BOOL success;
    success = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    if (!success) {
        trace("error: getOwnerSid: OpenProcessToken failed: %u",
            static_cast<unsigned>(GetLastError()));
        return SidDynamic();
    }
    AutoCloseHandle ac(token);
    if (token == nullptr) {
        trace("error: getOwnerSid: OpenProcessToken succeeded, "
              "but token is NULL");
        return SidDynamic();
    }
    DWORD actual = 0;
    success = GetTokenInformation(token, TokenOwner,
        nullptr, 0, &actual);
    if (success || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        trace("error: getOwnerSid: expected ERROR_INSUFFICIENT_BUFFER");
        return SidDynamic();
    }
    std::unique_ptr<OwnerSidImpl> assoc(new OwnerSidImpl);
    assoc->buffer = std::unique_ptr<char[]>(new char[actual]);
    success = GetTokenInformation(token, TokenOwner,
                                  assoc->buffer.get(), actual, &actual);
    if (!success) {
        trace("error: getOwnerSid: GetTokenInformation failed: %u",
            static_cast<unsigned>(GetLastError()));
        return SidDynamic();
    }
    TOKEN_OWNER tmp;
    memcpy(&tmp, assoc->buffer.get(), sizeof(tmp));
    return SidDynamic(tmp.Owner, std::move(assoc));
}

SidDefault wellKnownSid(const char *debuggingName,
                        SID_IDENTIFIER_AUTHORITY authority,
                        BYTE authorityCount,
                        DWORD subAuthority0/*=0*/,
                        DWORD subAuthority1/*=0*/) {
    PSID psid = nullptr;
    if (!AllocateAndInitializeSid(&authority, authorityCount,
            subAuthority0,
            subAuthority1,
            0, 0, 0, 0, 0, 0,
            &psid)) {
        trace("wellKnownSid: error getting %s SID: %u",
            debuggingName,
            static_cast<unsigned>(GetLastError()));
        return SidDefault();
    }
    return SidDefault(psid);
}

SidDefault builtinAdminsSid() {
    // S-1-5-32-544
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    return wellKnownSid("BUILTIN\\Administrators group",
            authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,    // 32
            DOMAIN_ALIAS_RID_ADMINS);       // 544
}

SidDefault localSystemSid() {
    // S-1-5-18
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    return wellKnownSid("LocalSystem account",
            authority, 1,
            SECURITY_LOCAL_SYSTEM_RID);     // 18
}

SidDefault everyoneSid() {
    // S-1-1-0
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_WORLD_SID_AUTHORITY };
    return wellKnownSid("Everyone account",
            authority, 1,
            SECURITY_WORLD_RID);            // 0
}

static SecurityDescriptorLocal finishSecurityDescriptor(
        size_t daclEntryCount,
        EXPLICIT_ACCESSW *daclEntries,
        AclLocal &outAcl) {
    {
        PACL aclRaw = nullptr;
        DWORD aclError =
            SetEntriesInAcl(daclEntryCount,
                            daclEntries,
                            nullptr, &aclRaw);
        if (aclError != ERROR_SUCCESS) {
            trace("error: SetEntriesInAcl failed: %u",
                static_cast<unsigned>(aclError));
            return SecurityDescriptorLocal();
        }
        outAcl = AclLocal(aclRaw);
    }

    SecurityDescriptorLocal sdLocal(
        reinterpret_cast<PSECURITY_DESCRIPTOR>(
            LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH)));
    if (!sdLocal) {
        trace("error: LocalAlloc failed");
        return SecurityDescriptorLocal();
    }
    PSECURITY_DESCRIPTOR sdRaw = sdLocal.get();

    if (!InitializeSecurityDescriptor(sdRaw, SECURITY_DESCRIPTOR_REVISION)) {
        trace("error: InitializeSecurityDescriptor failed: %u",
            static_cast<unsigned>(GetLastError()));
        return SecurityDescriptorLocal();
    }
    if (!SetSecurityDescriptorDacl(sdRaw, TRUE, outAcl.get(), FALSE)) {
        trace("error: SetSecurityDescriptorDacl failed: %u",
            static_cast<unsigned>(GetLastError()));
        return SecurityDescriptorLocal();
    }

    return sdLocal;
}

// Create a security descriptor that grants full control to the local system
// account, built-in administrators, and the owner.  Returns NULL on failure.
SecurityDescriptorDynamic
createPipeSecurityDescriptorOwnerFullControl() {

    struct Assoc : public DynamicAssoc {
        SidDynamic owner;
        SidDefault builtinAdmins;
        SidDefault localSystem;
        std::array<EXPLICIT_ACCESSW, 3> daclEntries;
        AclLocal dacl;
        SecurityDescriptorLocal value;
    };

    std::unique_ptr<Assoc> assoc(new Assoc);

    if (!(assoc->owner = getOwnerSid()) ||
            !(assoc->builtinAdmins = builtinAdminsSid()) ||
            !(assoc->localSystem = localSystemSid())) {
        return SecurityDescriptorDynamic();
    }

    for (auto &ea : assoc->daclEntries) {
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    }
    assoc->daclEntries[0].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->localSystem.get());
    assoc->daclEntries[1].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->builtinAdmins.get());
    assoc->daclEntries[2].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->owner.get());

    assoc->value = finishSecurityDescriptor(
        assoc->daclEntries.size(),
        assoc->daclEntries.data(),
        assoc->dacl);
    if (!assoc->value) {
        return SecurityDescriptorDynamic();
    }

    const PSECURITY_DESCRIPTOR ret = assoc->value.get();
    return SecurityDescriptorDynamic(ret, std::move(assoc));
}

SecurityDescriptorDynamic
createPipeSecurityDescriptorOwnerFullControlEveryoneWrite() {

    struct Assoc : public DynamicAssoc {
        SidDynamic owner;
        SidDefault builtinAdmins;
        SidDefault localSystem;
        SidDefault everyone;
        std::array<EXPLICIT_ACCESSW, 4> daclEntries;
        AclLocal dacl;
        SecurityDescriptorLocal value;
    };

    std::unique_ptr<Assoc> assoc(new Assoc);

    if (!(assoc->owner = getOwnerSid()) ||
            !(assoc->builtinAdmins = builtinAdminsSid()) ||
            !(assoc->localSystem = localSystemSid()) ||
            !(assoc->everyone = everyoneSid())) {
        return SecurityDescriptorDynamic();
    }

    for (auto &ea : assoc->daclEntries) {
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    }
    assoc->daclEntries[0].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->localSystem.get());
    assoc->daclEntries[1].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->builtinAdmins.get());
    assoc->daclEntries[2].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->owner.get());
    assoc->daclEntries[3].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(assoc->everyone.get());
    // Avoid using FILE_GENERIC_WRITE because it includes FILE_APPEND_DATA,
    // which is equal to FILE_CREATE_PIPE_INSTANCE.  Instead, include all the
    // flags that comprise FILE_GENERIC_WRITE, except for the one.
    assoc->daclEntries[3].grfAccessPermissions =
        FILE_GENERIC_READ |
        FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | FILE_WRITE_EA |
        STANDARD_RIGHTS_WRITE | SYNCHRONIZE;

    assoc->value = finishSecurityDescriptor(
        assoc->daclEntries.size(),
        assoc->daclEntries.data(),
        assoc->dacl);
    if (!assoc->value) {
        return SecurityDescriptorDynamic();
    }

    const PSECURITY_DESCRIPTOR ret = assoc->value.get();
    return SecurityDescriptorDynamic(ret, std::move(assoc));
}

SecurityDescriptorLocal getObjectSecurityDescriptor(HANDLE handle) {
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const DWORD errCode = GetSecurityInfo(handle, SE_KERNEL_OBJECT,
        OWNER_SECURITY_INFORMATION |
            GROUP_SECURITY_INFORMATION |
            DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &dacl, nullptr, &sd);
    if (errCode != ERROR_SUCCESS) {
        trace("error: GetSecurityInfo failed: %u",
            static_cast<unsigned>(errCode));
        return SecurityDescriptorLocal();
    } else {
        return SecurityDescriptorLocal(sd);
    }
}

std::wstring sidToString(PSID sid) {
    wchar_t *sidString = NULL;
    BOOL success = ConvertSidToStringSidW(sid, &sidString);
    if (!success) {
        trace("error: ConvertSidToStringSidW failed");
        return std::wstring();
    }
    PointerLocal freer(sidString);
    return std::wstring(sidString);
}

SidLocal stringToSid(const std::wstring &str) {
    // Cast the string from const wchar_t* to LPWSTR because the function is
    // incorrectly prototyped in the MinGW sddl.h header.  The API does not
    // modify the string -- it is correctly prototyped as taking LPCWSTR in
    // MinGW-w64, MSVC, and MSDN.
    PSID psid = nullptr;
    BOOL success = ConvertStringSidToSidW(const_cast<LPWSTR>(str.c_str()),
                                          &psid);
    if (!success) {
        trace("error: ConvertStringSidToSidW failed");
        return SidLocal();
    }
    return SidLocal(psid);
}

// The two string<->SD conversion APIs are used for debugging purposes.
// Converting a string to an SD is convenient, but it's too slow for ordinary
// use.  The APIs exist in XP and up, but the MinGW headers don't declare the
// functions.  (Of course, MinGW-w64 and MSVC both *do* declare them.)

typedef BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorW_t(
    LPCWSTR StringSecurityDescriptor,
    DWORD StringSDRevision,
    PSECURITY_DESCRIPTOR *SecurityDescriptor,
    PULONG SecurityDescriptorSize);

typedef BOOL WINAPI ConvertSecurityDescriptorToStringSecurityDescriptorW_t(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    DWORD RequestedStringSDRevision,
    SECURITY_INFORMATION SecurityInformation,
    LPWSTR *StringSecurityDescriptor,
    PULONG StringSecurityDescriptorLen);

const DWORD kSDDL_REVISION_1 = 1;

SecurityDescriptorLocal stringToSd(const std::wstring &str) {
    OsModule advapi32(L"advapi32.dll");
    const auto proc =
        reinterpret_cast<ConvertStringSecurityDescriptorToSecurityDescriptorW_t*>(
            advapi32.proc("ConvertStringSecurityDescriptorToSecurityDescriptorW"));
    if (proc == nullptr) {
        // The OsModule class already logged an error message.
        return SecurityDescriptorLocal();
    }
    PSECURITY_DESCRIPTOR desc = nullptr;
    if (!proc(str.c_str(),
              kSDDL_REVISION_1,
              &desc,
              nullptr)) {
        trace("error: ConvertStringSecurityDescriptorToSecurityDescriptorW failed: "
              "str='%ls'", str.c_str());
        return SecurityDescriptorLocal();
    }
    return SecurityDescriptorLocal(desc);
}

// Generates a human-readable representation of a PSECURITY_DESCRIPTOR.
// Returns an empty string on error.
std::wstring sdToString(PSECURITY_DESCRIPTOR sd) {
    OsModule advapi32(L"advapi32.dll");
    const auto proc =
        reinterpret_cast<ConvertSecurityDescriptorToStringSecurityDescriptorW_t*>(
            advapi32.proc("ConvertSecurityDescriptorToStringSecurityDescriptorW"));
    if (proc == nullptr) {
        // The OsModule class already logged an error message.
        return std::wstring();
    }
    wchar_t *sdString = nullptr;
    if (!proc(sd,
              kSDDL_REVISION_1,
              OWNER_SECURITY_INFORMATION |
                  GROUP_SECURITY_INFORMATION |
                  DACL_SECURITY_INFORMATION,
              &sdString,
              nullptr)) {
        trace("error: ConvertSecurityDescriptorToStringSecurityDescriptor failed");
        return std::wstring();
    }
    PointerLocal freer(sdString);
    return std::wstring(sdString);
}

// Vista added a useful flag to CreateNamedPipe, PIPE_REJECT_REMOTE_CLIENTS,
// that rejects remote connections.  Return this flag on Vista, or return 0
// otherwise.
DWORD rejectRemoteClientsPipeFlag() {
    // MinGW lacks this flag; MinGW-w64 has it.
    const DWORD kPIPE_REJECT_REMOTE_CLIENTS = 8;

    OSVERSIONINFOW info = { sizeof(info) };
    if (!GetVersionExW(&info)) {
        trace("error: GetVersionExW failed: %u",
            static_cast<unsigned>(GetLastError()));
        return kPIPE_REJECT_REMOTE_CLIENTS;
    } else if (info.dwMajorVersion >= 6) {
        return kPIPE_REJECT_REMOTE_CLIENTS;
    } else {
        trace("Omitting PIPE_REJECT_REMOTE_CLIENTS on old OS (%d.%d)",
            static_cast<int>(info.dwMajorVersion),
            static_cast<int>(info.dwMinorVersion));
        return 0;
    }
}

} // winpty_shared namespace
