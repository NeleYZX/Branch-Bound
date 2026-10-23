# -*- coding: utf-8 -*-
import json,pathlib,statistics
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=pathlib.Path(__file__).parent
rows=json.loads((p/'data.json').read_text(encoding='utf-8'))['rows']
out=p/'separate_charts';out.mkdir(exist_ok=True)
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans']
plt.rcParams['axes.unicode_minus']=False
combos=sorted(set((r[2],r[3]) for r in rows));ns=sorted(set(r[1] for r in rows))
colors=['#2563eb','#e58a24','#169b82','#9b59b6']
labels=['TF={}, RDD={}'.format(*c) for c in combos]
def new():
 fig,ax=plt.subplots(figsize=(10,6));return fig,ax
def save(fig,ax,name,note):
 ax.set_axisbelow(True);ax.grid(axis='y',alpha=.18)
 fig.text(.5,.025,note,ha='center',fontsize=9,color='#555555')
 fig.tight_layout(rect=[0,.06,1,1]);fig.savefig(str(out/name),dpi=200);plt.close(fig)
def lines(metric,title,ylabel,name,log=False):
 fig,ax=new()
 for i,c in enumerate(combos):
  rr=[r for r in rows if (r[2],r[3])==c]
  ax.plot([r[1] for r in rr],[r[metric]*(100 if metric==6 else 1) for r in rr],'-o',color=colors[i],label=labels[i],ms=4)
  if metric==5:
   tt=[r for r in rr if r[7]=='TIME_LIMIT']
   ax.scatter([r[1] for r in tt],[r[5] for r in tt],marker='x',s=80,color=colors[i],zorder=4)
 if log:ax.set_yscale('log')
 ax.set_title(title);ax.set_xlabel('零件数量');ax.set_ylabel(ylabel);ax.set_xticks(ns);ax.legend(fontsize=9)
 if metric==5:ax.axhline(3600,color='#666666',ls='--',lw=1);note='× 表示超时；搜索上限 3600 秒，超时点不代表完成求解所需耗时。'
 else:note='gap 使用原始 MIP_gap；0% 表示已证明最优。'
 save(fig,ax,name,note)
lines(5,'不同参数组合的搜索耗时','runtime（秒）','01_搜索耗时_线性折线图.png')
lines(5,'不同参数组合的搜索耗时（对数纵轴）','runtime（秒，对数刻度）','02_搜索耗时_对数折线图.png',True)
lines(6,'不同参数组合的 gap','gap（%）','03_gap_折线图.png')
fig,ax=new();rates=[sum(r[7]=='OPTIMAL' for r in rows if r[1]==n)/4*100 for n in ns]
ax.bar(ns,rates,color=colors[0]);ax.set_xticks(ns);ax.set_ylim(0,112);ax.set_xlabel('零件数量');ax.set_ylabel('最优求解率（%）');ax.set_title('各规模最优求解率')
for n,v in zip(ns,rates):ax.text(n,v+2,'{:.0f}%'.format(v),ha='center',fontsize=9)
save(fig,ax,'04_最优求解率_柱状图.png','每个规模4个实例；OPTIMAL / 实例数。')
fig,ax=new();means=[statistics.mean(r[5] for r in rows if (r[2],r[3])==c) for c in combos]
ax.bar(range(4),means,color=colors);ax.set_xticks(range(4));ax.set_xticklabels(['{} / {}'.format(*c) for c in combos]);ax.set_xlabel('TF / RDD');ax.set_ylabel('平均 runtime（秒）');ax.set_title('各参数组合的平均搜索耗时')
for i,v in enumerate(means):ax.text(i,v+10,'{:.1f}'.format(v),ha='center')
save(fig,ax,'05_参数组合平均耗时_柱状图.png','每组12例；均值包含超时记录，仅描述本批实例。')
# Categorical heatmaps avoid suggesting that parameter combinations are continuous.
for metric,title,name in [(5,'搜索耗时分布（秒）','06_搜索耗时_热力图.png'),(6,'gap 分布（%）','07_gap_热力图.png')]:
 fig,ax=plt.subplots(figsize=(12,5))
 values=np.array([[next(r[metric] for r in rows if r[1]==n and (r[2],r[3])==c) for n in ns] for c in combos])
 if metric==5:
  from matplotlib.colors import LogNorm
  im=ax.imshow(values,aspect='auto',cmap='YlOrRd',norm=LogNorm(vmin=values.min(),vmax=values.max()))
 else:values=values*100;im=ax.imshow(values,aspect='auto',cmap='YlOrRd',vmin=0,vmax=100)
 ax.set_xticks(range(len(ns)));ax.set_xticklabels(ns);ax.set_yticks(range(4));ax.set_yticklabels(labels);ax.set_xlabel('零件数量');ax.set_title(title)
 for i,c in enumerate(combos):
  for j,n in enumerate(ns):
   rr=next(r for r in rows if r[1]==n and (r[2],r[3])==c)
   text=('{:.1f}'.format(values[i,j]) if metric==6 else ('{:.2f}'.format(values[i,j]) if values[i,j]<10 else '{:.0f}'.format(values[i,j])))+('*' if rr[7]=='TIME_LIMIT' else '')
   ax.text(j,i,text,ha='center',va='center',fontsize=8,color='white' if (values[i,j]>100 if metric==5 else values[i,j]>60) else '#222222')
 ax.set_ylim(3.5,-0.5)
 fig.colorbar(im,ax=ax,label='秒（颜色为对数映射）' if metric==5 else 'gap（%）')
 save(fig,ax,name,'* 表示 TIME_LIMIT；横纵轴完整覆盖全部48个实例。')
