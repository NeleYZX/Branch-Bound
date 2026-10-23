import fs from 'node:fs/promises';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const {rows}=JSON.parse(await fs.readFile(new URL('./data.json',import.meta.url),'utf8'));
const wb=Workbook.create();
const sheet=wb.worksheets.add('数据');
sheet.getRange(`A1:G${rows.length+1}`).values=[['序号','零件数量','TF','RDD','最优解','runtime','gap值'],...rows.map(r=>r.slice(0,7))];
wb.recalculate();
await (await SpreadsheetFile.exportXlsx(wb)).save('D:/C++/Branch-Bound/.analysis-work/plain.xlsx');
console.log((await wb.inspect({kind:'region',sheetId:sheet.id,range:'A1:G3',maxChars:1000})).ndjson);
