const path=require('path');

/* XRT tools/build.py names each response file output.with_suffix('.exe.rsp')
 * and logs it relative to ROOT. The response is deleted after compilation.
 * Python's concurrent multi-argument print can interleave [build] prefixes,
 * command text and newlines. Resolve output tokens, not physical log lines;
 * the caller must require an exact unique manifest and check binary hashes.
 * Resolve that documented mapping as well as ordinary -o command lines;
 * never discover an unrelated/stale executable by recursively guessing names. */
function nativeBuildOutputs(log,vendor){
 const root=path.resolve(vendor),out=path.join(root,'out'),outputs=[];
 const tokens=[...log.matchAll(/ @([^\r\n]+?\.exe\.rsp) arguments=\d+/g)].map(m=>({Output:m[1].slice(0,-4),ViaResponse:true}));
 tokens.push(...[...log.matchAll(/ -o (?:"([^"\r\n]+?\.exe)"|([^\r\n]+?\.exe))/g)].map(m=>({Output:m[1]||m[2],ViaResponse:false})));
 for(const token of tokens){
  const candidate=path.resolve(root,token.Output);
  const relative=path.relative(out,candidate);
  if(!relative||relative.startsWith('..'+path.sep)||relative==='..'||path.isAbsolute(relative))throw Error('Build output outside candidate '+candidate);
  outputs.push({Path:candidate,ViaResponse:token.ViaResponse});
 }
 return outputs;
}
module.exports={nativeBuildOutputs};
if(require.main===module){
 const assert=require('assert'),root=path.resolve('test-build-root');
 const binary=path.join(root,'out','gcc','case.exe'),rsp=path.join('out','tcc','case.exe.rsp');
 const result=nativeBuildOutputs(`[build] gcc -std=c11 case.c -o ${binary}\n[build] tcc @${rsp} arguments=354\n[build] gcc -c case.c -o ${path.join(root,'out','case.o')}\n`,root);
 assert.deepStrictEqual(result,[{Path:path.join(root,'out','tcc','case.exe'),ViaResponse:true},{Path:binary,ViaResponse:false}]);
 const other=path.join(root,'out','gcc','other.exe');
 assert.deepStrictEqual(nativeBuildOutputs(`[build][build]  tcc -o ${binary}tcc -o ${other}\n`,root),
  [{Path:binary,ViaResponse:false},{Path:other,ViaResponse:false}]);
 assert.deepStrictEqual(nativeBuildOutputs(`tcc -o ${binary}\n`,root),[{Path:binary,ViaResponse:false}]);
 assert.deepStrictEqual(nativeBuildOutputs(`[build][build] tcc @${rsp} arguments=354gcc -o ${binary}\n`,root),
  [{Path:path.join(root,'out','tcc','case.exe'),ViaResponse:true},{Path:binary,ViaResponse:false}]);
 assert.deepStrictEqual(nativeBuildOutputs(`[build] gcc -o "${binary}"\n`,root),[{Path:binary,ViaResponse:false}]);
 assert.throws(()=>nativeBuildOutputs('[build] gcc @../outside.exe.rsp arguments=300',root));
 assert.throws(()=>nativeBuildOutputs('[build] gcc -o ../outside.exe',root));
 console.log('Native build output mapping: direct, response, interleaved prefixes/commands/newlines, quoted output, object exclusion and containment passed');
}
