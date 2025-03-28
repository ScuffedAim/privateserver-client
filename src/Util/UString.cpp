#include "UString.h"

#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "Font.h"

#define USTRING_MASK_1BYTE 0x80  /* 1000 0000 */
#define USTRING_VALUE_1BYTE 0x00 /* 0000 0000 */
#define USTRING_MASK_2BYTE 0xE0  /* 1110 0000 */
#define USTRING_VALUE_2BYTE 0xC0 /* 1100 0000 */
#define USTRING_MASK_3BYTE 0xF0  /* 1111 0000 */
#define USTRING_VALUE_3BYTE 0xE0 /* 1110 0000 */
#define USTRING_MASK_4BYTE 0xF8  /* 1111 1000 */
#define USTRING_VALUE_4BYTE 0xF0 /* 1111 0000 */
#define USTRING_MASK_5BYTE 0xFC  /* 1111 1100 */
#define USTRING_VALUE_5BYTE 0xF8 /* 1111 1000 */
#define USTRING_MASK_6BYTE 0xFE  /* 1111 1110 */
#define USTRING_VALUE_6BYTE 0xFC /* 1111 1100 */

#define USTRING_MASK_MULTIBYTE 0x3F /* 0011 1111 */

#define USTRING_ESCAPE_CHAR '\\'

constexpr char UString::nullString[];
constexpr wchar_t UString::nullWString[];

void trim(std::string *str) {
    while(!str->empty() && str->front() == ' ') {
        str->erase(0, 1);
    }
    while(!str->empty() && (str->back() == ' ' || str->back() == '\r')) {
        str->pop_back();
    }
}

MD5Hash::MD5Hash(const char *str) {
    strncpy(this->hash, str, 32);
    this->hash[32] = 0;
}

bool MD5Hash::operator==(const MD5Hash &other) const { return memcmp(this->hash, other.hash, 32) == 0; }
bool MD5Hash::operator==(const UString &other) const { return strncmp(this->hash, other.toUtf8(), 32) == 0; }

UString::UString() {
    this->mLength = 0;
    this->mLengthUtf8 = 0;
    this->mIsAsciiOnly = false;
    this->mUnicode = (wchar_t *)nullWString;
    this->mUtf8 = (char *)nullString;
}

UString::UString(const char *utf8) {
    this->mLength = 0;
    this->mLengthUtf8 = 0;
    this->mUnicode = (wchar_t *)nullWString;
    this->mUtf8 = (char *)nullString;

    this->fromUtf8(utf8);
}

UString::UString(const char *utf8, int length) {
    this->mLength = 0;
    this->mLengthUtf8 = length;
    this->mUnicode = (wchar_t *)nullWString;
    this->mUtf8 = (char *)nullString;

    this->fromUtf8(utf8, length);
}

UString::UString(const UString &ustr) {
    this->mLength = 0;
    this->mLengthUtf8 = 0;
    this->mUnicode = (wchar_t *)nullWString;
    this->mUtf8 = (char *)nullString;

    (*this) = ustr;
}

UString::UString(UString &&ustr) {
    // move
    this->mLength = ustr.mLength;
    this->mLengthUtf8 = ustr.mLengthUtf8;
    this->mIsAsciiOnly = ustr.mIsAsciiOnly;
    this->mUnicode = ustr.mUnicode;
    this->mUtf8 = ustr.mUtf8;

    // reset source
    ustr.mLength = 0;
    ustr.mIsAsciiOnly = false;
    ustr.mUnicode = NULL;
    ustr.mUtf8 = NULL;
}

UString::UString(const wchar_t *str) {
    // get length
    this->mLength = (str != NULL ? (int)std::wcslen(str) : 0);

    // allocate new mem for unicode data
    this->mUnicode = new wchar_t[this->mLength + 1];  // +1 for null termination later

    // copy contents and null terminate
    if(this->mLength > 0) memcpy(this->mUnicode, str, (this->mLength) * sizeof(wchar_t));

    this->mUnicode[this->mLength] = '\0';  // null terminate

    // null out and rebuild the utf version
    {
        this->mUtf8 = NULL;
        this->mLengthUtf8 = 0;
        this->mIsAsciiOnly = false;

        this->updateUtf8();
    }
}

