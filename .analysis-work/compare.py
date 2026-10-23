# -*- coding: utf-8 -*-
import csv,json,pathlib,math,statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
p=pathlib.Path(__file__).parent;out=p/'bb_gurobi';out.mkdir(exist_ok=True)
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans'];plt.rcParams['axes.unicode_minus']=False
with (p.parent/'Journal article/BB_Basic/basci.csv').open(encoding='utf-8-sig') as f:bb=list(csv.DictReader(f))
g=json.loads((p/'gurobi.json').read_text(encoding='utf-8'));gg={tuple(r[:3]):r for r in g['TT_1D'][1:] if r[0] is not None}
data=[]
for r in bb:
 import re
 n,tf,rdd=re.fullmatch(r'(\d+)part_TF=([\d.]+)_RDD=([\d.]+)',r['instance_id']).groups();key=(int(n),float(tf),float(rdd))
 if key not in gg:continue
 q=gg[key];bopt=r['solve_status']=='OPTIMAL';gopt=q[6]=='opt.'
 data.append(dict(n=key[0],tf=key[1],rdd=key[2],bb_search=float(r['search_time_seconds']),bb_call=float(r['solver_call_seconds']),g_time=q[4],bb_gap=float(r['MIP_gap']),g_gap=q[5],bb_ub=float(r['final_UB']),g_ub=q[3],bb_opt=bopt,g_opt=gopt,g_near=q[5]<=.001,g_at_limit=q[4]>=3600,bb_at_limit=not bopt))
data.sort(key=lambda x:(x['tf'],x['rdd'],x['n']))
with (out/'配对数据.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)
C=['#2563eb','#e58a24'];combos=sorted(set((r['tf'],r['rdd']) for r in data))
note='BB：现有 αser=0、αpos=0.2；最佳配置及历史节点策略待确认。'
def save(fig,ax,name,footer):
 ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True);fig.text(.5,.02,footer,ha='center',fontsize=8,color='#555555');fig.tight_layout(rect=[0,.06,1,1]);fig.savefig(str(out/(name+'.png')),dpi=200);plt.close(fig)
for c in combos:
 rr=[r for r in data if (r['tf'],r['rdd'])==c];n=[r['n'] for r in rr];tag='TF{}_RDD{}'.format(*c)
 fig,ax=plt.subplots(figsize=(10,6))
 for j,(field,label) in enumerate([('bb_call','BB 调用总耗时'),('g_time','Gurobi 表内耗时')]):
  ax.plot(n,[r[field] for r in rr],'-o',color=C[j],label=label)
  cut=[r for r in rr if r['bb_at_limit' if j==0 else 'g_at_limit']];ax.scatter([r['n'] for r in cut],[r[field] for r in cut],marker='x',s=85,color=C[j],zorder=5)
 ax.plot(n,[r['bb_search'] for r in rr],'--',color=C[0],alpha=.55,label='BB 搜索耗时（辅助口径）');ax.set_yscale('log');ax.set_xticks(n);ax.set_xlabel('零件数量');ax.set_ylabel('耗时（秒，对数刻度）');ax.set_title('BB 与 Gurobi 耗时：'+tag);ax.legend(fontsize=9)
 save(fig,ax,'01_耗时_'+tag,'×：BB搜索触及上限 / Gurobi记录耗时≥3600秒；停止精度不同，不作精确加速比。')
 fig,ax=plt.subplots(figsize=(10,6));x=np.arange(len(rr));width=.38
 ax.bar(x-width/2,[100*r['bb_gap'] for r in rr],width,label='BB',color=C[0]);ax.bar(x+width/2,[100*r['g_gap'] for r in rr],width,label='Gurobi',color=C[1]);ax.set_xticks(x);ax.set_xticklabels(n);ax.set_ylim(0,110);ax.set_xlabel('零件数量');ax.set_ylabel('最终 gap（%）');ax.set_title('BB 与 Gurobi 最终 gap：'+tag);ax.legend()
 save(fig,ax,'02_gap_'+tag,'分别保留两种求解器原始gap定义；未假定二者分母相同。')
# Honest speed ratios restricted to both explicitly optimal and agreeing objective.
fig,ax=plt.subplots(figsize=(10,6));counts={}
for i,c in enumerate(combos):
 rr=[r for r in data if (r['tf'],r['rdd'])==c and r['bb_opt'] and r['g_opt'] and math.isclose(r['bb_ub'],r['g_ub'],rel_tol=1e-6,abs_tol=1e-5)]
 counts[str(c)]=len(rr)
 ax.plot([r['n'] for r in rr],[r['g_time']/r['bb_call'] for r in rr],'-o',label='TF={}, RDD={}'.format(*c))
ax.axhline(1,color='#555555',ls='--');ax.set_yscale('log');ax.set_xlabel('零件数量');ax.set_ylabel('Gurobi耗时 / BB调用总耗时');ax.set_title('双方明确标最优且目标值一致的配对耗时比');ax.legend(fontsize=9)
save(fig,ax,'03_严格最优配对耗时比','>1表示BB更快；仅比较表内耗时，硬件/线程及Gurobi计时范围尚未核实。')
fig,ax=plt.subplots(figsize=(10,6));x=np.arange(4);width=.25
for off,key,label,color in [(-width,'bb_opt','BB 已证明最优',C[0]),(0,'g_opt','Gurobi opt.标记',C[1]),(width,'g_near','Gurobi gap≤0.1%','#169b82')]:
 vals=[sum(r[key] for r in data if (r['tf'],r['rdd'])==c)/12*100 for c in combos];ax.bar(x+off,vals,width,label=label,color=color)
