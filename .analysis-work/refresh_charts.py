# -*- coding: utf-8 -*-
import pathlib,csv,json,math,statistics
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize
p=pathlib.Path(__file__).resolve().parent;out=p/'refreshed_charts';out.mkdir(exist_ok=True)
names=['3_TF0.6_RDD0.6_算法效率','4_四种参数组合_方法影响'];d3=out/names[0];d4=out/names[1];d3.mkdir(exist_ok=True);d4.mkdir(exist_ok=True)
with (p.parent/'Journal article/BB_Basic/basci.csv').open(encoding='utf-8-sig') as f:bb=list(csv.DictReader(f))
g=json.loads((p/'update19/gurobi.json').read_text(encoding='utf-8'));gm={tuple(r[:3]):r for r in g[1:]};data=[]
for b in bb:
 import re
 m=re.fullmatch(r'(\d+)part_TF=([\d.]+)_RDD=([\d.]+)',b['instance_id']);n,tf,rdd=int(m[1]),float(m[2]),float(m[3]);q=gm[(n,tf,rdd)]
 data.append(dict(n=n,TF=tf,RDD=rdd,BB_runtime=float(b['search_time_seconds']),Gurobi_runtime=q[4],BB_gap=float(b['MIP_gap']),Gurobi_gap=q[5],BB_optimum=float(b['final_UB']) if b['solve_status']=='OPTIMAL' else None,Gurobi_TT=q[3],Gurobi_opt_mark=q[6] or '',Gurobi_gap_le_01pct=q[5]<=.001))
assert len(data)==48;cs=[(.3,.3),(.3,.6),(.6,.3),(.6,.6)];ns=list(range(10,22));colors=['#2563eb','#e58a24']
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
def save(fig,ax,d,name,note):
 ax.set_axisbelow(True);ax.grid(axis='y',alpha=.18);fig.text(.5,.025,note,ha='center',fontsize=8,color='#555555');fig.tight_layout(rect=[0,.07,1,.99]);fig.savefig(str(d/name),dpi=220);plt.close(fig)
for d,rr in [(d3,[r for r in data if r['TF']==r['RDD']==.6]),(d4,data)]:
 with (d/'对比数据.csv').open('w',encoding='utf-8-sig',newline='') as f:w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(rr)
rr=[r for r in data if r['TF']==r['RDD']==.6]
for metric in ['runtime','gap']:
 fig,ax=plt.subplots(figsize=(10,6))
 for i,alg in enumerate(['BB','Gurobi']):
  ax.plot(ns,[r[alg+'_'+metric]*(100 if metric=='gap' else 1) for r in rr],'-o',color=colors[i],label=alg)
  if metric=='runtime':
   cut=[r for r in rr if r[alg+'_runtime']>=3600];ax.scatter([r['n'] for r in cut],[r[alg+'_runtime'] for r in cut],marker='x',s=80,color=colors[i],zorder=5)
 if metric=='runtime':ax.set_yscale('log')
 else:ax.set_ylim(0,110)
 ax.set_xticks(ns);ax.set_xlabel('零件数量');ax.set_ylabel('runtime（秒，对数刻度）' if metric=='runtime' else '最终gap（%）');ax.set_title(('运行时间对比' if metric=='runtime' else '最终gap对比')+'（TF=0.6，RDD=0.6）');ax.legend()
 save(fig,ax,d3,metric+'_TF=0.6_RDD=0.6.png','已更新19件重算结果；两方法计时范围、停止精度及gap定义可能不同。')
def get(c,n):return next(r for r in data if (r['TF'],r['RDD'])==c and r['n']==n)
for mode,name in [('gap','01_各规模参数组合_gap差值热力图.png'),('runtime','02_各规模参数组合_耗时比热力图.png')]:
 v=np.array([[(get(c,n)['Gurobi_gap']-get(c,n)['BB_gap'])*100 if mode=='gap' else math.log2(get(c,n)['Gurobi_runtime']/get(c,n)['BB_runtime']) for n in ns] for c in cs]);bound=max(abs(v.min()),abs(v.max()),.01)
 fig,ax=plt.subplots(figsize=(13,5.5));im=ax.imshow(v,aspect='auto',cmap='RdBu',norm=Normalize(vmin=-bound,vmax=bound));ax.set_xticks(range(12));ax.set_xticklabels(ns);ax.set_yticks(range(4));ax.set_yticklabels(['TF={} / RDD={}'.format(*c) for c in cs]);ax.set_ylim(3.5,-.5);ax.set_xlabel('零件数量')
 for i,c in enumerate(cs):
  for j,n in enumerate(ns):
   r=get(c,n);flag='*' if max(r['BB_runtime'],r['Gurobi_runtime'])>=3600 else '';txt=('{:+.1f}'.format(v[i,j]) if mode=='gap' else '{:.1f}×'.format(2**v[i,j]))+flag;ax.text(j,i,txt,ha='center',va='center',fontsize=8,color='white' if abs(v[i,j])>.55*bound else '#222222')
 ax.set_title('最终gap差值：蓝色BB较小，红色Gurobi较小' if mode=='gap' else '记录耗时比：蓝色BB较短，红色Gurobi较短');fig.colorbar(im,ax=ax,label='Gurobi gap − BB gap（百分点）' if mode=='gap' else 'log2(Gurobi耗时 / BB耗时)，格内为倍数')
 save(fig,ax,d4,name,'* 至少一种方法耗时≥3600秒；已更新19件结果，耗时比不代表同精度加速比。')
for metric,name in [('runtime','03_参数组合_平均耗时折线图.png'),('gap','04_参数组合_平均gap折线图.png')]:
 fig,ax=plt.subplots(figsize=(11,6));series=[[statistics.mean(r[a+'_'+metric] for r in data if (r['TF'],r['RDD'])==c)*(100 if metric=='gap' else 1) for c in cs] for a in ['BB','Gurobi']]
 for i,alg in enumerate(['BB','Gurobi']):
  ax.plot(range(4),series[i],marker='o' if i==0 else 's',lw=2.5,ms=8,color=colors[i],label=alg)
  for j,v in enumerate(series[i]):ax.annotate(('{:.2f}%'.format(v) if metric=='gap' else '{:.1f}'.format(v)),(j,v),xytext=(0,12 if v>=series[1-i][j] else -20),textcoords='offset points',ha='center',color=colors[i],fontsize=11)
 ax.set_xticks(range(4));ax.set_xticklabels(['TF={}\nRDD={}'.format(*c) for c in cs]);ax.set_xlim(-.25,3.25);ax.set_ylim(0,max(max(s) for s in series)*1.25);ax.set_ylabel('平均runtime（秒）' if metric=='runtime' else '平均gap（%）');ax.set_title('四种参数组合的'+('平均运行时间' if metric=='runtime' else '平均最终gap'));ax.legend()
 save(fig,ax,d4,name,'每组n=10–21共12例，含超时记录；横轴为离散组合，连线仅辅助对比。已更新19件结果。')
print('6 charts, 2 CSVs generated')
for c in cs:print(c,'BB mean gap',statistics.mean(r['BB_gap'] for r in data if (r['TF'],r['RDD'])==c))