UString::~UString() {
    this->mLength = 0;
    this->mLengthUtf8 = 0;

    this->deleteUnicode();
    this->deleteUtf8();
}

void UString::clear() {
    this->mLength = 0;
    this->mLengthUtf8 = 0;
    this->mIsAsciiOnly = false;

    this->deleteUnicode();
    this->deleteUtf8();
}

UString UString::format(const char *utf8format, ...) {
    // decode the utf8 string
    UString formatted;
    int bufSize = formatted.fromUtf8(utf8format) + 1;  // +1 for default heuristic (no args, but null char). arguments
                                                       // will need multiple iterations and allocations anyway

    if(formatted.mLength == 0) return formatted;

    // print the args to the format
    wchar_t *buf = NULL;
    int written = -1;
    while(true) {
        if(bufSize >= 1024 * 1024) {
            printf("WARNING: Potential vswprintf(%s, ...) infinite loop, stopping ...\n", utf8format);
            return formatted;
        }

        buf = new wchar_t[bufSize];

#ifdef _MSC_VER
        // https://github.com/McKay42/McEngine/commit/f5461d835a6ffb24fb26f0f36505e83e65e4bb2e
        // MSVC's standard library is horribly broken and switched the meaning of %s and %S for wide char functions
        // TODO: this doesn't handle patterns like "%%s"
        while(true) {
            int pos = formatted.find("%s");
            if(pos == -1) break;

            formatted.mUnicode[pos + 1] = L'S';
        }
        formatted.updateUtf8();
#endif

        va_list ap;
        va_start(ap, utf8format);
        written = vswprintf(buf, bufSize, formatted.mUnicode, ap);
        va_end(ap);

        // if we didn't use the entire buffer
        if(written > 0 && written < bufSize) {
            // cool, keep the formatted string and we're done
            if(!formatted.isUnicodeNull()) delete[] formatted.mUnicode;

            formatted.mUnicode = buf;
            formatted.mLength = written;
            formatted.updateUtf8();

            break;
        } else {
            // we need a larger buffer
            delete[] buf;
            bufSize *= 2;
        }
    }

    return formatted;
}

bool UString::isWhitespaceOnly() const {
    int startPos = 0;
    while(startPos < this->mLength) {
        if(!std::iswspace(this->mUnicode[startPos])) return false;

        startPos++;
    }

    return true;
}

int UString::findChar(wchar_t ch, int start, bool respectEscapeChars) const {
    bool escaped = false;
    for(int i = start; i < this->mLength; i++) {
        // if we're respecting escape chars AND we are not in an escape
        // sequence AND we've found an escape character
        if(respectEscapeChars && !escaped && this->mUnicode[i] == USTRING_ESCAPE_CHAR) {
            // now we're in an escape sequence
            escaped = true;
        } else {
            if(!escaped && this->mUnicode[i] == ch) return i;

            escaped = false;
        }
    }

    return -1;
}

int UString::findChar(const UString &str, int start, bool respectEscapeChars) const {
    bool escaped = false;
    for(int i = start; i < this->mLength; i++) {
        // if we're respecting escape chars AND we are not in an escape
        // sequence AND we've found an escape character
        if(respectEscapeChars && !escaped && this->mUnicode[i] == USTRING_ESCAPE_CHAR) {
            // now we're in an escape sequence
            escaped = true;
        } else {
            if(!escaped && str.findChar(this->mUnicode[i]) >= 0) return i;

            escaped = false;
        }
    }

    return -1;
}

int UString::find(const UString &str, int start) const {
    const int lastPossibleMatch = this->mLength - str.mLength;
    for(int i = start; i <= lastPossibleMatch; i++) {
        if(memcmp(&(this->mUnicode[i]), str.mUnicode, str.mLength * sizeof(*this->mUnicode)) == 0) return i;
    }

    return -1;
}

int UString::find(const UString &str, int start, int end) const {
    const int lastPossibleMatch = this->mLength - str.mLength;
    for(int i = start; i <= lastPossibleMatch && i < end; i++) {
        if(memcmp(&(this->mUnicode[i]), str.mUnicode, str.mLength * sizeof(*this->mUnicode)) == 0) return i;
    }

    return -1;
}

