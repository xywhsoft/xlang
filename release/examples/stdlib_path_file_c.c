#define XRT_MODULE_FILE_TEXT
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_RANDOM_TEXT_DEFAULT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct PathFileScanState {
    size_t entries;
    size_t files;
    size_t dirs;
    size_t leaves;
    int hasValueFile;
    int hasNestedDir;
} PathFileScanState;



/* 输出与 xlang 示例一致的检查结果，便于发布门禁直接比较。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 安全比较可能为空的拥有型字符串。 */
static int text_eq(const char* text, const char* expected)
{
    return (text != NULL) && (expected != NULL) &&
        (strcmp(text, expected) == 0);
}



/* 清理示例创建的目录树，不把不存在视为错误。 */
static void remove_tree_if_exists(const char* path)
{
    if ( xrtPathExists(path) ) {
        xrtDirRemoveAll(path);
    }
}



/* 覆盖文件流的定位、短读语义、EOF 和截断契约。 */
static int probe_file_stream(const char* path)
{
    char first[7] = {0};
    char rest[7] = {0};
    char extra = 0;
    size_t count = 0;
    uint64 position = 0;
    uint64 size = 0;
    int ok = 1;
    xfile stream = xrtOpen(path, XFILE_READ | XFILE_WRITE);

    if ( stream == NULL ) {
        return 0;
    }

    /* 覆盖已有内容并验证三种定位基准。 */
    ok = ok && xrtSeek(stream, 0, XSEEK_START, &position) &&
        (position == 0);
    ok = ok && xrtWriteFull(stream, "stream-start", 12, &count) &&
        (count == 12);
    ok = ok && xrtSeek(stream, 0, XSEEK_END, &position) &&
        (position == 12);
    ok = ok && xrtSeek(stream, -6, XSEEK_CURRENT, &position) &&
        (position == 6);
    ok = ok && xrtWriteFull(stream, "IO", 2, &count) &&
        (count == 2);
    ok = ok && xrtSeek(stream, 0, XSEEK_START, &position) &&
        (position == 0);

    /* 验证读取结果、共享游标和零字节 EOF。 */
    ok = ok && xrtReadFull(stream, first, 6, &count) &&
        (count == 6) && text_eq(first, "stream");
    ok = ok && xrtTell(stream, &position) && (position == 6);
    ok = ok && xrtFileSize(stream, &size) && (size == 12);
    ok = ok && xrtReadFull(stream, rest, 6, &count) &&
        (count == 6) && text_eq(rest, "IOtart");
    ok = ok && xrtRead(stream, &extra, 1, &count) && (count == 0);

    /* 截断后关闭，供后续整文件接口继续验证。 */
    ok = ok && xrtFileResize(stream, 6);
    ok = xrtClose(stream) && ok;
    return ok;
}



/* 记录目录遍历边界以及关键条目名称。 */
static xwalkcontrol scan_path_file_example(
    const xwalkentry* entry,
    ptr userData)
{
    PathFileScanState* state = (PathFileScanState*)userData;

    if ( (entry == NULL) || (state == NULL) ) {
        return XWALK_ERROR;
    }

    state->entries++;
    if ( entry->Event == XWALK_LEAVE ) {
        state->leaves++;
        return XWALK_CONTINUE;
    }

    if ( entry->Info.Type == XFILE_TYPE_DIRECTORY ) {
        state->dirs++;
        if ( xrtStrEqual(entry->Name, XRT_STR_LITERAL("nested")) ) {
            state->hasNestedDir = 1;
        }
    } else if ( entry->Info.Type == XFILE_TYPE_FILE ) {
        state->files++;
        if ( xrtStrEqual(entry->Name, XRT_STR_LITERAL("value.txt")) ) {
            state->hasValueFile = 1;
        }
    }
    return XWALK_CONTINUE;
}



