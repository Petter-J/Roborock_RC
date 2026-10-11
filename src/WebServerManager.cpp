#include "WebServerManager.h"
#include "RouteStorage.h"
#include "DrawingStorage.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

static const char *AP_NAME = "Roborock_Web";
static const char *AP_PASSWORD = "roborock123";
static const char *SHOW_FILE = "/show.json";

static WebServer server(80);
static RouteStorage routeStorage;
static DrawingStorage drawingStorage;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sv">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Roborock</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:#202124;color:white}
header{padding:12px;background:#111;text-align:center;font-size:22px}
#main{max-width:1400px;margin:auto;padding:12px}
.toolbar{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:10px}
button,.fileButton{padding:10px 16px;border:0;border-radius:6px;font-size:16px;cursor:pointer;background:#ddd;color:#111}
button.active{background:#4da3ff;color:white}
.fileButton input{display:none}
#workspace{display:flex;gap:10px;align-items:stretch}
#cuePanel{width:180px;min-width:180px;background:#303134;border:1px solid #555;padding:10px}
#cuePanel h3{margin:0 0 10px}
#cueList{display:flex;flex-direction:column;gap:5px}
.cueItem{width:100%;text-align:left;padding:9px;background:#ddd;color:#111}
.cueItem.selected{background:#4da3ff;color:white}
#drawingArea{flex:1;min-width:0}
#drawingInfo,#cueInfo,#positionInfo,#status{margin:8px 0;font-size:15px}
#positionInfo{font-family:monospace;font-size:16px}
#canvasContainer{width:100%;overflow:hidden;background:#ddd;border:2px solid #666}
canvas{display:block;width:100%;height:auto;background:#ddd;touch-action:none;cursor:crosshair}
canvas.dragging{cursor:grabbing}
#status{font-weight:bold}
@media(max-width:700px){#workspace{flex-direction:column}#cuePanel{width:100%}#cueList{flex-direction:row;flex-wrap:wrap}.cueItem{width:auto}}
</style>
</head>
<body>

<header>Roborock Route Editor</header>
<div id="main">

<div class="toolbar">
<label class="fileButton">LADDA DXF<input id="dxfFile" type="file" accept=".dxf"></label>
<button id="saveDrawing">SPARA RITNING</button>
<button id="fitButton">PASSA RITNING</button>
<button id="zoomInButton">+</button>
<button id="zoomOutButton">−</button>
</div>

<div class="toolbar">
<button id="panButton" class="active">PANORERA</button>
<button id="straightButton">RAK</button>
<button id="followButton">FÖLJ</button>
<button id="nextCueButton">NEXT CUE</button>
<button id="undoButton">ÅNGRA</button>
</div>

<div class="toolbar">
<button id="saveShow">SPARA SHOWFIL</button>
<label class="fileButton">LADDA SHOWFIL<input id="showFile" type="file" accept=".json"></label>
</div>

<div id="workspace">
<div id="cuePanel"><h3>CUE-LISTA</h3><div id="cueList"></div></div>
<div id="drawingArea">
<div id="drawingInfo">Ingen ritning laddad</div>
<div id="cueInfo">Cue 1</div>
<div id="positionInfo">X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ---</div>
<div id="canvasContainer"><canvas id="canvas" width="1200" height="800"></canvas></div>
<div id="status">Klar</div>
</div>
</div>

</div>

<script>
const canvas=document.getElementById("canvas"),ctx=canvas.getContext("2d");
const drawingInfo=document.getElementById("drawingInfo"),cueInfo=document.getElementById("cueInfo"),cueList=document.getElementById("cueList");
const positionInfo=document.getElementById("positionInfo"),statusText=document.getElementById("status");
const panButton=document.getElementById("panButton"),straightButton=document.getElementById("straightButton"),followButton=document.getElementById("followButton");

let entities=[],bounds=null,currentDXF="",currentFileName="";
let nodes=[],cues=[],selectedCue=0;
let tool="pan",routeDrawing=false,routeHover=null;
let scale=1,fitScale=1,offsetX=0,offsetY=0;
let dragging=false,dragStartX=0,dragStartY=0,dragOffsetX=0,dragOffsetY=0;

let editMode=null,editNode=-1,editPathIndex=-1;
let editStartWorld=null,editNodeOriginal=null,editSnapshots=[];

const MARGIN=60,SMALL_GRID_MM=250,LARGE_GRID_MM=1000,MIN_ZOOM=.1,MAX_ZOOM=50;
const FOLLOW_STEP_MM=25,START_TOLERANCE_MM=150;
const FOLLOW_SIMPLIFY_MM=120;
const NODE_HIT_PX=17,CONTROL_HIT_PX=12,CURVE_HIT_PX=15;

function setStatus(t){statusText.textContent=t}
function createEmptyCue(){return{name:`Cue ${cues.length+1}`,type:"RAK",startNode:null,endNode:null,path:[]}}
function resetCues(){nodes=[];cues=[createEmptyCue()];selectedCue=0;updateCueList();updateCueInfo()}
function currentCue(){return cues[selectedCue]}

function setTool(t){
    tool=t;routeHover=null;
    panButton.classList.toggle("active",t==="pan");
    straightButton.classList.toggle("active",t==="straight");
    followButton.classList.toggle("active",t==="follow");
    if(t==="pan")setStatus("Panorering / redigering");
    else if(t==="straight")setStatus("RAK: markera start/slut");
    else setStatus("FÖLJ: håll nere och rita körvägen");
    draw();
}

function updateCueList(){
    cueList.innerHTML="";
    cues.forEach((cue,i)=>{
        const b=document.createElement("button");
        b.className="cueItem"+(i===selectedCue?" selected":"");
        b.textContent=`${cue.name} ${cue.type}`;
        b.addEventListener("click",()=>{selectedCue=i;routeHover=null;updateCueList();updateCueInfo();draw()});
        cueList.appendChild(b);
    });
}

function cueLength(cue){
    const pts=sampleCue(cue,12);let d=0;
    for(let i=1;i<pts.length;i++)d+=Math.hypot(pts[i].x-pts[i-1].x,pts[i].y-pts[i-1].y);
    return d;
}

function updateCueInfo(){
    const cue=currentCue();if(!cue)return;
    if(cue.startNode===null){cueInfo.textContent=`${cue.name} | ${cue.type} | ingen start`;return}
    if(cue.endNode===null){cueInfo.textContent=`${cue.name} | ${cue.type} | START klar`;return}
    cueInfo.textContent=`${cue.name} | ${cue.type} | ${(cueLength(cue)/1000).toFixed(2)} m`;
}

function pairLines(text){
    const raw=text.replace(/\r/g,"").split("\n"),pairs=[];
    for(let i=0;i+1<raw.length;i+=2)pairs.push({code:parseInt(raw[i].trim()),value:raw[i+1].trim()});
    return pairs;
}

function parseDXF(text){
    const pairs=pairLines(text),result=[];let i=0,section="";
    while(i<pairs.length){
        const p=pairs[i];
        if(p.code===0&&p.value==="SECTION"){if(i+1<pairs.length&&pairs[i+1].code===2)section=pairs[i+1].value;i+=2;continue}
        if(p.code===0&&p.value==="ENDSEC"){section="";i++;continue}
        if(section!=="ENTITIES"||p.code!==0){i++;continue}
        const type=p.value;

        if(type==="LINE"){
            const e={type:"LINE",x1:0,y1:0,x2:0,y2:0};i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===10)e.x1=+q.value;if(q.code===20)e.y1=+q.value;if(q.code===11)e.x2=+q.value;if(q.code===21)e.y2=+q.value;i++;
            }
            result.push(e);continue;
        }

        if(type==="CIRCLE"){
            const e={type:"CIRCLE",x:0,y:0,r:0};i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===10)e.x=+q.value;if(q.code===20)e.y=+q.value;if(q.code===40)e.r=+q.value;i++;
            }
            result.push(e);continue;
        }

        if(type==="ARC"){
            const e={type:"ARC",x:0,y:0,r:0,start:0,end:0};i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===10)e.x=+q.value;if(q.code===20)e.y=+q.value;if(q.code===40)e.r=+q.value;if(q.code===50)e.start=+q.value;if(q.code===51)e.end=+q.value;i++;
            }
            result.push(e);continue;
        }

        if(type==="LWPOLYLINE"){
            const e={type:"POLYLINE",points:[],closed:false};let cur=null;i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];
                if(q.code===70)e.closed=(+q.value&1)!==0;
                if(q.code===10){cur={x:+q.value,y:0};e.points.push(cur)}
                if(q.code===20&&cur)cur.y=+q.value;
                i++;
            }
            if(e.points.length>1)result.push(e);continue;
        }

        if(type==="POLYLINE"){
            const e={type:"POLYLINE",points:[],closed:false};i++;
            while(i<pairs.length){
                if(pairs[i].code===70)e.closed=(+pairs[i].value&1)!==0;
                if(pairs[i].code===0&&pairs[i].value==="VERTEX"){
                    const pt={x:0,y:0};i++;
                    while(i<pairs.length&&pairs[i].code!==0){if(pairs[i].code===10)pt.x=+pairs[i].value;if(pairs[i].code===20)pt.y=+pairs[i].value;i++}
                    e.points.push(pt);continue;
                }
                if(pairs[i].code===0&&pairs[i].value==="SEQEND"){i++;break}
                i++;
            }
            if(e.points.length>1)result.push(e);continue;
        }

        i++;
    }
    return result;
}