int UString::findLast(const UString &str, int start) const {
    int lastI = -1;
    for(int i = start; i < this->mLength; i++) {
        if(memcmp(&(this->mUnicode[i]), str.mUnicode, str.mLength * sizeof(*this->mUnicode)) == 0) lastI = i;
    }

    return lastI;
}

int UString::findLast(const UString &str, int start, int end) const {
    int lastI = -1;
    for(int i = start; i < this->mLength && i < end; i++) {
        if(memcmp(&(this->mUnicode[i]), str.mUnicode, str.mLength * sizeof(*this->mUnicode)) == 0) lastI = i;
    }

    return lastI;
}

int UString::findIgnoreCase(const UString &str, int start) const {
    const int lastPossibleMatch = this->mLength - str.mLength;
    for(int i = start; i <= lastPossibleMatch; i++) {
        bool equal = true;
        for(int c = 0; c < str.mLength; c++) {
            if((std::towlower(this->mUnicode[i + c]) - std::towlower(str.mUnicode[c])) != 0) {
                equal = false;
                break;
            }
        }
        if(equal) return i;
    }

    return -1;
}

int UString::findIgnoreCase(const UString &str, int start, int end) const {
    const int lastPossibleMatch = this->mLength - str.mLength;
    for(int i = start; i <= lastPossibleMatch && i < end; i++) {
        bool equal = true;
        for(int c = 0; c < str.mLength; c++) {
            if((std::towlower(this->mUnicode[i + c]) - std::towlower(str.mUnicode[c])) != 0) {
                equal = false;
                break;
            }
        }
        if(equal) return i;
    }

    return -1;
}

std::vector<UString> UString::wrap(McFont *font, f64 max_width) {
    std::vector<UString> lines;
    lines.push_back(UString());

    UString word = "";
    u32 line = 0;
    f64 line_width = 0.0;
    f64 word_width = 0.0;
    for(int i = 0; i < this->length(); i++) {
        if(this->mUnicode[i] == '\n') {
            lines[line].append(word);
            lines.push_back(UString());
            line++;
            line_width = 0.0;
            word = "";
            word_width = 0.0;
            continue;
        }

        f32 char_width = font->getGlyphMetrics(this->mUnicode[i]).advance_x;

        if(this->mUnicode[i] == ' ') {
            lines[line].append(word);
            line_width += word_width;
            word = "";
            word_width = 0.0;

            if(line_width + char_width > max_width) {
                // Ignore spaces at the end of a line
                lines.push_back(UString());
                line++;
                line_width = 0.0;
            } else if(line_width > 0.0) {
                lines[line].append(' ');
                line_width += char_width;
            }
        } else {
            if(word_width + char_width > max_width) {
                // Split word onto new line
                lines[line].append(word);
                lines.push_back(UString());
                line++;
                line_width = 0.0;
                word = "";
                word_width = 0.0;
            } else if(line_width + word_width + char_width > max_width) {
                // Wrap word on new line
                lines.push_back(UString());
                line++;
                line_width = 0.0;
                word.append(this->mUnicode[i]);
                word_width += char_width;
            } else {
                // Add character to word
                word.append(this->mUnicode[i]);
                word_width += char_width;
            }
        }
    }

    // Don't forget! ;)
    lines[line].append(word);

    return lines;
}

void UString::collapseEscapes() {
    if(this->mLength == 0) return;

    int writeIndex = 0;
    bool escaped = false;
    wchar_t *buf = new wchar_t[this->mLength];

    // iterate over the unicode string
    for(int readIndex = 0; readIndex < this->mLength; readIndex++) {
        // if we're not already escaped and this is an escape char
        if(!escaped && this->mUnicode[readIndex] == USTRING_ESCAPE_CHAR) {
            // we're escaped
            escaped = true;
        } else {
            // move this char over and increment the write index
            buf[writeIndex] = this->mUnicode[readIndex];
            writeIndex++;

            // we're no longer escaped
            escaped = false;
        }
    }

    // replace old data with new data
    this->deleteUnicode();
    this->mLength = writeIndex;
    this->mUnicode = new wchar_t[this->mLength];
    memcpy(this->mUnicode, buf, this->mLength * sizeof(wchar_t));

    // free the temporary buffer
    delete[] buf;

    // the utf encoding is out of date, update it
    this->updateUtf8();
}

