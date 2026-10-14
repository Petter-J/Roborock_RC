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
body{margin:0;font-family:Arial,sans-serif;background:#202124;color:#fff}
header{padding:12px;background:#111;text-align:center;font-size:22px}
#main{max-width:1400px;margin:auto;padding:12px}
.toolbar{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:10px}
button,.fileButton{padding:10px 16px;border:0;border-radius:6px;font-size:16px;cursor:pointer;background:#ddd;color:#111}
button.active{background:#4da3ff;color:#fff}.fileButton input{display:none}
#workspace{display:flex;gap:10px}
#cuePanel{width:190px;min-width:190px;background:#303134;border:1px solid #555;padding:10px}
#cuePanel h3{margin:0 0 10px}#cueList{display:flex;flex-direction:column;gap:5px}
.cueItem{width:100%;text-align:left;padding:9px}.cueItem.selected{background:#4da3ff;color:#fff}
#drawingArea{flex:1;min-width:0}
#drawingInfo,#cueInfo,#positionInfo,#status{margin:8px 0;font-size:15px}
#positionInfo{font-family:monospace}
#canvasContainer{width:100%;overflow:hidden;background:#ddd;border:2px solid #666}
canvas{display:block;width:100%;height:auto;background:#ddd;touch-action:none;cursor:crosshair}
canvas.dragging{cursor:grabbing}#status{font-weight:bold}

#canvasRow{display:flex;gap:10px;align-items:stretch}
#canvasContainer{flex:1;min-width:0}
#drivePanel{width:120px;display:flex;flex-direction:column;align-items:center;gap:10px}
#driveDisplay{width:115px;background:#303134;color:#fff;border:1px solid #666;border-radius:5px;padding:7px 4px;text-align:center}
#driveDisplay.simActive{background:#16853b;border-color:#26c75a}
#driveCueName{font-weight:bold;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#driveSegmentInfo{font-family:monospace;font-size:13px;margin-top:4px;color:#ddd;white-space:nowrap}
#cueButtons{display:flex;gap:7px}
#cueButtons button{width:52px;height:42px;padding:0;font-size:24px}
#cueButtons button:disabled{opacity:.35;cursor:default}
#cueButtons button.runAllowed{background:#20b84b;color:#fff;opacity:1;}
#cueButtons button.runSelected{background:#20b84b;color:#fff;opacity:1;}


#driveSlider{
    writing-mode:vertical-lr;
    direction:rtl;
    width:25px;
    height:300px;
    appearance:none;
    border-radius:5px;
}

#driveSlider::-webkit-slider-thumb{
    appearance:none;
    width:22px;height:22px;
    border-radius:50%;
    background:#777;
}
#driveSlider::-moz-range-thumb{
    width:22px;height:22px;border:0;
    border-radius:50%;
    background:#777;
}
#driveSlider.simActive::-webkit-slider-thumb{background:#20b84b}
#driveSlider.simActive::-moz-range-thumb{background:#20b84b}

#driveValue{font-family:monospace;font-size:15px}

dialog{border:0;border-radius:8px;padding:18px;background:#303134;color:#fff;min-width:380px}
dialog::backdrop{background:#0008}
.cfg{display:flex;flex-direction:column;gap:9px}.cfg h3{margin:0 0 8px}
.cfg label{display:flex;justify-content:space-between;align-items:center;gap:18px}
.cfg input{width:145px;padding:7px;font-size:16px}
.cfg .buttons{display:flex;justify-content:flex-end;gap:8px;margin-top:10px}
.xy{display:flex;gap:12px}.xy label{display:block}.xy input{display:block;width:135px;margin-top:4px}
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
<button id="vehicleButton">VAGN CONFIG</button>
<button id="zoomInButton">+</button>
<button id="zoomOutButton">−</button>
</div>

<div class="toolbar">
<button id="panButton" class="active">REDIGERA</button>
<button id="newCueButton">NY CUE</button>
<button id="straightButton">RAK</button>
<button id="rotateButton">ROT</button>
<button id="reverseButton">VÄND</button>
<button id="followButton">FÖLJ</button>
<button id="splitButton">SPLIT SEG</button>
<button id="endCueButton">SLUT CUE</button>
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
<div id="cueInfo">Ingen Cue</div>
<div id="positionInfo">X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ---</div>
<div id="canvasRow">
<div id="canvasContainer"><canvas id="canvas" width="1200" height="800"></canvas></div>
<div id="drivePanel">
<button id="driveDisplay" type="button">
<div id="driveCueName">Ingen Cue</div>
<div id="driveSegmentInfo">--.</div>
</button>
<div id="cueButtons"><button id="prevCueButton" disabled>−</button><button id="nextCueButton" disabled>+</button></div>
<input id="driveSlider" type="range" min="-100" max="100" value="0">
<div id="driveValue">0</div>
</div>
</div>
<div id="status">Klar</div>
</div>
</div>
</div>

<dialog id="vehicleDialog">
<form method="dialog" class="cfg">
<h3>VAGN CONFIG</h3>
<label>Vagnnamn <input id="vName"></label>
<label>Längd mm <input id="vLength" type="number"></label>
<label>Bredd mm <input id="vWidth" type="number"></label>
<label>Diameter mm <input id="vDiameter" type="number"></label>
<label>Hjuldiameter mm <input id="vWheelDia" type="number"></label>
<label>Hjul c/c mm <input id="vTrack" type="number"></label>
<label>Utväxling motor:hjul <input id="vGear" type="number" step="0.01"></label>
<label>Max motorvarv rpm <input id="vRpm" type="number"></label>
<label>Encoder pulser/varv <input id="vEncoder" type="number" step="0.1"></label>
<div class="buttons"><button value="cancel">AVBRYT</button><button id="vehicleSave" value="default">SPARA</button></div>
</form>
</dialog>

<dialog id="nodeDialog">
<form method="dialog" class="cfg">
<h3 id="nodeTitle">PUNKT</h3>
<div class="xy">
<label>X mm<input id="nodeX" type="number"></label>
<label>Y mm<input id="nodeY" type="number"></label>
</div>
<div class="buttons"><button value="cancel">AVBRYT</button><button id="nodeSave" value="default">SPARA</button></div>
</form>
</dialog>

<dialog id="rotateDialog">
<form method="dialog" class="cfg">
<h3>ROTATION</h3>
<label>Grader <input id="rotateDeg" type="number" step="1"></label>
<label>Vänta efter <input id="rotateWait" type="number" min="0" step="0.1" value="0"></label>
<div class="buttons"><button value="cancel">AVBRYT</button><button id="rotateSave" value="default">SPARA</button></div>
</form>
</dialog>

<dialog id="reverseDialog">
<form method="dialog" class="cfg">
<h3>VÄND</h3>

<div class="buttons">
<button id="reverseCW" value="default">+ MEDSOLS</button>
<button id="reverseCCW" value="default">- MOTSOLS</button>
</div>

<div class="buttons">
<button value="cancel">AVBRYT</button>
</div>

</form>
</dialog>

<script>
const $=id=>document.getElementById(id),canvas=$("canvas"),ctx=canvas.getContext("2d");
const drawingInfo=$("drawingInfo"),cueInfo=$("cueInfo"),cueList=$("cueList"),positionInfo=$("positionInfo"),statusText=$("status");
const panButton=$("panButton"),straightButton=$("straightButton"),followButton=$("followButton");

let entities=[],bounds=null,currentDXF="",currentFileName="",nodes=[],cues=[],selectedCue=-1,cueNumber=0,activeCueName="";
let tool="pan",routeDrawing=false,routeHover=null,splitAfter=-1;
let scale=1,fitScale=1,offsetX=0,offsetY=0,dragging=false,dragStartX=0,dragStartY=0,dragOffsetX=0,dragOffsetY=0;
let editMode=null,editPathIndex=-1,editNodeId=-1,editNodeLabel="",editNodeMoved=false,editNodeDownX=0,editNodeDownY=0;
let dialogNodeId=-1,dialogNodeLabel="";
let rotateTarget=null,rotateInsert=false;

let vehicle={
    name:"Roborock",
    lengthMm:350,widthMm:350,diameterMm:350,
    wheelDiameterMm:70,trackWidthMm:280,
    gearRatio:1,maxMotorRpm:90,encoderPulsesPerRev:232.7
};

const MARGIN=60,SMALL_GRID_MM=250,LARGE_GRID_MM=1000,MIN_ZOOM=.1,MAX_ZOOM=50;
const FOLLOW_STEP_MM=25,FOLLOW_SIMPLIFY_MM=120,START_TOLERANCE_MM=150;
const NODE_HIT_PX=17,CONTROL_HIT_PX=12,CURVE_HIT_PX=15;

function setStatus(t){statusText.textContent=t}

function createEmptyCue(name,type="RAK"){
    return{
        name,type,
        startNode:null,
        endNode:null,
        path:[],
        rotateDeg:0,
        vandDir:1,
        waitSec:0,
        endOfCue:false
    };
}
function currentCue(){return selectedCue>=0?cues[selectedCue]:null}

function lastSegment(){return cues.length?cues[cues.length-1]:null}

function updateSegmentButtons(){
    const cue=currentCue();

    const canAppend=
        cue &&
        selectedCue===cues.length-1 &&
        !cue.endOfCue &&
        cue.endNode!==null;

    const canInsert=splitAfter>=0;

    $("reverseButton").disabled=!(canAppend||canInsert);
}

function resetCues(){
    nodes=[];cues=[];selectedCue=-1;cueNumber=0;activeCueName="";splitAfter=-1;
    updateCueList();updateCueInfo();
}

function connectFrom(index,nodeId){
    if(index<0||index>=cues.length||nodeId===null)return;

    const c=cues[index];
    c.startNode=nodeId;

    if(c.type==="VÄND"){
        c.endNode=nodeId;
        connectFrom(index+1,nodeId);
        return;
    }

    if(c.type==="ROT"){
        const n=nodes[nodeId];

        if(c.endNode===null||!nodes[c.endNode])
            c.endNode=addNode(n.x,n.y);
        else{
            nodes[c.endNode].x=n.x;
            nodes[c.endNode].y=n.y;
        }

        connectFrom(index+1,c.endNode);
    }
}

function connectNext(cue){
    const i=cues.indexOf(cue);
    if(i>=0&&cue.endNode!==null)connectFrom(i+1,cue.endNode);
}

function createNextSegment(type){
    const last=lastSegment();
    if(!last){setStatus("Skapa NY CUE först");return null}
    if(last.endOfCue){setStatus("Cue är avslutad - skapa NY CUE");return null}
    if(last.endNode===null){last.type=type;selectedCue=cues.length-1;return last}
    const seg=createEmptyCue(last.name,type);seg.startNode=last.endNode;
    cues.push(seg);selectedCue=cues.length-1;updateCueList();updateCueInfo();draw();return seg;
}

function insertSegment(type){
    if(splitAfter<0)return null;
    const old=cues[splitAfter],seg=createEmptyCue(old.name,type);seg.startNode=old.endNode;
    if(old.endOfCue){old.endOfCue=false;seg.endOfCue=true}
    cues.splice(splitAfter+1,0,seg);selectedCue=splitAfter+1;splitAfter=-1;
    updateCueList();updateCueInfo();draw();return seg;
}

function setTool(t){
    tool=t;routeHover=null;
    panButton.classList.toggle("active",t==="pan");
    straightButton.classList.toggle("active",t==="straight");
    followButton.classList.toggle("active",t==="follow");
    setStatus(t==="pan"?"Panorering / redigering":t==="straight"?"RAK: markera slutpunkt":"FÖLJ: håll nere och rita körvägen");
    draw();
}

function renameCue(index){
    if(index<0||index>=cues.length)return;
    const old=cues[index].name,name=prompt("Nytt Cue-namn:",old);
    if(name===null)return;
    const n=name.trim();if(!n||n===old)return;

    let a=index,b=index;
    while(a>0&&cues[a-1].name===old)a--;
    while(b+1<cues.length&&cues[b+1].name===old)b++;

    for(let i=a;i<=b;i++)cues[i].name=n;
    if(activeCueName===old)activeCueName=n;

    updateCueList();updateCueInfo();draw();
    setStatus(`Cue ändrad: ${old} → ${n}`);
}

function updateCueList(){
    cueList.innerHTML="";let prev="",step=0;
    cues.forEach((cue,i)=>{
        if(cue.name!==prev){
            prev=cue.name;step=1;
            const h=document.createElement("div");
            Object.assign(h.style,{marginTop:"10px",marginBottom:"4px",fontWeight:"bold"});
            h.textContent=cue.name;
h.style.cursor="pointer";
h.title="Klicka för att ändra Cue-namn";
h.onclick=()=>renameCue(i);
cueList.appendChild(h);
        }else step++;
        const b=document.createElement("button");
        b.className="cueItem"+(i===selectedCue?" selected":"");
        b.textContent=`${step}. ${cue.type}`+(cue.type==="ROT"?` ${cue.rotateDeg>0?"+":""}${cue.rotateDeg}°`:"")+(cue.endOfCue?" ■":"");
        b.onclick=()=>{selectedCue=i;splitAfter=-1;routeHover=null;updateCueList();updateCueInfo();draw()};
        cueList.appendChild(b);
    });
    updateSegmentButtons();
}

function cueLength(cue){
    if(cue.type==="ROT"||cue.type==="VÄND")return 0;
    const p=sampleCue(cue,12);let d=0;
    for(let i=1;i<p.length;i++)d+=Math.hypot(p[i].x-p[i-1].x,p[i].y-p[i-1].y);
    return d;
}

function cueSegmentNumber(index){
    if(index<0||index>=cues.length)return 0;
    const name=cues[index].name;let n=0;
    for(let i=0;i<=index;i++)if(cues[i].name===name)n++;
    return n;
}

function updateDriveDisplay(){
    const cue=currentCue();
    if(!cue){
        $("driveCueName").textContent="Ingen Cue";
        $("driveSegmentInfo").textContent="--.";
        return;
    }

    const nr=cueSegmentNumber(selectedCue);
$("driveCueName").textContent=cue.name;

if(cue.type==="ROT")
    $("driveSegmentInfo").textContent=`${nr}. ROT ${Math.abs(cue.rotateDeg)}°`;
else if(cue.type==="VÄND")
    $("driveSegmentInfo").textContent=`${nr}. VÄND`;
else
    $("driveSegmentInfo").textContent=`${nr}. ${cue.type} ${(cueLength(cue)/1000).toFixed(2)}m`;
}

function updateCueInfo(){
    updateDriveDisplay();
    const cue=currentCue();
    if(!cue){cueInfo.textContent="Ingen Cue";return}
    if(cue.type==="ROT"){cueInfo.textContent=`${cue.name} | ROT ${cue.rotateDeg>0?"+":""}${cue.rotateDeg}°`;return}
    if(cue.startNode===null){cueInfo.textContent=`${cue.name} | ${cue.type} | ingen start`;return}
    if(cue.endNode===null){cueInfo.textContent=`${cue.name} | ${cue.type} | START klar`;return}
    cueInfo.textContent=`${cue.name} | ${cue.type} | ${(cueLength(cue)/1000).toFixed(2)} m`;
}

function selectCue(dir){
    if(!cues.length)return;
    const name=currentCue()?.name;let i=selectedCue;

    if(dir>0){
        while(i<cues.length&&cues[i].name===name)i++;
        if(i>=cues.length)return;
    }else{
        i--;
        while(i>=0&&cues[i].name===name)i--;
        if(i<0)return;
        const prev=cues[i].name;
        while(i>0&&cues[i-1].name===prev)i--;
    }

    selectedCue=i;splitAfter=-1;
    updateCueList();updateCueInfo();draw();
}

function updateRunButtons(){
    const prev=$("prevCueButton");
    const next=$("nextCueButton");

    prev.classList.remove("runAllowed","runSelected");
    next.classList.remove("runAllowed","runSelected");

    if(!simActive)return;

    // Vid Cue-gräns är tillåtna val vita
    if(!simRunEnabled)return;

    // Den riktning jag valt blir grön direkt
    if(simRunDir>0)
        next.classList.add("runSelected");
    else if(simRunDir<0)
        prev.classList.add("runSelected");

    // Först när vi faktiskt börjat röra oss
    // blir motsatt riktning också tillåten/grön
    if(simMovedAfterArm){
        prev.classList.add("runAllowed");
        next.classList.add("runAllowed");
    }
}

function updateBoundaryButtons(){
    const prev=$("prevCueButton");
    const next=$("nextCueButton");

    if(!simActive||simCueIndex<0)return;

    const name=cues[simCueIndex].name;

    if(simBoundary<0){
        // Står vid START på aktuell Cue

        let i=simCueIndex-1;
        while(i>=0&&cues[i].name===name)i--;

        // + kör aktuell Cue framåt
        next.disabled=false;

        // - bara om det finns en tidigare Cue
        prev.disabled=i<0;

    }else if(simBoundary>0){
        // Står vid SLUT på aktuell Cue

        let i=simCueIndex+1;
        while(i<cues.length&&cues[i].name===name)i++;

        // - backar aktuell Cue
        prev.disabled=false;

        // + bara om det finns en nästa Cue
        next.disabled=i>=cues.length;
    }

    updateRunButtons();
}

$("nextCueButton").onclick=()=>{
    if(!simActive||simRunEnabled)return;

    if(simBoundary<0){
        // står vid början av aktuell cue -> kör den framåt
        setSimSegment(cueStartIndex(simCueIndex),false,true);
    }else{
        // står vid slutet -> välj nästa cue
        const currentName=cues[simCueIndex].name;
        let i=simCueIndex+1;

        while(i<cues.length&&cues[i].name===currentName)i++;
        if(i>=cues.length){
            setStatus("Ingen nästa Cue");
            return;
        }

        setSimSegment(i,false,true);
    }

    simBoundary=0;
    simRunDir=1;
    simRunEnabled=true;
    simMovedAfterArm=false;

    $("prevCueButton").disabled=true;
    $("nextCueButton").disabled=true;

    updateRunButtons();

    setStatus(`${cues[simCueIndex].name} - kör framåt`);
};

$("prevCueButton").onclick=()=>{
    if(!simActive||simRunEnabled)return;

    if(simBoundary>0){
        // står vid slutet av aktuell cue -> backa samma cue
        let i=simCueIndex;
        const name=cues[i].name;

        while(i+1<cues.length&&cues[i+1].name===name)i++;

        setSimSegment(i,true,true);
    }else{
        // står vid starten -> välj föregående cue och börja vid dess slut
        const currentName=cues[simCueIndex].name;
        let i=simCueIndex-1;

        while(i>=0&&cues[i].name===currentName)i--;

        if(i<0){
            setStatus("Ingen föregående Cue");
            return;
        }

        const prevName=cues[i].name;
        while(i+1<cues.length&&cues[i+1].name===prevName)i++;

        setSimSegment(i,true,true);
    }

    simBoundary=0;
    simRunDir=-1;
    simRunEnabled=true;
    simMovedAfterArm=false;

    $("prevCueButton").disabled=true;
    $("nextCueButton").disabled=true;

    updateRunButtons();

    setStatus(`${cues[simCueIndex].name} - backa`);
};

const driveSlider=$("driveSlider"),driveValue=$("driveValue");

let sliderPct=0,simActive=false,simPose=null,simCueIndex=-1;
let simProgress=0,simLastTime=0,simFrame=0;
let simAlign=false,simAlignTarget=0,simReverse=false;

let simRotStartHeading=0;
let simRotFinalDeg=0;
let simRotWaitDeg=0;
let simRotWaitDone=false;
let simRotWaitUntil=0;
let simRotDir=0;
let simMovedAfterArm=false;
let simVandStartHeading=0;
let simVandTargetHeading=0;
let simVandTotalDeg=0;
let simVandTurnDir=0;

let simBoundary=0,simRunEnabled=false,simRunDir=0;

const SIM_SPEED_MM_S=1000,SIM_ROT_DEG_S=180;

function setDriveSlider(v){
    let x=+v;

    if(!simActive||!simRunEnabled)x=0;

    sliderPct=x;
    driveSlider.value=x;
    driveValue.textContent=x;

    const p=50-x/2,a=Math.min(50,p),b=Math.max(50,p);
    const c=simActive?"#20b84b":"#777";

    driveSlider.style.background=
        `linear-gradient(to bottom,#ddd 0%,#ddd ${a}%,${c} ${a}%,${c} ${b}%,#ddd ${b}%,#ddd 100%)`;
}

function cueStartIndex(index){
    if(index<0||index>=cues.length)return -1;
    const name=cues[index].name;
    while(index>0&&cues[index-1].name===name)index--;
    return index;
}

function segmentStartHeading(cue){
    const pts=getCuePoints(cue);
    if(pts.length<2)return 0;
    return Math.atan2(pts[1].y-pts[0].y,pts[1].x-pts[0].x);
}

function startSimAtCue(index){
    const i=cueStartIndex(index);if(i<0)return;
    setSimSegment(i,false);
}

function simPath(cue){
    const p=sampleCue(cue,30),out=[],cum=[0];let total=0;
    for(let i=1;i<p.length;i++){total+=Math.hypot(p[i].x-p[i-1].x,p[i].y-p[i-1].y);cum.push(total)}
    return{p,cum,total};
}

function poseOnPath(cue,d){
    const s=simPath(cue);if(s.p.length<2)return null;
    d=Math.max(0,Math.min(s.total,d));

    let i=1;
    while(i<s.cum.length&&s.cum[i]<d)i++;
    i=Math.min(i,s.p.length-1);

    const a=s.p[i-1],b=s.p[i],d0=s.cum[i-1],d1=s.cum[i];
    const t=d1>d0?(d-d0)/(d1-d0):0;

    return{
        x:a.x+(b.x-a.x)*t,
        y:a.y+(b.y-a.y)*t,
        heading:Math.atan2(b.y-a.y,b.x-a.x)
    };
}

function headingBefore(index){
    let h=0;
    for(let i=0;i<index;i++){
        const c=cues[i];
        if(c.type==="ROT")h-=c.rotateDeg*Math.PI/180;
        else if(c.endNode!==null)h=segmentEndHeading(c);
    }
    return h;
}

function normAngle(a){
    while(a>Math.PI)a-=Math.PI*2;
    while(a<-Math.PI)a+=Math.PI*2;
    return a;
}

function vandPathHeading(index){
    for(let i=index+1;i<cues.length;i++){
        if(cues[i].name!==cues[index].name)break;

        const c=cues[i];

        if(c.type==="ROT"||c.type==="VÄND")
            continue;

        const p=poseOnPath(c,0);

        if(p)
            return p.heading;
    }

    return null;
}

function vandTargetHeading(index){
    const newReverse=!simReverse;

    for(let i=index+1;i<cues.length;i++){
        if(cues[i].name!==cues[index].name)break;

        if(cues[i].type==="ROT"||cues[i].type==="VÄND")
            continue;

        const p=poseOnPath(cues[i],0);

        if(p)
            return p.heading+(newReverse?Math.PI:0);
    }

    return null;
}

function positiveAngle(a){
    const full=Math.PI*2;
    a%=full;
    if(a<0)a+=full;
    return a;
}

function angleInDirection(from,to,dir){
    return dir>0
        ? positiveAngle(to-from)
        : positiveAngle(from-to);
}

function nextMoveHeading(index){
    const i=index+1;

    if(i>=cues.length)return null;
    if(cues[i].name!==cues[index].name)return null;

    const c=cues[i];

    // Två ROT efter varandra skall köras separat.
    if(c.type==="ROT"||c.type==="VÄND")return null;

    const p=poseOnPath(c,0);
    return p?driveHeading(p.heading):null;
}

function makeRotPlan(index,startHeading){
    const cue=cues[index];
    const programmed=Math.abs(cue.rotateDeg);
    const dir=cue.rotateDeg>=0?-1:1;
    const target=nextMoveHeading(index);

    // Ingen direkt körsekvens efter ROT.
    if(target===null){
        return{
            dir,
            waitDeg:programmed,
            finalDeg:programmed
        };
    }

    const firstHit=
        angleInDirection(startHeading,target,dir)*180/Math.PI;

    // Under ett varv får rätt körriktning avsluta ROT tidigare.
    if(programmed<360&&firstHit<=programmed+0.001){
        return{
            dir,
            waitDeg:firstHit,
            finalDeg:firstHit
        };
    }

    // Annars gör först hela programmerade ROT.
    const headingAfterProgrammed=
        startHeading+dir*programmed*Math.PI/180;

    const extra=
        angleInDirection(headingAfterProgrammed,target,dir)*180/Math.PI;

    return{
        dir,
        waitDeg:programmed,
        finalDeg:programmed+extra
    };
}

function directedAngleRemaining(current,target,dir){
    const full=Math.PI*2;
    let d=dir>0 ? target-current : current-target;
    d%=full;
    if(d<0)d+=full;
    return d;
}

function driveHeading(pathHeading){
    return pathHeading+(simReverse?Math.PI:0);
}


function simSegmentHeading(cue,atEnd=false){
    if(cue.type==="ROT")return simPose?simPose.heading:headingBefore(simCueIndex);
    const s=simPath(cue);if(!s.total)return 0;
    const p=poseOnPath(cue,atEnd?s.total:0);
    return p?p.heading:0;
}

function setSimSegment(index,atEnd=false,keepHeading=false){
    if(index<0||index>=cues.length)return false;

    const cue=cues[index];
    const oldHeading=simPose?simPose.heading:null;

    simCueIndex=index;
    selectedCue=index;
    simAlign=false;

    if(cue.type==="VÄND"){
    const n=nodes[cue.startNode];

    if(!atEnd){
        simVandStartHeading=
            oldHeading!==null
                ? oldHeading
                : headingBefore(index);

        const pathHeading=vandPathHeading(index);

        let target;

        if(pathHeading!==null){
            const newReverse=!simReverse;
            target=pathHeading+(newReverse?Math.PI:0);
        }else{
            target=simVandStartHeading+Math.PI;
        }

        const wantedDir=cue.vandDir>=0 ? -1 : 1;

        let delta;

        if(wantedDir<0){
            delta=positiveAngle(
                simVandStartHeading-target
            );
        }else{
            delta=positiveAngle(
                target-simVandStartHeading
            );
        }

        simVandTurnDir=wantedDir;
        simVandTotalDeg=delta*180/Math.PI;

        // Om matematiken ger 0 men vi faktiskt ska VÄND:
        if(simVandTotalDeg<0.001)
            simVandTotalDeg=360;

        simVandTargetHeading=
            simVandStartHeading+
            simVandTurnDir*
            simVandTotalDeg*Math.PI/180;

        simProgress=0;

        cue.simVandStartHeading=simVandStartHeading;
        cue.simVandTargetHeading=simVandTargetHeading;
        cue.simVandTotalDeg=simVandTotalDeg;
        cue.simVandTurnDir=simVandTurnDir;

    }else{
        if(Number.isFinite(cue.simVandTotalDeg)){
            simVandStartHeading=cue.simVandStartHeading;
            simVandTargetHeading=cue.simVandTargetHeading;
            simVandTotalDeg=cue.simVandTotalDeg;
            simVandTurnDir=cue.simVandTurnDir;
        }else{
            simVandStartHeading=
                oldHeading!==null
                    ? oldHeading-Math.PI
                    : headingBefore(index);

            simVandTargetHeading=
                oldHeading!==null
                    ? oldHeading
                    : simVandStartHeading+Math.PI;

            simVandTotalDeg=180;
            simVandTurnDir=cue.vandDir>=0?-1:1;
        }

        simProgress=simVandTotalDeg;
    }

    simPose={
        x:n.x,
        y:n.y,
        heading:simVandStartHeading+
                simVandTurnDir*
                simProgress*Math.PI/180
    };

    updateCueList();
    updateCueInfo();
    updateSimDisplay();
    draw();
    return true;
}
    if(cue.type==="ROT"){
    const n=nodes[cue.startNode];

    if(!atEnd){
        if(oldHeading!==null)
            simRotStartHeading=oldHeading;
        else
            simRotStartHeading=headingBefore(index);

        const plan=makeRotPlan(index,simRotStartHeading);

        simRotWaitDeg=plan.waitDeg;
        simRotFinalDeg=plan.finalDeg;

        // Spara den verkliga planen för backning
        cue.simRotStartHeading=simRotStartHeading;
        cue.simRotWaitDeg=simRotWaitDeg;
        cue.simRotFinalDeg=simRotFinalDeg;

        simProgress=0;
    }else{
        // Backning: använd samma ROT-plan som faktiskt gällde framåt
        if(Number.isFinite(cue.simRotStartHeading)){
            simRotStartHeading=cue.simRotStartHeading;
            simRotWaitDeg=cue.simRotWaitDeg;
            simRotFinalDeg=cue.simRotFinalDeg;
        }else{
            // reserv om ROT ännu inte körts framåt
            simRotStartHeading=headingBefore(index);

            const plan=makeRotPlan(index,simRotStartHeading);
            simRotWaitDeg=plan.waitDeg;
            simRotFinalDeg=plan.finalDeg;
        }

        simProgress=simRotFinalDeg;
    }

    simRotWaitDone=false;
    simRotWaitUntil=0;

    const plan=makeRotPlan(index,simRotStartHeading);

    simPose={
        x:n.x,
        y:n.y,
        heading:simRotStartHeading+
                plan.dir*simProgress*Math.PI/180
    };
}else{
        const s=simPath(cue);
        simProgress=atEnd?s.total:0;

        const p=poseOnPath(cue,simProgress);
        if(!p)return false;

        const wanted=driveHeading(p.heading);

        if(keepHeading&&oldHeading!==null){
            simPose={x:p.x,y:p.y,heading:oldHeading};
            simAlignTarget=wanted;
            simAlign=Math.abs(normAngle(simAlignTarget-oldHeading))>0.01;
        }else{
            simPose={x:p.x,y:p.y,heading:wanted};
        }
    }

    updateCueList();
    updateCueInfo();
    updateSimDisplay();
    draw();
    return true;
}



function nextSimSegment(){
    const i=simCueIndex+1;

    if(i>=cues.length)return false;

    const current=cues[simCueIndex];
    const next=cues[i];

    if(next.name!==current.name)return false;

    return setSimSegment(i,false,true);
}

function prevSimSegment(){
    const name=cues[simCueIndex].name;
    const i=simCueIndex-1;
    if(i>=0&&cues[i].name===name)return setSimSegment(i,true,true);
    return false;
}

function updateSimDisplay(){
    if(!simActive||simCueIndex<0)return;

    const cue=cues[simCueIndex],nr=cueSegmentNumber(simCueIndex);
    $("driveCueName").textContent=cue.name;

    if(cue.type==="ROT"){
        const left=Math.max(0,Math.abs(cue.rotateDeg)-simProgress);
        $("driveSegmentInfo").textContent=`${nr}. ROT ${left.toFixed(0)}°`;
    }else if(cue.type==="VÄND"){
        $("driveSegmentInfo").textContent=`${nr}. VÄND`;
    }else{
        const left=Math.max(0,simPath(cue).total-simProgress);
        $("driveSegmentInfo").textContent=`${nr}. ${cue.type} ${(left/1000).toFixed(2)}m`;
    }
}

function stopAtCueBoundary(side){
    simBoundary=side;
    simRunEnabled=false;
    simRunDir=0;
    simMovedAfterArm=false;

    setDriveSlider(0);

    updateBoundaryButtons();
    updateSimDisplay();

    setStatus(
        side<0
        ? "Cue START - välj + eller -"
        : "Cue SLUT - välj + eller -"
    );
}

function simLoop(t){
    if(!simActive){simFrame=0;return}
    if(!simLastTime)simLastTime=t;

    const dt=Math.min((t-simLastTime)/1000,.05);
    simLastTime=t;

    if(sliderPct!==0&&simCueIndex>=0){
        const cue=cues[simCueIndex];
        const dir=Math.sign(sliderPct);
        const power=Math.abs(sliderPct)/100;
        if(!simMovedAfterArm){
    simMovedAfterArm=true;
    updateRunButtons();
}

        if(cue.type==="VÄND"){
    const step=SIM_ROT_DEG_S*power*dt;

    if(dir>0){
        simProgress=Math.min(
            simVandTotalDeg,
            simProgress+step
        );

        simPose={
            x:nodes[cue.startNode].x,
            y:nodes[cue.startNode].y,
            heading:
                simVandStartHeading+
                simVandTurnDir*
                simProgress*Math.PI/180
        };

        $("driveSegmentInfo").textContent=
            `${cueSegmentNumber(simCueIndex)}. VÄND ${simProgress.toFixed(0)}°`;

        if(simProgress>=simVandTotalDeg-0.001){
            simPose.heading=simVandTargetHeading;

            simReverse=!simReverse;

            if(nextSimSegment()){
                simLastTime=t;
                simFrame=requestAnimationFrame(simLoop);
                return;
            }

            stopAtCueBoundary(1);
            simFrame=requestAnimationFrame(simLoop);
            return;
        }

    }else{
        simProgress=Math.max(
            0,
            simProgress-step
        );

        simPose={
            x:nodes[cue.startNode].x,
            y:nodes[cue.startNode].y,
            heading:
                simVandStartHeading+
                simVandTurnDir*
                simProgress*Math.PI/180
        };

        $("driveSegmentInfo").textContent=
            `${cueSegmentNumber(simCueIndex)}. VÄND ${simProgress.toFixed(0)}°`;

        if(simProgress<=0.001){
            simPose.heading=simVandStartHeading;

            simReverse=!simReverse;

            if(prevSimSegment()){
                simLastTime=t;
                simFrame=requestAnimationFrame(simLoop);
                return;
            }

            stopAtCueBoundary(-1);
            simFrame=requestAnimationFrame(simLoop);
            return;
        }
    }

    draw();
    simFrame=requestAnimationFrame(simLoop);
    return;
}

        

        if(cue.type==="ROT"){
    const plan=makeRotPlan(simCueIndex,simRotStartHeading);
    const step=SIM_ROT_DEG_S*power*dt;

    // WAIT efter den programmerade ROT-punkten
    if(!simRotWaitDone&&
       simProgress>=simRotWaitDeg-0.001){

        if(simRotWaitUntil===0){
            simRotWaitUntil=t+(cue.waitSec||0)*1000;
        }

        if(t<simRotWaitUntil){
            const left=(simRotWaitUntil-t)/1000;
            $("driveSegmentInfo").textContent=
                `${cueSegmentNumber(simCueIndex)}. WAIT ${left.toFixed(1)}s`;

            draw();
            simFrame=requestAnimationFrame(simLoop);
            return;
        }

        simRotWaitDone=true;
        simRotWaitUntil=0;
    }

    if(dir>0){
        const next=Math.min(
            simProgress+step,
            simRotWaitDone
                ? simRotFinalDeg
                : simRotWaitDeg
        );

        simProgress=next;

        simPose={
            x:nodes[cue.startNode].x,
            y:nodes[cue.startNode].y,
            heading:simRotStartHeading+
                    plan.dir*simProgress*Math.PI/180
        };

        // Kommit till WAIT-punkten
        if(!simRotWaitDone&&
           simProgress>=simRotWaitDeg-0.001){

            updateSimDisplay();
            draw();
            updateRunButtons();
            simFrame=requestAnimationFrame(simLoop);
            return;
        }

        // Hela ROT + eventuell extra riktning klar
if(simRotWaitDone&&
   simProgress>=simRotFinalDeg-0.001){

    const nextIndex=simCueIndex+1;

    if(nextIndex<cues.length &&
       cues[nextIndex].name===cue.name){

        setSimSegment(nextIndex,false,true);

        simLastTime=t;
        simFrame=requestAnimationFrame(simLoop);
        return;
    }

    stopAtCueBoundary(1);
    simFrame=requestAnimationFrame(simLoop);
    return;
}

    }else{
    // Backa först mot WAIT-punkten
    if(!simRotWaitDone&&simProgress>simRotWaitDeg){
        simProgress=Math.max(
            simRotWaitDeg,
            simProgress-step
        );

        simPose={
            x:nodes[cue.startNode].x,
            y:nodes[cue.startNode].y,
            heading:simRotStartHeading+
                    plan.dir*simProgress*Math.PI/180
        };

        if(simProgress<=simRotWaitDeg+0.001){
            simRotWaitUntil=t+(cue.waitSec||0)*1000;
        }

    }else if(!simRotWaitDone){
        // Vänta även vid backning
        if(t<simRotWaitUntil){
            const left=(simRotWaitUntil-t)/1000;

            $("driveSegmentInfo").textContent=
                `${cueSegmentNumber(simCueIndex)}. WAIT ${left.toFixed(1)}s`;

            draw();
            simFrame=requestAnimationFrame(simLoop);
            return;
        }

        simRotWaitDone=true;

    }else{
        // Efter WAIT fortsätter vi tillbaka mot ROT-start
        simProgress=Math.max(0,simProgress-step);

        simPose={
            x:nodes[cue.startNode].x,
            y:nodes[cue.startNode].y,
            heading:simRotStartHeading+
                    plan.dir*simProgress*Math.PI/180
        };

        if(simProgress<=0){
            if(prevSimSegment()){
                simLastTime=t;
                simFrame=requestAnimationFrame(simLoop);
                return;
            }

            stopAtCueBoundary(-1);
            simFrame=requestAnimationFrame(simLoop);
            return;
        }
    }
}

}else{
            const s=simPath(cue);
            simProgress+=dir*SIM_SPEED_MM_S*power*dt;

            if(simProgress>s.total){
                simProgress=s.total;

                if(dir>0){
                    if(nextSimSegment()){
                        simLastTime=t;
                        simFrame=requestAnimationFrame(simLoop);
                        return;
                    }

                    simPose=poseOnPath(cue,s.total);
                    if(simPose)simPose.heading=driveHeading(simPose.heading);

                    draw();
                    stopAtCueBoundary(1);
                    simFrame=requestAnimationFrame(simLoop);
                    return;
                }
            }

            if(simProgress<0){
                simProgress=0;

                if(dir<0){
                    if(prevSimSegment()){
                        simLastTime=t;
                        simFrame=requestAnimationFrame(simLoop);
                        return;
                    }

                    simPose=poseOnPath(cue,0);
                    if(simPose)simPose.heading=driveHeading(simPose.heading);

                    draw();
                    stopAtCueBoundary(-1);
                    simFrame=requestAnimationFrame(simLoop);
                    return;
                }
            }

            const p=poseOnPath(cue,simProgress);
            if(p){
                simPose={
                    x:p.x,
                    y:p.y,
                    heading:driveHeading(p.heading)
                };
            }
        }

        updateSimDisplay();
        draw();
    }

    simFrame=requestAnimationFrame(simLoop);
}

function setSimActive(on){
    simActive=on;

    $("driveDisplay").classList.toggle("simActive",on);
    driveSlider.classList.toggle("simActive",on);

    setDriveSlider(0);

    if(on){
        if(!cues.length){
            simActive=false;
            setStatus("Ingen Cue finns");
            return;
        }

        simReverse=false;
        simRotDir=0;
        simRunEnabled=false;
        simRunDir=0;
        simBoundary=-1;

        selectedCue=0;
        simCueIndex=0;

        setSimSegment(0,false,false);

        $("prevCueButton").disabled=true;
        $("nextCueButton").disabled=false;

        simLastTime=0;
        if(!simFrame)simFrame=requestAnimationFrame(simLoop);

        setStatus("Cue 1 START - tryck +");
    }else{
        simPose=null;
        simCueIndex=-1;
        simLastTime=0;

        simBoundary=0;
        simRunEnabled=false;
        simRunDir=0;
        simReverse=false;
        simRotDir=0;

        $("prevCueButton").disabled=true;
        $("nextCueButton").disabled=true;

        if(simFrame){
            cancelAnimationFrame(simFrame);
            simFrame=0;
        }

        updateCueInfo();
        draw();
        setStatus("Torrkörning av");
    }
}

$("driveDisplay").onclick=()=>setSimActive(!simActive);

driveSlider.oninput=()=>setDriveSlider(driveSlider.value);
driveSlider.onpointerup=()=>setDriveSlider(0);
driveSlider.onpointercancel=()=>setDriveSlider(0);

setSimActive(false);



function editNodeValue(id,label){
    if(id===null||!nodes[id])return;
    dialogNodeId=id;dialogNodeLabel=label;
    $("nodeTitle").textContent=label;$("nodeX").value=nodes[id].x;$("nodeY").value=nodes[id].y;
    $("nodeDialog").showModal();
}

$("nodeSave").onclick=()=>{
    if(dialogNodeId<0||!nodes[dialogNodeId])return;
    const x=+$("nodeX").value,y=+$("nodeY").value;
    if(!Number.isFinite(x)||!Number.isFinite(y)){setStatus("Ogiltigt värde");return}
    nodes[dialogNodeId].x=Math.round(x);nodes[dialogNodeId].y=Math.round(y);
    const cue=currentCue();
    if(cue&&dialogNodeId===cue.endNode)connectNext(cue);
    updateCueInfo();draw();setStatus(`${dialogNodeLabel} uppdaterad`);
};

function beginNodeEdit(id,label,p){
    editMode="node";editNodeId=id;editNodeLabel=label;editNodeMoved=false;
    editNodeDownX=p.x;editNodeDownY=p.y;canvas.style.cursor="grabbing";
}

function updateNodeEdit(p,w){
    if(editNodeId<0||!nodes[editNodeId])return;
    if(Math.hypot(p.x-editNodeDownX,p.y-editNodeDownY)>4)editNodeMoved=true;
    if(!editNodeMoved)return;
    nodes[editNodeId].x=Math.round(w.x);nodes[editNodeId].y=Math.round(w.y);
    const cue=currentCue();
    if(cue&&editNodeId===cue.endNode)connectNext(cue);
    updateCueInfo();draw();
}

function editRotation(cue){
    rotateTarget=cue;
    rotateInsert=false;
    $("rotateDeg").value=cue.rotateDeg;
    $("rotateWait").value=cue.waitSec||0;
    $("rotateDialog").showModal();
}
function editReverse(cue){
    reverseInsert=false;

    $("reverseDialog").showModal();

    $("reverseCW").onclick=()=>{
        cue.vandDir=1;
        updateCueList();
        updateCueInfo();
        draw();
        setStatus("VÄND medsols");
    };

    $("reverseCCW").onclick=()=>{
        cue.vandDir=-1;
        updateCueList();
        updateCueInfo();
        draw();
        setStatus("VÄND motsols");
    };
}


function pairLines(text){
    const raw=text.replace(/\r/g,"").split("\n"),pairs=[];
    for(let i=0;i+1<raw.length;i+=2)pairs.push({code:parseInt(raw[i].trim()),value:raw[i+1].trim()});
    return pairs;
}

function parseDXF(text){
    const pairs=pairLines(text),out=[];let i=0,section="";
    while(i<pairs.length){
        const p=pairs[i];
        if(p.code===0&&p.value==="SECTION"){if(i+1<pairs.length&&pairs[i+1].code===2)section=pairs[i+1].value;i+=2;continue}
        if(p.code===0&&p.value==="ENDSEC"){section="";i++;continue}
        if(section!=="ENTITIES"||p.code!==0){i++;continue}
        const type=p.value;

        if(type==="LINE"){
            const e={type:"LINE",x1:0,y1:0,x2:0,y2:0};i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===10)e.x1=+q.value;if(q.code===20)e.y1=+q.value;
                if(q.code===11)e.x2=+q.value;if(q.code===21)e.y2=+q.value;i++;
            }out.push(e);continue;
        }

        if(type==="CIRCLE"){
            const e={type:"CIRCLE",x:0,y:0,r:0};i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===10)e.x=+q.value;if(q.code===20)e.y=+q.value;if(q.code===40)e.r=+q.value;i++;
            }out.push(e);continue;
        }

        if(type==="ARC"){
            const e={type:"ARC",x:0,y:0,r:0,start:0,end:0};i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===10)e.x=+q.value;if(q.code===20)e.y=+q.value;if(q.code===40)e.r=+q.value;
                if(q.code===50)e.start=+q.value;if(q.code===51)e.end=+q.value;i++;
            }out.push(e);continue;
        }

        if(type==="LWPOLYLINE"){
            const e={type:"POLYLINE",points:[],closed:false};let cur=null;i++;
            while(i<pairs.length&&pairs[i].code!==0){
                const q=pairs[i];if(q.code===70)e.closed=(+q.value&1)!==0;
                if(q.code===10){cur={x:+q.value,y:0};e.points.push(cur)}
                if(q.code===20&&cur)cur.y=+q.value;i++;
            }
            if(e.points.length>1)out.push(e);continue;
        }

        if(type==="POLYLINE"){
            const e={type:"POLYLINE",points:[],closed:false};i++;
            while(i<pairs.length){
                if(pairs[i].code===70)e.closed=(+pairs[i].value&1)!==0;
                if(pairs[i].code===0&&pairs[i].value==="VERTEX"){
                    const pt={x:0,y:0};i++;
                    while(i<pairs.length&&pairs[i].code!==0){
                        if(pairs[i].code===10)pt.x=+pairs[i].value;
                        if(pairs[i].code===20)pt.y=+pairs[i].value;i++;
                    }e.points.push(pt);continue;
                }
                if(pairs[i].code===0&&pairs[i].value==="SEQEND"){i++;break}
                i++;
            }
            if(e.points.length>1)out.push(e);continue;
        }
        i++;
    }
    return out;
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

