import fs from 'node:fs/promises';
import {FileBlob,SpreadsheetFile,Workbook} from '@oai/artifact-tool';
const input='D:/C++/Data-analysis/BB_Basic_2026-10-21/BB_Gurobi.xlsx';
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(input));
const csv=await Workbook.fromCSV(await fs.readFile('D:/C++/Branch-Bound/Journal article/BB_Basic/basci.csv','utf8'),{sheetName:'source'});
const values=csv.worksheets.getItemAt(0).getUsedRange().values;const headers=values[0].map(v=>String(v).replace(/^\\uFEFF/,'').trim());const ix=headers.indexOf('instance_id'),ig=headers.indexOf('MIP_gap');if(ix<0||ig<0)throw Error('Missing header');
const gaps=new Map(values.slice(1).filter(r=>r[ix]).map(r=>[String(r[ix]),Number(r[ig])]));
const s=wb.worksheets.getItemAt(0);const rows=s.getUsedRange().values;
s.unmergeCells('H1:I1');s.mergeCells('H1:J1');s.getRange('J2').values=[['Gap']];
let count=0;for(let i=2;i<rows.length;i++){const r=rows[i];if(typeof r[0]!=='number')continue;const key=`${r[0]}part_TF=${r[1]}_RDD=${r[2]}`;if(!gaps.has(key))throw Error('Unmatched '+key);s.getRange(`J${i+1}`).values=[[gaps.get(key)]];count++;}
s.getRange(`J3:J${rows.length}`).setNumberFormat('0.00%');s.getRange('J2').copyFrom(s.getRange('F2'),'all');s.getRange('J2').values=[['Gap']];s.getRange(`J3:J${rows.length}`).setNumberFormat('0.00%');s.getRange(`J1:J${rows.length}`).format.columnWidth=13;
wb.recalculate();const preview=await wb.render({sheetName:s.name,range:'G38:J44',scale:1.8,format:'png'});await fs.writeFile('D:/C++/Branch-Bound/.analysis-work/bb_gap_preview.png',new Uint8Array(await preview.arrayBuffer()));await(await SpreadsheetFile.exportXlsx(wb)).save('D:/C++/Branch-Bound/.analysis-work/BB_Gurobi_with_gap.xlsx');console.log('Added BB gap for '+count+' rows');