void UString::append(const UString &str) {
    if(str.mLength == 0) return;

    // calculate new size
    const int newSize = this->mLength + str.mLength;

    // allocate new data buffer
    wchar_t *newUnicode = new wchar_t[newSize + 1];  // +1 for null termination later

    // copy existing data
    if(this->mLength > 0) memcpy(newUnicode, this->mUnicode, this->mLength * sizeof(wchar_t));

    // copy appended data
    memcpy(&(newUnicode[this->mLength]), str.mUnicode,
           (str.mLength + 1) * sizeof(wchar_t));  // +1 to also copy the null char from the old string

    // replace the old values with the new
    this->deleteUnicode();
    this->mUnicode = newUnicode;
    this->mLength = newSize;

    // reencode to utf8
    this->updateUtf8();
}

void UString::append(wchar_t ch) {
    // calculate new size
    const int newSize = this->mLength + 1;

    // allocate new data buffer
    wchar_t *newUnicode = new wchar_t[newSize + 1];  // +1 for null termination later

    // copy existing data
    if(this->mLength > 0) memcpy(newUnicode, this->mUnicode, this->mLength * sizeof(wchar_t));

    // copy appended data and null terminate
    newUnicode[this->mLength] = ch;
    newUnicode[this->mLength + 1] = '\0';

    // replace the old values with the new
    this->deleteUnicode();
    this->mUnicode = newUnicode;
    this->mLength = newSize;

    // reencode to utf8
    this->updateUtf8();
}

void UString::insert(int offset, const UString &str) {
    if(str.mLength == 0) return;

    offset = std::clamp<int>(offset, 0, this->mLength);

    // calculate new size
    const int newSize = this->mLength + str.mLength;

    // allocate new data buffer
    wchar_t *newUnicode = new wchar_t[newSize + 1];  // +1 for null termination later

    // if we're not inserting at the beginning of the string
    if(offset > 0) memcpy(newUnicode, this->mUnicode, offset * sizeof(wchar_t));  // copy first part of data

    // copy inserted string
    memcpy(&(newUnicode[offset]), str.mUnicode, str.mLength * sizeof(wchar_t));

    // if we're not inserting at the end of the string
    if(offset < this->mLength) {
        // copy rest of string (including terminating null char)
        const int numRightChars = (this->mLength - offset + 1);
        if(numRightChars > 0)
            memcpy(&(newUnicode[offset + str.mLength]), &(this->mUnicode[offset]), (numRightChars) * sizeof(wchar_t));
    } else
        newUnicode[newSize] = '\0';  // null terminate

    // replace the old values with the new
    this->deleteUnicode();
    this->mUnicode = newUnicode;
    this->mLength = newSize;

    // reencode to utf8
    this->updateUtf8();
}

void UString::insert(int offset, wchar_t ch) {
    offset = std::clamp<int>(offset, 0, this->mLength);

    // calculate new size
    const int newSize = this->mLength + 1;  // +1 for the added character

    // allocate new data buffer
    wchar_t *newUnicode = new wchar_t[newSize + 1];  // and again +1 for null termination later

    // copy first part of data
    if(offset > 0) memcpy(newUnicode, this->mUnicode, offset * sizeof(wchar_t));

    // place the inserted char
    newUnicode[offset] = ch;

    // copy rest of string (including terminating null char)
    const int numRightChars = this->mLength - offset + 1;
    if(numRightChars > 0)
        memcpy(&(newUnicode[offset + 1]), &(this->mUnicode[offset]), (numRightChars) * sizeof(wchar_t));

    // replace the old values with the new
    this->deleteUnicode();
    this->mUnicode = newUnicode;
    this->mLength = newSize;

    // reencode to utf8
    this->updateUtf8();
}