function worldToScreen(x,y){return{x:x*scale+offsetX,y:offsetY-y*scale}}
function screenToWorld(x,y){return{x:(x-offsetX)/scale,y:(offsetY-y)/scale}}

function fitDrawing(){
    if(!bounds)return;
    const w=Math.max(bounds.maxX-bounds.minX,1),h=Math.max(bounds.maxY-bounds.minY,1);
    fitScale=Math.min((canvas.width-MARGIN*2)/w,(canvas.height-MARGIN*2)/h);scale=fitScale;
    const cx=(bounds.minX+bounds.maxX)/2,cy=(bounds.minY+bounds.maxY)/2;
    offsetX=canvas.width/2-cx*scale;offsetY=canvas.height/2+cy*scale;draw();updatePositionInfo();
}

function getVisibleWorld(){
    const a=screenToWorld(0,canvas.height),b=screenToWorld(canvas.width,0);
    return{minX:Math.min(a.x,b.x),maxX:Math.max(a.x,b.x),minY:Math.min(a.y,b.y),maxY:Math.max(a.y,b.y)};
}

function drawGrid(){
    const v=getVisibleWorld();let grid=SMALL_GRID_MM;
    if(grid*scale<12)grid=1000;if(grid*scale<12)grid=5000;
    const sx=Math.floor(v.minX/grid)*grid,ex=Math.ceil(v.maxX/grid)*grid;
    const sy=Math.floor(v.minY/grid)*grid,ey=Math.ceil(v.maxY/grid)*grid;
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
            let p=worldToScreen(e.points[0].x,e.points[0].y);ctx.beginPath();ctx.moveTo(p.x,p.y);
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
        for(let s=0;s<steps;s++){const p=catmullPoint(p0,p1,p2,p3,s/steps);out.push({x:p.x,y:p.y,segment:i})}
    }
    const e=pts[pts.length-1];out.push({x:e.x,y:e.y,segment:pts.length-2});return out;
}

