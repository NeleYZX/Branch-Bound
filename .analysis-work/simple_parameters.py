# -*- coding: utf-8 -*-
import csv,pathlib,statistics
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=pathlib.Path(__file__).parent;out=p/'simple_parameters';out.mkdir(exist_ok=True)
with (p/'final_comparison/4_四种参数组合_方法影响/对比数据.csv').open(encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
for r in rows:
 for k in ['n','TF','RDD','BB_runtime','Gurobi_runtime','BB_gap','Gurobi_gap']:r[k]=float(r[k])
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
cs=[(.3,.3),(.3,.6),(.6,.3),(.6,.6)];colors=['#2563eb','#e58a24'];names=['BB','Gurobi'];labels=['TF={}\nRDD={}'.format(*c) for c in cs]
def save(fig,name,note):
 fig.text(.5,.025,note,ha='center',fontsize=9,color='#555555');fig.tight_layout(rect=[0,.075,1,.98]);fig.savefig(str(out/(name+'.png')),dpi=220);plt.close(fig)
for metric,title,yl,name in [('gap','四种参数组合：平均最终gap','平均gap（%，越低越好）','01_平均gap_并排柱状图'),('runtime','四种参数组合：平均运行时间','平均runtime（秒，越低越短）','02_平均耗时_并排柱状图'),('count','四种参数组合：达到近优标准的实例数','实例数量（共12例，越高越多）','03_近优实例数_并排柱状图')]:
 fig,ax=plt.subplots(figsize=(11,6));x=np.arange(4);width=.34;allvals=[]
 for i,alg in enumerate(names):
  vals=[sum(r[alg+'_gap']<=.001 for r in rows if (r['TF'],r['RDD'])==c) if metric=='count' else statistics.mean(r[alg+'_'+metric] for r in rows if (r['TF'],r['RDD'])==c)*(100 if metric=='gap' else 1) for c in cs];allvals+=vals
  bars=ax.bar(x+(i-.5)*width,vals,width,color=colors[i],label=alg)
  for bar,v in zip(bars,vals):ax.annotate(('{:.2f}%'.format(v) if metric=='gap' else '{:.0f}'.format(v)),(bar.get_x()+bar.get_width()/2,v),xytext=(0,5),textcoords='offset points',ha='center',fontsize=12,fontweight='bold')
 ax.set_xticks(x);ax.set_xticklabels(labels,fontsize=12);ax.set_ylabel(yl);ax.set_title(title,fontsize=15);ax.legend(loc='upper right');ax.set_ylim(0,(14 if metric=='count' else max(allvals)*1.25));ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
 if metric=='count':ax.set_yticks(range(0,13,2))
 save(fig,name,'统一近优标准：最终gap≤0.1%，不等同于已证明精确最优。' if metric=='count' else '每组均为n=10–21的12例，包含达到时间上限的记录；两方法停止精度及计时范围可能不同。')
# Four aligned panels isolate the parameter combination while keeping scales identical.
for metric,title,yl,name in [('gap','按相同规模逐一比较最终gap','gap（%）','04_各规模gap_四组分面柱状图'),('runtime','按相同规模逐一比较运行时间','runtime（秒）','05_各规模耗时_四组分面柱状图')]:
 fig,axs=plt.subplots(2,2,figsize=(15,9),sharex=True,sharey=True);ns=sorted(set(r['n'] for r in rows));x=np.arange(len(ns));maxval=max(r[a+'_'+metric] for r in rows for a in names)*(100 if metric=='gap' else 1)
 for ax,c in zip(axs.flat,cs):
  rr=sorted([r for r in rows if (r['TF'],r['RDD'])==c],key=lambda r:r['n'])
  for i,alg in enumerate(names):
   vals=[r[alg+'_'+metric]*(100 if metric=='gap' else 1) for r in rr];ax.bar(x+(i-.5)*.38,vals,.38,label=alg,color=colors[i])
  ax.set_title('TF={}，RDD={}'.format(*c),fontsize=13);ax.set_xticks(x);ax.set_xticklabels([int(n) for n in ns]);ax.set_xlabel('零件数量');ax.set_ylabel(yl);ax.set_ylim(0,maxval*1.12);ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
 axs[0,0].legend();fig.suptitle(title,fontsize=16)
 save(fig,name,'各面板使用相同纵轴；耗时为线性轴，优先查看大规模差异。' if metric=='runtime' else '各面板使用相同纵轴；同一规模下两种方法的柱子并排，保留各自原始gap定义。')
(out/'阅读说明.md').write_text('''# 参数对比：简单柱状图版

先看01：每个参数组合只有两根柱，数值标在顶部，直接比较平均gap。
再看03：同一近优标准gap≤0.1%下，两种方法分别达到多少例，避免仅依据各自最优标记比较。
02比较平均记录耗时；04和05按相同零件数量逐一比较，四个面板采用相同纵轴。

蓝色始终为BB，橙色始终为Gurobi。平均gap和耗时越低越小，近优实例数越高越多。所有平均值均包括时间上限记录，覆盖n=10–21。记录耗时不代表相同停止精度下的完整求解时间。

数据限制仍保留：n=19、TF=0.3、RDD=0.6，双方最优目标值明显不一致；n=16、TF=0.6、RDD=0.3也有小差异。正式结论前应核对实例与模型。当前图用于描述提供的数据。
''',encoding='utf-8')
print('5 charts created')
