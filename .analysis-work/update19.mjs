import fs from 'node:fs/promises';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const base='D:/C++/Data-analysis/BB_Basic_2026-10-21';const output='D:/C++/Branch-Bound/.analysis-work/update19';
const {updates}=JSON.parse(await fs.readFile(`${output}/rows.json`,'utf8'));
const keyed=new Map(updates.map(u=>[u.instance,u.new]));
for(const name of ['BB_Basic_数据分析.xlsx','BB_Gurobi.xlsx']){
 const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(`${base}/${name}`));
 const s=wb.worksheets.getItemAt(0);
 const before=await wb.render({sheetName:s.name,range:name.startsWith('BB_Basic')?'A37:G42':'A38:I43',scale:1.5,format:'png'});await fs.writeFile(`${output}/${name}.before.png`,new Uint8Array(await before.arrayBuffer()));
 let count=0;
 const values=s.getUsedRange().values;
 for(let i=0;i<values.length;i++){
  const v=values[i];const offset=name.startsWith('BB_Basic')?1:0;if(v[offset]!==19)continue;
  const id=`19part_TF=${v[offset+1]}_RDD=${v[offset+2]}`;const r=keyed.get(id);if(!r)throw Error('Missing '+id);
  if(offset===1)s.getRange(`E${i+1}:G${i+1}`).values=[[r.solve_status==='OPTIMAL'?Number(r.final_UB):null,Number(r.search_time_seconds),Number(r.MIP_gap)]];
  else s.getRange(`H${i+1}:I${i+1}`).values=[[Number(r.final_UB),Number(r.search_time_seconds)]];
  count++;
 }
 if(count!==4)throw Error('Unexpected count '+count);
 if(name==='BB_Gurobi.xlsx'){
  const audit=wb.worksheets.getItemAt(1);const va=audit.getUsedRange().values;
  for(let i=1;i<va.length;i++){const v=va[i];if(v[0]!==19)continue;const r=keyed.get(`19part_TF=${v[1]}_RDD=${v[2]}`);audit.getRange(`I${i+1}:K${i+1}`).values=[[r.solve_status,Number(r.MIP_gap),'按n、TF、RDD一致匹配；已采用重算日志更新']];}
 }
 wb.recalculate();
 const after=await wb.render({sheetName:s.name,range:name.startsWith('BB_Basic')?'A37:G42':'A38:I43',scale:1.5,format:'png'});await fs.writeFile(`${output}/${name}.after.png`,new Uint8Array(await after.arrayBuffer()));
 await(await SpreadsheetFile.exportXlsx(wb)).save(`${output}/${name}`);console.log('Updated '+name+' 4 rows');
}
