#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

#include "ChatTranslateConfigVariables.h"

#if IS_WIN64()
#include <Windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")
#endif

namespace chat_translate
{

struct LanguageCodes {
    const char* deepl;
    const wchar_t* microsoft;
};

[[nodiscard]] inline LanguageCodes languageCodes(std::uint8_t language) noexcept
{
    switch (language) {
    case chat_translate_vars::TargetGerman: return {"DE", L"de"};
    case chat_translate_vars::TargetFrench: return {"FR", L"fr"};
    case chat_translate_vars::TargetSpanish: return {"ES", L"es"};
    case chat_translate_vars::TargetPortuguese: return {"PT-PT", L"pt"};
    case chat_translate_vars::TargetRussian: return {"RU", L"ru"};
    case chat_translate_vars::TargetTurkish: return {"TR", L"tr"};
    case chat_translate_vars::TargetPolish: return {"PL", L"pl"};
    case chat_translate_vars::TargetJapanese: return {"JA", L"ja"};
    case chat_translate_vars::TargetKorean: return {"KO", L"ko"};
    case chat_translate_vars::TargetChinese: return {"ZH", L"zh-Hans"};
    default: return {"EN-US", L"en"};
    }
}

[[nodiscard]] inline bool endsWith(std::string_view value, std::string_view suffix) noexcept
{
    if (value.size() < suffix.size())
        return false;
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        if (value[value.size() - suffix.size() + i] != suffix[i])
            return false;
    }
    return true;
}

inline void append(char* output, std::size_t& outputSize, std::size_t outputCapacity, std::string_view text) noexcept
{
    const auto toCopy = (std::min)(text.size(), outputCapacity > outputSize ? outputCapacity - outputSize - 1 : 0);
    std::copy_n(text.data(), toCopy, output + outputSize);
    outputSize += toCopy;
    output[outputSize] = '\0';
}

inline void urlEncode(std::string_view input, char* output, std::size_t outputCapacity) noexcept
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::size_t outputSize = 0;
    for (const unsigned char ch : input) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            append(output, outputSize, outputCapacity, std::string_view{reinterpret_cast<const char*>(&ch), 1});
        } else if (outputSize + 4 <= outputCapacity) {
            output[outputSize++] = '%';
            output[outputSize++] = hex[ch >> 4];
            output[outputSize++] = hex[ch & 0xF];
            output[outputSize] = '\0';
        }
    }
}

inline void jsonEscape(std::string_view input, char* output, std::size_t outputCapacity) noexcept
{
    std::size_t outputSize = 0;
    for (const char ch : input) {
        switch (ch) {
        case '"': append(output, outputSize, outputCapacity, "\\\""); break;
        case '\\': append(output, outputSize, outputCapacity, "\\\\"); break;
        case '\n': append(output, outputSize, outputCapacity, "\\n"); break;
        case '\r': append(output, outputSize, outputCapacity, "\\r"); break;
        case '\t': append(output, outputSize, outputCapacity, "\\t"); break;
        default: append(output, outputSize, outputCapacity, std::string_view{&ch, 1}); break;
        }
    }
}

[[nodiscard]] inline bool extractJsonStringAfter(std::string_view json, std::string_view key, char* output, std::size_t outputCapacity) noexcept
{
    const auto keyPosition = json.find(key);
    if (keyPosition == std::string_view::npos)
        return false;

    auto quotePosition = json.find('"', keyPosition + key.size());
    if (quotePosition == std::string_view::npos)
        return false;
    ++quotePosition;

    std::size_t outputSize = 0;
    for (std::size_t i = quotePosition; i < json.size() && outputSize + 1 < outputCapacity; ++i) {
        const char ch = json[i];
        if (ch == '"') {
            output[outputSize] = '\0';
            return outputSize != 0;
        }
        if (ch == '\\' && i + 1 < json.size()) {
            const char escaped = json[++i];
            switch (escaped) {
            case 'n': output[outputSize++] = '\n'; break;
            case 'r': output[outputSize++] = '\r'; break;
            case 't': output[outputSize++] = '\t'; break;
            default: output[outputSize++] = escaped; break;
            }
        } else {
            output[outputSize++] = ch;
        }
    }
    output[outputSize] = '\0';
    return false;
}

#if IS_WIN64()
struct WinHttpHandle {
    HINTERNET handle{};

    ~WinHttpHandle()
    {
        if (handle)
            WinHttpCloseHandle(handle);
    }
};

struct WinHeapBuffer {
    explicit WinHeapBuffer(std::size_t size) noexcept
        : data{static_cast<char*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size))}
    {
    }

    ~WinHeapBuffer()
    {
        if (data)
            HeapFree(GetProcessHeap(), 0, data);
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return data != nullptr;
    }

    char* data{};
};

[[nodiscard]] inline bool readResponse(HINTERNET request, char* output, std::size_t outputCapacity) noexcept
{
    std::size_t outputSize = 0;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(request, &bytesAvailable) && bytesAvailable != 0 && outputSize + 1 < outputCapacity) {
        char buffer[512];
        DWORD bytesRead = 0;
        const auto bytesToRead = (std::min<DWORD>)(bytesAvailable, sizeof(buffer));
        if (!WinHttpReadData(request, buffer, bytesToRead, &bytesRead) || bytesRead == 0)
            break;
        append(output, outputSize, outputCapacity, {buffer, bytesRead});
    }
    return outputSize != 0;
}

