#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct PathFileScanState {
    int entries;
    int files;
    int dirs;
    int leaves;
    int hasValueFile;
    int hasNestedDir;
} PathFileScanState;

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int probe_file_stream(const char* path)
{
    int ok = TRUE;
    size_t retSize = 0;
    str first = NULL;
    str rest = NULL;
    xfile stream = xrtOpen((str)path, FALSE, XRT_CP_UTF8);

    if ( stream == NULL ) {
        return FALSE;
    }

    ok = ok && xrtSeek(stream, 0, XRT_SEEK_SET) == 0;
    ok = ok && xrtWrite(stream, (str)"stream-start", 12) == 12;
    ok = ok && xrtSeek(stream, 0, XRT_SEEK_END) == 12;
    ok = ok && xrtSeek(stream, -6, XRT_SEEK_CUR) == 6;
    ok = ok && xrtWrite(stream, (str)"IO", 2) == 2;
    ok = ok && xrtSeek(stream, 0, XRT_SEEK_SET) == 0;

    first = xrtRead(stream, 6, &retSize);
    ok = ok && retSize == 6 && xrtStrComp(first, "stream", 0, FALSE) == 0;
    ok = ok && xrtTell(stream) == 6;

    rest = xrtRead(stream, xrtGetEOF(stream) - xrtTell(stream), &retSize);
    ok = ok && retSize == 6 && xrtStrComp(rest, "IOtart", 0, FALSE) == 0;
    ok = ok && xrtEOF(stream);
    ok = ok && xrtSeek(stream, 6, XRT_SEEK_SET) == 6;
    ok = ok && xrtSetEOF(stream);

    xrtClose(stream);
    xrtFree(first);
    xrtFree(rest);
    return ok;
}

static int scan_path_file_example(str dir, size_t dirSize, str name, size_t nameSize, str path, size_t pathSize, int isDir, ptr data, ptr param)
{
    PathFileScanState* state = (PathFileScanState*)param;

    (void)dir;
    (void)dirSize;
    (void)path;
    (void)pathSize;
    (void)data;

    if ( isDir == 2 ) {
        state->leaves++;
        state->entries++;
    } else if ( isDir == 1 ) {
        state->dirs++;
        state->entries++;
        if ( nameSize == 6 && xrtStrComp(name, "nested", nameSize, FALSE) == 0 ) {
            state->hasNestedDir = TRUE;
        }
    } else {
        state->files++;
        state->entries++;
        if ( nameSize == 9 && xrtStrComp(name, "value.txt", nameSize, FALSE) == 0 ) {
            state->hasValueFile = TRUE;
        }
    }
    return FALSE;
}