function addBoundsPoint(b,x,y){
    if(!Number.isFinite(x)||!Number.isFinite(y))return;
    b.minX=Math.min(b.minX,x);b.maxX=Math.max(b.maxX,x);b.minY=Math.min(b.minY,y);b.maxY=Math.max(b.maxY,y);
}

function calculateBounds(){
    const b={minX:Infinity,minY:Infinity,maxX:-Infinity,maxY:-Infinity};
    for(const e of entities){
        if(e.type==="LINE"){addBoundsPoint(b,e.x1,e.y1);addBoundsPoint(b,e.x2,e.y2)}
        else if(e.type==="POLYLINE")for(const p of e.points)addBoundsPoint(b,p.x,p.y);
        else if(e.type==="CIRCLE"||e.type==="ARC"){addBoundsPoint(b,e.x-e.r,e.y-e.r);addBoundsPoint(b,e.x+e.r,e.y+e.r)}
    }
    return Number.isFinite(b.minX)?b:null;
}

function loadDrawing(name,dxf){
    currentFileName=name;currentDXF=dxf;entities=parseDXF(dxf);bounds=calculateBounds();
    if(!bounds||!entities.length){setStatus("Ingen stödd geometri hittades");return false}
    showDrawingInfo();fitDrawing();return true;
}