void UString::erase(int offset, int count) {
    if(this->isUnicodeNull() || this->mLength == 0 || count == 0 || offset > (this->mLength - 1)) return;

    offset = std::clamp<int>(offset, 0, this->mLength);
    count = std::clamp<int>(count, 0, this->mLength - offset);

    // calculate new size
    const int newLength = this->mLength - count;

    // allocate new data buffer
    wchar_t *newUnicode = new wchar_t[newLength + 1];  // +1 for null termination later

    // copy first part of data
    if(offset > 0) memcpy(newUnicode, this->mUnicode, offset * sizeof(wchar_t));

    // copy rest of string (including terminating null char)
    const int numRightChars = newLength - offset + 1;
    if(numRightChars > 0)
        memcpy(&(newUnicode[offset]), &(this->mUnicode[offset + count]), (numRightChars) * sizeof(wchar_t));

    // replace the old values with the new
    this->deleteUnicode();
    this->mUnicode = newUnicode;
    this->mLength = newLength;

    // reencode to utf8
    this->updateUtf8();
}

UString UString::substr(int offset, int charCount) const {
    offset = std::clamp<int>(offset, 0, this->mLength);

    if(charCount < 0) charCount = this->mLength - offset;

    charCount = std::clamp<int>(charCount, 0, this->mLength - offset);

    // allocate new mem
    UString str;
    str.mLength = charCount;
    str.mUnicode = new wchar_t[charCount + 1];  // +1 for null termination later

    // copy mem contents
    if(charCount > 0) memcpy(str.mUnicode, &(this->mUnicode[offset]), charCount * sizeof(wchar_t));

    str.mUnicode[charCount] = '\0';  // null terminate

    // update the substring's utf encoding
    str.updateUtf8();

    return str;
}

std::vector<UString> UString::split(UString delim) const {
    std::vector<UString> results;
    if(delim.length() < 1 || this->mLength < 1) return results;

    int start = 0;
    int end = 0;

    while((end = this->find(delim, start)) != -1) {
        results.push_back(this->substr(start, end - start));
        start = end + delim.length();
    }
    results.push_back(this->substr(start, end - start));

    return results;
}

UString UString::trim() const {
    int startPos = 0;
    while(startPos < this->mLength && std::iswspace(this->mUnicode[startPos])) {
        startPos++;
    }

    int endPos = this->mLength - 1;
    while((endPos >= 0) && (endPos < this->mLength) && std::iswspace(this->mUnicode[endPos])) {
        endPos--;
    }

    return this->substr(startPos, endPos - startPos + 1);
}

float UString::toFloat() const { return !this->isUtf8Null() ? std::strtof(this->mUtf8, NULL) : 0.0f; }

double UString::toDouble() const { return !this->isUtf8Null() ? std::strtod(this->mUtf8, NULL) : 0.0; }

long double UString::toLongDouble() const { return !this->isUtf8Null() ? std::strtold(this->mUtf8, NULL) : 0.0l; }

int UString::toInt() const { return !this->isUtf8Null() ? (int)std::strtol(this->mUtf8, NULL, 0) : 0; }

long UString::toLong() const { return !this->isUtf8Null() ? std::strtol(this->mUtf8, NULL, 0) : 0; }

long long UString::toLongLong() const { return !this->isUtf8Null() ? std::strtoll(this->mUtf8, NULL, 0) : 0; }

unsigned int UString::toUnsignedInt() const {
    return !this->isUtf8Null() ? (unsigned int)std::strtoul(this->mUtf8, NULL, 0) : 0;
}

unsigned long UString::toUnsignedLong() const { return !this->isUtf8Null() ? std::strtoul(this->mUtf8, NULL, 0) : 0; }

unsigned long long UString::toUnsignedLongLong() const {
    return !this->isUtf8Null() ? std::strtoull(this->mUtf8, NULL, 0) : 0;
}

void UString::lowerCase() {
    if(!this->isUnicodeNull() && !this->isUtf8Null() && this->mLength > 0) {
        for(int i = 0; i < this->mLength; i++) {
            this->mUnicode[i] = std::towlower(this->mUnicode[i]);
            this->mUtf8[i] = std::tolower(this->mUtf8[i]);
        }

        if(!this->mIsAsciiOnly) this->updateUtf8();
    }
}

