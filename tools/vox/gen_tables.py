import json
v=json.load(open('vowels.json')); s=json.load(open('syr.json'))
order=['u','o','O','a','E','e','i','y']
o=["static const float voxCore_vowels[VOX_VOWELS][VOX_REFN] =\n{"]
for k in order: o.append("\t{ "+", ".join("%.3ff"%a for a in v[k])+" },\t/* %s */"%k)
o.append("};")
T=s['T'];lo=s['lo'];hi=s['hi']
for g,f in ((T,1.15),(lo,1),(hi,1)):
    for row in g:
        for ai in range(len(row)-1,-1,-1):
            if row[ai] is None: row[ai]=row[ai+1]*f
for name,g,fmt in (("syrT",T,"%.2ff"),("syrLo",lo,"%.3ff"),("syrHi",hi,"%.3ff")):
    o.append("static const float voxCore_%s[VOX_SYR][VOX_SYR] =\n{"%name)
    for row in g: o.append("\t{ "+", ".join(fmt%x for x in row)+" },")
    o.append("};")
open('tables.inc','w').write("\n".join(o)+"\n")
