"""Build GUI distribution separately; preserve normal local executables."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess,sys
PORT=Path(__file__).resolve().parents[1]
ROOT=PORT.parents[1]
OUT=PORT/'build-distribution'
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(command,log):
    print(' '.join(map(str,command)),flush=True)
    with log.open('w',encoding='utf-8') as f:
        subprocess.run(list(map(str,command)),stdout=f,stderr=subprocess.STDOUT,check=True)
def main():
    OUT.mkdir(exist_ok=True)
    names=['CodeVeronicaPort.exe','CodeVeronicaPort_JP.exe','CodeVeronicaPort_US.exe','cv_launcher.ini']
    kept={n:sha(PORT/'build-release/bin'/n) for n in names}
    results={}
    # eu/us/jp: the normal AVX2 executables. eu-sse4/us-sse4/jp-sse4: the
    # compatibility executables for processors without AVX2 (CV_CPU_COMPAT),
    # which reuse the objects of build-release*-sse4 (build-release.cmd with
    # CV_BUILD_COMPAT=1), never the AVX2 ones.
    every={'eu','us','jp','eu-sse4','us-sse4','jp-sse4'}
    # *-sse4 only on request: CV_CPU_UNIFIED executables already run without AVX2.
    selected=set(sys.argv[1:]) or {'eu','us','jp'}
    if not selected <= every:raise ValueError('Regions must be eu, us, jp, eu-sse4, us-sse4 or jp-sse4')
    variants=[(r,s,c,compat) for compat in (False,True)
              for r,s,c in [('JP','_jp','build-release-jp'),('EU','','build-release'),('US','_us','build-release-us')]]
    for region,suffix,cache,compat in variants:
        key=region.lower()+('-sse4' if compat else '')
        if key not in selected:continue
        build=OUT/key;build.mkdir(exist_ok=True)
        old=PORT/(cache+('-sse4' if compat else ''))
        if not old.is_dir():raise RuntimeError(f'Missing {old.name}: run build-release.cmd'+(' with CV_BUILD_COMPAT=1' if compat else ''))
        objects=[];provenance=[]
        # Rebuild host/runtime/UI, but reuse only source-verified lifted units.
        for source in sorted((ROOT/('src/recomp'+suffix)).glob('ppu_recomp_*.cpp')):
            fresh=build/'verified-lift'/source.name;fresh.parent.mkdir(exist_ok=True)
            run([sys.executable,ROOT/'tools/cv_appearance_hooks.py','--input',source,'--output',fresh,'--region',region.lower()],build/f'verify-{source.stem}.log')
            saved=old/'native_menu_lift'/source.name
            obj=old/'CMakeFiles/MyGameRecomp.dir/native_menu_lift'/(source.name+'.obj')
            if not saved.is_file() or sha(saved)!=sha(fresh):
                objects.append(fresh)
                provenance.append(dict(source=str(source),generated_sha256=sha(fresh),compiled_fresh=True))
                continue
            if not obj.is_file() or obj.stat().st_mtime<saved.stat().st_mtime:raise RuntimeError(f'Stale lifted object: {obj}')
            objects.append(obj);provenance.append(dict(source=str(source),generated_sha256=sha(fresh),object=str(obj),object_sha256=sha(obj)))
        for source in sorted((PORT/'spu').glob('*/spu_recomp.c')):
            obj=old/'CMakeFiles/MyGameRecomp.dir/spu'/source.parent.name/'spu_recomp.c.obj'
            if not obj.is_file() or obj.stat().st_mtime<source.stat().st_mtime:raise RuntimeError(f'Stale SPU object: {obj}')
            objects.append(obj);provenance.append(dict(source=str(source),source_sha256=sha(source),object=str(obj),object_sha256=sha(obj)))
        copied=[]
        for index,obj in enumerate(objects):
            if obj.suffix=='.cpp':
                copied.append(obj.as_posix());continue
            dst=build/'lifted-objects'/f'{index:03}.obj';dst.parent.mkdir(exist_ok=True)
            shutil.copy2(obj,dst);copied.append(dst.as_posix())
        (build/'reuse.cmake').write_text('set(CV_LIFTED_OBJECTS\n'+''.join(f' "{p}"\n' for p in copied)+')\n',encoding='utf-8')
        (build/'lift-provenance.json').write_text(json.dumps(provenance,indent=2),encoding='utf-8')
        run(['cmake','-S',PORT/'distribution','-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DPython3_EXECUTABLE='+sys.executable,
             '-DCMAKE_C_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe','-DCMAKE_CXX_COMPILER=C:/Program Files/LLVM/bin/clang-cl.exe',
             '-DCMAKE_LINKER=C:/Program Files/LLVM/bin/lld-link.exe','-DCMAKE_C_FLAGS_RELEASE=/O2 /Ob2',
             '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2',f'-DCV_REGION={region}',f'-DRECOMP_DIR={(ROOT/("src/recomp"+suffix)).as_posix()}',
             f'-DSPU_DIR={(PORT/"spu").as_posix()}','-DCV_CPU_COMPAT='+('ON' if compat else 'OFF')],build/'configure.log')
        run(['cmake','--build',build,'--target','MyGameRecomp','CodeVeronicaLauncher','-j','2'],build/'build.log')
        exe=OUT/'bin'/('CodeVeronicaPort'+('' if region=='EU' else '_'+region)+('_SSE4' if compat else '')+'.exe')
        results[exe.name]=sha(exe)
        print(f'DONE {key}: {exe}',flush=True)
    results['Code Veronica.exe']=sha(OUT/'Code Veronica.exe')
    if kept!={n:sha(PORT/'build-release/bin'/n) for n in names}:raise RuntimeError('Local normal files changed')
    result_file=OUT/('build-result-'+next(iter(selected))+'.json' if len(selected)==1 else 'build-result.json')
    if result_file.name=='build-result.json' and result_file.is_file():
        # Building some variants later (e.g. only the *-sse4 ones) keeps the
        # digests of the executables built before, as long as they are unchanged.
        previous=json.loads(result_file.read_text(encoding='utf-8')).get('executables',{})
        for name,digest in previous.items():
            if name not in results and (OUT/'bin'/name).is_file() and sha(OUT/'bin'/name)==digest:results[name]=digest
    result_file.write_text(json.dumps(dict(distribution=True,version=re.search(r'k_launcher_version\s*=\s*L"([^"]+)"',(PORT/'launcher.cpp').read_text(encoding='utf-8'))[1],executables=results,normal_preserved=kept),indent=2),encoding='utf-8')
    print('PASS: distribution builds completed ('+', '.join(sorted(selected))+'); normal local files unchanged',flush=True)
if __name__=='__main__':main()
