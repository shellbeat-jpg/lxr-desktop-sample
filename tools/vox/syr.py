import numpy as np, json
def rhs(x,y,a,b): return y, -a-b*x-x**3-x*x*y+x*x-x*y
def sim(a,b,h=0.02,T=400):
    x,y=0.2,0.0; xs=[]
    for i in range(int(T/h)):
        k1=rhs(x,y,a,b);k2=rhs(x+h/2*k1[0],y+h/2*k1[1],a,b);k3=rhs(x+h/2*k2[0],y+h/2*k2[1],a,b);k4=rhs(x+h*k3[0],y+h*k3[1],a,b)
        x+=h/6*(k1[0]+2*k2[0]+2*k3[0]+k4[0]); y+=h/6*(k1[1]+2*k2[1]+2*k3[1]+k4[1])
        if abs(x)>10: return None
        xs.append(x)
    xs=np.array(xs[len(xs)//2:])
    if xs.max()-xs.min()<0.05: return None
    m=(xs.max()+xs.min())/2; c=np.where((xs[:-1]<m)&(xs[1:]>=m))[0]
    if len(c)<3: return None
    return (c[-1]-c[0])/(len(c)-1)*h, xs.min(), xs.max()
As=np.linspace(0.07,0.6,8); Bs=np.linspace(0.1,1.3,8)
tab={'a':list(As),'b':list(Bs),'T':[],'lo':[],'hi':[]}
for b in Bs:
    row=[sim(a,b) for a in As]; print(round(b,2),[None if r is None else round(r[0],2) for r in row],flush=True)
    tab['T'].append([None if r is None else r[0] for r in row]); tab['lo'].append([None if r is None else r[1] for r in row]); tab['hi'].append([None if r is None else r[2] for r in row])
json.dump(tab,open('syr.json','w'))
