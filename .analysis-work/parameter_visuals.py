# -*- coding: utf-8 -*-
import csv,pathlib,math,statistics
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize
p=pathlib.Path(__file__).parent;out=p/'parameter_redesign';out.mkdir(exist_ok=True)
with (p/'final_comparison/4_四种参数组合_方法影响/对比数据.csv').open(encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
for r in rows:
 for k in ['n','TF','RDD','BB_runtime','Gurobi_runtime','BB_gap','Gurobi_gap']:r[k]=float(r[k])
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
cs=[(.3,.3),(.3,.6),(.6,.3),(.6,.6)];ns=sorted(set(r['n'] for r in rows))
def get(c,n):return next(r for r in rows if (r['TF'],r['RDD'])==c and r['n']==n)
def save(fig,name,note):
 fig.text(.5,.025,note,ha='center',fontsize=9,color='#555555');fig.tight_layout(rect=[0,.075,1,.98]);fig.savefig(str(out/(name+'.png')),dpi=220);plt.close(fig)
for mode in ['gap','time']:
 fig,ax=plt.subplots(figsize=(13,5.5))
 v=np.array([[(get(c,n)['Gurobi_gap']-get(c,n)['BB_gap'])*100 if mode=='gap' else math.log2(get(c,n)['Gurobi_runtime']/get(c,n)['BB_runtime']) for n in ns] for c in cs])
 bound=max(abs(v.min()),abs(v.max()),.01);im=ax.imshow(v,aspect='auto',cmap='RdBu',norm=Normalize(vmin=-bound,vmax=bound))
 ax.set_xticks(range(len(ns)));ax.set_xticklabels([int(n) for n in ns]);ax.set_yticks(range(4));ax.set_yticklabels(['TF={} / RDD={}'.format(*c) for c in cs]);ax.set_ylim(3.5,-.5);ax.set_xlabel('零件数量')
 for i,c in enumerate(cs):
  for j,n in enumerate(ns):
   r=get(c,n);flag='*' if r['BB_runtime']>=3600 or r['Gurobi_runtime']>=3600 else ''
   txt=('{:+.1f}'.format(v[i,j]) if mode=='gap' else '{:.1f}×'.format(2**v[i,j]))+flag
   ax.text(j,i,txt,ha='center',va='center',fontsize=8,color='white' if abs(v[i,j])>bound*.55 else '#222222')
 if mode=='gap':
  ax.set_title('最终gap差值：蓝色BB较小，红色Gurobi较小');fig.colorbar(im,ax=ax,label='Gurobi gap − BB gap（百分点）');name='01_各规模参数组合_gap差值热力图'
 else:
  ax.set_title('记录耗时比：蓝色BB较短，红色Gurobi较短');cb=fig.colorbar(im,ax=ax);cb.set_label('颜色为 log2(Gurobi耗时 / BB耗时)；格内为原始倍数');name='02_各规模参数组合_耗时比热力图'
 save(fig,name,'* 至少一种方法记录耗时≥3600秒；耗时比不代表同精度求解加速比。gap保留各自原始定义。')
# Small multiple interaction plot with identical y scales for a fair comparison.
fig,axs=plt.subplots(1,2,figsize=(12,5.5),sharey=True)
for ax,alg in zip(axs,['BB','Gurobi']):
 for rdd,col in [(.3,'#2563eb'),(.6,'#e58a24')]:
  vals=[statistics.mean(r[alg+'_gap'] for r in rows if r['TF']==tf and r['RDD']==rdd)*100 for tf in [.3,.6]]
  ax.plot([.3,.6],vals,'-o',lw=2.5,ms=8,color=col,label='RDD={}'.format(rdd))
  for x,y in zip([.3,.6],vals):ax.annotate('{:.2f}%'.format(y),(x,y),xytext=(0,(12 if ((alg=='BB' and rdd==.6) or (alg=='Gurobi' and rdd==.3)) else -20)),textcoords='offset points',ha='center',fontsize=10,color=col)
 ax.set_title(alg);ax.set_xticks([.3,.6]);ax.set_xlim(.25,.65);ax.set_ylim(0,23);ax.set_xlabel('TF');ax.grid(axis='y',alpha=.2);ax.legend(loc='upper center')
axs[0].set_ylabel('平均最终gap（%）');fig.suptitle('TF 与 RDD 对两种方法的影响（相同纵轴）',fontsize=14)
save(fig,'03_TF_RDD交互效应图','每个点汇总n=10–21共12例；包含未最优记录。折线仅连接两个实验水平。')
fig,ax=plt.subplots(figsize=(10,5.5));y=np.arange(4)
b=[statistics.mean(r['BB_gap'] for r in rows if (r['TF'],r['RDD'])==c)*100 for c in cs];g=[statistics.mean(r['Gurobi_gap'] for r in rows if (r['TF'],r['RDD'])==c)*100 for c in cs]
for i,(bv,gv) in enumerate(zip(b,g)):ax.plot([bv,gv],[i,i],color='#bbc3cc',lw=4)
ax.scatter(b,y,s=100,color='#2563eb',label='BB',zorder=3);ax.scatter(g,y,s=100,color='#e58a24',label='Gurobi',zorder=3)
for i,(bv,gv) in enumerate(zip(b,g)):
 ax.annotate('{:.2f}%'.format(bv),(bv,i),xytext=(0,12),textcoords='offset points',ha='center',color='#2563eb')
 ax.annotate('{:.2f}%'.format(gv),(gv,i),xytext=(0,-20),textcoords='offset points',ha='center',color='#b86600')
ax.set_yticks(y);ax.set_yticklabels(['TF={} / RDD={}'.format(*c) for c in cs]);ax.set_ylim(3.6,-.6);ax.set_xlim(0,23);ax.set_xlabel('平均最终gap（%，越靠左越小）');ax.set_title('同一参数组合下，两种方法的最终gap差距');ax.grid(axis='x',alpha=.2);ax.legend(loc='upper right')
save(fig,'04_参数组合_gap哑铃图','每组12例；横线长度表示平均gap差距，不能代表运行时间或统计显著性。')
text='''# 参数分析新版图表

推荐先看01和04。01保留每个规模，直接定位哪种参数组合、哪种规模下哪种方法gap较小；04用哑铃图直接比较每组平均gap差距。03专门查看TF改变时，RDD不同是否导致变化方向或幅度不同。02查看记录耗时的相对差异，比两条耗时折线更容易比较。

01：Gurobi gap − BB gap，单位为百分点；正值蓝色代表BB报告gap较小，负值红色代表Gurobi报告gap较小。
02：格内数字为Gurobi runtime / BB runtime，>1表示BB记录耗时较短；颜色使用以1为中性的对数映射。两算法计时范围、停止精度可能不同，因此不能称作同精度求解加速比。

本批数据的平均gap：TF=0.3、RDD=0.6时Gurobi较小；其余三个组合BB较小。TF由0.3升到0.6时，BB的平均gap在两种RDD下均下降；Gurobi在RDD=0.3时变化较小，在RDD=0.6时明显上升。以上仅描述该批实例，不作因果推断。

数据核对限制：n=19、TF=0.3、RDD=0.6，双方报告最优目标值分别约291.741与103.908，存在明显不一致；n=16、TF=0.6、RDD=0.3也有较小目标值差异。所有图保留提供的数据，不擅自修正。正式结论前需要核对实例及模型。两方gap也沿用各自定义。

每个规模/参数组合仅一个实例，各组合平均值均覆盖n=10–21的12例。*表示至少一种方法的记录耗时达到3600秒。未添加α参数说明。
'''
(out/'阅读说明.md').write_text(text,encoding='utf-8');print('4 charts created')