fig,ax=new();raw=json.loads((p/'data.json').read_text(encoding='utf-8'))['raw']
total=[sum(float(r['solver_call_seconds']) for r in raw if int(r['n_parts'])==n)/4 for n in ns]
search=[statistics.mean(r[5] for r in rows if r[1]==n) for n in ns]
ax.plot(ns,search,'o-',color=colors[0],label='平均搜索耗时');ax.plot(ns,total,'s-',color=colors[1],label='平均调用总耗时');ax.set_yscale('log');ax.set_xticks(ns);ax.set_xlabel('零件数量');ax.set_ylabel('耗时（秒，对数刻度）');ax.set_title('搜索耗时与调用总耗时的区别');ax.legend()
save(fig,ax,'08_两种计时口径_对比图.png','每个规模4例，包含超时记录；两字段计时范围不同，不等同于搜索速度差异。')
fig,ax=new();tt=[r for r in rows if r[7]=='TIME_LIMIT'];y=np.arange(len(tt));ub=[r[8] for r in tt];lb=[r[9] for r in tt]
ax.barh(y,ub,color='#dbe6f7',label='当前最好可行解 UB');ax.barh(y,lb,color=colors[0],label='已知下界 LB');ax.set_yticks(y);ax.set_yticklabels([r[11] for r in tt],fontsize=9);ax.invert_yaxis();ax.set_xlabel('目标值（原始单位）');ax.set_title('超时实例的上下界差距');ax.legend(loc='upper center',bbox_to_anchor=(0.5,1.02),ncol=2,fontsize=8)
for i,r in enumerate(tt):ax.text(r[8]+3,i,'gap {:.1%}'.format(r[6]),va='center',fontsize=9)
ax.set_xlim(0,max(ub)*1.3)
save(fig,ax,'09_超时实例上下界_条形图.png','浅色条从0延伸至UB，深色叠加至LB；UB为当前可行解，尚未证明最优。')
text='''# 分图查看建议

建议优先查看：

1. **02 搜索耗时对数折线图**：耗时从约0.12秒跨越至3600秒，对数轴能同时看清小规模和大规模变化。线性版本01保留真实绝对差异。
2. **06 搜索耗时热力图**：一次比较全部48个实例，快速定位困难的规模与参数组合。
3. **07 gap热力图**：最直观地呈现6个超时实例的解质量差距；比大部分为零的折线图更容易定位问题。
4. **04 最优求解率柱状图**：10–19件全部证明最优，20件50%，21件0%，体现当前3600秒搜索预算下的能力边界。
5. **09 超时实例上下界条形图**：区分“找到可行解”与“证明最优”，帮助理解大gap来自怎样的上下界差距。
6. **08 两种计时口径对比图**：搜索耗时与调用总耗时可能明显不同，报告性能时应明确采用哪种口径。

03展示各参数组合gap随规模的变化。05用于概览参数组合的平均耗时，但均值混合不同规模且包含超时，不宜单凭这一张图判断参数的因果影响。

口径：runtime=search_time_seconds，单位秒；gap=MIP_gap×100%。所有实例保留，超时点标记清楚，未将3600秒视为完成求解时间。

不优先使用饼图：难以体现规模增长和gap差异。不建议对当前数据做回归预测或显著性检验：每个规模/参数组合仅一个实例，且大规模数据有时间截断。箱线图会混合规模影响，也不如分组折线图和热力图清楚。不同实例目标值不可直接作为算法性能排名依据。
'''
(out/'图表阅读建议.md').write_text(text,encoding='utf-8')
print('Created {} PNG charts'.format(len(list(out.glob('*.png')))))

