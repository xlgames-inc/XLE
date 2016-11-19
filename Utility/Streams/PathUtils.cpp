// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PathUtils.h"
#include "../PtrUtils.h"
#include "../IteratorUtils.h"
#include "../MemoryUtils.h"
#include "../../Core/SelectConfiguration.h"
#include <utility>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <cctype>

namespace Utility
{

#if 0
template <class T, char SEP>
void conv_separator(T* dst, int count, const T* path)
{
    // just replace '\\' to '/'.
    // if you need path compaction, use _fullpath().

    const T* s = path;
    T* d = dst;

    bool hit = false;
    while (*s) {
        if (*s == '/' || *s == '\\') {
            if (!hit) {
                *d++ = (const T)SEP;
            }
            hit = true;
            ++s;
            continue;
        }
        hit = false;
        *d++ = *s++;
    }
    *d = 0;
}

template <class T>
void normalize_path(T* dst, int count, const T* path)
{
    conv_separator<T, '/'>(dst, count, path);
    // always use lower case drive letters
    if (dst[0] && dst[1] == ':' && dst[2] == '/') {
        dst[0] = XlToLower(dst[0]);
    }
}

void XlNormalizePath(char* dst, int count, const char* path)
{
    normalize_path<char>(dst, count, path);
}

void XlNormalizePath(ucs2* dst, int count, const ucs2* path)
{
    normalize_path<ucs2>(dst, count, path);
}

template <class T>
void simplify_path(T* dst, int count, const T* path0, const T* sep)
{
    T* tokens[128] = {0};
    T path[MaxPath];
    static const T dot[2] = { '.', '\0' };
    static const T dotdot[3] = { '.', '.', '\0' };

    XlCopyString(path, dimof(path), path0);
    size_t cnt = XlTokenizeString(path, dimof(path), sep, tokens, dimof(tokens));
    for (size_t i = 0; i < cnt; ++i) {
		assert(tokens[i]);
        if (XlEqString(tokens[i], dot) || tokens[i][0] == T('\0')) {
            tokens[i] = NULL;
        } else if (XlEqString(tokens[i], dotdot)) {
            tokens[i] = NULL;
            for (size_t j = i; j > 0; --j) {
                if (tokens[j - 1]) {
                    tokens[j - 1] = NULL;
                    break;
                }
            }
        }
    }

    if (tokens[0] == NULL) {
        dst[0] = '\0';
        return;
    }

    XlCopyString(dst, count, tokens[0]);

    for (size_t i = 1; i < cnt; ++i) {
        if (tokens[i]) {
            //remark_comment("we've no wide version of XlFormatString yet!")
            //offset += XlFormatString(dst + offset, count - offset, "%s%s", del, tokens[i]); 
            XlCatString(dst, count - XlStringSize(dst), sep[0]);
            XlCatString(dst, count - XlStringSize(dst), tokens[i]);
        }
    }
}

void XlSimplifyPath(char* dst, int count, const char* path0, const char* sep)
{
    simplify_path<char>(dst, count, path0, sep);
}

void XlSimplifyPath(ucs2* dst, int count, const ucs2* path0, const ucs2* sep)
{
    simplify_path<ucs2>(dst, count, path0, sep);
}


void XlToUnixPath(char* dst, int count, const char* path)
{
    conv_separator<char, '/'>(dst, count, path);    
}

void XlToUnixPath(ucs2* dst, int count, const ucs2* path)
{
    conv_separator<ucs2, '/'>(dst, count, path);
}

void XlToDosPath(char* dst, int count, const char* path)
{
    conv_separator<char, '\\'>(dst, count, path);
}

void XlToDosPath(ucs2* dst, int count, const ucs2* path)
{
    conv_separator<ucs2, '\\'>(dst, count, path);
}
#endif

template <class T>
void concat_path(T* dst, int count, const T* a, const T* b, const T* bEnd)
{
    // do simple concatenation first
    T tmp[MaxPath];
    if (a) {
        XlCopyString(tmp, dimof(tmp), a);
            // note -- there maybe be some problems with the following
            //          lines with network paths... todo, more robust
            //          implementation
        static const T slash[2] = { '/', '\0' };
        XlCatString(tmp, dimof(tmp), slash);
    }
    if (b) {
        XlCatNString(tmp, dimof(tmp), b, bEnd - b);
    }
    // XlNormalizePath(dst, count, tmp);        DavidJ -- this has a problem with network paths. \\NetworkServer\path\... -> /NetworkServer/path/... so skip this step (caller can always normalize afterwards)
    XlCopyString(dst, count, tmp);

    if (!dst[0]) {
        static const T dot[2] = { '.', '\0' };
        XlCopyString(dst, count, dot);
    }
}

void XlConcatPath(char* dst, int count, const char* a, const char* b, const char* bEnd)
{
    concat_path<char>(dst, count, a, b, bEnd);
}

void XlConcatPath(ucs2* dst, int count, const ucs2* a, const ucs2* b, const ucs2* bEnd)
{
    concat_path<ucs2>(dst, count, a, b, bEnd);
}

#if 0

void XlMakeRelPath(char* dst, int count, const char* root, const char* path)
{
        // Note -- DavidJ -- there are some problems with this implementation
        //          it should be replaced with a more reliable version
    const char* c = root;
    const char* s = path;
    const char* lastSlashS = path;
    const char* lastSlashC = root;
    for (;;) {
        if ((*c != *s) || (*c == '\0')) {
            if (*c == '\0' && (*s == '\\' || *s == '/')) {
                lastSlashC = c-1;
                lastSlashS = s-1;
            } else {
                s = lastSlashS;
                c = lastSlashC;
                if ((*c == '\\' || *c == '/') && (*s == '\\' || *s == '/')) {
                    ++s; ++c;
                }
            }
            break;
        }
        if (*c == '\\' || *c == '/') {
            lastSlashS = s;
            lastSlashC = c;
        }
        ++c;
        ++s;
    }

    while (*s == '/' || *s =='\\') { ++s; }

    int depth = 0;
    while (*c) {
        if (*c == '/' || *c == '\\') {
            ++depth;
            while (*c == '/' || *c == '\\') { ++c; }
        } else {
            ++c;
        }
    }

    dst[0] = 0;
    for (int i = 0; i < depth; ++i) {
        XlCatString(dst, count, "../");
    }
    XlCatString(dst, count, s);
}

void XlResolveRelPath(char* dst, int count, const char* base, const char* rel)
{
    char tmp[MaxPath];
    XlDirname(tmp, dimof(tmp), base);
    XlConcatPath(dst, count, tmp, rel, XlStringEnd(rel));
}

#endif

template<typename CharType>
    const CharType* XlExtension(const CharType* path)
{
    auto len = XlStringLen(path);
    if (!len) return nullptr;
    const auto* p = &path[len-1];
    while (p >= path) {
        if (*p == '\\' || *p == '/') return nullptr;
        if (*p == '.') return p + 1;
        --p;
    }
    return nullptr;
}

template const char* XlExtension(const char* path);
template const utf8* XlExtension(const utf8* path);
template const ucs2* XlExtension(const ucs2* path);
template const ucs4* XlExtension(const ucs4* path);

void XlChopExtension(char* path)
{
    char* p = (char*)XlExtension(path);
    if (!p || !p[0])
        return;

    // p points to the first character of the extension, so move one character
    // backwards
    --p;
    *p = '\0';
}

template <class T>
void get_dir_name(T* dst, int count, const T* path)
{
    const T* p = path;
    const T* sep = 0;
    while (*p) {
        if ((*p == '/' || *p == '\\') && p[1]) {
            sep = p;
        }
        ++p;
    }

    if (sep) {
        int n = (int)(sep - path)+1;
        if (n == 1) {
            n = 2;  // for root(/) dir
        }
        if (count < n)
            n = count;
        XlCopyNString(dst, count, path, n);
    } else {
        static const T dot[2] = { '.', '\0' };
        XlCopyString(dst, count, dot);
    }
}

void XlDirname(char* dst, int count, const char* path)
{
    get_dir_name<char>(dst, count, path);
}

void XlDirname(ucs2* dst, int count, const ucs2* path)
{
    get_dir_name<ucs2>(dst, count, path);
}

void XlBasename(char* dst, int count, const char* path)
{
    const char* p = path;
    const char* sep = 0;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            sep = p;
        }
        ++p;
    }

    XlCopyString(dst, count, sep ? sep + 1 : path);
}