function drawCue(cue,index){
    if(cue.type==="ROT"||cue.type==="VÄND")return;
    const pts=getCuePoints(cue);if(!pts.length)return;
    ctx.strokeStyle=index===selectedCue?"#006cff":"#777";ctx.lineWidth=index===selectedCue?4:2;
    if(pts.length>1){
        const s=sampleCue(cue,16);let p=worldToScreen(s[0].x,s[0].y);
        ctx.beginPath();ctx.moveTo(p.x,p.y);
        for(let i=1;i<s.length;i++){p=worldToScreen(s[i].x,s[i].y);ctx.lineTo(p.x,p.y)}
        ctx.stroke();
    }
    if(index!==selectedCue)return;
    for(const q of cue.path){
        const p=worldToScreen(q.x,q.y);ctx.beginPath();ctx.arc(p.x,p.y,4,0,Math.PI*2);
        ctx.fillStyle="#006cff";ctx.fill();ctx.strokeStyle="#fff";ctx.lineWidth=1;ctx.stroke();
    }
}

function isCueStart(index){return index===0||cues[index-1].name!==cues[index].name}

function drawMarkers(){
    cues.forEach((cue,i)=>{
        if(cue.startNode===null||!nodes[cue.startNode])return;

        const n=nodes[cue.startNode];
        const p=worldToScreen(n.x,n.y);
        const cueStart=isCueStart(i);

        if(cueStart){
            ctx.beginPath();
            ctx.arc(p.x,p.y,10,0,Math.PI*2);
            ctx.fillStyle="#00b83f";
            ctx.fill();

            if(cue.type==="ROT"){
                ctx.strokeStyle="#ff9800";
                ctx.lineWidth=3;
                ctx.stroke();
            }

            if(cue.type==="VÄND"){
                ctx.strokeStyle="#9c27b0";
                ctx.lineWidth=3;
                ctx.stroke();
            }

            ctx.font="bold 16px Arial";
            ctx.fillStyle="#008f32";
            ctx.fillText(cue.name,p.x+14,p.y-14);

            if(cue.type==="ROT"){
                ctx.font="bold 15px Arial";
                ctx.fillStyle="#d97800";
                ctx.fillText(
                    `${cue.rotateDeg>=0?"↻":"↺"} ${Math.abs(cue.rotateDeg)}°`,
                    p.x+14,p.y+18
                );
            }

            if(cue.type==="VÄND"){
                ctx.font="bold 22px Arial";
                ctx.fillStyle="#9c27b0";
                ctx.fillText(
                    cue.vandDir>=0?"↻":"↺",
                    p.x+14,p.y+20
                );
            }

            return;
        }

        if(cue.type==="ROT"){
            ctx.beginPath();
            ctx.arc(p.x,p.y,7,0,Math.PI*2);
            ctx.fillStyle="#ff9800";
            ctx.fill();

            ctx.font="bold 15px Arial";
            ctx.fillStyle="#d97800";
            ctx.fillText(
                `${cue.rotateDeg>=0?"↻":"↺"} ${Math.abs(cue.rotateDeg)}°`,
                p.x+11,p.y-9
            );
            return;
        }

        if(cue.type==="VÄND"){
            ctx.beginPath();
            ctx.arc(p.x,p.y,7,0,Math.PI*2);
            ctx.fillStyle="#9c27b0";
            ctx.fill();

            ctx.font="bold 22px Arial";
            ctx.fillStyle="#9c27b0";
            ctx.fillText(
                cue.vandDir>=0?"↻":"↺",
                p.x+11,p.y-8
            );
            return;
        }

        ctx.beginPath();
        ctx.arc(p.x,p.y,5,0,Math.PI*2);
        ctx.fillStyle="#e32626";
        ctx.fill();
    });
}

