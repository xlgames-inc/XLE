// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PathUtils.h"

namespace Utility
{

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

template <class T>
void concat_path(T* dst, int count, const T* a, const T* b)
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
        XlCatString(tmp, dimof(tmp), b);
    }
    // XlNormalizePath(dst, count, tmp);        DavidJ -- this has a problem with network paths. \\NetworkServer\path\... -> /NetworkServer/path/... so skip this step (caller can always normalize afterwards)
    XlCopyString(dst, count, tmp);

    if (!dst[0]) {
        static const T dot[2] = { '.', '\0' };
        XlCopyString(dst, count, dot);
    }
}

void XlConcatPath(char* dst, int count, const char* a, const char* b)
{
    concat_path<char>(dst, count, a, b);
}

void XlConcatPath(ucs2* dst, int count, const ucs2* a, const ucs2* b)
{
    concat_path<ucs2>(dst, count, a, b);
}

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
    XlConcatPath(dst, count, tmp, rel);
}

const char* XlExtension(const char* path)
{
    auto len = XlStringLen(path);
    if (!len) return nullptr;
    const char* p = &path[len-1];
    while (p >= path) {
        if (*p == '\\' || *p == '/') return nullptr;
        if (*p == '.') return p + 1;
        --p;
    }
    return nullptr;
}

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

}