function fitDrawing(){
    if(!bounds)return;
    const w=Math.max(bounds.maxX-bounds.minX,1),h=Math.max(bounds.maxY-bounds.minY,1);
    fitScale=Math.min((canvas.width-MARGIN*2)/w,(canvas.height-MARGIN*2)/h);scale=fitScale;
    const cx=(bounds.minX+bounds.maxX)/2,cy=(bounds.minY+bounds.maxY)/2;
    offsetX=canvas.width/2-cx*scale;offsetY=canvas.height/2+cy*scale;draw();updatePositionInfo();
}

function worldToScreen(x,y){return{x:x*scale+offsetX,y:offsetY-y*scale}}
function screenToWorld(x,y){return{x:(x-offsetX)/scale,y:(offsetY-y)/scale}}

function getVisibleWorld(){
    const a=screenToWorld(0,canvas.height),b=screenToWorld(canvas.width,0);
    return{minX:Math.min(a.x,b.x),maxX:Math.max(a.x,b.x),minY:Math.min(a.y,b.y),maxY:Math.max(a.y,b.y)};
}

function drawGrid(){
    const v=getVisibleWorld();let grid=SMALL_GRID_MM;
    if(grid*scale<12)grid=1000;if(grid*scale<12)grid=5000;
    const sx=Math.floor(v.minX/grid)*grid,ex=Math.ceil(v.maxX/grid)*grid,sy=Math.floor(v.minY/grid)*grid,ey=Math.ceil(v.maxY/grid)*grid;
    ctx.lineWidth=1;

    for(let x=sx;x<=ex;x+=grid){
        const p=worldToScreen(x,0);ctx.strokeStyle=x%LARGE_GRID_MM===0?"#aaa":"#c9c9c9";
        ctx.beginPath();ctx.moveTo(p.x,0);ctx.lineTo(p.x,canvas.height);ctx.stroke();
    }

    for(let y=sy;y<=ey;y+=grid){
        const p=worldToScreen(0,y);ctx.strokeStyle=y%LARGE_GRID_MM===0?"#aaa":"#c9c9c9";
        ctx.beginPath();ctx.moveTo(0,p.y);ctx.lineTo(canvas.width,p.y);ctx.stroke();
    }
}

function drawEntities(){
    ctx.strokeStyle="#222";ctx.lineWidth=2;

    for(const e of entities){
        if(e.type==="LINE"){
            const a=worldToScreen(e.x1,e.y1),b=worldToScreen(e.x2,e.y2);
            ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();
        }else if(e.type==="POLYLINE"){
            if(e.points.length<2)continue;
            let p=worldToScreen(e.points[0].x,e.points[0].y);
            ctx.beginPath();ctx.moveTo(p.x,p.y);
            for(let i=1;i<e.points.length;i++){p=worldToScreen(e.points[i].x,e.points[i].y);ctx.lineTo(p.x,p.y)}
            if(e.closed)ctx.closePath();ctx.stroke();
        }else if(e.type==="CIRCLE"){
            const c=worldToScreen(e.x,e.y);ctx.beginPath();ctx.arc(c.x,c.y,e.r*scale,0,Math.PI*2);ctx.stroke();
        }else if(e.type==="ARC"){
            const c=worldToScreen(e.x,e.y);ctx.beginPath();ctx.arc(c.x,c.y,e.r*scale,-e.end*Math.PI/180,-e.start*Math.PI/180);ctx.stroke();
        }
    }
}

function getCuePoints(cue){
    if(cue.startNode===null)return[];
    const p=[nodes[cue.startNode]];
    for(const x of cue.path)p.push(x);
    if(cue.endNode!==null)p.push(nodes[cue.endNode]);
    return p;
}

function catmullPoint(p0,p1,p2,p3,t){
    const t2=t*t,t3=t2*t;
    return{
        x:.5*((2*p1.x)+(-p0.x+p2.x)*t+(2*p0.x-5*p1.x+4*p2.x-p3.x)*t2+(-p0.x+3*p1.x-3*p2.x+p3.x)*t3),
        y:.5*((2*p1.y)+(-p0.y+p2.y)*t+(2*p0.y-5*p1.y+4*p2.y-p3.y)*t2+(-p0.y+3*p1.y-3*p2.y+p3.y)*t3)
    };
}