void UString::upperCase() {
    if(!this->isUnicodeNull() && !this->isUtf8Null() && this->mLength > 0) {
        for(int i = 0; i < this->mLength; i++) {
            this->mUnicode[i] = std::towupper(this->mUnicode[i]);
            this->mUtf8[i] = std::toupper(this->mUtf8[i]);
        }

        if(!this->mIsAsciiOnly) this->updateUtf8();
    }
}

wchar_t UString::operator[](int index) const {
    if(this->mLength > 0) return this->mUnicode[std::clamp<int>(index, 0, this->mLength - 1)];

    return (wchar_t)0;
}

UString &UString::operator=(const UString &ustr) {
    wchar_t *newUnicode = (wchar_t *)nullWString;

    // if this is not a null string
    if(ustr.mLength > 0 && !ustr.isUnicodeNull()) {
        // allocate new mem for unicode data
        newUnicode = new wchar_t[ustr.mLength + 1];

        // copy unicode mem contents
        memcpy(newUnicode, ustr.mUnicode, (ustr.mLength + 1) * sizeof(wchar_t));
    }

    // deallocate old mem
    if(!this->isUnicodeNull()) delete[] this->mUnicode;

    // init variables
    this->mLength = ustr.mLength;
    this->mUnicode = newUnicode;

    // reencode to utf8
    this->updateUtf8();

    return *this;
}

UString &UString::operator=(UString &&ustr) {
    if(this != &ustr) {
        // free ourself
        if(!this->isUnicodeNull()) delete[] this->mUnicode;
        if(!this->isUtf8Null()) delete[] this->mUtf8;

        // move
        this->mLength = ustr.mLength;
        this->mLengthUtf8 = ustr.mLengthUtf8;
        this->mIsAsciiOnly = ustr.mIsAsciiOnly;
        this->mUnicode = ustr.mUnicode;
        this->mUtf8 = ustr.mUtf8;

        // reset source
        ustr.mLength = 0;
        ustr.mLengthUtf8 = 0;
        ustr.mIsAsciiOnly = false;
        ustr.mUnicode = NULL;
        ustr.mUtf8 = NULL;
    }

    return *this;
}

bool UString::operator==(const UString &ustr) const {
    if(this->mLength != ustr.mLength) return false;

    if(this->isUnicodeNull() && ustr.isUnicodeNull())
        return true;
    else if(this->isUnicodeNull() || ustr.isUnicodeNull())
        return (this->mLength == 0 && ustr.mLength == 0);

    return memcmp(this->mUnicode, ustr.mUnicode, this->mLength * sizeof(wchar_t)) == 0;
}

bool UString::operator!=(const UString &ustr) const {
    bool equal = (*this == ustr);
    return !equal;
}

bool UString::operator<(const UString &ustr) const {
    for(int i = 0; i < this->mLength && i < ustr.mLength; i++) {
        if(this->mUnicode[i] != ustr.mUnicode[i]) return this->mUnicode[i] < ustr.mUnicode[i];
    }

    if(this->mLength == ustr.mLength) return false;

    return this->mLength < ustr.mLength;
}

bool UString::startsWith(const UString &ustr) const {
    if(this->mLength < ustr.mLength) return false;
    for(int i = 0; i < ustr.mLength; i++) {
        if(this->mUnicode[i] != ustr.mUnicode[i]) return false;
    }
    return true;
}

bool UString::endsWith(const UString &ustr) const {
    if(this->mLength < ustr.mLength) return false;
    for(int i = 0; i < ustr.mLength; i++) {
        if(this->mUnicode[this->mLength - ustr.mLength + i] != ustr.mUnicode[i]) return false;
    }
    return true;
}

bool UString::startsWithIgnoreCase(const UString &ustr) const {
    if(this->mLength < ustr.mLength) return false;
    for(int i = 0; i < ustr.mLength; i++) {
        if(std::towlower(this->mUnicode[i]) != std::towlower(ustr.mUnicode[i])) return false;
    }
    return true;
}

