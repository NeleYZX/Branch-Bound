# -*- coding: utf-8 -*-
import pathlib,re,csv,json,shutil,hashlib
root=pathlib.Path(__file__).resolve().parent.parent;p=root/'.analysis-work/update19';p.mkdir(exist_ok=True)
source=root/'Journal article/BB_Basic/basci.csv';shutil.copy2(source,p/'basci_before.csv')
with source.open(encoding='utf-8-sig',newline='') as f:reader=csv.DictReader(f);fields=reader.fieldnames;rows=list(reader)
updates=[]
for row in rows:
 if row['n_parts']!='19':continue
 log=root/row['source_log'];text=log.read_text(encoding='utf-8-sig',errors='replace')
 def val(k):
  m=re.search(r'^'+re.escape(k)+r'\s*:([^\r\n]+)',text,re.M)
  if not m:raise ValueError('Missing '+k)
  return m[1].strip()
 mapping={'solve_status':'solve_status','optimal_time_seconds':'optimal_time_seconds','search_time_seconds':'search_time_seconds','solver_call_seconds':'solver_call_seconds','final_UB':'final_UB','final_LB':'final_LB','MIP_gap':'MIP_gap','MIP_gap_percent':'MIP_gap_percent','total_nodes':'The handled total nodes(pop)','serial_dp_trigger_count':'serial_dp_trigger_count','serial_pruning_count':'serial_pruning_count'}
 old=dict(row)
 for col,label in mapping.items():row[col]=val(label)
 row['optimal_time_seconds']='' if row['optimal_time_seconds']=='NA' else row['optimal_time_seconds']
 # The percentage field has greater precision in the text log than MIP_gap.
 row['MIP_gap']=str(float(row['MIP_gap_percent'])/100)
 assert row['solve_status'] in ['OPTIMAL','TIME_LIMIT']
 updates.append({'instance':row['instance_id'],'old':old,'new':dict(row)})
assert len(updates)==4 and len(rows)==48
with (p/'basci.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
(p/'rows.json').write_text(json.dumps({'rows':rows,'updates':updates},ensure_ascii=False),encoding='utf-8')
print([(u['instance'],u['new']['final_UB'],u['new']['search_time_seconds'],u['new']['solve_status']) for u in updates])