const char* XlBasename(const char* path)
{
    const char* p = path;
    const char* sep = 0;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            sep = p;
        }
        ++p;
    }

    return sep ? sep + 1 : path;
}

#if 0

template <class T>
void split_path(const T* path, T* drive, T* dir, T* fname, T* ext)
{
    T *p;
    T *last_slash = NULL, *dot = NULL;
    unsigned len;

    /* we assume that the path argument has the following form, where any
    * or all of the components may be missing.
    *
    *  <drive><dir><fname><ext>
    *
    * and each of the components has the following expected form(s)
    *
    *  drive:
    *  0 to MaxDrive-1 characters, the last of which, if any, is a
    *  ':'
    *  dir:
    *  0 to MaxDir-1 characters in the form of an absolute path
    *  (leading '/' or '\') or relative path, the last of which, if
    *  any, must be a '/' or '\'.  E.g -
    *  absolute path:
    *      \top\next\last\     ; or
    *      /top/next/last/
    *  relative path:
    *      top\next\last\  ; or
    *      top/next/last/
    *  Mixed use of '/' and '\' within a path is also tolerated
    *  fname:
    *  0 to _MAX_FNAME-1 characters not including the '.' character
    *  ext:
    *  0 to MaxExtension-1 characters where, if any, the first must be a
    *  '.'
    *
    */

    /* extract drive letter and :, if any */

    if ((XlStringSize(path) >= (MaxDrive - 2)) && (*(path + MaxDrive - 2) == (':'))) {
        if (drive) {
            XlCopyNString(drive, MaxDrive, path, MaxDrive - 1);
            *(drive + MaxDrive-1) = ('\0');
        }
        path += MaxDrive - 1;
    }
    else if (drive) {
        *drive = ('\0');
    }

    /* extract path string, if any.  Path now points to the first character
    * of the path, if any, or the filename or extension, if no path was
    * specified.  Scan ahead for the last occurrence, if any, of a '/' or
    * '\' path separator character.  If none is found, there is no path.
    * We will also note the last '.' character found, if any, to aid in
    * handling the extension.
    */

    for (last_slash = NULL, p = (T *)path; *p; p++) {
        if (*p == ('/') || *p == ('\\'))
            /* point to one beyond for later copy */
            last_slash = p + 1;
        else if (*p == ('.'))
            dot = p;
    }

    if (last_slash) {

        /* found a path - copy up through last_slash or max. characters
        * allowed, whichever is smaller
        */

        if (dir) {
            len = std::min((unsigned int)(((char *)last_slash - (char *)path) / sizeof(char)),(unsigned int)(MaxDir - 1));
            XlCopyNString(dir, len+1, path, len);
            *(dir + len) = ('\0');
        }
        path = last_slash;
    }
    else if (dir) {

        /* no path found */

        *dir = ('\0');
    }

    /* extract file name and extension, if any.  Path now points to the
    * first character of the file name, if any, or the extension if no
    * file name was given.  Dot points to the '.' beginning the extension,
    * if any.
    */

    if (dot && (dot >= path)) {
        /* found the marker for an extension - copy the file name up to
        * the '.'.
        */
        if (fname) {
            len = std::min((unsigned int)(((char *)dot - (char *)path) / sizeof(char)),(unsigned int)(MaxFilename- 1));
            XlCopyNString(fname, len+1, path, len);
            *(fname + len) = ('\0');
        }
        /* now we can get the extension - remember that p still points
        * to the terminating null character of path.
        */
        if (ext) {
            len = std::min((unsigned int)(((char *)p - (char *)dot) / sizeof(char)),(unsigned int)(MaxExtension - 1));
            XlCopyNString(ext, len+1, dot, len);
            *(ext + len) = ('\0');
        }
    }
    else {
        /* found no extension, give empty extension and copy rest of
        * string into fname.
        */
        if (fname) {
            len = std::min((unsigned int)(((char *)p - (char *)path) / sizeof(char)),(unsigned int)(MaxFilename- 1));
            XlCopyNString(fname, len+1, path, len);
            *(fname + len) = ('\0');
        }
        if (ext) {
            *ext = ('\0');
        }
    }
}