function sampleCue(cue,steps=16){
    const pts=getCuePoints(cue);
    if(pts.length<2)return pts.map((p,i)=>({x:p.x,y:p.y,segment:Math.max(0,i-1)}));
    if(pts.length===2)return[{x:pts[0].x,y:pts[0].y,segment:0},{x:pts[1].x,y:pts[1].y,segment:0}];

    const out=[];
    for(let i=0;i<pts.length-1;i++){
        const p0=pts[Math.max(0,i-1)],p1=pts[i],p2=pts[i+1],p3=pts[Math.min(pts.length-1,i+2)];
        for(let s=0;s<steps;s++){
            const p=catmullPoint(p0,p1,p2,p3,s/steps);
            out.push({x:p.x,y:p.y,segment:i});
        }
    }
    const e=pts[pts.length-1];
    out.push({x:e.x,y:e.y,segment:pts.length-2});
    return out;
}

function drawCue(cue,index){
    const pts=getCuePoints(cue);if(!pts.length)return;
    ctx.strokeStyle=index===selectedCue?"#006cff":"#777";
    ctx.lineWidth=index===selectedCue?4:2;

    if(pts.length>1){
        const samples=sampleCue(cue,16);
        let p=worldToScreen(samples[0].x,samples[0].y);
        ctx.beginPath();ctx.moveTo(p.x,p.y);
        for(let i=1;i<samples.length;i++){p=worldToScreen(samples[i].x,samples[i].y);ctx.lineTo(p.x,p.y)}
        ctx.stroke();
    }

    if(index!==selectedCue)return;

    for(let i=0;i<cue.path.length;i++){
        const p=worldToScreen(cue.path[i].x,cue.path[i].y);
        ctx.beginPath();ctx.arc(p.x,p.y,4,0,Math.PI*2);
ctx.fillStyle="#006cff";ctx.fill();
ctx.strokeStyle="#fff";ctx.lineWidth=1;ctx.stroke();
    }

    if(cue.startNode!==null){
        const n=nodes[cue.startNode],p=worldToScreen(n.x,n.y);
        ctx.beginPath();ctx.arc(p.x,p.y,10,0,Math.PI*2);ctx.fillStyle="#00b000";ctx.fill();
    }

    if(cue.endNode!==null){
        const n=nodes[cue.endNode],p=worldToScreen(n.x,n.y);
        ctx.beginPath();ctx.arc(p.x,p.y,10,0,Math.PI*2);ctx.fillStyle="#e00000";ctx.fill();
    }
}

function drawCues(){
    cues.forEach((c,i)=>drawCue(c,i));

    if(!routeHover||tool!=="straight")return;
    const cue=currentCue();
    if(cue.startNode===null||cue.endNode!==null)return;

    const a=worldToScreen(nodes[cue.startNode].x,nodes[cue.startNode].y);
    const b=worldToScreen(routeHover.x,routeHover.y);

    ctx.strokeStyle="#006cff";ctx.lineWidth=2;ctx.setLineDash([8,6]);
    ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();
    ctx.setLineDash([]);

    ctx.beginPath();ctx.arc(b.x,b.y,10,0,Math.PI*2);ctx.fillStyle="#e00000";ctx.fill();
}

function draw(){
    ctx.clearRect(0,0,canvas.width,canvas.height);
    ctx.fillStyle="#ddd";ctx.fillRect(0,0,canvas.width,canvas.height);
    if(!bounds)return;
    drawGrid();drawEntities();drawCues();
}

function addNode(x,y){nodes.push({id:nodes.length,x:Math.round(x),y:Math.round(y)});return nodes.length-1}

function addFollowPoint(cue,x,y){
    const p={x:Math.round(x),y:Math.round(y)};
    const last=cue.path.length?cue.path[cue.path.length-1]:nodes[cue.startNode];
    if(Math.hypot(p.x-last.x,p.y-last.y)<FOLLOW_STEP_MM)return;
    cue.path.push(p);
}