function drawCues(){
    cues.forEach((c,i)=>drawCue(c,i));drawMarkers();
    if(!routeHover||tool!=="straight")return;
    const cue=currentCue();if(!cue||cue.startNode===null||cue.endNode!==null)return;
    const a=worldToScreen(nodes[cue.startNode].x,nodes[cue.startNode].y),b=worldToScreen(routeHover.x,routeHover.y);
    ctx.strokeStyle="#006cff";ctx.lineWidth=2;ctx.setLineDash([8,6]);
    ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();ctx.setLineDash([]);
    ctx.beginPath();ctx.arc(b.x,b.y,5,0,Math.PI*2);ctx.fillStyle="#e32626";ctx.fill();
}

function segmentEndHeading(cue){
    if(!cue||cue.startNode===null)return 0;
    const pts=getCuePoints(cue);
    if(pts.length<2)return 0;
    const a=pts[pts.length-2],b=pts[pts.length-1];
    return Math.atan2(b.y-a.y,b.x-a.x);
}

function vehiclePose(index){
    if(index<0||index>=cues.length)return null;

    let heading=0,haveHeading=false;

    for(let i=0;i<=index;i++){
        const c=cues[i];

        if(c.type==="ROT"){
            if(!haveHeading){
                for(let j=i-1;j>=0;j--){
                    if(cues[j].type!=="ROT"&&cues[j].endNode!==null){
                        heading=segmentEndHeading(cues[j]);haveHeading=true;break;
                    }
                }
            }

            // +ROT är medurs
            heading-=c.rotateDeg*Math.PI/180;
        }else if(c.endNode!==null){
            heading=segmentEndHeading(c);
            haveHeading=true;
        }
    }

    const cue=cues[index];
    const id=cue.endNode!==null?cue.endNode:cue.startNode;
    if(id===null||!nodes[id])return null;

    return{x:nodes[id].x,y:nodes[id].y,heading};
}

