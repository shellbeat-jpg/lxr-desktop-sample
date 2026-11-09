import numpy as np
FS=44140/2  # tract rate
def areas(p,N=22):
    pos,deg,wid,lip,phx=p
    x=(np.arange(N)+0.5)/N
    A=np.full(N,3.0)
    # tongue constriction (gaussian in log area)
    A*=np.exp(np.log(max(deg,0.05)/3.0)*np.exp(-((x-pos)/wid)**2))
    # pharynx widening/narrowing (low part)
    A*=np.exp(phx*np.exp(-((x-0.15)/0.15)**2))
    # lips: last 2 sections
    A[-2:]=np.maximum(lip,0.05)
    return np.clip(A,0.05,12)
def response(A,n=4096,rg=0.75,rl=-0.85,loss=0.995):
    N=len(A)
    k=np.zeros(N); k[1:]=(A[:-1]-A[1:])/(A[:-1]+A[1:])
    R=np.zeros(N); out=np.zeros(n)
    Lin=np.zeros(N)
    for t in range(n):
        src=1.0 if t==0 else 0.0
        Rn=np.empty(N)
        l=rl*R[N-1]          # left wave entering last section from lips
        out[t]=R[N-1]*(1+rl)
        for j in range(N-1,0,-1):
            w=k[j]*(R[j-1]+l)
            Rn[j]=(R[j-1]-w)*loss
            l=(l+w)*loss
        Rn[0]=src+rg*l
        R=Rn
    return out
def formants(A,nf=3):
    h=response(A)
    H=np.abs(np.fft.rfft(h,16384)); f=np.fft.rfftfreq(16384,1/FS)
    Hd=20*np.log10(H+1e-9)
    pk=[f[i] for i in range(1,len(H)-1) if Hd[i]>Hd[i-1] and Hd[i]>Hd[i+1] and f[i]>150]
    return pk[:nf]
if __name__=='__main__':
    print(formants(np.full(22,3.0)))