function perpendicularDistance(p,a,b){
    const dx=b.x-a.x,dy=b.y-a.y;
    if(dx===0&&dy===0)return Math.hypot(p.x-a.x,p.y-a.y);
    const t=((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy);
    const x=a.x+t*dx,y=a.y+t*dy;
    return Math.hypot(p.x-x,p.y-y);
}

function simplifyRDP(points,eps){
    if(points.length<3)return points.slice();

    let max=0,index=0;
    for(let i=1;i<points.length-1;i++){
        const d=perpendicularDistance(points[i],points[0],points[points.length-1]);
        if(d>max){max=d;index=i}
    }

    if(max<=eps)return[points[0],points[points.length-1]];

    const a=simplifyRDP(points.slice(0,index+1),eps);
    const b=simplifyRDP(points.slice(index),eps);
    return a.slice(0,-1).concat(b);
}

function smoothFollowCue(cue){
    if(cue.startNode===null||cue.endNode===null)return;

    let pts=[
        {...nodes[cue.startNode]},
        ...cue.path.map(p=>({...p})),
        {...nodes[cue.endNode]}
    ];

    if(pts.length<3){
        cue.path=[];
        return;
    }

    // Jämna bort små hand-/musrörelser.
    for(let pass=0;pass<4;pass++){
        const next=[pts[0]];

        for(let i=1;i<pts.length-1;i++){
            next.push({
                x:pts[i-1].x*.25+pts[i].x*.5+pts[i+1].x*.25,
                y:pts[i-1].y*.25+pts[i].y*.5+pts[i+1].y*.25
            });
        }

        next.push(pts[pts.length-1]);
        pts=next;
    }

    // Reducera alla råpunkter till ett fåtal riktiga kontrollpunkter.
    pts=simplifyRDP(pts,FOLLOW_SIMPLIFY_MM);

    cue.path=pts.slice(1,-1).map(p=>({
        x:Math.round(p.x),
        y:Math.round(p.y)
    }));
}

function distancePointSegment(px,py,ax,ay,bx,by){
    const vx=bx-ax,vy=by-ay,l2=vx*vx+vy*vy;
    let t=l2?((px-ax)*vx+(py-ay)*vy)/l2:0;
    t=Math.max(0,Math.min(1,t));
    const x=ax+t*vx,y=ay+t*vy;
    return{distance:Math.hypot(px-x,py-y),t,x,y};
}

function hitNode(screen,nodeId){
    if(nodeId===null||!nodes[nodeId])return false;
    const p=worldToScreen(nodes[nodeId].x,nodes[nodeId].y);
    return Math.hypot(screen.x-p.x,screen.y-p.y)<=NODE_HIT_PX;
}

function hitControl(screen,cue){
    let best=null;
    for(let i=0;i<cue.path.length;i++){
        const p=worldToScreen(cue.path[i].x,cue.path[i].y);
        const d=Math.hypot(screen.x-p.x,screen.y-p.y);
        if(d<=CONTROL_HIT_PX&&(!best||d<best.distance))best={index:i,distance:d};
    }
    return best;
}

function hitCurve(screen,cue){
    if(cue.startNode===null||cue.endNode===null)return null;

    const samples=sampleCue(cue,20);
    let best=null;

    for(let i=0;i<samples.length-1;i++){
        const a=worldToScreen(samples[i].x,samples[i].y);
        const b=worldToScreen(samples[i+1].x,samples[i+1].y);
        const h=distancePointSegment(screen.x,screen.y,a.x,a.y,b.x,b.y);

        if(h.distance<=CURVE_HIT_PX&&(!best||h.distance<best.distance)){
            const wx=samples[i].x+(samples[i+1].x-samples[i].x)*h.t;
            const wy=samples[i].y+(samples[i+1].y-samples[i].y)*h.t;
            best={distance:h.distance,segment:samples[i].segment,x:wx,y:wy};
        }
    }

    return best;
}

function beginNodeEdit(nodeId,world){
    editMode="node";editNode=nodeId;
    editStartWorld={x:world.x,y:world.y};
    editNodeOriginal={x:nodes[nodeId].x,y:nodes[nodeId].y};
    editSnapshots=[];

    cues.forEach((cue,i)=>{
        if(cue.startNode===nodeId||cue.endNode===nodeId){
            editSnapshots.push({
                cueIndex:i,
                start:cue.startNode===nodeId,
                end:cue.endNode===nodeId,
                path:cue.path.map(p=>({x:p.x,y:p.y}))
            });
        }
    });

    canvas.style.cursor="grabbing";
}

function updateNodeEdit(world){
    const dx=world.x-editStartWorld.x,dy=world.y-editStartWorld.y;

    nodes[editNode].x=Math.round(editNodeOriginal.x+dx);
    nodes[editNode].y=Math.round(editNodeOriginal.y+dy);

    for(const s of editSnapshots){
        const cue=cues[s.cueIndex],n=s.path.length;

        cue.path=s.path.map((p,i)=>{
            const t=(i+1)/(n+1);
            let f=0;
            if(s.start)f=Math.max(f,1-t);
            if(s.end)f=Math.max(f,t);
            return{x:Math.round(p.x+dx*f),y:Math.round(p.y+dy*f)};
        });
    }

    updateCueInfo();draw();
}

function beginControlEdit(index){
    editMode="control";editPathIndex=index;
    canvas.style.cursor="grabbing";
}

function updateControlEdit(world){
    const cue=currentCue();
    if(editPathIndex<0||editPathIndex>=cue.path.length)return;
    cue.path[editPathIndex]={x:Math.round(world.x),y:Math.round(world.y)};
    updateCueInfo();draw();
}

function beginCurveEdit(hit){
    const cue=currentCue();

    cue.path.splice(hit.segment,0,{x:Math.round(hit.x),y:Math.round(hit.y)});
    editMode="control";
    editPathIndex=hit.segment;

    if(cue.type==="RAK")cue.type="KURVA";

    updateCueList();updateCueInfo();
    canvas.style.cursor="grabbing";
}

function endEdit(){
    editMode=null;editNode=-1;editPathIndex=-1;editSnapshots=[];
    canvas.style.cursor="crosshair";
    updateCueInfo();updateCueList();draw();
}

function zoomAt(x,y,factor){
    if(!bounds)return;

    const w=screenToWorld(x,y);
    scale=Math.min(Math.max(scale*factor,fitScale*MIN_ZOOM),fitScale*MAX_ZOOM);
    offsetX=x-w.x*scale;offsetY=y+w.y*scale;

    draw();updatePositionInfo(x,y);
}

function getCanvasPosition(e){
    const r=canvas.getBoundingClientRect();
    return{x:(e.clientX-r.left)*canvas.width/r.width,y:(e.clientY-r.top)*canvas.height/r.height};
}

function updatePositionInfo(x=null,y=null){
    if(!bounds){positionInfo.innerHTML="X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ---";return}

    const z=Math.round(scale/fitScale*100);

    if(x===null||y===null){
        positionInfo.innerHTML=`X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ${z}%`;
        return;
    }

    const w=screenToWorld(x,y);
    positionInfo.innerHTML=`X: ${w.x.toFixed(0)} mm &nbsp; Y: ${w.y.toFixed(0)} mm &nbsp; Zoom: ${z}%`;
}

function showDrawingInfo(){
    const w=bounds.maxX-bounds.minX,h=bounds.maxY-bounds.minY;
    drawingInfo.textContent=`${currentFileName} | ${entities.length} objekt | ${(w/1000).toFixed(2)} × ${(h/1000).toFixed(2)} m`;
}

canvas.addEventListener("wheel",e=>{
    if(!bounds)return;
    e.preventDefault();

    const p=getCanvasPosition(e);
    zoomAt(p.x,p.y,e.deltaY<0?1.15:1/1.15);
},{passive:false});

canvas.addEventListener("pointerdown",e=>{
    if(!bounds)return;

    const p=getCanvasPosition(e);
    const world=screenToWorld(p.x,p.y);
    const cue=currentCue();

    if(tool==="pan"&&cue){
        if(hitNode(p,cue.startNode)){
            beginNodeEdit(cue.startNode,world);
            canvas.setPointerCapture(e.pointerId);
            return;
        }

        if(hitNode(p,cue.endNode)){
            beginNodeEdit(cue.endNode,world);
            canvas.setPointerCapture(e.pointerId);
            return;
        }

        const control=hitControl(p,cue);
        if(control){
            beginControlEdit(control.index);
            canvas.setPointerCapture(e.pointerId);
            setStatus("Flyttar kontrollpunkt");
            return;
        }

        const curve=hitCurve(p,cue);
        if(curve){
            beginCurveEdit(curve);
            canvas.setPointerCapture(e.pointerId);
            setStatus("Ny kontrollpunkt skapad");
            return;
        }
    }

    if(tool==="straight"){
        cue.type="RAK";
        cue.path=[];

        if(cue.startNode===null){
            cue.startNode=addNode(world.x,world.y);
            updateCueInfo();updateCueList();draw();
            setStatus("START markerad - klicka SLUT");
            return;
        }

        if(cue.endNode===null){
            cue.endNode=addNode(world.x,world.y);
            updateCueInfo();updateCueList();draw();
            setStatus("Cue klar - tryck NEXT CUE");
        }

        return;
    }

    if(tool==="follow"){
        if(cue.endNode!==null){
            setStatus("Cue är redan klar");
            return;
        }

        cue.type="FÖLJ";
        cue.path=[];

        if(cue.startNode===null){
            cue.startNode=addNode(world.x,world.y);
        }else{
            const s=nodes[cue.startNode];
            const d=Math.hypot(world.x-s.x,world.y-s.y);

            if(d>START_TOLERANCE_MM){
                setStatus("Börja dra från START-punkten");
                return;
            }
        }

        routeDrawing=true;
        canvas.setPointerCapture(e.pointerId);

        updateCueInfo();updateCueList();draw();
        setStatus("Ritar Cue...");
        return;
    }

    dragging=true;
    dragStartX=p.x;dragStartY=p.y;
    dragOffsetX=offsetX;dragOffsetY=offsetY;

    canvas.classList.add("dragging");
    canvas.setPointerCapture(e.pointerId);
});

canvas.addEventListener("pointermove",e=>{
    if(!bounds)return;

    const p=getCanvasPosition(e);
    const world=screenToWorld(p.x,p.y);

    if(editMode==="node"){
        updateNodeEdit(world);
    }else if(editMode==="control"){
        updateControlEdit(world);
    }else if(routeDrawing){
        addFollowPoint(currentCue(),world.x,world.y);
        draw();
    }else if(tool==="straight"){
        routeHover=world;
        draw();
    }else if(dragging){
        offsetX=dragOffsetX+p.x-dragStartX;
        offsetY=dragOffsetY+p.y-dragStartY;
        draw();
    }else if(tool==="pan"){
        const cue=currentCue();
        if(cue&&(hitNode(p,cue.startNode)||hitNode(p,cue.endNode)||hitControl(p,cue)||hitCurve(p,cue)))canvas.style.cursor="grab";
        else canvas.style.cursor="crosshair";
    }

    updatePositionInfo(p.x,p.y);
});

function stopPointer(e){
    if(editMode){
        endEdit();
    }else if(routeDrawing){
        routeDrawing=false;

        const p=getCanvasPosition(e);
        const world=screenToWorld(p.x,p.y);
        const cue=currentCue();

        cue.endNode=addNode(world.x,world.y);
        smoothFollowCue(cue);

        updateCueInfo();updateCueList();draw();
        setStatus("Cue klar och utjämnad - tryck NEXT CUE");
    }

    dragging=false;
    canvas.classList.remove("dragging");

    if(e&&canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
}

canvas.addEventListener("pointerup",stopPointer);
canvas.addEventListener("pointercancel",stopPointer);
canvas.addEventListener("pointerleave",()=>{
    routeHover=null;
    if(!dragging&&!routeDrawing&&!editMode)updatePositionInfo();
    draw();
});

panButton.addEventListener("click",()=>setTool("pan"));

straightButton.addEventListener("click",()=>{
    currentCue().type="RAK";
    updateCueList();updateCueInfo();
    setTool("straight");
});

followButton.addEventListener("click",()=>{
    currentCue().type="FÖLJ";
    updateCueList();updateCueInfo();
    setTool("follow");
});

document.getElementById("nextCueButton").addEventListener("click",()=>{
    const last=cues[cues.length-1];

    if(last.endNode===null){
        selectedCue=cues.length-1;
        updateCueList();updateCueInfo();draw();
        setStatus("Gör färdigt sista Cue först");
        return;
    }

    const next=createEmptyCue();
    next.startNode=last.endNode;

    cues.push(next);
    selectedCue=cues.length-1;

    updateCueList();updateCueInfo();draw();
    setStatus(`Cue ${selectedCue+1} skapad - start är föregående slutpunkt`);
});

document.getElementById("undoButton").addEventListener("click",()=>{
    const cue=currentCue();

    if(cue.path.length){
        cue.path.pop();
        if(!cue.path.length&&cue.type==="KURVA")cue.type="RAK";
        updateCueList();updateCueInfo();draw();
        setStatus("Senaste kontrollpunkt borttagen");
        return;
    }

    if(cue.endNode!==null){
        const end=cue.endNode;
        cue.endNode=null;

        if(selectedCue+1<cues.length&&cues[selectedCue+1].startNode===end)cues.splice(selectedCue+1);

        updateCueList();updateCueInfo();draw();
        setStatus("Slutpunkt borttagen");
        return;
    }

    if(cue.startNode!==null&&selectedCue===0){
        cue.startNode=null;
        updateCueInfo();draw();
        setStatus("Startpunkt borttagen");
    }
});

document.getElementById("fitButton").addEventListener("click",fitDrawing);
document.getElementById("zoomInButton").addEventListener("click",()=>zoomAt(canvas.width/2,canvas.height/2,1.25));
document.getElementById("zoomOutButton").addEventListener("click",()=>zoomAt(canvas.width/2,canvas.height/2,1/1.25));

document.getElementById("dxfFile").addEventListener("change",async e=>{
    const file=e.target.files[0];if(!file)return;

    try{
        const dxf=await file.text();
        resetCues();

        if(loadDrawing(file.name,dxf))setStatus("DXF laddad - ej sparad i ESP32");
    }catch{
        setStatus("Fel vid DXF-import");
    }
});

document.getElementById("saveDrawing").addEventListener("click",async()=>{
    if(!currentDXF){setStatus("Ingen ritning att spara");return}

    try{
        const r=await fetch("/drawing",{
            method:"POST",
            headers:{"Content-Type":"application/octet-stream","X-Filename":encodeURIComponent(currentFileName)},
            body:currentDXF
        });

        setStatus(await r.text());
    }catch{
        setStatus("Fel vid sparning");
    }
});

document.getElementById("saveShow").addEventListener("click",async()=>{
    if(!currentDXF){setStatus("Ingen ritning laddad");return}

    const show={version:4,drawing:{fileName:currentFileName,dxf:currentDXF},nodes:nodes,cues:cues};
    const json=JSON.stringify(show);

    setStatus("Sparar show...");

    try{
        const r=await fetch("/show",{method:"POST",headers:{"Content-Type":"application/json"},body:json});

        if(!r.ok){
            setStatus("Fel vid sparning till ESP32");
            return;
        }

        const blob=new Blob([json],{type:"application/json"});
        const link=document.createElement("a");

        link.href=URL.createObjectURL(blob);
        link.download="Roborock_Show.json";
        link.click();

        URL.revokeObjectURL(link.href);
        setStatus("Show sparad i ESP32 och på datorn");
    }catch{
        setStatus("Fel vid sparning av show");
    }
});

document.getElementById("showFile").addEventListener("change",async e=>{
    const file=e.target.files[0];if(!file)return;

    try{
        const show=JSON.parse(await file.text());

        if(!show.drawing||!show.drawing.dxf){
            setStatus("Ogiltig showfil");
            return;
        }

        if(show.nodes&&show.cues){
            nodes=show.nodes;
            cues=show.cues;
        }else{
            resetCues();
        }

        selectedCue=0;
        updateCueList();updateCueInfo();

        if(loadDrawing(show.drawing.fileName,show.drawing.dxf))setStatus("Showfil laddad från dator");
    }catch{
        setStatus("Felaktig showfil");
    }
});

async function loadSavedShow(){
    try{
        const r=await fetch("/show");
        if(!r.ok)return false;

        const show=await r.json();
        if(!show.drawing||!show.drawing.dxf)return false;

        nodes=show.nodes||[];
        cues=show.cues||[];

        if(!cues.length)cues=[createEmptyCue()];

        selectedCue=0;
        updateCueList();updateCueInfo();

        if(loadDrawing(show.drawing.fileName,show.drawing.dxf)){
            setStatus("Sparad show laddad från ESP32");
            return true;
        }
    }catch{}

    return false;
}

async function loadSavedDrawing(){
    try{
        const r=await fetch("/drawing");
        if(!r.ok)return;

        const name=decodeURIComponent(r.headers.get("X-Filename")||"drawing.dxf");
        const dxf=await r.text();

        if(loadDrawing(name,dxf))setStatus("Sparad ritning laddad");
    }catch{}
}

resetCues();
draw();
loadSavedShow().then(ok=>{if(!ok)loadSavedDrawing()});
</script>
</body>
</html>
)rawliteral";

static bool saveShowToFlash(const String &data)
{
    File f = LittleFS.open(SHOW_FILE, "w");
    if (!f)
        return false;
    size_t n = f.print(data);
    f.close();
    return n == data.length();
}

static String loadShowFromFlash()
{
    if (!LittleFS.exists(SHOW_FILE))
        return "";
    File f = LittleFS.open(SHOW_FILE, "r");
    if (!f)
        return "";
    String s = f.readString();
    f.close();
    return s;
}

static void handleRoot()
{
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleGetDrawing()
{
    if (!drawingStorage.exists())
    {
        server.send(404, "text/plain", "");
        return;
    }

    server.sendHeader("X-Filename", drawingStorage.loadName());
    server.send(200, "application/octet-stream", drawingStorage.loadDXF());
}

static void handleSaveDrawing()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "text/plain", "Ingen ritning mottagen");
        return;
    }

    String name = server.header("X-Filename");
    if (!name.length())
        name = "drawing.dxf";
    name.replace("%20", " ");

    if (!drawingStorage.save(name, server.arg("plain")))
    {
        server.send(500, "text/plain", "Fel vid sparning");
        return;
    }

    Serial.printf("Drawing saved: %u bytes\n", server.arg("plain").length());
    server.send(200, "text/plain", "Ritning sparad i ESP32");
}