int main(void)
{
    const char* dirPath = "examples_out/example_stdlib_dir";
    const char* nestedDir = "examples_out/example_stdlib_dir/nested";
    const char* copyDir = "examples_out/example_stdlib_dir_copy";
    const char* movedDir = "examples_out/example_stdlib_dir_moved";
    const char* cleanDir = "examples_out/example_stdlib_dir_clean";
    const char* cleanNested = "examples_out/example_stdlib_dir_clean/nested";
    const char* cleanFile = "examples_out/example_stdlib_dir_clean/nested/value.txt";
    const char* nestedFile = "examples_out/example_stdlib_dir/nested/value.txt";
    const char* filePath = "examples_out/example_stdlib_file.txt";
    const char* copyPath = "examples_out/example_stdlib_file.copy.txt";
    const char* movePath = "examples_out/example_stdlib_file.move.txt";
    const char* renamePath = "examples_out/example_stdlib_file.renamed.txt";
    const char* replacePath = "examples_out/example_stdlib_file.replace.txt";

    xrtInit();

    xrtFileDelete((str)filePath);
    xrtFileDelete((str)copyPath);
    xrtFileDelete((str)movePath);
    xrtFileDelete((str)renamePath);
    xrtFileDelete((str)replacePath);
    xrtDirDelete((str)copyDir);
    xrtDirDelete((str)movedDir);
    xrtDirDelete((str)cleanDir);
    xrtDirDelete((str)nestedDir);
    xrtDirDelete((str)dirPath);
    xrtDirCreateAll((str)"examples_out");

    str sep = xrtPathSep();
    str cwd = xrtPathCwd();
    str absRelease = xrtPathAbs((str)"release", 0);
    str childPath = xrtPathJoin(2, cwd, "example_child.txt");
    str relChild = xrtPathRel(cwd, childPath);
    str homeDir = xrtPathHome();
    str tempDir = xrtPathTemp();
    str appDir = xrtPathAppDir();
    str fileNameExt = xrtPathGetNameExt((str)filePath, 0);
    str fileName = xrtPathGetName((str)filePath, 0);
    str fileExt = xrtPathGetExt((str)filePath, 0);
    str fileDir = xrtPathGetDir((str)filePath, 0);
    str normalizedPath = xrtPathNormalize((str)"examples_out/./example_stdlib_dir/../example_stdlib_file.txt", 0);
    str joinedFile = xrtPathJoin(2, "examples_out", "example_stdlib_file.txt");
    str changedExtPath = xrtPathWithExt((str)filePath, (str)"log");
    str changedExt = xrtPathGetExt(changedExtPath, 0);
    str renamedNamePath = xrtPathWithName((str)filePath, (str)"renamed.log");
    str renamedName = xrtPathGetNameExt(renamedNamePath, 0);
    str joinedNestedDir = xrtPathJoin(3, "examples_out", "example_stdlib_dir", "nested");
    str joinedNestedName = xrtPathGetNameExt(joinedNestedDir, 0);
    str randomName = xrtPathRandom((str)"tmp_", 0, (str)".txt", 0, 8);

    require_line("path",
        xrtPathExists((str)"release") &&
        !xrtPathIsAbs((str)"examples_out", 0) &&
        xrtStrByteLen(sep, 0) == 1 &&
        xrtPathIsAbs(cwd, 0) &&
        xrtPathIsAbs(absRelease, 0) &&
        xrtStrComp(relChild, "example_child.txt", 0, FALSE) == 0 &&
        xrtStrByteLen(homeDir, 0) > 0 &&
        xrtStrByteLen(tempDir, 0) > 0 &&
        xrtPathIsAbs(appDir, 0) &&
        xrtStrComp(fileNameExt, "example_stdlib_file.txt", 0, FALSE) == 0 &&
        xrtStrComp(fileName, "example_stdlib_file", 0, FALSE) == 0 &&
        xrtStrComp(fileExt, "txt", 0, FALSE) == 0 &&
        xrtStrComp(fileDir, "examples_out", 0, FALSE) == 0 &&
        xrtStrComp(normalizedPath, joinedFile, 0, FALSE) == 0 &&
        xrtStrComp(renamedName, "renamed.log", 0, FALSE) == 0 &&
        xrtStrComp(changedExt, "log", 0, FALSE) == 0 &&
        xrtStrComp(joinedNestedName, "nested", 0, FALSE) == 0 &&
        xrtStrByteLen(randomName, 0) == 16);

    bool dirCreated = xrtDirCreate((str)dirPath);
    bool nestedCreated = xrtDirCreateAll((str)nestedDir);
    bool touched = xrtFileTouch((str)filePath);
    bool written = xrtFileWriteAll((str)filePath, (str)"hello", 0, XRT_CP_UTF8);
    str firstText = xrtFileReadAll((str)filePath, XRT_CP_UTF8, NULL);
    bool appended = xrtFileAppend((str)filePath, (str)" xlang", 0, XRT_CP_UTF8);
    str fullText = xrtFileReadAll((str)filePath, XRT_CP_UTF8, NULL);
    size_t size = xrtFileGetSize((str)filePath);
    bool resized = xrtFileSetSize((str)filePath, 5);
    size_t resizedSize = xrtFileGetSize((str)filePath);
    int attr = xrtFileGetAttr((str)filePath);
    bool attrSaved = xrtFileSetAttr((str)filePath, attr);
    int64 accessTime = xrtFileGetAccessTime((str)filePath);
    int64 changeTime = xrtFileGetChangeTime((str)filePath);
    int streamOk = probe_file_stream(filePath);
    str streamText = xrtFileReadAll((str)filePath, XRT_CP_UTF8, NULL);
    size_t streamSize = xrtFileGetSize((str)filePath);
    bool atomicWritten = xrtFileWriteAllAtomic((str)filePath, (str)"atomic", 0, XRT_CP_UTF8);
    bool renamed = xrtFileRename((str)filePath, (str)renamePath, FALSE);
    bool replaceSeed = xrtFileWriteAll((str)replacePath, (str)"old", 0, XRT_CP_UTF8);
    bool replaced = xrtFileReplace((str)renamePath, (str)replacePath);
    str replacedText = xrtFileReadAll((str)replacePath, XRT_CP_UTF8, NULL);
    bool restored = xrtFileMove((str)replacePath, (str)filePath, TRUE);
    bool nestedWritten = xrtFileWriteAll((str)nestedFile, (str)"nested", 0, XRT_CP_UTF8);
    bool copied = xrtFileCopy((str)filePath, (str)copyPath, TRUE);
    bool moved = xrtFileMove((str)copyPath, (str)movePath, TRUE);
    bool fileExists = xrtFileExists((str)filePath);
    bool movedExists = xrtFileExists((str)movePath);

    require_line("file",
        touched &&
        written &&
        xrtStrComp(firstText, "hello", 0, FALSE) == 0 &&
        appended &&
        xrtStrComp(fullText, "hello xlang", 0, FALSE) == 0 &&
        size == 11 &&
        resized &&
        resizedSize == 5 &&
        attr >= 0 &&
        attrSaved &&
        xrtYear(accessTime) >= 2020 &&
        xrtYear(changeTime) >= 2020 &&
        streamOk &&
        xrtStrComp(streamText, "stream", 0, FALSE) == 0 &&
        streamSize == 6 &&
        atomicWritten &&
        renamed &&
        replaceSeed &&
        replaced &&
        xrtStrComp(replacedText, "atomic", 0, FALSE) == 0 &&
        restored &&
        copied &&
        moved &&
        fileExists &&
        movedExists);

    PathFileScanState allEntries = {0};
    PathFileScanState topEntries = {0};
    bool dirEmpty = xrtDirIsEmpty((str)dirPath);
    int64 dirTopCount = xrtDirCount((str)dirPath, FALSE, 0);
    int64 dirRecursiveCount = xrtDirCount((str)dirPath, TRUE, 0);
    int64 dirRecursiveSize = xrtDirSize((str)dirPath, TRUE);
    xrtDirScanEx((str)dirPath, TRUE, scan_path_file_example, &allEntries);
    xrtDirScanEx((str)dirPath, FALSE, scan_path_file_example, &topEntries);
    bool cleanCreated = xrtDirEnsureEmpty((str)cleanNested);
    bool cleanWritten = xrtFileWriteAll((str)cleanFile, (str)"abc", 0, XRT_CP_UTF8);
    bool cleanEmptyBefore = xrtDirIsEmpty((str)cleanDir);
    bool cleanDone = xrtDirClean((str)cleanDir);
    bool cleanEmptyAfter = xrtDirIsEmpty((str)cleanDir);

    bool dirCopied = xrtDirCopy((str)dirPath, (str)copyDir, TRUE);
    bool dirMoved = xrtDirMove((str)copyDir, (str)movedDir, TRUE);
    bool sourceCopyGone = !xrtDirExists((str)copyDir);
    bool movedDirExists = xrtDirExists((str)movedDir);

    require_line("dir",
        dirCreated &&
        nestedCreated &&
        nestedWritten &&
        !dirEmpty &&
        dirTopCount == 1 &&
        dirRecursiveCount == 2 &&
        dirRecursiveSize == 6 &&
        allEntries.entries >= 3 &&
        allEntries.files >= 1 &&
        allEntries.dirs >= 1 &&
        allEntries.leaves >= 1 &&
        allEntries.hasValueFile &&
        allEntries.hasNestedDir &&
        topEntries.dirs >= 1 &&
        topEntries.leaves >= 1 &&
        cleanCreated &&
        cleanWritten &&
        !cleanEmptyBefore &&
        cleanDone &&
        cleanEmptyAfter &&
        dirCopied &&
        dirMoved &&
        sourceCopyGone &&
        movedDirExists);

    xrtFileDelete((str)filePath);
    xrtFileDelete((str)movePath);
    xrtDirDelete((str)movedDir);
    xrtDirDelete((str)cleanDir);
    xrtDirDelete((str)nestedDir);
    xrtDirDelete((str)dirPath);

    xrtFree(sep);
    xrtFree(cwd);
    xrtFree(absRelease);
    xrtFree(childPath);
    xrtFree(relChild);
    xrtFree(homeDir);
    xrtFree(tempDir);
    xrtFree(appDir);
    xrtFree(fileNameExt);
    xrtFree(fileName);
    xrtFree(fileExt);
    xrtFree(fileDir);
    xrtFree(normalizedPath);
    xrtFree(joinedFile);
    xrtFree(changedExtPath);
    xrtFree(changedExt);
    xrtFree(renamedNamePath);
    xrtFree(renamedName);
    xrtFree(joinedNestedDir);
    xrtFree(joinedNestedName);
    xrtFree(randomName);
    xrtFree(firstText);
    xrtFree(fullText);
    xrtFree(streamText);
    xrtFree(replacedText);

    xrtUnit();
    return 0;
}