/* 演示并校验 Path、File 和 Directory 的当前标准库契约。 */
int main(void)
{
    const char* dirPath = "examples_out/example_stdlib_dir";
    const char* nestedDir = "examples_out/example_stdlib_dir/nested";
    const char* copyDir = "examples_out/example_stdlib_dir_copy";
    const char* movedDir = "examples_out/example_stdlib_dir_moved";
    const char* cleanDir = "examples_out/example_stdlib_dir_clean";
    const char* cleanNested = "examples_out/example_stdlib_dir_clean/nested";
    const char* cleanFile =
        "examples_out/example_stdlib_dir_clean/nested/value.txt";
    const char* nestedFile =
        "examples_out/example_stdlib_dir/nested/value.txt";
    const char* filePath = "examples_out/example_stdlib_file.txt";
    const char* copyPath = "examples_out/example_stdlib_file.copy.txt";
    const char* movePath = "examples_out/example_stdlib_file.move.txt";
    const char* renamePath = "examples_out/example_stdlib_file.renamed.txt";
    const char* replacePath = "examples_out/example_stdlib_file.replace.txt";

    char separator[2] = {xrtPathSep(), 0};
    str cwd = NULL;
    str absRelease = NULL;
    str childPath = NULL;
    str relChild = NULL;
    str homeDir = NULL;
    str tempDir = NULL;
    str appDir = NULL;
    str fileNameExt = NULL;
    str fileName = NULL;
    str fileExt = NULL;
    str fileDir = NULL;
    str normalizedPath = NULL;
    str joinedFile = NULL;
    str changedExtPath = NULL;
    str changedExt = NULL;
    str renamedNamePath = NULL;
    str renamedName = NULL;
    str joinedDir = NULL;
    str joinedNestedDir = NULL;
    str joinedNestedName = NULL;
    str randomPart = NULL;
    str firstText = NULL;
    str fullText = NULL;
    str streamText = NULL;
    str replacedText = NULL;
    size_t textSize = 0;
    xfileinfo info;
    xwalkstats topStats;
    xwalkstats recursiveStats;
    xwalkstats walkStats;
    PathFileScanState allEntries;
    uint64 directSize = 0;
    uint64 recursiveSize = 0;
    bool dirEmpty = false;
    bool cleanEmptyBefore = true;
    bool cleanEmptyAfter = false;

    /* 只清理本示例拥有的路径。 */
    xrtFileDelete(filePath);
    xrtFileDelete(copyPath);
    xrtFileDelete(movePath);
    xrtFileDelete(renamePath);
    xrtFileDelete(replacePath);
    remove_tree_if_exists(copyDir);
    remove_tree_if_exists(movedDir);
    remove_tree_if_exists(cleanDir);
    remove_tree_if_exists(dirPath);
    xrtDirCreateAll("examples_out");

    /* 路径层直接返回拥有型结果，调用者统一释放。 */
    cwd = xrtPathCwd();
    absRelease = xrtPathAbs("release");
    childPath = xrtPathJoin(cwd, "example_child.txt");
    relChild = xrtPathRel(cwd, childPath);
    homeDir = xrtPathHome();
    tempDir = xrtPathTemp();
    appDir = xrtPathAppDir();
    fileNameExt = xrtPathName(filePath);
    fileName = xrtPathStem(filePath);
    fileExt = xrtPathExt(filePath);
    fileDir = xrtPathParent(filePath);
    normalizedPath = xrtPathClean(
        XRT_STR_LITERAL(
            "examples_out/./example_stdlib_dir/../example_stdlib_file.txt"
        ),
        XPATH_NATIVE
    );
    joinedFile = xrtPathJoin("examples_out", "example_stdlib_file.txt");
    changedExtPath = xrtPathWithExt(filePath, "log");
    changedExt = xrtPathExt(changedExtPath);
    renamedNamePath = xrtPathWithName(filePath, "renamed.log");
    renamedName = xrtPathName(renamedNamePath);
    joinedDir = xrtPathJoin("examples_out", "example_stdlib_dir");
    joinedNestedDir = xrtPathJoin(joinedDir, "nested");
    joinedNestedName = xrtPathName(joinedNestedDir);
    randomPart = xrtRandString(8);

    require_line(
        "path",
        xrtPathExists("release") &&
        !xrtPathIsAbs("examples_out") &&
        (strlen(separator) == 1) &&
        xrtPathIsAbs(cwd) &&
        xrtPathIsAbs(absRelease) &&
        text_eq(relChild, "example_child.txt") &&
        (homeDir != NULL) && (homeDir[0] != 0) &&
        (tempDir != NULL) && (tempDir[0] != 0) &&
        xrtPathIsAbs(appDir) &&
        text_eq(fileNameExt, "example_stdlib_file.txt") &&
        text_eq(fileName, "example_stdlib_file") &&
        text_eq(fileExt, ".txt") &&
        text_eq(fileDir, "examples_out") &&
        text_eq(normalizedPath, joinedFile) &&
        text_eq(renamedName, "renamed.log") &&
        text_eq(changedExt, ".log") &&
        text_eq(joinedNestedName, "nested") &&
        (randomPart != NULL) && (strlen(randomPart) == 8)
    );

    /* 整文件、文本、元数据与流式接口共享同一套所有权规则。 */
    require_line(
        "file",
        xrtFileTouch(filePath) &&
        xrtFileWriteText(
            filePath,
            XRT_STR_LITERAL("hello"),
            XENCODING_UTF8,
            XUTF_STRICT,
            false
        ) &&
        ((firstText = xrtFileReadText(
            filePath,
            XENCODING_UTF8,
            XUTF_STRICT,
            &textSize
        )) != NULL) &&
        (textSize == 5) && text_eq(firstText, "hello") &&
        xrtFileAppend(filePath, XRT_BYTES_LITERAL(" xlang")) &&
        ((fullText = xrtFileReadText(
            filePath,
            XENCODING_UTF8,
            XUTF_STRICT,
            &textSize
        )) != NULL) &&
        (textSize == 11) && text_eq(fullText, "hello xlang") &&
        xrtFileSetSize(filePath, 5) &&
        xrtPathStat(filePath, true, &info) &&
        (info.Type == XFILE_TYPE_FILE) &&
        ((info.Available & XFILE_INFO_SIZE) != 0) &&
        (info.Size == 5) &&
        ((info.Available & XFILE_INFO_MODIFY_TIME) != 0) &&
        (info.Modified > 0) &&
        probe_file_stream(filePath) &&
        ((streamText = xrtFileReadText(
            filePath,
            XENCODING_UTF8,
            XUTF_STRICT,
            &textSize
        )) != NULL) &&
        (textSize == 6) && text_eq(streamText, "stream") &&
        xrtFileWriteTextAtomic(
            filePath,
            XRT_STR_LITERAL("atomic"),
            XENCODING_UTF8,
            XUTF_STRICT,
            false
        ) &&
        xrtPathRename(filePath, renamePath, false) &&
        xrtFileWriteText(
            replacePath,
            XRT_STR_LITERAL("old"),
            XENCODING_UTF8,
            XUTF_STRICT,
            false
        ) &&
        xrtPathRename(renamePath, replacePath, true) &&
        ((replacedText = xrtFileReadText(
            replacePath,
            XENCODING_UTF8,
            XUTF_STRICT,
            &textSize
        )) != NULL) &&
        (textSize == 6) && text_eq(replacedText, "atomic") &&
        xrtFileMove(replacePath, filePath, true) &&
        xrtFileCopy(filePath, copyPath, true) &&
        xrtFileMove(copyPath, movePath, true) &&
        xrtFileExists(filePath) &&
        xrtFileExists(movePath)
    );

    /* 目录层覆盖创建、遍历、统计、复制、移动、清空和递归删除。 */
    memset(&allEntries, 0, sizeof(allEntries));
    require_line(
        "dir",
        xrtDirCreate(dirPath) &&
        xrtDirCreateAll(nestedDir) &&
        xrtFileWriteText(
            nestedFile,
            XRT_STR_LITERAL("nested"),
            XENCODING_UTF8,
            XUTF_STRICT,
            false
        ) &&
        xrtDirEmpty(dirPath, &dirEmpty) && !dirEmpty &&
        xrtDirStats(dirPath, false, &topStats) &&
        xrtDirStats(dirPath, true, &recursiveStats) &&
        xrtDirSize(dirPath, false, &directSize) &&
        xrtDirSize(dirPath, true, &recursiveSize) &&
        (topStats.Directories >= 2) && (topStats.Files == 0) &&
        (recursiveStats.Directories >= 2) &&
        (recursiveStats.Files == 1) &&
        (recursiveStats.Bytes == 6) &&
        (directSize == 0) && (recursiveSize == 6) &&
        xrtFileWalk(
            dirPath,
            NULL,
            scan_path_file_example,
            &allEntries,
            &walkStats
        ) &&
        (walkStats.Directories >= 2) &&
        (walkStats.Files == 1) &&
        (allEntries.entries >= 5) &&
        (allEntries.files == 1) &&
        (allEntries.dirs >= 2) &&
        (allEntries.leaves >= 2) &&
        allEntries.hasValueFile &&
        allEntries.hasNestedDir &&
        xrtDirEnsureEmpty(cleanNested) &&
        xrtFileWriteAll(cleanFile, XRT_BYTES_LITERAL("abc")) &&
        xrtDirEmpty(cleanDir, &cleanEmptyBefore) && !cleanEmptyBefore &&
        xrtDirClean(cleanDir) &&
        xrtDirEmpty(cleanDir, &cleanEmptyAfter) && cleanEmptyAfter &&
        xrtDirCopy(dirPath, copyDir, true) &&
        xrtDirMove(copyDir, movedDir, true) &&
        !xrtDirExists(copyDir) &&
        xrtDirExists(movedDir)
    );

    /* 释放拥有型结果并移除示例产物。 */
    xrtFileDelete(filePath);
    xrtFileDelete(movePath);
    remove_tree_if_exists(movedDir);
    remove_tree_if_exists(cleanDir);
    remove_tree_if_exists(dirPath);

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
    xrtFree(joinedDir);
    xrtFree(joinedNestedDir);
    xrtFree(joinedNestedName);
    xrtFree(randomPart);
    xrtFree(firstText);
    xrtFree(fullText);
    xrtFree(streamText);
    xrtFree(replacedText);
    return 0;
}