function drawVehicle(){
    const cue=currentCue();
    const pose=simActive&&simPose?simPose:vehiclePose(selectedCue);
    if(!cue||!pose)return;

    const p=worldToScreen(pose.x,pose.y),heading=pose.heading;

    const L=vehicle.lengthMm*scale,W=vehicle.widthMm*scale,D=vehicle.diameterMm*scale;
    const track=vehicle.trackWidthMm*scale,wheel=vehicle.wheelDiameterMm*scale;
    const front=Math.max(L/2,D/2,30);

    ctx.save();ctx.translate(p.x,p.y);ctx.rotate(-heading);
    ctx.strokeStyle="#7b2cbf";ctx.fillStyle="#7b2cbf";ctx.lineWidth=2;ctx.setLineDash([7,5]);

    if(D>0){ctx.beginPath();ctx.arc(0,0,D/2,0,Math.PI*2);ctx.stroke()}
    if(L>0&&W>0)ctx.strokeRect(-L/2,-W/2,L,W);

    ctx.setLineDash([]);ctx.lineWidth=4;
    ctx.beginPath();
    ctx.moveTo(-wheel/2,-track/2);ctx.lineTo(wheel/2,-track/2);
    ctx.moveTo(-wheel/2, track/2);ctx.lineTo(wheel/2, track/2);
    ctx.stroke();

    ctx.lineWidth=3;
    ctx.beginPath();ctx.moveTo(0,0);ctx.lineTo(front+15,0);ctx.stroke();
    ctx.beginPath();ctx.moveTo(front+15,0);ctx.lineTo(front+4,-7);ctx.lineTo(front+4,7);ctx.closePath();ctx.fill();
    ctx.restore();
}