[[nodiscard]] inline bool post(
    const wchar_t* host,
    const wchar_t* path,
    const wchar_t* extraHeaders,
    std::string_view body,
    char* response,
    std::size_t responseCapacity) noexcept
{
    WinHttpHandle session{WinHttpOpen(L"Osiris ChatTranslate/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.handle)
        return false;

    WinHttpHandle connect{WinHttpConnect(session.handle, host, INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connect.handle)
        return false;

    WinHttpHandle request{WinHttpOpenRequest(connect.handle, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!request.handle)
        return false;

    WinHttpSetTimeouts(request.handle, 3000, 3000, 5000, 5000);

    if (!WinHttpSendRequest(request.handle, extraHeaders, static_cast<DWORD>(-1), const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0))
        return false;
    if (!WinHttpReceiveResponse(request.handle, nullptr))
        return false;

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX))
        return false;
    if (statusCode < 200 || statusCode >= 300)
        return false;

    return readResponse(request.handle, response, responseCapacity);
}
#endif

[[nodiscard]] inline bool translateWithDeepL(std::string_view apiKey, std::uint8_t language, std::string_view text, char* output, std::size_t outputCapacity) noexcept
{
#if IS_WIN64()
    WinHeapBuffer encodedText{1536};
    WinHeapBuffer body{1800};
    WinHeapBuffer response{4096};
    if (!encodedText || !body || !response)
        return false;

    urlEncode(text, encodedText.data, 1536);

    std::size_t bodySize = 0;
    append(body.data, bodySize, 1800, "text=");
    append(body.data, bodySize, 1800, encodedText.data);
    append(body.data, bodySize, 1800, "&target_lang=");
    append(body.data, bodySize, 1800, languageCodes(language).deepl);

    wchar_t headers[512]{};
    const auto headerPrefix = L"Content-Type: application/x-www-form-urlencoded\r\nAuthorization: DeepL-Auth-Key ";
    std::size_t writeIndex = 0;
    while (headerPrefix[writeIndex] != L'\0') {
        headers[writeIndex] = headerPrefix[writeIndex];
        ++writeIndex;
    }
    for (const char ch : apiKey) {
        if (writeIndex + 3 >= std::size(headers))
            break;
        headers[writeIndex++] = static_cast<unsigned char>(ch);
    }
    headers[writeIndex++] = L'\r';
    headers[writeIndex++] = L'\n';
    headers[writeIndex] = L'\0';

    const auto host = endsWith(apiKey, ":fx") ? L"api-free.deepl.com" : L"api.deepl.com";
    if (!post(host, L"/v2/translate", headers, {body.data, bodySize}, response.data, 4096))
        return false;
    return extractJsonStringAfter(response.data, "\"text\"", output, outputCapacity);
#else
    return false;
#endif
}

[[nodiscard]] inline bool translateWithMicrosoft(std::string_view apiKey, std::string_view region, std::uint8_t language, std::string_view text, char* output, std::size_t outputCapacity) noexcept
{
#if IS_WIN64()
    WinHeapBuffer escapedText{1200};
    WinHeapBuffer body{1400};
    WinHeapBuffer response{4096};
    if (!escapedText || !body || !response)
        return false;

    jsonEscape(text, escapedText.data, 1200);

    std::size_t bodySize = 0;
    append(body.data, bodySize, 1400, "[{\"Text\":\"");
    append(body.data, bodySize, 1400, escapedText.data);
    append(body.data, bodySize, 1400, "\"}]");

    wchar_t path[160]{};
    const wchar_t* prefix = L"/translate?api-version=3.0&to=";
    std::size_t pathIndex = 0;
    while (prefix[pathIndex] != L'\0') {
        path[pathIndex] = prefix[pathIndex];
        ++pathIndex;
    }
    const auto* lang = languageCodes(language).microsoft;
    for (std::size_t i = 0; lang[i] != L'\0' && pathIndex + 1 < std::size(path); ++i)
        path[pathIndex++] = lang[i];
    path[pathIndex] = L'\0';

    wchar_t headers[700]{};
    std::size_t headerSize = 0;
    const auto appendWide = [&headers, &headerSize](const wchar_t* text) {
        while (*text != L'\0' && headerSize + 1 < std::size(headers))
            headers[headerSize++] = *text++;
        headers[headerSize] = L'\0';
    };
    const auto appendAscii = [&headers, &headerSize](std::string_view text) {
        for (const char ch : text) {
            if (headerSize + 1 >= std::size(headers))
                break;
            headers[headerSize++] = static_cast<unsigned char>(ch);
        }
        headers[headerSize] = L'\0';
    };
    appendWide(L"Content-Type: application/json\r\nOcp-Apim-Subscription-Key: ");
    appendAscii(apiKey);
    appendWide(L"\r\nOcp-Apim-Subscription-Region: ");
    appendAscii(region);
    appendWide(L"\r\n");

    if (!post(L"api.cognitive.microsofttranslator.com", path, headers, {body.data, bodySize}, response.data, 4096))
        return false;
    return extractJsonStringAfter(response.data, "\"text\"", output, outputCapacity);
#else
    return false;
#endif
}

}
