"""Compare pre-feature observations to post-feature public bench observations."""
import hashlib,json,sys
from pathlib import Path
before_path,after_path,out_path = map(Path,sys.argv[1:])
before=[json.loads(line) for line in before_path.read_text().splitlines()]
after=[json.loads(line) for line in after_path.read_text().splitlines()]
assert len(before)==len(after)
differences=[]
count=0
def compare(x,y,path):
    global count
    if isinstance(x,dict):
        for key,value in x.items():
            if key not in y:
                differences.append({'path':path+'.'+key,'reason':'missing'})
            else: compare(value,y[key],path+'.'+key)
    elif isinstance(x,list):
        if len(x)!=len(y): differences.append({'path':path,'reason':'length'})
        else:
            for i,(a,b) in enumerate(zip(x,y)): compare(a,b,path+'['+str(i)+']')
    else:
        count+=1
        if x!=y: differences.append({'path':path,'before':x,'after':y})
for i,(x,y) in enumerate(zip(before,after)):
    assert x['case']==y['case']
    compare(x['actual'],y['actual'],x['case'])
report={'baseline_commit':'2d2c0646457181fbb176d85f0aa166d04554f257','observations':len(before),'baseline_scalar_values_compared':count,'differences':differences,'baseline_sha256':hashlib.sha256(before_path.read_bytes()).hexdigest(),'after_sha256':hashlib.sha256(after_path.read_bytes()).hexdigest()}
out_path.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(bool(differences))
