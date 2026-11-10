#ifndef XIDE_TEST_RENAME_IDENTIFIER_FIXTURE_H
#define XIDE_TEST_RENAME_IDENTIFIER_FIXTURE_H
static const char* identifierLibrary="\xef\xbb\xbf" "export class 型号\r\n int 😀字段\r\nend\r\nexport class Other\r\n int 😀字段\r\nend\r\nexport func compute(int 参数) as int\r\n return 参数\r\nend\r\n";
static const char* identifierEntry="\xef\xbb\xbf" "import dep\r\nfunc read(dep.型号? input) as int\r\n return 0\r\nend\r\nfunc main(...) as int\r\n string 文本=\"😀\"; auto box=dep.型号()\r\n auto other=dep.Other()\r\n box.😀字段=3\r\n return dep.compute(box.😀字段)+other.😀字段\r\nend\r\n";
static const char* identifierOther="import dep\nfunc main(...) as int\n auto box=dep.型号()\n return box.😀字段\nend\n";
#endif
