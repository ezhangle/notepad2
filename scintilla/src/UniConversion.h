// Scintilla source code edit control
/** @file UniConversion.h
 ** Functions to handle UTF-8 and UTF-16 strings.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef UNICONVERSION_H
#define UNICONVERSION_H

namespace Scintilla {

const int UTF8MaxBytes = 4;

const int unicodeReplacementChar = 0xFFFD;

size_t UTF8Length(const wchar_t *uptr, size_t tlen);
void UTF8FromUTF16(const wchar_t *uptr, size_t tlen, char *putf, size_t len);
void UTF8FromUTF32Character(int uch, char *putf);
size_t UTF16Length(const char *s, size_t len);
size_t UTF16FromUTF8(const char *s, size_t len, wchar_t *tbuf, size_t tlen);
size_t UTF32FromUTF8(const char *s, size_t len, unsigned int *tbuf, size_t tlen);
unsigned int UTF16FromUTF32Character(unsigned int val, wchar_t *tbuf);
std::string FixInvalidUTF8(const std::string &text);

extern const unsigned char UTF8ClassifyTable[256];

enum {
	UTF8ClassifyMaskOctetCount = 7,
	UTF8ClassifyMaskTrailByte = 8,
};

static inline int UTF8BytesOfLead(unsigned char ch) {
	return UTF8ClassifyTable[ch] & UTF8ClassifyMaskOctetCount;
}

static inline int UnicodeFromUTF8(const unsigned char *us) {
	switch (UTF8BytesOfLead(us[0])) {
	case 1:
		return us[0];
	case 2:
		return ((us[0] & 0x1F) << 6) + (us[1] & 0x3F);
	case 3:
		return ((us[0] & 0xF) << 12) + ((us[1] & 0x3F) << 6) + (us[2] & 0x3F);
	default:
		return ((us[0] & 0x7) << 18) + ((us[1] & 0x3F) << 12) + ((us[2] & 0x3F) << 6) + (us[3] & 0x3F);
	}
}

static inline bool UTF8IsTrailByte(unsigned char ch) {
	return (UTF8ClassifyTable[ch] & UTF8ClassifyMaskTrailByte) !=0;
}

static inline bool UTF8IsAscii(unsigned char ch) {
	return (ch & 0x80) == 0;
}

static inline bool UTF8IsAscii(unsigned int ch) {
	return ch < 0x80;
}

enum { UTF8MaskWidth=0x7, UTF8MaskInvalid=0x8 };
int UTF8Classify(const unsigned char *us, int len);

// Similar to UTF8Classify but returns a length of 1 for invalid bytes
// instead of setting the invalid flag
int UTF8DrawBytes(const unsigned char *us, int len);

// Line separator is U+2028 \xe2\x80\xa8
// Paragraph separator is U+2029 \xe2\x80\xa9
const int UTF8SeparatorLength = 3;
static inline bool UTF8IsSeparator(const unsigned char *us) {
	const unsigned int value = (us[0] << 16) | (us[1] << 8) | us[2];
	return value == 0xe280a8 || value == 0xe280a9;
}

// NEL is U+0085 \xc2\x85
const int UTF8NELLength = 2;
static inline bool UTF8IsNEL(const unsigned char *us) {
	const unsigned int value = (us[0] << 8) | us[1];
	return value == 0xc285;
}

enum { SURROGATE_LEAD_FIRST = 0xD800 };
enum { SURROGATE_LEAD_LAST = 0xDBFF };
enum { SURROGATE_TRAIL_FIRST = 0xDC00 };
enum { SURROGATE_TRAIL_LAST = 0xDFFF };
enum { SUPPLEMENTAL_PLANE_FIRST = 0x10000 };

static inline unsigned int UTF16CharLength(wchar_t uch) {
	return ((uch >= SURROGATE_LEAD_FIRST) && (uch <= SURROGATE_LEAD_LAST)) ? 2 : 1;
}

static inline unsigned int UTF16LengthFromUTF8ByteCount(unsigned int byteCount) {
	return (byteCount < 4) ? 1 : 2;
}

}

#endif