function draw(){
    ctx.clearRect(0,0,canvas.width,canvas.height);ctx.fillStyle="#ddd";ctx.fillRect(0,0,canvas.width,canvas.height);
    if(!bounds)return;drawGrid();drawEntities();drawCues();drawVehicle();
}

function addNode(x,y){nodes.push({id:nodes.length,x:Math.round(x),y:Math.round(y)});return nodes.length-1}

function addFollowPoint(cue,x,y){
    const p={x:Math.round(x),y:Math.round(y)},last=cue.path.length?cue.path[cue.path.length-1]:nodes[cue.startNode];
    if(Math.hypot(p.x-last.x,p.y-last.y)<FOLLOW_STEP_MM)return;cue.path.push(p);
}

function perpendicularDistance(p,a,b){
    const dx=b.x-a.x,dy=b.y-a.y;if(!dx&&!dy)return Math.hypot(p.x-a.x,p.y-a.y);
    const t=((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy),x=a.x+t*dx,y=a.y+t*dy;
    return Math.hypot(p.x-x,p.y-y);
}

function simplifyRDP(points,eps){
    if(points.length<3)return points.slice();
    let max=0,index=0;
    for(let i=1;i<points.length-1;i++){const d=perpendicularDistance(points[i],points[0],points[points.length-1]);if(d>max){max=d;index=i}}
    if(max<=eps)return[points[0],points[points.length-1]];
    const a=simplifyRDP(points.slice(0,index+1),eps),b=simplifyRDP(points.slice(index),eps);
    return a.slice(0,-1).concat(b);
}

function smoothFollowCue(cue){
    if(cue.startNode===null||cue.endNode===null)return;
    let pts=[{...nodes[cue.startNode]},...cue.path.map(p=>({...p})),{...nodes[cue.endNode]}];
    if(pts.length<3){cue.path=[];return}
    for(let pass=0;pass<4;pass++){
        const next=[pts[0]];
        for(let i=1;i<pts.length-1;i++)next.push({x:pts[i-1].x*.25+pts[i].x*.5+pts[i+1].x*.25,y:pts[i-1].y*.25+pts[i].y*.5+pts[i+1].y*.25});
        next.push(pts[pts.length-1]);pts=next;
    }
    pts=simplifyRDP(pts,FOLLOW_SIMPLIFY_MM);
    cue.path=pts.slice(1,-1).map(p=>({x:Math.round(p.x),y:Math.round(p.y)}));
}

function distancePointSegment(px,py,ax,ay,bx,by){
    const vx=bx-ax,vy=by-ay,l2=vx*vx+vy*vy;let t=l2?((px-ax)*vx+(py-ay)*vy)/l2:0;
    t=Math.max(0,Math.min(1,t));const x=ax+t*vx,y=ay+t*vy;
    return{distance:Math.hypot(px-x,py-y),t,x,y};
}

function hitNode(s,id){
    if(id===null||!nodes[id])return false;
    const p=worldToScreen(nodes[id].x,nodes[id].y);return Math.hypot(s.x-p.x,s.y-p.y)<=NODE_HIT_PX;
}

function hitControl(s,cue){
    let best=null;
    cue.path.forEach((q,i)=>{
        const p=worldToScreen(q.x,q.y),d=Math.hypot(s.x-p.x,s.y-p.y);
        if(d<=CONTROL_HIT_PX&&(!best||d<best.distance))best={index:i,distance:d};
    });
    return best;
}

function hitCurve(s,cue){
    if(cue.startNode===null||cue.endNode===null||cue.type==="ROT")return null;
    const p=sampleCue(cue,20);let best=null;
    for(let i=0;i<p.length-1;i++){
        const a=worldToScreen(p[i].x,p[i].y),b=worldToScreen(p[i+1].x,p[i+1].y);
        const h=distancePointSegment(s.x,s.y,a.x,a.y,b.x,b.y);
        if(h.distance<=CURVE_HIT_PX&&(!best||h.distance<best.distance))
            best={distance:h.distance,segment:p[i].segment,x:p[i].x+(p[i+1].x-p[i].x)*h.t,y:p[i].y+(p[i+1].y-p[i].y)*h.t};
    }
    return best;
}

function beginControlEdit(i){editMode="control";editPathIndex=i;canvas.style.cursor="grabbing"}

function updateControlEdit(w){
    const cue=currentCue();if(!cue||editPathIndex<0||editPathIndex>=cue.path.length)return;
    cue.path[editPathIndex]={x:Math.round(w.x),y:Math.round(w.y)};updateCueInfo();draw();
}

function beginCurveEdit(hit){
    const cue=currentCue();cue.path.splice(hit.segment,0,{x:Math.round(hit.x),y:Math.round(hit.y)});
    editMode="control";editPathIndex=hit.segment;if(cue.type==="RAK")cue.type="KURVA";
    updateCueList();updateCueInfo();canvas.style.cursor="grabbing";
}

function endEdit(){editMode=null;editPathIndex=-1;canvas.style.cursor="crosshair";updateCueInfo();updateCueList();draw()}

function zoomAt(x,y,f){
    if(!bounds)return;
    const w=screenToWorld(x,y);scale=Math.min(Math.max(scale*f,fitScale*MIN_ZOOM),fitScale*MAX_ZOOM);
    offsetX=x-w.x*scale;offsetY=y+w.y*scale;draw();updatePositionInfo(x,y);
}

function getCanvasPosition(e){
    const r=canvas.getBoundingClientRect();
    return{x:(e.clientX-r.left)*canvas.width/r.width,y:(e.clientY-r.top)*canvas.height/r.height};
}

function updatePositionInfo(x=null,y=null){
    if(!bounds){positionInfo.innerHTML="X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ---";return}
    const z=Math.round(scale/fitScale*100);
    if(x===null){positionInfo.innerHTML=`X: ---- mm &nbsp; Y: ---- mm &nbsp; Zoom: ${z}%`;return}
    const w=screenToWorld(x,y);
    positionInfo.innerHTML=`X: ${w.x.toFixed(0)} mm &nbsp; Y: ${w.y.toFixed(0)} mm &nbsp; Zoom: ${z}%`;
}

function showDrawingInfo(){
    const w=bounds.maxX-bounds.minX,h=bounds.maxY-bounds.minY;
    drawingInfo.textContent=`${currentFileName} | ${entities.length} objekt | ${(w/1000).toFixed(2)} × ${(h/1000).toFixed(2)} m`;
}

canvas.addEventListener("wheel",e=>{
    if(!bounds)return;e.preventDefault();const p=getCanvasPosition(e);zoomAt(p.x,p.y,e.deltaY<0?1.15:1/1.15);
},{passive:false});

canvas.addEventListener("pointerdown",e=>{
    if(!bounds)return;
    const p=getCanvasPosition(e),w=screenToWorld(p.x,p.y),cue=currentCue();

    if(tool==="pan"&&cue){
    if((cue.type==="ROT"||cue.type==="VÄND")&&cue.startNode!==null){
        const n=nodes[cue.startNode],rp=worldToScreen(n.x,n.y);

        if(Math.hypot(p.x-rp.x,p.y-rp.y)<=20){
            if(cue.type==="ROT")editRotation(cue);
            else editReverse(cue);

            return;
        }
    }

        if(hitNode(p,cue.startNode)){beginNodeEdit(cue.startNode,"START",p);canvas.setPointerCapture(e.pointerId);return}
        if(hitNode(p,cue.endNode)){beginNodeEdit(cue.endNode,"SLUT",p);canvas.setPointerCapture(e.pointerId);return}

        const c=hitControl(p,cue);
        if(c){beginControlEdit(c.index);canvas.setPointerCapture(e.pointerId);setStatus("Flyttar kontrollpunkt");return}

        const h=hitCurve(p,cue);
        if(h){beginCurveEdit(h);canvas.setPointerCapture(e.pointerId);setStatus("Ny kontrollpunkt skapad");return}
    }

    if(tool==="straight"){
        if(!cue){setStatus("Skapa NY CUE först");return}
        cue.type="RAK";cue.path=[];
        if(cue.startNode===null){cue.startNode=addNode(w.x,w.y);updateCueInfo();updateCueList();draw();setStatus("START markerad - klicka SLUT");return}
        if(cue.endNode===null){
            cue.endNode=addNode(w.x,w.y);connectNext(cue);updateCueInfo();updateCueList();draw();
            setStatus("Segment klart - välj RAK, ROT, FÖLJ eller SLUT CUE");
        }
        return;
    }

    if(tool==="follow"){
        if(!cue){setStatus("Skapa NY CUE först");return}
        if(cue.endNode!==null){setStatus("Segmentet är redan klart");return}
        cue.type="FÖLJ";cue.path=[];
        if(cue.startNode===null)cue.startNode=addNode(w.x,w.y);
        else{
            const s=nodes[cue.startNode];
            if(Math.hypot(w.x-s.x,w.y-s.y)>START_TOLERANCE_MM){setStatus("Börja dra från START-punkten");return}
        }
        routeDrawing=true;canvas.setPointerCapture(e.pointerId);updateCueInfo();updateCueList();draw();setStatus("Ritar segment...");return;
    }

    dragging=true;dragStartX=p.x;dragStartY=p.y;dragOffsetX=offsetX;dragOffsetY=offsetY;
    canvas.classList.add("dragging");canvas.setPointerCapture(e.pointerId);
});

canvas.addEventListener("pointermove",e=>{
    if(!bounds)return;
    const p=getCanvasPosition(e),w=screenToWorld(p.x,p.y);

    if(editMode==="node")updateNodeEdit(p,w);
    else if(editMode==="control")updateControlEdit(w);
    else if(routeDrawing){addFollowPoint(currentCue(),w.x,w.y);draw()}
    else if(tool==="straight"){routeHover=w;draw()}
    else if(dragging){offsetX=dragOffsetX+p.x-dragStartX;offsetY=dragOffsetY+p.y-dragStartY;draw()}
    else if(tool==="pan"){
        const cue=currentCue();
        canvas.style.cursor=cue&&(hitNode(p,cue.startNode)||hitNode(p,cue.endNode)||hitControl(p,cue)||hitCurve(p,cue))?"grab":"crosshair";
    }
    updatePositionInfo(p.x,p.y);
});

function stopPointer(e){
    if(editMode==="node"){
        const id=editNodeId,label=editNodeLabel,moved=editNodeMoved;
        editMode=null;editNodeId=-1;editNodeLabel="";editNodeMoved=false;canvas.style.cursor="crosshair";
        if(!moved)editNodeValue(id,label);
        else{
            const cue=currentCue();if(cue&&id===cue.endNode)connectNext(cue);
            updateCueInfo();updateCueList();draw();setStatus(`${label}-punkt flyttad`);
        }
        if(e&&canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
        return;
    }

    if(editMode)endEdit();
    else if(routeDrawing){
        routeDrawing=false;
        const p=getCanvasPosition(e),w=screenToWorld(p.x,p.y),cue=currentCue();
        cue.endNode=addNode(w.x,w.y);smoothFollowCue(cue);connectNext(cue);
        updateCueInfo();updateCueList();draw();setStatus("Segment klart - välj RAK, ROT, FÖLJ eller SLUT CUE");
    }

    dragging=false;canvas.classList.remove("dragging");
    if(e&&canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
}

canvas.addEventListener("pointerup",stopPointer);

canvas.addEventListener("pointercancel",e=>{
    if(editMode==="node"){editMode=null;editNodeId=-1;editNodeLabel="";editNodeMoved=false;canvas.style.cursor="crosshair"}
    else if(editMode)endEdit();
    routeDrawing=false;dragging=false;canvas.classList.remove("dragging");
    if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);draw();
});

canvas.addEventListener("pointerleave",()=>{
    routeHover=null;if(!dragging&&!routeDrawing&&!editMode)updatePositionInfo();draw();
});

panButton.onclick=()=>setTool("pan");

$("newCueButton").onclick=()=>{
    const last=lastSegment();
    if(last&&!last.endOfCue){setStatus("Avsluta nuvarande Cue med SLUT CUE först");return}
    const name=prompt("Namn på Cue:",`Cue ${cueNumber+1}`);if(name===null)return;
    cueNumber++;activeCueName=name.trim()||`Cue ${cueNumber}`;
    const cue=createEmptyCue(activeCueName);if(last&&last.endNode!==null)cue.startNode=last.endNode;
    cues.push(cue);selectedCue=cues.length-1;splitAfter=-1;
    updateCueList();updateCueInfo();draw();setTool("pan");setStatus(`Ny Cue: ${activeCueName} - välj RAK, ROT eller FÖLJ`);
};

$("splitButton").onclick=()=>{
    const cue=currentCue();if(!cue||cue.endNode===null){setStatus("Välj ett färdigt segment först");return}
    splitAfter=selectedCue;updateSegmentButtons();setStatus("SPLIT aktiv - välj RAK, ROT eller FÖLJ");
};

straightButton.onclick=()=>{
    const current=currentCue();

    // REDIGERA: räta ut markerad kurva
    if(splitAfter<0 &&
       tool==="pan" &&
       current &&
       current.type==="KURVA" &&
       current.startNode!==null &&
       current.endNode!==null){

        current.type="RAK";
        current.path=[];

        updateCueList();
        updateCueInfo();
        draw();
        setTool("pan");
        setStatus("Kurvan ändrad till RAK");
        return;
    }

    // Vanligt: skapa nytt RAK-segment
    const cue=splitAfter>=0
        ? insertSegment("RAK")
        : createNextSegment("RAK");

    if(!cue)return;

    cue.rotateDeg=0;
    updateCueList();
    updateCueInfo();
    setTool("straight");
};

followButton.onclick=()=>{
    const cue=splitAfter>=0?insertSegment("FÖLJ"):createNextSegment("FÖLJ");if(!cue)return;
    cue.rotateDeg=0;updateCueList();updateCueInfo();setTool("follow");
};

$("rotateButton").onclick=()=>{
    const last=lastSegment();

    if(splitAfter<0){
        if(!last){setStatus("Skapa NY CUE först");return}
        if(last.endOfCue){setStatus("Cue är avslutad");return}
        if(last.endNode===null&&last.startNode===null){
            setStatus("Första Cue behöver en startposition");
            return;
        }
    }

    rotateInsert=splitAfter>=0;
    rotateTarget=null;

    $("rotateDeg").value="";
$("rotateWait").value=0;
$("rotateDialog").showModal();
    $("rotateDeg").focus();
};

let reverseInsert=false;

$("reverseButton").onclick=()=>{
    const cue=currentCue();

    const canAppend=
        cue &&
        selectedCue===cues.length-1 &&
        !cue.endOfCue &&
        cue.endNode!==null;

    if(splitAfter<0&&!canAppend)return;

    reverseInsert=splitAfter>=0;
    $("reverseDialog").showModal();
};;

function addReverse(dir){
    let seg=null;

    if(reverseInsert){
        seg=insertSegment("VÄND");
        if(!seg)return;
    }else{
        const last=lastSegment();

        if(!last){
            setStatus("Skapa NY CUE först");
            return;
        }

        if(last.endOfCue){
            setStatus("Cue är avslutad");
            return;
        }

        if(last.endNode===null){
            setStatus("Gör färdigt segmentet först");
            return;
        }

        seg=createEmptyCue(last.name,"VÄND");
        seg.startNode=last.endNode;
        cues.push(seg);
        selectedCue=cues.length-1;
    }

    seg.vandDir=dir;

    // Ingen separat nod.
    seg.endNode=seg.startNode;

    connectNext(seg);

    updateCueList();
    updateCueInfo();
    draw();

    setTool("pan");

    setStatus(
        dir>0
            ?"VÄND medsols tillagd"
            :"VÄND motsols tillagd"
    );
}

$("reverseCW").onclick=()=>addReverse(1);
$("reverseCCW").onclick=()=>addReverse(-1);


$("rotateSave").onclick=()=>{
    const a=+$("rotateDeg").value;
const wait=Math.max(0,+$("rotateWait").value||0);
    if(!Number.isFinite(a)||a===0){setStatus("Ogiltig rotation");return}

    if(rotateTarget){
    rotateTarget.rotateDeg=a;
    rotateTarget.waitSec=wait;
    rotateTarget=null;
    updateCueList();updateCueInfo();draw();
    setStatus(`ROT ${a>0?"+":""}${a}°`);
    return;
}

    let target=null,last=lastSegment();

    if(rotateInsert){
        target=insertSegment("ROT");
        if(!target)return;
    }else if(last.endNode===null){
        target=last;target.type="ROT";
    }else{
        target=createEmptyCue(last.name,"ROT");
        target.startNode=last.endNode;
        cues.push(target);selectedCue=cues.length-1;
    }

    target.rotateDeg=a;
target.waitSec=wait;
    const n=nodes[target.startNode];
    target.endNode=addNode(n.x,n.y);
    connectNext(target);

    updateCueList();updateCueInfo();draw();setTool("pan");
    setStatus(`ROT ${a>0?"+":""}${a}° tillagd`);
};

$("endCueButton").onclick=()=>{
    const cue=currentCue();if(!cue){setStatus("Ingen Cue vald");return}
    if(cue.endNode===null){setStatus("Gör färdigt segmentet först");return}
    cue.endOfCue=true;splitAfter=-1;updateCueList();updateCueInfo();draw();setTool("pan");setStatus(`${cue.name} klar`);
};

$("undoButton").onclick=()=>{
    const cue=currentCue();if(!cue||selectedCue<0)return;
    const i=selectedCue,prev=i>0?cues[i-1]:null,next=i+1<cues.length?cues[i+1]:null;
    const prevSame=prev&&prev.name===cue.name,nextSame=next&&next.name===cue.name,wasEnd=cue.endOfCue;

    let reconnect=null;
    if(prevSame&&prev.endNode!==null)reconnect=prev.endNode;
    else if(cue.startNode!==null)reconnect=cue.startNode;
    else if(prev&&prev.endNode!==null)reconnect=prev.endNode;

    cues.splice(i,1);
    if(i<cues.length&&nextSame&&reconnect!==null)connectFrom(i,reconnect);

    if(wasEnd&&prevSame){
        prev.endOfCue=true;
        if(i<cues.length&&cues[i].name!==prev.name)connectFrom(i,prev.endNode);
    }

    selectedCue=cues.length?Math.min(i,cues.length-1):-1;splitAfter=-1;
    updateCueList();updateCueInfo();draw();setStatus("Segment borttaget");
};

$("vehicleButton").onclick=()=>{
    $("vName").value=vehicle.name;$("vLength").value=vehicle.lengthMm;$("vWidth").value=vehicle.widthMm;
    $("vDiameter").value=vehicle.diameterMm;$("vWheelDia").value=vehicle.wheelDiameterMm;
    $("vTrack").value=vehicle.trackWidthMm;$("vGear").value=vehicle.gearRatio;
    $("vRpm").value=vehicle.maxMotorRpm;$("vEncoder").value=vehicle.encoderPulsesPerRev;
    $("vehicleDialog").showModal();
};

$("vehicleSave").onclick=()=>{
    vehicle.name=$("vName").value.trim()||"Vagn";
    vehicle.lengthMm=Math.max(0,+$("vLength").value||0);vehicle.widthMm=Math.max(0,+$("vWidth").value||0);
    vehicle.diameterMm=Math.max(0,+$("vDiameter").value||0);vehicle.wheelDiameterMm=Math.max(1,+$("vWheelDia").value||1);
    vehicle.trackWidthMm=Math.max(1,+$("vTrack").value||1);vehicle.gearRatio=Math.max(.001,+$("vGear").value||1);
    vehicle.maxMotorRpm=Math.max(1,+$("vRpm").value||1);vehicle.encoderPulsesPerRev=Math.max(1,+$("vEncoder").value||1);
    draw();setStatus(`Vagn: ${vehicle.name}`);
};

$("fitButton").onclick=fitDrawing;
$("zoomInButton").onclick=()=>zoomAt(canvas.width/2,canvas.height/2,1.25);
$("zoomOutButton").onclick=()=>zoomAt(canvas.width/2,canvas.height/2,1/1.25);

$("dxfFile").onchange=async e=>{
    const file=e.target.files[0];if(!file)return;
    try{
        const dxf=await file.text();resetCues();
        if(loadDrawing(file.name,dxf))setStatus("DXF laddad - ej sparad i ESP32");
    }catch{setStatus("Fel vid DXF-import")}
};

$("saveDrawing").onclick=async()=>{
    if(!currentDXF){setStatus("Ingen ritning att spara");return}
    try{
        const r=await fetch("/drawing",{method:"POST",headers:{"Content-Type":"application/octet-stream","X-Filename":encodeURIComponent(currentFileName)},body:currentDXF});
        setStatus(await r.text());
    }catch{setStatus("Fel vid sparning")}
};

$("saveShow").onclick=async()=>{
    if(!currentDXF){setStatus("Ingen ritning laddad");return}
    const json=JSON.stringify({version:8,drawing:{fileName:currentFileName,dxf:currentDXF},vehicle,nodes,cues});
    setStatus("Sparar show...");
    try{
        const r=await fetch("/show",{method:"POST",headers:{"Content-Type":"application/json"},body:json});
        if(!r.ok){setStatus("Fel vid sparning till ESP32");return}
        const blob=new Blob([json],{type:"application/json"}),a=document.createElement("a");
        a.href=URL.createObjectURL(blob);a.download="Roborock_Show.json";a.click();URL.revokeObjectURL(a.href);
        setStatus("Show sparad i ESP32 och på datorn");
    }catch{setStatus("Fel vid sparning av show")}
};

function loadShow(show,msg){
    if(!show.drawing||!show.drawing.dxf)return false;
    nodes=show.nodes||[];cues=show.cues||[];
    if(show.vehicle)vehicle={...vehicle,...show.vehicle};
    selectedCue=cues.length?0:-1;splitAfter=-1;cueNumber=new Set(cues.map(c=>c.name)).size;
    updateCueList();updateCueInfo();
    if(loadDrawing(show.drawing.fileName,show.drawing.dxf)){setStatus(msg);return true}
    return false;
}

$("showFile").onchange=async e=>{
    const file=e.target.files[0];if(!file)return;
    try{
        if(!loadShow(JSON.parse(await file.text()),"Showfil laddad från dator"))setStatus("Ogiltig showfil");
    }catch{setStatus("Felaktig showfil")}
};

async function loadSavedShow(){
    try{
        const r=await fetch("/show");if(!r.ok)return false;
        return loadShow(await r.json(),"Sparad show laddad från ESP32");
    }catch{return false}
}

async function loadSavedDrawing(){
    try{
        const r=await fetch("/drawing");if(!r.ok)return;
        const name=decodeURIComponent(r.headers.get("X-Filename")||"drawing.dxf"),dxf=await r.text();
        if(loadDrawing(name,dxf))setStatus("Sparad ritning laddad");
    }catch{}
}

resetCues();draw();
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