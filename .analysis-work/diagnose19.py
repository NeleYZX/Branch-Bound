# -*- coding: utf-8 -*-
import pathlib,re,json
root=pathlib.Path(__file__).parent.parent
results=[]
for tf in [.3,.6]:
 for rdd in [.3,.6]:
  name='19part_TF={}_RDD={}'.format(tf,rdd);p=root/'GeneratedResults'/(name+'.txt');lines=[l.strip() for l in p.read_text().splitlines() if l.strip()];head=list(map(int,lines[1].split()));machine=list(map(float,lines[2].split()));records=[]
  for i in range(3,len(lines)-1,2):
   a=list(map(float,lines[i].split()));z=list(map(float,lines[i+1].split()));records.append((a,z))
  actual=list(map(float,lines[-1].split()));remainder=[]
  for a,z in records[19:]:remainder+=a+z
  remainder+=actual;parsed=remainder[:19]
  log=(root/'Journal article/BB_Basic'/(name+'_log_1.txt')).read_text(encoding='utf-8-sig',errors='replace');batches=[list(map(int,re.findall(r'\d+',s))) for s in re.findall(r'Batch \d+: \[([^\]]*)\]',log)]
  def score(d):
   t=0;tt=0
   for batch in batches:
    t+=machine[4]+machine[2]*sum(records[j][0][3] for j in batch)+machine[3]*max(records[j][1][2] for j in batch)
    tt+=sum(max(0,t-d[j]) for j in batch)
   return tt
  item=dict(instance=name,declared_parts=head[1],actual_part_records=len(records),due_dates_count=len(actual),parser_due_dates=parsed,file_due_dates=actual,logged_UB=float(re.search(r'final_UB:([\d.]+)',log)[1]),recomputed_parser_due=score(parsed),recomputed_file_due=score(actual),batches=batches)
  results.append(item)
print(json.dumps(results,ensure_ascii=False,indent=2));(root/'.analysis-work/diagnosis19.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
