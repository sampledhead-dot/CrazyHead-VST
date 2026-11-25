// Paridad JS ↔ C++ del generador de CrazyHead. Uso: node tests/parity.js <ejecutable parity> [ruta a CrazyHead/index.html]
const fs=require('fs'),cp=require('child_process'),path=require('path');
const exe=process.argv[2],html=process.argv[3]||process.env.CRAZYHEAD_HTML||path.join(__dirname,'../../CrazyHead/index.html');
const src=fs.readFileSync(html,'utf8');
const a=src.indexOf('function mulberry32'),b=src.indexOf('/* Detección de golpes');
if(a<0||b<0)throw new Error('No se encuentra el generador en '+html);
const clamp=(x,l,h)=>Math.max(l,Math.min(h,x));
const G=new Function('clamp',src.slice(a,b)+';return{mulberry32,segsOf,poolOf,origSlice,pinCell,genCell}')(clamp);
let seed0=99;const rnd=()=>{seed0=(seed0*1664525+1013904223)>>>0;return seed0/4294967296};
const ri=(lo,hi)=>lo+Math.floor(rnd()*(hi-lo+1));
const cases=[];
for(let c=0;c<40;c++){
  const nseg=ri(1,16),dur=2+rnd()*4,mk=[0];for(let k=1;k<nseg;k++)mk.push(+(dur*k/nseg+rnd()*.05).toFixed(6));
  const pins=new Array(16).fill(-1);if(c%3===0){for(let k=0;k<4;k++)pins[ri(0,15)]=ri(0,nseg-1)}
  const pool=[];for(let k=0;k<16;k++)pool.push(c%4===1&&k%3===0&&k<nseg?false:true);
  cases.push({dens:ri(10,100),sync:ri(0,100),fid:ri(0,100),ratch:ri(0,80),dbl:ri(0,60),vmin:ri(30,70),vmax:ri(80,127),glen:ri(0,80),rev:ri(0,60),pitP:ri(0,100),pitR:ri(1,12),hum:c%2?ri(0,20):0,mut:ri(0,100),grid:[4,8,16,32][c%4],loopBars:ri(1,4),len:ri(1,4),seed:ri(1,1e9),pinProb:c%5===0?ri(30,90):100,pinBars:c%6===0?ri(1,15):0,dur,markers:mk,pool,pins});
}
fs.writeFileSync('/tmp/ch_cases.txt',cases.map(D=>[D.dens,D.sync,D.fid,D.ratch,D.dbl,D.vmin,D.vmax,D.glen,D.rev,D.pitP,D.pitR,D.hum,D.mut,D.grid,D.loopBars,D.len,D.seed,D.pinProb,D.pinBars,D.dur.toFixed(9),D.markers.length,...D.markers,D.pool.map(x=>x?1:0).join(''),...D.pins].join(' ')).join('\n')+'\n');
const cpp=cp.execFileSync(exe,['/tmp/ch_cases.txt'],{maxBuffer:1<<28}).toString().split('=====\n');
const f9=x=>{const s=Number(x).toPrecision(9);return String(parseFloat(s))};
function dump(pat,L){let o='';for(let i=0;i<L;i++){o+=i+':';const c=pat[i];if(c)for(const h of c)o+=`[s${h.s} o${f9(h.o)} v${f9(h.v)} d${f9(h.d)} r${h.r} p${h.p} t${h.t} n${h.n} l${h.l||0}]`;o+='\n'}return o}
let bad=0;
cases.forEach((D0,ci)=>{
  const D=Object.assign({},D0);D.dur=+D.dur.toFixed(9);D.grid=String(D.grid);const sg=G.segsOf(D.markers,D.dur),L=D.len*16;
  const pat=[];let rng=G.mulberry32(D.seed);
  for(let i=0;i<L;i++){const c=G.genCell(D,sg,D.dur,i,rng,false);pat.push(G.pinCell(D,sg,i)||c)}
  let o=dump(pat,L)+'--mut\n';
  for(let k=1;k<=3;k++){const rg=G.mulberry32((D.seed+k*977)>>>0),f=D.mut/100*.9;if(D.mut>0)for(let i=0;i<L;i++)if(rg()<f&&!G.pinCell(D,sg,i))pat[i]=G.genCell(D,sg,D.dur,i,rg,false)}
  o+=dump(pat,L)+'--dice\n';
  const rd=G.mulberry32(123456);for(let i=0;i<pat.length;i++)if(rd()<.25&&!G.pinCell(D,sg,i))pat[i]=G.genCell(D,sg,D.dur,i,rd,false);
  o+=dump(pat,L);
  // formato de C++: %.9g de números enteros → «1», igual que JS
  const norm=s=>s.replace(/\[s(\d+) o([^ ]+) v([^ ]+) d([^ ]+)/g,(m,s1,o1,v1,d1)=>`[s${s1} o${f9(o1)} v${f9(v1)} d${f9(d1)}`);
  if(norm(cpp[ci])+'\n'!==o+'\n'&&norm(cpp[ci])!==o){bad++;if(bad<3){const A=norm(cpp[ci]).split('\n'),B=o.split('\n');for(let i=0;i<A.length;i++)if(A[i]!==B[i]){console.log('caso',ci,'línea',i,'\n C++',A[i],'\n JS ',B[i]);break}}}
});
console.log(bad?`FALLO: ${bad} de ${cases.length} casos distintos`:`OK: ${cases.length} casos idénticos (regen + mutación + dado)`);
process.exit(bad?1:0);
