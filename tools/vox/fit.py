import numpy as np, json
from scipy.optimize import minimize
import tract
tract_resp=tract.response
def fm(A):
    h=tract_resp(A,n=1024)
    H=20*np.log10(np.abs(np.fft.rfft(h,8192))+1e-9); f=np.fft.rfftfreq(8192,1/tract.FS)
    return [f[i] for i in range(1,len(H)-1) if H[i]>H[i-1] and H[i]>H[i+1] and f[i]>150][:3]
V={'u':(300,870,2240),'o':(400,800,2400),'O':(570,880,2500),'a':(730,1200,2550),
   'E':(560,1750,2550),'e':(400,2050,2600),'i':(280,2250,2950),'y':(280,1650,2150)}
def cost(p,t):
    if not(0.25<p[0]<0.95 and 0.05<p[1]<6 and 0.06<p[2]<0.5 and 0.05<p[3]<8 and -1.5<p[4]<1.5): return 1e3
    f=fm(tract.areas(p))
    if len(f)<3: return 1e3
    return sum((np.log(f[i]/t[i]))**2*w for i,w in zip(range(3),(1,1,0.3)))+0.002*(np.log(p[1]/3)**2)
res={}
for v,t in V.items():
    best=None
    for s in [(0.4,1,0.2,1,0.5),(0.75,0.5,0.15,2,-0.5),(0.6,0.5,0.2,0.5,0),(0.85,1,0.15,3,0)]:
        r=minimize(cost,s,args=(t,),method='Nelder-Mead',options={'maxiter':250,'xatol':1e-3,'fatol':1e-5})
        if best is None or r.fun<best.fun: best=r
    A=tract.areas(best.x); res[v]=[round(a,3) for a in A]
    print(v,t,[round(x) for x in fm(A)],np.round(best.x,3),round(best.fun,4),flush=True)
json.dump(res,open('vowels.json','w'))