bool UString::equalsIgnoreCase(const UString &ustr) const {
    if(this->mLength != ustr.mLength) return false;

    if(this->isUnicodeNull() && ustr.isUnicodeNull())
        return true;
    else if(this->isUnicodeNull() || ustr.isUnicodeNull())
        return false;

    for(int i = 0; i < this->mLength; i++) {
        if(std::towlower(this->mUnicode[i]) != std::towlower(ustr.mUnicode[i])) return false;
    }

    return true;
}

bool UString::lessThanIgnoreCase(const UString &ustr) const {
    for(int i = 0; i < this->mLength && i < ustr.mLength; i++) {
        if(std::towlower(this->mUnicode[i]) != std::towlower(ustr.mUnicode[i]))
            return std::towlower(this->mUnicode[i]) < std::towlower(ustr.mUnicode[i]);
    }

    if(this->mLength == ustr.mLength) return false;

    return this->mLength < ustr.mLength;
}

int UString::fromUtf8(const char *utf8, int length) {
    if(utf8 == NULL) return 0;

    const int supposedFullStringSize =
        (length > -1 ? length
                     : strlen(utf8) + 1);  // NOTE: +1 only for the strlen() case, since if we are called with a
                                           // specific length the buffer might not have a null byte at the end

    int startIndex = 0;
    if(supposedFullStringSize > 2) {
        if(utf8[0] == (char)0xef && utf8[1] == (char)0xbb && utf8[2] == (char)0xbf)  // utf-8
            startIndex = 3;
        else {
            // check for utf-16
            char c0 = utf8[0];
            char c1 = utf8[1];
            char c2 = utf8[2];
            bool utf16le = (c0 == (char)0xff && c1 == (char)0xfe && c2 != (char)0x00);
            bool utf16be = (c0 == (char)0xfe && c1 == (char)0xff && c2 != (char)0x00);
            if(utf16le || utf16be) {
                // TODO: UTF-16 not yet supported
                return 0;
            }

            // check for utf-32
            // HACKHACK: TODO: this check will never work, due to the null characters reporting strlen() as too short
            // (i.e. c1 == 0x00)
            if(supposedFullStringSize > 3) {
                char c3 = utf8[3];
                bool utf32le = (c0 == (char)0xff && c1 == (char)0xfe && c2 == (char)0x00 && c3 == (char)0x00);
                bool utf32be = (c0 == (char)0x00 && c1 == (char)0x00 && c2 == (char)0xfe && c3 == (char)0xff);

                if(utf32le || utf32be) {
                    // TODO: UTF-32 not yet supported
                    return 0;
                }
            }
        }
    }

    this->mLength = decode(&(utf8[startIndex]), NULL, supposedFullStringSize);
    this->mUnicode = new wchar_t[this->mLength + 1];  // +1 for null termination later
    length = decode(&(utf8[startIndex]), this->mUnicode, supposedFullStringSize);

    // reencode to utf8
    this->updateUtf8();

    return length;
}

int UString::decode(const char *utf8, wchar_t *unicode, int utf8Length) {
    if(utf8 == NULL) return 0;  // unicode is checked below

    int length = 0;
    for(int i = 0; (i < utf8Length && utf8[i] != 0); i++) {
        const char b = utf8[i];
        if((b & USTRING_MASK_1BYTE) == USTRING_VALUE_1BYTE)  // if this is a single byte code point
        {
            if(unicode != NULL) unicode[length] = b;
        } else if((b & USTRING_MASK_2BYTE) == USTRING_VALUE_2BYTE)  // if this is a 2 byte code point
        {
            if(unicode != NULL) unicode[length] = getCodePoint(utf8, i, 2, (unsigned char)(~USTRING_MASK_2BYTE));

            i += 1;
        } else if((b & USTRING_MASK_3BYTE) == USTRING_VALUE_3BYTE)  // if this is a 3 byte code point
        {
            if(unicode != NULL) unicode[length] = getCodePoint(utf8, i, 3, (unsigned char)(~USTRING_MASK_3BYTE));

            i += 2;
        } else if((b & USTRING_MASK_4BYTE) == USTRING_VALUE_4BYTE)  // if this is a 4 byte code point
        {
            if(unicode != NULL) unicode[length] = getCodePoint(utf8, i, 4, (unsigned char)(~USTRING_MASK_4BYTE));

            i += 3;
        } else if((b & USTRING_MASK_5BYTE) == USTRING_VALUE_5BYTE)  // if this is a 5 byte code point
        {
            if(unicode != NULL) unicode[length] = getCodePoint(utf8, i, 5, (unsigned char)(~USTRING_MASK_5BYTE));

            i += 4;
        } else if((b & USTRING_MASK_6BYTE) == USTRING_VALUE_6BYTE)  // if this is a 6 byte code point
        {
            if(unicode != NULL) unicode[length] = getCodePoint(utf8, i, 6, (unsigned char)(~USTRING_MASK_6BYTE));

            i += 5;
        }

        length++;
    }

    if(unicode != NULL) unicode[length] = '\0';  // null terminate

    return length;
}

