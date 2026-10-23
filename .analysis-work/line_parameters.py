# -*- coding: utf-8 -*-
import csv,pathlib,statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=pathlib.Path(__file__).parent;out=p/'line_parameters';out.mkdir(exist_ok=True)
with (p/'final_comparison/4_四种参数组合_方法影响/对比数据.csv').open(encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
for r in rows:
 for k in ['n','TF','RDD','BB_runtime','Gurobi_runtime','BB_gap','Gurobi_gap']:r[k]=float(r[k])
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
cs=[(.3,.3),(.3,.6),(.6,.3),(.6,.6)];colors=['#2563eb','#e58a24'];labels=['TF={}\nRDD={}'.format(*c) for c in cs]
def save(fig,ax,name,note):
 ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True);ax.legend();fig.text(.5,.025,note,ha='center',fontsize=8,color='#555555');fig.tight_layout(rect=[0,.075,1,.98]);fig.savefig(str(out/(name+'.png')),dpi=220);plt.close(fig)
for metric,title,yl,name in [('gap','四种参数组合的平均最终gap','平均gap（%）','01_参数组合_平均gap折线图'),('runtime','四种参数组合的平均运行时间','平均runtime（秒）','02_参数组合_平均耗时折线图'),('count','四种参数组合的近优实例数','实例数（共12例）','03_参数组合_近优实例数折线图')]:
 fig,ax=plt.subplots(figsize=(11,6));series=[]
 for alg in ['BB','Gurobi']:
  series.append([sum(r[alg+'_gap']<=.001 for r in rows if (r['TF'],r['RDD'])==c) if metric=='count' else statistics.mean(r[alg+'_'+metric] for r in rows if (r['TF'],r['RDD'])==c)*(100 if metric=='gap' else 1) for c in cs])
 for i,alg in enumerate(['BB','Gurobi']):
  vals=series[i];ax.plot(range(4),vals,marker='o' if i==0 else 's',lw=2.5,ms=8,color=colors[i],label=alg)
  for j,v in enumerate(vals):
   offset=12 if v>=series[1-i][j] else -20
   if v==series[1-i][j]:offset=12 if i==0 else -20
   ax.annotate('{:.2f}%'.format(v) if metric=='gap' else ('{:.1f}'.format(v) if metric=='runtime' else str(v)),(j,v),xytext=(0,offset),textcoords='offset points',ha='center',color=colors[i],fontsize=11)
 ax.set_xticks(range(4));ax.set_xticklabels(labels);ax.set_xlim(-.25,3.25);ax.set_ylim(0,14 if metric=='count' else max(max(s) for s in series)*1.25);ax.set_ylabel(yl);ax.set_title(title)
 save(fig,ax,name,'每组n=10–21共12例；横轴为离散参数组合，连线仅辅助比较，不表示连续变化。'+('近优标准gap≤0.1%。' if metric=='count' else ''))
for c in cs:
 rr=sorted([r for r in rows if (r['TF'],r['RDD'])==c],key=lambda r:r['n'])
 for metric in ['gap','runtime']:
  fig,ax=plt.subplots(figsize=(10,6))
  for i,alg in enumerate(['BB','Gurobi']):ax.plot([r['n'] for r in rr],[r[alg+'_'+metric]*(100 if metric=='gap' else 1) for r in rr],'-o',color=colors[i],label=alg)
  if metric=='runtime':ax.set_yscale('log')
  else:ax.set_ylim(0,110)
  ax.set_xticks([r['n'] for r in rr]);ax.set_xlabel('零件数量');ax.set_ylabel('gap（%）' if metric=='gap' else 'runtime（秒，对数刻度）');ax.set_title('TF={}，RDD={}：{}'.format(c[0],c[1],'最终gap' if metric=='gap' else '运行时间'))
  save(fig,ax,'{}_TF{}_RDD{}_折线图'.format(metric,*c),'保留时间上限记录；两方法计时范围、停止精度和gap定义可能不同。')
(out/'阅读说明.md').write_text('先看01–03：四种参数组合下，BB与Gurobi各一条线，标出汇总数值。其余8张图分别展示每种参数组合内，随零件数量变化的耗时和gap。\n横轴参数组合为离散类别，连线仅便于对比。平均数包含时间上限记录，近优标准为gap≤0.1%，不等同于精确最优。\n此前目标值不一致的记录仍保留原值：n=19、TF=0.3、RDD=0.6差异明显，n=16、TF=0.6、RDD=0.3差异较小；正式结论前需核对实例与模型。',encoding='utf-8')
print('11 line charts created')
