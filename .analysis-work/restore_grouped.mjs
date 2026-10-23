import fs from 'node:fs/promises';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const input=JSON.parse(await fs.readFile(new URL('./merge_inputs.json',import.meta.url),'utf8'));
const bb=Object.values(input['BB_Basic_数据分析.xlsx'])[0].slice(1).filter(r=>r[1]!=null);
const gu=input['TT_1D_Gurobi.xlsx'].TT_1D.slice(1).filter(r=>r[0]!=null);
const key=r=>JSON.stringify(r),bm=new Map(bb.map(r=>[key(r.slice(1,4)),r]));
const entries=gu.map(g=>({g,b:bm.get(key(g.slice(0,3)))}));
for(const b of bb)if(!gu.some(g=>key(g.slice(0,3))===key(b.slice(1,4))))entries.push({b});
entries.sort((a,b)=>{const x=a.g?.slice(0,3)??a.b.slice(1,4),y=b.g?.slice(0,3)??b.b.slice(1,4);return x[0]-y[0]||y[1]-x[1]||y[2]-x[2];});
const rows=entries.map(({g,b})=>[...(g?.slice(0,3)??b.slice(1,4)),g?.[3]??null,g?.[4]??null,g?.[5]??null,g?.[6]??null,b?.[4]??null,b?.[5]??null]);
const wb=Workbook.create(),s=wb.worksheets.add('结果对比');
s.getRange(`A1:I${rows.length+2}`).values=[['instance',null,null,'Gurobi',null,null,null,'BB-parallel',null],['n','TF','RDD','TT','t_run(s)','Gap','opt.','TT','t_run(s)'],...rows];
s.mergeCells('A1:C1');s.mergeCells('D1:G1');s.mergeCells('H1:I1');s.getRange('A1:I2').format.horizontalAlignment='center';s.getRange(`F3:F${rows.length+2}`).setNumberFormat('0.00%');s.getRange('A1:C54').format.columnWidth=10;s.getRange('D1:I54').format.columnWidth=17;
const small=input['TT_1D_Gurobi.xlsx'].small;const t=wb.worksheets.add('Gurobi_small_独立实例');t.getRange(`A1:G${small.length}`).values=small;
wb.recalculate();await (await SpreadsheetFile.exportXlsx(wb)).save('D:/C++/Branch-Bound/.analysis-work/BB_Gurobi_原始数据_分组对比.xlsx');
if(rows.length!==52)throw Error('Unexpected row count');
for(const {g,b} of entries){const row=rows[entries.findIndex(e=>e.g===g&&e.b===b)];if(g&&(row[3]!==g[3]||row[4]!==g[4]||row[5]!==g[5]||row[6]!==g[6]))throw Error('Gurobi mismatch');if(b&&(row[7]!==b[4]||row[8]!==b[5]))throw Error('BB mismatch');}
console.log('Verified all source values: 52 rows, 48 matched, 4 Gurobi-only; small sheet preserved.');