int UString::encode(const wchar_t *unicode, int length, char *utf8, bool *isAsciiOnly) {
    if(unicode == NULL) return 0;  // utf8 is checked below

    int utf8len = 0;
    bool foundMultiByte = false;
    for(int i = 0; i < length; i++) {
        const wchar_t ch = unicode[i];

        if(ch < 0x00000080)  // 1 byte
        {
            if(utf8 != NULL) utf8[utf8len] = (char)ch;

            utf8len += 1;
        } else if(ch < 0x00000800)  // 2 bytes
        {
            foundMultiByte = true;

            if(utf8 != NULL) getUtf8(ch, &(utf8[utf8len]), 2, USTRING_VALUE_2BYTE);

            utf8len += 2;
        } else if(ch < 0x00010000)  // 3 bytes
        {
            foundMultiByte = true;

            if(utf8 != NULL) getUtf8(ch, &(utf8[utf8len]), 3, USTRING_VALUE_3BYTE);

            utf8len += 3;
        } else if(ch < 0x00200000)  // 4 bytes
        {
            foundMultiByte = true;

            if(utf8 != NULL) getUtf8(ch, &(utf8[utf8len]), 4, USTRING_VALUE_4BYTE);

            utf8len += 4;
        } else if(ch < 0x04000000)  // 5 bytes
        {
            foundMultiByte = true;

            if(utf8 != NULL) getUtf8(ch, &(utf8[utf8len]), 5, USTRING_VALUE_5BYTE);

            utf8len += 5;
        } else  // 6 bytes
        {
            foundMultiByte = true;

            if(utf8 != NULL) getUtf8(ch, &(utf8[utf8len]), 6, USTRING_VALUE_6BYTE);

            utf8len += 6;
        }
    }

    if(isAsciiOnly != NULL) *isAsciiOnly = !foundMultiByte;

    return utf8len;
}

wchar_t UString::getCodePoint(const char *utf8, int offset, int numBytes, unsigned char firstByteMask) {
    if(utf8 == NULL) return (wchar_t)0;

    // get the bits out of the first byte
    wchar_t wc = utf8[offset] & firstByteMask;

    // iterate over the rest of the bytes
    for(int i = 1; i < numBytes; i++) {
        // shift the code point bits to make room for the new bits
        wc = wc << 6;

        // add the new bits
        wc |= utf8[offset + i] & USTRING_MASK_MULTIBYTE;
    }

    // return the code point
    return wc;
}

void UString::getUtf8(wchar_t ch, char *utf8, int numBytes, int firstByteValue) {
    if(utf8 == NULL) return;

    for(int i = numBytes - 1; i > 0; i--) {
        // store the lowest bits in a utf8 byte
        utf8[i] = (ch & USTRING_MASK_MULTIBYTE) | 0x80;
        ch >>= 6;
    }

    // store the remaining bits
    *utf8 = (firstByteValue | ch);
}

void UString::updateUtf8() {
    // delete previous
    this->deleteUtf8();

    // rebuild
    this->mLengthUtf8 = encode(this->mUnicode, this->mLength, NULL, &this->mIsAsciiOnly);
    if(this->mLengthUtf8 > 0) {
        this->mUtf8 = new char[this->mLengthUtf8 + 1];  // +1 for null termination later
        encode(this->mUnicode, this->mLength, this->mUtf8, NULL);
        this->mUtf8[this->mLengthUtf8] = '\0';  // null terminate
    }
}
