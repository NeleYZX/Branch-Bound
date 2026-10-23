import fs from 'node:fs/promises';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const dir=new URL('.',import.meta.url).pathname.replace(/^\/([A-Za-z]:)/,'$1');
const {rows,raw}=JSON.parse(await fs.readFile(`${dir}/data.json`,'utf8'));
const wb=Workbook.create(),s=wb.worksheets.add('分析总览'),d=wb.worksheets.add('结果明细'),src=wb.worksheets.add('原始数据');
for(const sh of [s,d,src]){sh.showGridLines=false;sh.getRange('A1:T90').format.font={name:'Arial',size:10};sh.getRange('A1:T90').format.rowHeight=23;sh.getRange('A1:T90').format.columnWidth=15;}
const head=(sh,r)=>{sh.getRange(r).format.fill='#243e61';sh.getRange(r).format.font={bold:true,color:'#FFFFFF'};};
d.getRange('A1:L49').values=[['序号','零件数量','TF','RDD','最优解','runtime（秒）','gap值','求解状态','当前最好可行解','下界','调用总耗时（秒）','实例标识'],...rows];
head(d,'A1:L1');d.tables.add('A1:L49',true,'Results');d.freezePanes.freezeRows(1);d.getRange('E2:F49').setNumberFormat('0.000000');d.getRange('G2:G49').setNumberFormat('0.00%');d.getRange('I2:K49').setNumberFormat('0.000000');d.getRange('L1:L49').format.columnWidth=34;d.getRange('I1:K49').format.columnWidth=22;
d.getRange('H2:H49').conditionalFormats.addCustom('H2="TIME_LIMIT"',{fill:'#FFF0CB',font:{color:'#9B4800',bold:true}});
const keys=Object.keys(raw[0]);const numeric=new Set(['n_parts','alpha_ser','alpha_pos','time_limit_seconds','optimal_time_seconds','search_time_seconds','solver_call_seconds','final_UB','final_LB','MIP_gap','MIP_gap_percent','total_nodes','serial_dp_trigger_count','serial_pruning_count']);
src.getRange('A1:T49').values=[keys,...raw.map(r=>keys.map(k=>numeric.has(k)?(r[k]===''?null:Number(r[k])):r[k]))];head(src,'A1:T1');src.freezePanes.freezeRows(1);src.tables.add('A1:T49',true,'SourceData');src.getRange('A1:A49').format.columnWidth=34;
s.getRange('A2').values=[['BB Basic 实验分析']];s.getRange('A2').format.font={size:16,bold:true,color:'#243e61'};
s.getRange('A4:F5').values=[['实例数','已证明最优','超时','最优求解率','时间上限（秒）','规模范围'],[48,42,6,.875,3600,'10–21']];head(s,'A4:F4');s.getRange('D5').setNumberFormat('0.0%');
s.getRange('A7').values=[['runtime 为搜索耗时；超时记录最优解留空，当前最好可行解另列。']];
s.getRange('A8').values=[['每个参数组合仅1例；耗时均值包含超时记录，不代表完成求解所需时间。']];
s.getRange('A10:H10').values=[['零件数量','平均耗时（秒）','最优求解率','平均gap','0.3 / 0.3','0.3 / 0.6','0.6 / 0.3','0.6 / 0.6']];head(s,'A10:H10');
const combos=[[.3,.3],[.3,.6],[.6,.3],[.6,.6]];
for(let i=0;i<12;i++){
 const r=11+i,n=10+i;s.getRange(`A${r}`).values=[[n]];
 s.getRange(`B${r}:D${r}`).formulas=[[`=AVERAGEIF('结果明细'!B$2:B$49,A${r},'结果明细'!F$2:F$49)`,`=COUNTIFS('结果明细'!B$2:B$49,A${r},'结果明细'!H$2:H$49,"OPTIMAL")/COUNTIF('结果明细'!B$2:B$49,A${r})`,`=AVERAGEIF('结果明细'!B$2:B$49,A${r},'结果明细'!G$2:G$49)`]];
 for(let j=0;j<4;j++){const idx=rows.findIndex(x=>x[1]===n&&x[2]===combos[j][0]&&x[3]===combos[j][1])+2;s.getCell(r-1,4+j).formulas=[[`='结果明细'!F${idx}`]];}
}
s.getRange('B11:B22').setNumberFormat('0.000');s.getRange('C11:D22').setNumberFormat('0.00%');s.getRange('E11:H22').setNumberFormat('0.000');
s.getRange('A25:E25').values=[['TF / RDD','平均耗时（秒）','最优求解率','平均gap','超时数量']];head(s,'A25:E25');
for(let j=0;j<4;j++){const r=26+j,[tf,rdd]=combos[j];s.getRange(`A${r}`).values=[[`${tf} / ${rdd}`]];s.getRange(`B${r}:E${r}`).formulas=[[`=AVERAGEIFS('结果明细'!F$2:F$49,'结果明细'!C$2:C$49,${tf},'结果明细'!D$2:D$49,${rdd})`,`=COUNTIFS('结果明细'!C$2:C$49,${tf},'结果明细'!D$2:D$49,${rdd},'结果明细'!H$2:H$49,"OPTIMAL")/12`,`=AVERAGEIFS('结果明细'!G$2:G$49,'结果明细'!C$2:C$49,${tf},'结果明细'!D$2:D$49,${rdd})`,`=12*(1-C${r})`]];}
s.getRange('B26:B29').setNumberFormat('0.000');s.getRange('C26:D29').setNumberFormat('0.00%');
s.getRange('A32').values=[['结论：10–19件全部证明最优；20件最优求解率50%，21件全部超时。']];
s.getRange('A34').values=[['来源：Journal article/BB_Basic/basci.csv；完整字段保留在“原始数据”。']];
s.getRange('A35').values=[['gap 使用原始 MIP_gap 比例；最优解仅采用 OPTIMAL 的 final_UB。']];
const palette=['#2563EB','#E58A24','#169B82','#9B59B6'];
function chart(type,ranges,title,from,to,percent=false){const c=s.charts.add(type,ranges.length===1?s.getRange(ranges[0]):ranges.map(r=>s.getRange(r)));c.title=title;c.titleTextStyle.fontSize=13;c.titleTextStyle.typeface='Arial';c.setPosition(from,to);c.xAxis={axisType:'textAxis'};c.yAxis={numberFormatCode:percent?'0%':'0',numberFormatSourceLinked:false};c.legend={position:'bottom'};c.series.items.forEach((ser,i)=>{ser.fill=palette[i%4];if(type==='line')ser.line={fill:palette[i%4],style:'solid',width:2};});return c;}
chart('line',['A10:A22','E10:E22','F10:F22','G10:G22','H10:H22'],'不同 TF/RDD 的搜索耗时（秒）','J2','S17');
chart('bar',['A10:A22','C10:C22'],'各规模最优求解率','J19','S34',true);
chart('bar',['A25:B29'],'参数组合平均搜索耗时（秒）','A38','I53');
chart('line',['A10:A22','D10:D22'],'各规模平均 gap','J38','S53',true);
wb.recalculate();
console.log((await wb.inspect({kind:'region',sheetId:s.id,range:'A10:H13',maxChars:2500,tableMaxCols:8})).ndjson);
await (await SpreadsheetFile.exportXlsx(wb)).save(`${dir}/output/BB_Basic_数据分析.xlsx`);
const preview=await wb.render({sheetName:'分析总览',range:'A1:S35',scale:1.4,format:'png'});await fs.writeFile(`${dir}/preview.png`,new Uint8Array(await preview.arrayBuffer()));
console.log('EXPORTED');

