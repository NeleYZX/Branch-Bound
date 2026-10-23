# -*- coding: utf-8 -*-
import csv,json,re,pathlib,statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=pathlib.Path(__file__).parent
out=p/'output';out.mkdir(exist_ok=True)
src=p.parent/'Journal article/BB_Basic/basci.csv'
with src.open(encoding='utf-8-sig',newline='') as f: raw=list(csv.DictReader(f))
rows=[]
for i,r in enumerate(raw,1):
 m=re.fullmatch(r'(\d+)part_TF=([\d.]+)_RDD=([\d.]+)',r['instance_id']);assert m
 n,tf,rdd=int(m[1]),float(m[2]),float(m[3]);assert n==int(r['n_parts'])
 rows.append([i,n,tf,rdd,float(r['final_UB']) if r['solve_status']=='OPTIMAL' else None,float(r['search_time_seconds']),float(r['MIP_gap']),r['solve_status'],float(r['final_UB']),float(r['final_LB']),float(r['solver_call_seconds']),r['instance_id']])
assert len(set(r[11] for r in rows))==len(rows)
(p/'data.json').write_text(json.dumps({'rows':rows,'raw':raw},ensure_ascii=False),encoding='utf-8')
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
colors=['#2563eb','#e58a24','#169b82','#9b59b6']
combos=sorted(set((r[2],r[3]) for r in rows));ns=sorted(set(r[1] for r in rows))
fig,axs=plt.subplots(2,2,figsize=(15,10),constrained_layout=True)
for k,(tf,rdd) in enumerate(combos):
 rr=[r for r in rows if r[2:4]==[tf,rdd]];lab=f'TF={tf}, RDD={rdd}'
 axs[0,0].plot([r[1] for r in rr],[r[5] for r in rr],'-o',label=lab,color=colors[k],ms=4)
 axs[0,1].plot([r[1] for r in rr],[100*r[6] for r in rr],'-o',label=lab,color=colors[k],ms=4)
axs[0,0].set_yscale('log');axs[0,0].set_title('搜索耗时随规模变化（对数纵轴）');axs[0,0].set_ylabel('runtime（秒）');axs[0,0].legend(fontsize=8)
axs[0,1].set_title('gap 随规模变化');axs[0,1].set_ylabel('gap（%）');axs[0,1].legend(fontsize=8)
rates=[sum(r[7]=='OPTIMAL' for r in rows if r[1]==n)/4*100 for n in ns]
axs[1,0].bar(ns,rates,color=colors[0]);axs[1,0].set_title('各规模最优求解率（每组4例）');axs[1,0].set_ylabel('最优求解率（%）');axs[1,0].set_ylim(0,110)
means=[statistics.mean(r[5] for r in rows if (r[2],r[3])==c) for c in combos]
axs[1,1].bar([f'{a}/{b}' for a,b in combos],means,color=colors);axs[1,1].set_title('TF / RDD 组合平均搜索耗时');axs[1,1].set_ylabel('平均 runtime（秒）');axs[1,1].set_xlabel('TF / RDD')
for ax in axs.flat: ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
for ax in [axs[0,0],axs[0,1],axs[1,0]]:ax.set_xlabel('零件数量');ax.set_xticks(ns)
fig.savefig(out/'图表总览.png',dpi=180);plt.close(fig)
lines=['# BB Basic 实验数据分析','',f'共 {len(rows)} 条记录，零件数量 10–21；TF、RDD 均为 0.3 / 0.6，每个规模4个组合。','', '## 主要结论','',f'- 已证明最优 42 例（87.5%），超时 6 例（12.5%）。', '- 10–19 个零件全部证明最优；20 个零件最优求解率为 50%，21 个零件为 0%。','- 超时均采用 3600 秒搜索上限，图中的耗时属于受上限截断的数据，不能代表完成求解所需的实际时间。','']
for c,v in zip(combos,means):
 rr=[r for r in rows if (r[2],r[3])==c]
 lines.append(f'- TF={c[0]}、RDD={c[1]}：平均搜索耗时 {v:.3f} 秒，最优求解 {sum(r[7]=="OPTIMAL" for r in rr)}/12，平均 gap {statistics.mean(r[6] for r in rr):.2%}。')
lines+=['','## 口径与限制','','- 最优解仅对 OPTIMAL 填写；TIME_LIMIT 留空，当前最好可行解（final_UB）及下界另列保留。','- runtime = search_time_seconds，单位秒；求解器调用总耗时 solver_call_seconds 另列，二者计时范围不同。','- gap 直接使用 MIP_gap（比例），以百分比显示；所有均值包含超时记录。','- 序号按原 CSV 顺序生成；TF/RDD 从 instance_id 提取；保留源数据供核查。','- 每个规模与参数组合只有一个实例，无重复试验；结果属于描述性分析，不能据此判定统计显著性。','- 不同 TF/RDD 对应不同问题实例，目标值不宜直接用于比较算法优劣。','','## 超时实例','']
for r in rows:
 if r[7]!='OPTIMAL':lines.append(f'- {r[11]}：当前最好可行解 {r[8]:.6f}，下界 {r[9]:.6f}，gap {r[6]:.2%}。')
(out/'数据分析说明.md').write_text('\n'.join(lines),encoding='utf-8')
print('\n'.join(lines[:16]))