static void handleSaveShow()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "text/plain", "Ingen show mottagen");
        return;
    }

    String data = server.arg("plain");

    if (!saveShowToFlash(data))
    {
        server.send(500, "text/plain", "Fel vid sparning av show");
        return;
    }

    Serial.printf("Show saved: %u bytes\n", data.length());
    server.send(200, "text/plain", "Show sparad i ESP32");
}

static void handleGetShow()
{
    String data = loadShowFromFlash();

    if (!data.length())
    {
        server.send(404, "application/json", "");
        return;
    }

    server.send(200, "application/json", data);
}

bool WebServerManager::begin()
{
    if (!routeStorage.begin())
    {
        Serial.println("RouteStorage FAILED");
        return false;
    }

    if (!drawingStorage.begin())
    {
        Serial.println("DrawingStorage FAILED");
        return false;
    }

    WiFi.mode(WIFI_AP);
    delay(100);

    if (!WiFi.softAP(AP_NAME, AP_PASSWORD))
    {
        Serial.println("WiFi AP FAILED");
        return false;
    }

    const char *headers[] = {"X-Filename"};
    server.collectHeaders(headers, 1);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/drawing", HTTP_GET, handleGetDrawing);
    server.on("/drawing", HTTP_POST, handleSaveDrawing);
    server.on("/show", HTTP_GET, handleGetShow);
    server.on("/show", HTTP_POST, handleSaveShow);

    server.begin();

    Serial.println("\nWEB SERVER READY");
    Serial.printf("WiFi: %s\nPassword: %s\n", AP_NAME, AP_PASSWORD);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    return true;
}

void WebServerManager::update()
{
    server.handleClient();
}