ax.set_xticks(x);ax.set_xticklabels(['{} / {}'.format(*c) for c in combos]);ax.set_ylim(0,115);ax.set_xlabel('TF / RDD');ax.set_ylabel('实例占比（%）');ax.set_title('四种参数组合的最优标记与近优覆盖率');ax.legend(fontsize=9)
save(fig,ax,'04_参数组合求解质量','每组12例；gap≤0.1%仅为分析阈值，不等同于已证明精确最优。')
for key,title,name in [('bb_gap','BB 最终gap（%）','05_BB_gap热力图'),('g_gap','Gurobi 最终gap（%）','06_Gurobi_gap热力图')]:
 fig,ax=plt.subplots(figsize=(12,5));ns=sorted(set(r['n'] for r in data));v=np.array([[next(r[key]*100 for r in data if (r['tf'],r['rdd'])==c and r['n']==n) for n in ns] for c in combos]);im=ax.imshow(v,aspect='auto',vmin=0,vmax=100,cmap='YlOrRd');ax.set_xticks(range(12));ax.set_xticklabels(ns);ax.set_yticks(range(4));ax.set_yticklabels(['TF={}, RDD={}'.format(*c) for c in combos]);ax.set_ylim(3.5,-.5);ax.set_xlabel('零件数量');ax.set_title(title)
 for i in range(4):
  for j in range(12):ax.text(j,i,'{:.1f}'.format(v[i,j]),ha='center',va='center',fontsize=8,color='white' if v[i,j]>60 else '#222222')
 fig.colorbar(im,ax=ax,label='gap（%）');save(fig,ax,name,'两张热力图采用相同色阶0–100%；仅描述各自报告的最终gap。')
summary=[]
for c in combos:
 rr=[r for r in data if (r['tf'],r['rdd'])==c];summary.append('TF={}、RDD={}：BB明确最优 {}/12，Gurobi opt.标记 {}/12，Gurobi gap≤0.1% {}/12；平均gap BB {:.2%} / Gurobi {:.2%}。'.format(c[0],c[1],sum(r['bb_opt'] for r in rr),sum(r['g_opt'] for r in rr),sum(r['g_near'] for r in rr),statistics.mean(r['bb_gap'] for r in rr),statistics.mean(r['g_gap'] for r in rr)))
report=['# BB 与 Gurobi 对比分析（现有BB配置，待确认最佳设置）','','## 比较范围','','- 使用 TT_1D 工作表，匹配 n、TF、RDD，共48对，n=10–21；Gurobi n=5的4例因缺少BB结果不纳入。','- small 工作表不是同一批实例：例如n=10、TF=RDD=0.6目标值124.7808704，与TT_1D及BB的116.3003072不同，因此不混用。','- 第3项对应 TF=RDD=0.6 两张专图；第4项暂按TF、RDD各取0.3/0.6的全部四种组合处理。','','## 配置与可比性','','- BB来源：Journal article/BB_Basic/basci.csv，αser=0、αpos=0.2。历史BB日志记录节点策略为Hybrid (Best-first + DFS)，但尚无证据说明其为比较试验中最佳策略。','- 调参汇总使用16part_1等不同实例；不能逐实例挑选最快参数混合成“最佳BB”。最佳配置需用统一选定的参数及节点策略的独立测试结果。','- 主耗时图采用BB solver_call_seconds，并辅以search_time_seconds；Gurobi采用t_run(s)。硬件、线程数、计时范围及停止容差尚未核实，因此仅作为记录耗时比较。','- BB时间上限约束搜索阶段，调用总耗时可超过3600秒；Gurobi也有耗时未到3600秒但gap>0的记录，不能把所有空opt.状态视为超时。','- Gurobi仅opt.字符串视为明确最优，另列gap≤0.1%的近优覆盖率。极小正gap不自动重写为精确最优。','- 严格配对耗时比只纳入双方均明确最优且目标值在相对1e-6/绝对1e-5内一致的记录；未最优实例不计算精确求解加速比。','- 最终gap保留各自定义；未核实Gurobi的gap分母与BB是否一致。','','## 结果概览','']+['- '+s for s in summary]+['','## 结论边界','','- 优先结合耗时与最终gap评价效率：耗时短但剩余gap高，不能视为同等求解质量下更高效。','- 参数影响应在相同规模下横向比较；各组合每个规模只有一个实例，无重复试验，不作显著性或因果结论。','- 这套图可用于现有BB配置的初步比较；在确认最佳α参数和节点策略前，不应作为“最佳BB vs Gurobi”的最终论文结论。','','## 图表索引','','- 01：四组耗时折线图（每组独立图片）；02：四组最终gap分组柱状图。','- 03：严格最优配对耗时比；04：参数组合求解质量；05–06：统一色阶gap热力图。','- 配对数据.csv：全部48对原始指标，纯列数据，可用Excel打开。']
(out/'分析说明.md').write_text('\n'.join(report),encoding='utf-8')
print('\n'.join(summary));print('strict paired counts',counts);print('charts',len(list(out.glob('*.png'))))


