"""Deterministic, network-free owning-frame/entry tests for shared async CFG."""
from pathlib import Path

PRELUDE='''int live=0
string trace=""
class HeldOwner
 int id=0
 create(int value)
  self.id=value
  live += 1
 end
 destroy()
  trace += string(self.id)+","
  live -= 1
 end
end
async func ready(int value) as int
 return value
end
'''


def scale_source(count):
    source=PRELUDE+'async func holdOwners() as int\n'
    for index in range(count):
        source+=(' auto owner'+str(index)+'=HeldOwner('+str(index)+')\n'
                 ' assert await ready('+str(index)+') == '+str(index)+'\n'
                 ' assert live == '+str(index+1)+'\n')
    source+=' return '+str(count)+'\nend\nassert await holdOwners() == '+str(count)+'\nassert live == 0\n'
    source+='assert trace == "'+''.join(str(q)+',' for q in reversed(range(count)))+'"\n'
    return source+'println("shared async owners '+str(count)+': ok")\n'


def fixtures(out):
    out=Path(out);cases=[]
    for count in [4,8,16,32]:
        target=out/('shared_async_owners_'+str(count)+'.xl');target.write_text(scale_source(count),encoding='utf8');cases.append(target)
    target=out/'shared_async_owner_exits.xl'
    target.write_text(PRELUDE+'''async func returnOwner() as HeldOwner
 auto first=HeldOwner(300)
 assert await ready(1) == 1
 auto second=HeldOwner(301)
 assert await ready(2) == 2
 return first
end
async func checkReturn()
 auto survivor=await returnOwner()
 assert survivor.id == 300 and live == 1
 assert trace == "301,"
end
await checkReturn()
assert live == 0 and trace == "301,300,"
trace=""
async func failOwners()
 auto outer=HeldOwner(100)
 try
  auto inner=HeldOwner(101)
  assert await ready(4) == 4
  throw "expected owning failure"
 finally
  assert await ready(5) == 5
  trace += "F,"
 end
end
int caught=0
try
 await failOwners()
catch failure
 assert failure.message == "expected owning failure"
 caught += 1
end
assert caught == 1 and live == 0
assert trace == "101,F,100,"
println("shared async owning exits: ok")
''',encoding='utf8');cases.append(target)
    return cases
