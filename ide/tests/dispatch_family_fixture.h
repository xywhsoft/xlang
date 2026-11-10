#ifndef XIDE_DISPATCH_FAMILY_FIXTURE_H
#define XIDE_DISPATCH_FAMILY_FIXTURE_H
static const char* dispatchFamilyText=
 "protocol Reader\n func read() as int\nend\n"
 "protocol AlsoReader\n func read() as int\nend\n"
 "class Base\n virtual func read() as int\n return 1\n end\nend\n"
 "class Derived extends Base\n override func read() as int\n return 2\n end\nend\n"
 "class Leaf extends Derived\nend\n"
 "class Plain\n func read() as int\n return 3\n end\nend\n"
 "class Wrong\n func read(int value) as int\n return value\n end\nend\n"
 "func through(Reader value) as int\n return value.read()\nend\n"
 "func other(AlsoReader value) as int\n return value.read()\nend\n"
 "func main(...) as int\n auto value=Derived()\n return value.read()+through(value)+other(Plain())+Wrong().read(7)\nend\n";
#endif