void XlSplitPath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
    split_path<char>(path, drive, dir, fname, ext);
}

void XlSplitPath(const ucs2* path, ucs2* drive, ucs2* dir, ucs2* fname, ucs2* ext)
{
    split_path<ucs2>(path, drive, dir, fname, ext);
}

#pragma warning(disable:4706)   // assignment within conditional expression

template <class T>
void make_path(T* path, const T* drive, const T* dir, const T* fname, const T* ext)
{
    const T *p;

    /* we assume that the arguments are in the following form (although we
    * do not diagnose invalid arguments or illegal filenames (such as
    * names longer than 8.3 or with illegal characters in them)
    *
    *  drive:
    *      A           ; or
    *      A:
    *  dir:
    *      \top\next\last\     ; or
    *      /top/next/last/     ; or
    *      either of the above forms with either/both the leading
    *      and trailing / or \ removed.  Mixed use of '/' and '\' is
    *      also tolerated
    *  fname:
    *      any valid file name
    *  ext:
    *      any valid extension (none if empty or null )
    */

    /* copy drive */

    if (drive && *drive) {
        *path++ = *drive;
        *path++ = (':');
    }

    /* copy dir */

    if ((p = dir) && *p) {
        do {
            *path++ = *p++;
        }
        while (*p);
        if (*(p-1) != '/' && *(p-1) != ('\\')) {
            *path++ = ('\\');
        }
    }

    /* copy fname */

    if ((p = fname)) {
        while (*p) {
            *path++ = *p++;
        }
    }

    /* copy ext, including 0-terminator - check to see if a '.' needs
    * to be inserted.
    */

    if ((p = ext)) {
        if (*p && *p != ('.')) {
            *path++ = ('.');
        }
        while ((*path++ = *p++))
            ;
    }
    else {
        /* better add the 0-terminator */
        *path = ('\0');
    }
}

