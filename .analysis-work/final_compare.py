# -*- coding: utf-8 -*-
import json,csv,pathlib,statistics,math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
p=pathlib.Path(__file__).parent;out=p/'final_comparison';out.mkdir(exist_ok=True)
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
b=json.loads((p/'bb_user.json').read_text(encoding='utf-8'))[1:];g=json.loads((p/'gurobi.json').read_text(encoding='utf-8'))['TT_1D'][1:];gm={tuple(r[:3]):r for r in g};data=[]
for r in b:
 q=gm[tuple(r[1:4])];data.append(dict(n=r[1],TF=r[2],RDD=r[3],BB_runtime=r[5],Gurobi_runtime=q[4],BB_gap=r[6],Gurobi_gap=q[5],BB_optimum=r[4],Gurobi_TT=q[3],Gurobi_opt_mark=q[6] or '',Gurobi_gap_le_01pct=q[5]<=.001))
colors=['#2563eb','#e58a24','#169b82','#9b59b6'];combos=[(.3,.3),(.3,.6),(.6,.3),(.6,.6)]
def save(fig,ax,d,name,note):
 ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True);fig.text(.5,.02,note,ha='center',fontsize=8,color='#555555');fig.tight_layout(rect=[0,.065,1,1]);fig.savefig(str(d/(name+'.png')),dpi=200);plt.close(fig)
def charts(rr,d,tag):
 n=[r['n'] for r in rr]
 for metric,title,yl in [('runtime','运行时间对比','runtime（秒，对数刻度）'),('gap','最终gap对比','最终gap（%）')]:
  fig,ax=plt.subplots(figsize=(10,6))
  for i,alg in enumerate(['BB','Gurobi']):
   vals=[r[alg+'_'+metric]*(100 if metric=='gap' else 1) for r in rr]
   ax.plot(n,vals,'-o',label=alg,color=colors[i])
   if metric=='runtime':
    cut=[r for r in rr if r[alg+'_runtime']>=3600];ax.scatter([r['n'] for r in cut],[r[alg+'_runtime'] for r in cut],color=colors[i],marker='x',s=90,zorder=5)
  if metric=='runtime':ax.set_yscale('log')
  else:ax.set_ylim(0,110)
  ax.set_xticks(n);ax.set_xlabel('零件数量');ax.set_ylabel(yl);ax.set_title(title+'（'+tag+'）');ax.legend()
  save(fig,ax,d,metric+'_'+tag,'×表示记录耗时≥3600秒；两方法停止精度及计时范围未核实。' if metric=='runtime' else '直接采用两文件的gap比例×100%；各自停止准则及gap定义可能不同。')
d3=out/'3_TF0.6_RDD0.6_算法效率';d4=out/'4_四种参数组合_方法影响';d3.mkdir(exist_ok=True);d4.mkdir(exist_ok=True)
charts(sorted([r for r in data if r['TF']==r['RDD']==.6],key=lambda r:r['n']),d3,'TF=0.6_RDD=0.6')
for c in combos:charts(sorted([r for r in data if (r['TF'],r['RDD'])==c],key=lambda r:r['n']),d4,'TF={}_RDD={}'.format(*c))
fig,ax=plt.subplots(figsize=(10,6));x=np.arange(4);w=.36
for i,alg in enumerate(['BB','Gurobi']):
 vals=[statistics.mean(r[alg+'_gap'] for r in data if (r['TF'],r['RDD'])==c)*100 for c in combos];ax.bar(x+(i-.5)*w,vals,w,label=alg,color=colors[i])
 for j,v in enumerate(vals):ax.text(x[j]+(i-.5)*w,v+.3,'{:.2f}'.format(v),ha='center',fontsize=9)
ax.set_xticks(x);ax.set_xticklabels(['{} / {}'.format(*c) for c in combos]);ax.set_xlabel('TF / RDD');ax.set_ylabel('平均最终gap（%）');ax.set_title('问题参数与最终gap');ax.legend();save(fig,ax,d4,'参数组合_平均gap','每组12例；包括未证明最优的记录。')
for d,rr in [(d3,[r for r in data if r['TF']==r['RDD']==.6]),(d4,data)]:
 with (d/'对比数据.csv').open('w',encoding='utf-8-sig',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=list(data[0]));wr.writeheader();wr.writerows(rr)
bad=[r for r in data if r['BB_optimum'] is not None and r['Gurobi_opt_mark']=='opt.' and not math.isclose(r['BB_optimum'],r['Gurobi_TT'],rel_tol=1e-6,abs_tol=1e-5)]
text=['# 两项实验对比分析','','使用用户指定目录的 BB_Basic_数据分析.xlsx（七列数据）与 TT_1D_Gurobi.xlsx 的 TT_1D 工作表；按n、TF、RDD配对48例。small工作表属于另一组实例，不纳入。','','## 3. TF=RDD=0.6：算法效率','','- n=10–20，BB记录运行时间均低于Gurobi；n=21两者均约3600秒。','- BB在n=10–20的最优解列有值；Gurobi在n=10–14明确标opt.，n=15、16、18虽未标opt.但gap≤0.1%。','- n=21时BB最终gap约81.11%，Gurobi约70.65%。达到时间上限后，Gurobi报告的gap更小；不能只依据小规模耗时推断所有规模均占优。','- 两方法未保证相同停止精度，且BB的runtime为搜索耗时；运行时间曲线不直接等价于同精度的完整求解加速比。','','## 4. TF、RDD各取0.3/0.6：问题参数影响','']
for c in combos:
 rr=[r for r in data if (r['TF'],r['RDD'])==c];text.append('- TF={}、RDD={}：BB平均gap {:.2%}；Gurobi平均gap {:.2%}；Gurobi gap≤0.1% 为 {}/12。'.format(*c,statistics.mean(r['BB_gap'] for r in rr),statistics.mean(r['Gurobi_gap'] for r in rr),sum(r['Gurobi_gap_le_01pct'] for r in rr)))
text+=['','- 本批记录中，TF=0.6的两个组合BB平均gap较小；TF=0.3、RDD=0.6时Gurobi平均gap较小。参数影响应结合相同n的耗时与gap一起查看。','- 每种规模与参数组合只有1例，以上为描述性结果；不作因果与显著性结论。','','## 必须核对的数据差异','','两条记录在双方都报告最优时目标值不一致，尚不能确认完全同一实例/模型：']
for r in bad:text.append('- n={}、TF={}、RDD={}：BB={}，Gurobi={}。'.format(r['n'],r['TF'],r['RDD'],r['BB_optimum'],r['Gurobi_TT']))
text+=['','尤其n=19、TF=0.3、RDD=0.6差异很大；第4项结论应视为暂定，待核对输入实例、目标函数与模型约束后用于正式实验结论。','图中保留提供的数据，未擅自纠正目标值、重写状态或填补缺失最优解。Gurobi空opt.不等于超时。']
(out/'分析说明.md').write_text('\n'.join(text),encoding='utf-8');print('12 charts? actual',len(list(out.rglob('*.png'))));print('objective mismatches',len(bad))