void XlMakePath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
    make_path<char>(path, drive, dir, fname, ext);
}

void XlMakePath(ucs2* path, const ucs2* drive, const ucs2* dir, const ucs2* fname, const ucs2* ext)
{
    make_path<ucs2>(path, drive, dir, fname, ext);
}

#endif

    #define TC template<typename CharType>

    TC FileNameSplitter<CharType>::FileNameSplitter(Section rawString)
	{
        _fullFilename = rawString;

        const CharType seps[] = { (CharType)'\\', (CharType)'/' };
        const auto* sepsEnd = ArrayEnd(seps);

        const auto* pathStart = rawString._start;

		auto firstColon = std::find(rawString._start, rawString._end, ':');
		if (firstColon > std::find_first_of(rawString._start, rawString._end, seps, sepsEnd)) {
    		_drive = Section(rawString._start, firstColon+1);
            pathStart = firstColon+1;
        } else {
            _drive = Section(rawString._start, rawString._end);
        }

		auto lastSlash = FindLastOf(rawString._start, rawString._end, seps, sepsEnd);
		if (lastSlash == rawString._end) {
            _path = Section(pathStart, pathStart);
        } else {
            _path = Section(pathStart, lastSlash+1);
        }

		auto paramStart = std::find(_path._end, rawString._end, ':');
        auto endOfFile = FindLastOf(_path._end, paramStart, '.');
		_file = Section(_path._end, endOfFile);
        _extension = Section(endOfFile, paramStart);
        _parameters = Section(paramStart, rawString._end);
	}

    TC FileNameSplitter<CharType>::FileNameSplitter(const CharType rawString[])
        : FileNameSplitter(Section(rawString)) {}

    TC FileNameSplitter<CharType>::FileNameSplitter(const std::basic_string<CharType>& rawString)
        : FileNameSplitter(Section(rawString)) {}


	TC unsigned SplitPath<CharType>::GetSectionCount() const { return (unsigned)_sections.size(); }
    
	TC typename SplitPath<CharType>::SectionType AsSectionType(StringSection<CharType> e)
	{
		if ((e._end - e._start) == 1 && *e._start == '.')
            return SplitPath<CharType>::SectionType::CurrentDir;
		if ((e._end - e._start) == 2 && *e._start == '.' && *(e._start + 1) == '.')
            return SplitPath<CharType>::SectionType::BackOne;

        // extended series of dots (eg, "...") are considered just a name, and not special
        // commands
		return SplitPath<CharType>::SectionType::Name;
	}

	TC auto SplitPath<CharType>::GetSectionType(unsigned index) const -> SectionType
	{
		return AsSectionType(GetSection(index));
	}

	TC auto SplitPath<CharType>::GetSection(unsigned index) const -> Section
	{
		return _sections[index];
	}

    TC auto SplitPath<CharType>::GetDrive() const -> Section { return _drive; }

	TC auto SplitPath<CharType>::Simplify() const -> SplitPath
	{
		auto result = _sections;    // (copy)

		    // remove "." and ".." elements where possible
		for (auto i = result.begin(); i != result.end();) {
			auto e = *i;
			auto type = AsSectionType(e);
			if (type == SectionType::CurrentDir) {
				i = result.erase(i);
			} else if (type == SectionType::BackOne) {
				if (i != result.begin() && AsSectionType(*(i-1)) == SectionType::Name) {
					i = result.erase(i - 1, i + 1);
				} else ++i;		// can't go back too far (this means some ".." may be left)
			} else ++i;
		}

			// If we remove all path elements (ie, because of "." or ".." elements) then we must
			// supress the "endsWithSeparator" flag. Consider -- "somepath/../". If this becomes
			// "/", then the meaning has changed
		auto endsWithSeparator = _endsWithSeparator;
		if (result.empty()) endsWithSeparator = false;

		return SplitPath(std::move(result), _beginsWithSeparator, endsWithSeparator, _drive);
	}

	TC auto SplitPath<CharType>::Rebuild(const FilenameRules& rules) const -> String
	{
		std::basic_stringstream<CharType> stream;

        if (!_drive.Empty()) {
            auto*s = _drive._start;
            while (s!=_drive._end) {
                auto chr = ConvertPathChar(*s++, rules);
                stream.write(&chr, 1);
            }
        }

        auto sep = rules.GetSeparator<CharType>();
        if (_beginsWithSeparator)
			stream.write(&sep, 1);

        if (!_sections.empty()) {
		    for (auto e=_sections.cbegin();;) {
			    auto*s = e->_start;
                while (s!=e->_end) {
                    auto chr = ConvertPathChar(*s++, rules);
                    stream.write(&chr, 1);
                }

                ++e;
                if (e==_sections.cend()) break;
                stream.write(&sep, 1);
		    }
        }

		if (_endsWithSeparator)
			stream.write(&sep, 1);
		return stream.str();
	}

    TC void SplitPath<CharType>::Rebuild(
        CharType dest[], size_t destCount, 
        const FilenameRules& rules) const
    {
        auto* i = dest;
        auto* iend = &dest[destCount];

        if (!_drive.Empty()) {
            auto*s = _drive._start;
            while (s!=_drive._end && i!=iend) {
                if (s >= dest && s < iend) assert(s>=i);   // check for reading&writing from the same place
                *i++ = ConvertPathChar(*s++, rules);
            }
        }

        if (_beginsWithSeparator && i < iend)
			*i++ = rules.GetSeparator<CharType>();

        if (!_sections.empty()) {
            for (auto e=_sections.cbegin();;) {
                auto*s = e->_start;
                while (s!=e->_end && i!=iend) {
                    if (s >= dest && s < iend) assert(s>=i);   // check for reading&writing from the same place
                    *i++ = ConvertPathChar(*s++, rules);
                }

                ++e;
                if (e==_sections.cend() || i == iend) break;
                *i++ = rules.GetSeparator<CharType>();
		    }
        }

        if (_endsWithSeparator && i < iend)
			*i++ = rules.GetSeparator<CharType>();

        // force null terminator, even if it causes a truncate
        *std::min(i, iend-1) = '\0';
    }
    
    TC SplitPath<CharType>::SplitPath(Section rawString)
    {
        _endsWithSeparator = false;
        _beginsWithSeparator = false;

        const CharType seps[] = { (CharType)'\\', (CharType)'/' };
        const auto* sepsEnd = ArrayEnd(seps);
		const auto* i = rawString._start;
        const auto* iend = rawString._end;

            // (not supporting "d:file.txt" type filenames currently! (ambiguity with parameters)
        auto firstSep = std::find_first_of(i, iend, seps, sepsEnd);
        auto firstColon = std::find(i, iend, ':');
        if (firstColon != iend && firstSep != iend && firstColon < firstSep) {
            _drive = Section(i, firstColon+1);
            i = firstColon+1;
        } else {
            _drive = Section(i, i);
        }

        auto* leadingSeps = FindFirstNotOf(i, iend, seps, sepsEnd);
        if (leadingSeps != i) {
            _beginsWithSeparator = true;
            i = leadingSeps;
        }

		if (i != iend) {
            for (;;) {
                auto* start = i;
                i = std::find_first_of(i, iend, seps, sepsEnd);
            
                _sections.push_back(Section(start, i));
                if (i == iend) break;

                i = FindFirstNotOf(i, iend, seps, sepsEnd);
                if (i == iend) { _endsWithSeparator = true; break; }
		    }
        }
    }

	TC SplitPath<CharType>::SplitPath(const String& input)
        : SplitPath(Section(input))
	{
	}

	TC SplitPath<CharType>::SplitPath(const CharType input[])
		: SplitPath(Section(input))
	{}

    TC SplitPath<CharType>::SplitPath() { _beginsWithSeparator = false; _endsWithSeparator = false; }

	TC SplitPath<CharType>::SplitPath(std::vector<Section>&& sections, 
        bool beginsWithSeparator, bool endsWithSeparator, 
        Section drive)
	: _sections(std::forward<std::vector<Section>>(sections))
	, _beginsWithSeparator(beginsWithSeparator)
    , _endsWithSeparator(endsWithSeparator)
    , _drive(drive)
	{}

    TC SplitPath<CharType>::~SplitPath() {}

    TC SplitPath<CharType>::SplitPath(SplitPath&& moveFrom) never_throws
    : _sections(std::move(moveFrom._sections))
    , _beginsWithSeparator(moveFrom._beginsWithSeparator)
    , _endsWithSeparator(moveFrom._endsWithSeparator)
    , _drive(moveFrom._drive)
    {
    }

    TC auto SplitPath<CharType>::operator=(SplitPath&& moveFrom) never_throws -> SplitPath&
    {
        _sections = std::move(moveFrom._sections);
        _beginsWithSeparator = moveFrom._beginsWithSeparator;
        _endsWithSeparator = moveFrom._endsWithSeparator;
        _drive = moveFrom._drive;
        return *this;
    }

	

	TC static bool PathElementMatch(StringSection<CharType> lhs, StringSection<CharType> rhs, bool isCaseSensitive)
	{
        if (isCaseSensitive) return XlEqString(lhs, rhs);
        else return XlEqStringI(lhs, rhs);
	}

	TC std::basic_string<CharType> MakeRelativePath(
        const SplitPath<CharType>& iBasePath, 
        const SplitPath<CharType>& iDestinationObject,
        const FilenameRules& rules)
	{

        ////////////////////////////////////////////////////////////////////////////////
		    // Find initial parts of "basePath" that match "destinationObject"
            // Note that if there is a drive specified for "destinationObject",
            // then it must match the drive in "basePath". If there is a drive
            // in destinationObject, but no drive in basePath (or a different drive), 
            // then we can't make a relative path.
            // Alternatively, if there is a drive in basePath, and no drive in destinationObject,
            // then we'll assume that destinationObject is on the same drive
        auto destinationObject = iDestinationObject.Simplify();
        
        if (!iDestinationObject.GetDrive().Empty()) {
            if (!XlEqStringI(iBasePath.GetDrive(), iDestinationObject.GetDrive()))
                return destinationObject.Rebuild(rules);
        }

		auto basePath = iBasePath.Simplify();

        using SectionType = SplitPath<CharType>::SectionType;

		unsigned basePrefix = 0, destinationPrefix = 0;
		unsigned baseCount = basePath.GetSectionCount();
		unsigned destinationCount = destinationObject.GetSectionCount();
		for (; basePrefix < baseCount && destinationPrefix < destinationCount;) {
			if (basePath.GetSectionType(basePrefix) == SectionType::CurrentDir) {
				++basePrefix;
				continue;
			}
			if (destinationObject.GetSectionType(destinationPrefix) == SectionType::CurrentDir) {
				++destinationPrefix;
				continue;
			}

			if (	basePath.GetSectionType(basePrefix) == SectionType::BackOne
				||	destinationObject.GetSectionType(destinationPrefix) == SectionType::BackOne) {
				break;
			}

			if (PathElementMatch(basePath.GetSection(basePrefix), destinationObject.GetSection(destinationPrefix), rules.IsCaseSensitive())) {
				++basePrefix;
				++destinationPrefix;
				continue;
			}

			break;		// paths have diverged
		}

		if (basePrefix == 0) {
			// if there's no agreement, we just have to assume that the destination path is absolute
			return destinationObject.Rebuild(rules);
		}

		// any remaining paths in "basePath" need to be translated into "../"
		signed backPaths = 0;
		auto baseIterator = basePrefix;
		for (; baseIterator < baseCount; ++baseIterator) {
			auto type = basePath.GetSectionType(baseIterator);
			if (type == SectionType::CurrentDir) {
			} else if (type == SectionType::BackOne) {
				--backPaths;
			} else {
				++backPaths;
			}
		}

        CharType dots[] = { (CharType)'.', (CharType)'.' };
        auto sep = rules.GetSeparator<CharType>();

		    // now we can just append the rest of "destinationObject"
		    //		... if backPaths is 0, we should prepend "./"
		    //		and backPaths shouldn't be less than zero if we properly simplified
		    //		the paths, and the base path is valid
            //  (note that we never need to write the drive out here)
		std::basic_stringstream<CharType> meld;
		bool pendingSlash = false;
		if (backPaths < 0) {
			assert(0);
		} else if (!backPaths) {
			// meld << "./";
		} else {
			meld.write(dots, 2);
			for (signed c = 1; c < backPaths; ++c) {
				meld.write(&sep, 1);
                meld.write(dots, 2);
            }
			pendingSlash = true;
		}

		auto destinationIterator = destinationPrefix;
		for (; destinationIterator < destinationCount; ++destinationIterator) {
			auto element = destinationObject.GetSection(destinationIterator);
			if (pendingSlash)
				meld.write(&sep, 1);
            if (rules.IsCaseSensitive()) {
			    meld.write(element._start, element._end - element._start);
            } else {
                    // convert to lower case for non-case sensitive file systems
                for (auto* i=element._start; i!=element._end; ++i) {
                    auto c = XlToLower(*i);
                    meld.write(&c, 1);
                }
            }
			pendingSlash = true;
		}

		if (destinationObject.EndsWithSeparator()) {
			if (!pendingSlash)
                meld.write(dots, 1);    // (creates "./" for empty paths)
            meld.write(&sep, 1);
		}

		return meld.str();
	}

        //  Selectively convert case to lower case on filesystems where it doesn't matter.
    char ConvertPathChar(char input, const FilenameRules& rules) { if (rules.IsCaseSensitive()) return input; return XlToLower(input); }
    utf8 ConvertPathChar(utf8 input, const FilenameRules& rules) { if (rules.IsCaseSensitive()) return input; return XlToLower(input); }
    ucs2 ConvertPathChar(ucs2 input, const FilenameRules& rules) { if (rules.IsCaseSensitive()) return input; return XlToLower(input); }

	static const uint64 s_FNV_init64 =  0xcbf29ce484222325ULL;

	static uint64 FNVHash64(const void* start, const void* end, uint64 hval = s_FNV_init64)
	{
		auto* b = (unsigned char*)start;
		auto* e = (unsigned char*)end;

		while (b<e) {
			hval ^= (uint64)*b++;
			hval += 
				(hval << 1) + (hval << 4) + (hval << 5) +
				(hval << 7) + (hval << 8) + (hval << 40);
		}
		return hval;
	}

	static uint64 FNVHash64(uint16 chr, uint64 hval)
	{
		// FNV always works with 8 bit values entering the machine
		//	-- so we have to split this value up into 2, and do it twice.
		// However, since we're using this for UTF16 filenames, the upper
		// 8 bits will very frequently be zero. Maybe it would be best to
		// actually skip the second hash part when that is the case?
		hval ^= (uint64)(chr & 0xff);
		hval +=
			(hval << 1) + (hval << 4) + (hval << 5) +
			(hval << 7) + (hval << 8) + (hval << 40);

		hval ^= (uint64)(chr >> 8);
		hval +=
			(hval << 1) + (hval << 4) + (hval << 5) +
			(hval << 7) + (hval << 8) + (hval << 40);
		return hval;
	}

	template<>
		uint64 HashFilename(StringSection<utf16> filename, const FilenameRules& rules)
	{
		// Note -- see also an interesting hashing for filenames in the linux source /fs/ folder.
		//		it does a simple 32 bit hash, but does 4 characters at a time.
		//
		// We're going to apply "tolower" to the filename, as if were ucs2. Given
		// the format of utf16, this seems like it should be ok...?
		// However, just using basic "tolower" behaviour here... it's not clear
		// if it matches the way the underlying OS ignore case exactly. It should be
		// ok for ASCII chars; but some cases may not be accounted for correctly.
		//
		// To make this function simple and efficient, we'll use the FNV-1a algorithm.
		// See details here: http://isthe.com/chongo/tech/comp/fnv/
		//
		// This method is just well suited to our needs in this case
		if (rules.IsCaseSensitive())
			return FNVHash64(filename.begin(), filename.end());

		uint64 hval = s_FNV_init64;
		for (auto i:filename)
			hval = FNVHash64((utf16)std::tolower(i), hval);		// note -- potentially non-ideal tolower usage here...?
		return hval;
	}

	template<>
		uint64 HashFilename(StringSection<utf8> filename, const FilenameRules& rules)
	{
		// Implemented so we get the same hash for utf8 and utf16 versions
		// of the same string
		uint64 hval = s_FNV_init64;
		if (rules.IsCaseSensitive()) {

			for (auto i = filename.begin(); i<filename.end();) {
				auto chr = (utf16)utf8_nextchar(i, filename.end());
				hval = FNVHash64(chr, hval);
			}

		} else {

			for (auto i = filename.begin(); i<filename.end();) {
				auto chr = (utf16)std::tolower(utf8_nextchar(i, filename.end()));
				hval = FNVHash64(chr, hval);
			}

		}
		return hval;
	}

	static inline bool IsSeparator(utf16 chr)	{ return chr == '/' || chr == '\\'; }
	static inline bool IsSeparator(utf8 chr)	{ return chr == '/' || chr == '\\'; }

	template<>
		uint64 HashFilenameAndPath(StringSection<utf16> filename, const FilenameRules& rules)
	{
		// This is a special version of HashFilenameAndPath where we assume there may be path
		// separators in the filename. Whenever we find any sequence of either type of path
		// separator, we hash a single '/'

		uint64 hval = s_FNV_init64; 
		if (rules.IsCaseSensitive()) {
			for (auto i = filename.begin(); i<filename.end();) {
				if (IsSeparator(*i)) {
					++i;
					while (i < filename.end() && IsSeparator(*i)) ++i;	// skip over additionals
					hval = FNVHash64('/', hval);
				} else {
					hval = FNVHash64(*i++, hval);
				}
			}
		} else {
			for (auto i = filename.begin(); i<filename.end();) {
				if (IsSeparator(*i)) {
					++i;
					while (i < filename.end() && IsSeparator(*i)) ++i;	// skip over additionals
					hval = FNVHash64('/', hval);
				} else {
					hval = FNVHash64((utf16)std::tolower(*i++), hval);
				}
			}
		}

		return hval;
	}

	template<>
		uint64 HashFilenameAndPath(StringSection<utf8> filename, const FilenameRules& rules)
	{
		// This is a special version of HashFilenameAndPath where we assume there may be path
		// separators in the filename. Whenever we find any sequence of either type of path
		// separator, we hash a single '/'

		uint64 hval = s_FNV_init64; 
		if (rules.IsCaseSensitive()) {
			for (auto i = filename.begin(); i<filename.end();) {
				if (IsSeparator(*i)) {
					++i;
					while (i < filename.end() && IsSeparator(*i)) ++i;	// skip over additionals
					hval = FNVHash64('/', hval);
				} else {
					hval = FNVHash64((utf16)utf8_nextchar(i, filename.end()), hval);
				}
			}
		} else {
			for (auto i = filename.begin(); i<filename.end();) {
				if (IsSeparator(*i)) {
					++i;
					while (i < filename.end() && IsSeparator(*i)) ++i;	// skip over additionals
					hval = FNVHash64('/', hval);
				} else {
					hval = FNVHash64((utf16)std::tolower(utf8_nextchar(i, filename.end())), hval);
				}
			}
		}

		return hval;
	}

    #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
        FilenameRules s_defaultFilenameRules('/', false);
    #else
        FilenameRules s_defaultFilenameRules('/', true);
    #endif

    template class FileNameSplitter<char>;
    template class FileNameSplitter<utf8>;
    template class FileNameSplitter<ucs2>;
    template class SplitPath<char>;
    template class SplitPath<utf8>;
    template class SplitPath<ucs2>;
    template std::basic_string<char> MakeRelativePath(const SplitPath<char>&, const SplitPath<char>&, const FilenameRules&);
    template std::basic_string<utf8> MakeRelativePath(const SplitPath<utf8>&, const SplitPath<utf8>&, const FilenameRules&);
    template std::basic_string<ucs2> MakeRelativePath(const SplitPath<ucs2>&, const SplitPath<ucs2>&, const FilenameRules&);

}


