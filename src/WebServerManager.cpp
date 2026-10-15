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
.cueHead{display:flex;justify-content:space-between;align-items:baseline;gap:6px}
.cueTime{font-weight:normal;font-family:monospace;font-size:13px;white-space:nowrap}
.cueDetail{display:block;font-family:monospace;font-size:12px;opacity:.75;margin-top:3px}
.cueDetail.late{color:#d32f2f;opacity:1;font-weight:bold}
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
#driveDisplay.empty{opacity:.45;cursor:default}
#driveCueName{font-weight:bold;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#driveSegmentInfo{font-family:monospace;font-size:13px;margin-top:4px;color:#ddd;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#driveSpeedInfo{font-family:monospace;font-size:13px;margin-top:2px;color:#ddd;white-space:nowrap;min-height:16px}
#driveSpeedInfo.limited{color:#ffb020;font-weight:bold}
#segInfo{font-size:13px;color:#aaa;margin:4px 0 8px}
#segInfo.error{color:#ff5252;font-weight:bold}
input.invalid{border:2px solid #ff5252}
#cueButtons{display:flex;gap:7px}
#cueButtons button{width:52px;height:42px;padding:0;font-size:24px}
#cueButtons button:disabled{opacity:.35;cursor:default}
#undoButton:disabled,#deleteSegButton:disabled{opacity:.35;cursor:default;}
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
.cfg .buttons button:disabled{opacity:.35;cursor:default}
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
<button id="panButton">REDIGERA</button>
<button id="newCueButton">NY CUE</button>
<button id="straightButton">RAK</button>
<button id="rotateButton">ROT</button>
<button id="reverseButton">VÄND</button>
<button id="followButton">FÖLJ</button>
<button id="splitButton">SPLIT SEG</button>
<button id="endCueButton">SLUT CUE</button>
<button id="deleteSegButton">TA BORT SEG</button>
<button id="undoButton" title="Återställ till läget när REDIGERA slogs på">ÅNGRA</button>
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
<div id="driveCueName">▶ STARTA</div>
<div id="driveSegmentInfo">--.</div>
<div id="driveSpeedInfo"></div>
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

<label>
Grader
<input id="rotateDeg" type="number" step="1">
</label>

<div class="buttons">
<button value="cancel">AVBRYT</button>
<button id="rotateSave" value="default">SPARA</button>
</div>

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

<dialog id="segmentDialog">
<form method="dialog" class="cfg">

<h3 id="segmentTitle">SEGMENT</h3>

<label>
Hastighet %
<input id="segSpeedPct" type="number" min="0.1" max="100" step="0.1">
</label>

<label>
Tid sek
<input id="segDuration" type="number" min="0.1" step="0.1">
</label>

<div id="segInfo"></div>

<label>
Vänta efter
<input id="segWait" type="number" min="0" step="0.1">
</label>

<div class="buttons">
<button value="cancel">AVBRYT</button>
<button id="segmentSave" value="default">SPARA</button>
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
let rotateTarget=null,rotateInsert=false,reverseTarget=null,reverseInsert=false;
let editEnabled=false;
let expandedCueName=null;
let segmentTarget=null;

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

        speedMode:"percent",
        speedPct:100,
        durationSec:1,
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
    nodes=[];cues=[];selectedCue=-1;cueNumber=0;activeCueName="";
    splitAfter=-1;expandedCueName=null;
    updateCueList();updateCueInfo();updateEditButtons();
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
    updateEditButtons();

    $("splitButton").classList.remove("active");
    updateCueList();updateCueInfo();draw();return seg;
}

function isSegmentMode(){
    return splitAfter>=0||tool==="straight"||tool==="follow"||tool==="rotate"||tool==="reverse";
}

function updateEditButtons(){
    $("deleteSegButton").disabled=!editEnabled&&!isSegmentMode();
    $("undoButton").disabled=!editEnabled||!editSnapshot;
}

// --------------------------------------------------
// ÅNGRA: läget när REDIGERA slogs på
// --------------------------------------------------

let editSnapshot=null;

function takeEditSnapshot(){
    editSnapshot=JSON.stringify({nodes,cues,vehicle});
}

function restoreEditSnapshot(){
    if(!editSnapshot)return;
    const s=JSON.parse(editSnapshot);
    if(simActive)setSimActive(false);
    nodes=s.nodes;cues=s.cues;vehicle=s.vehicle;
    rotCache=new WeakMap();vandCache=new WeakMap();
    selectedCue=Math.min(selectedCue,cues.length-1);
    if(expandedCueName!==null&&!cues.some(c=>c.name===expandedCueName))expandedCueName=null;
    splitAfter=-1;
    $("splitButton").classList.remove("active");
    setTool("pan");
    updateCueList();updateCueInfo();draw();
}

// --------------------------------------------------
// OSPARADE ÄNDRINGAR
// Jämför mot läget vid senaste laddning/sparning av show.
// --------------------------------------------------

let savedSignature="";

function showSignature(){
    return JSON.stringify({file:currentFileName,nodes,cues,vehicle});
}

function markSaved(){savedSignature=showSignature()}

function isDirty(){
    return cues.length>0&&showSignature()!==savedSignature;
}

function confirmDiscard(action){
    return !isDirty()||confirm(`Du har osparade ändringar i showen.\n${action} ändå? Ändringarna går förlorade.`);
}

function setTool(t){
    tool=t;routeHover=null;
    panButton.classList.toggle("active",t==="pan"&&editEnabled);
    straightButton.classList.toggle("active",t==="straight");
    followButton.classList.toggle("active",t==="follow");
    $("rotateButton").classList.toggle("active",t==="rotate");
    $("reverseButton").classList.toggle("active",t==="reverse");
    setStatus(
        t==="straight"?"RAK: markera slutpunkt":
        t==="follow"?"FÖLJ: håll nere och rita körvägen":
        t==="rotate"?"ROT vald":
        t==="reverse"?"VÄND vald":
        editEnabled?"REDIGERA aktiv":"REDIGERA av"
    );
    updateEditButtons();draw();
}

// Cue-namn måste vara unika: lista, totaltid och omdöpning går på namnet
function cueNameExists(name){return cues.some(c=>c.name===name)}

function freeCueName(){
    let n=cueNumber+1;
    while(cueNameExists(`Cue ${n}`))n++;
    return `Cue ${n}`;
}

// Fråga efter namn tills det är unikt. null = avbrutet.
function askCueName(text,def,old=null){
    let value=def;
    for(;;){
        const name=prompt(text,value);
        if(name===null)return null;
        const n=name.trim()||def;
        if(n===old||!cueNameExists(n))return n;
        alert(`Det finns redan en Cue som heter "${n}".\nVälj ett annat namn.`);
        value=n;
    }
}

function renameCue(index){
    if(index<0||index>=cues.length)return;
    const old=cues[index].name,n=askCueName("Nytt Cue-namn:",old,old);
    if(n===null||n===old)return;
    if(expandedCueName===old)expandedCueName=n;

    let a=index,b=index;
    while(a>0&&cues[a-1].name===old)a--;
    while(b+1<cues.length&&cues[b+1].name===old)b++;

    for(let i=a;i<=b;i++)cues[i].name=n;
    if(activeCueName===old)activeCueName=n;

    updateCueList();updateCueInfo();draw();
    setStatus(`Cue ändrad: ${old} → ${n}`);
}

function updateCueList(){
    cueList.innerHTML="";
    let prev="",step=0;

    cues.forEach((cue,i)=>{
        if(cue.name!==prev){
            prev=cue.name;
            step=1;

            const h=document.createElement("div");
            const open=expandedCueName===cue.name;

            const title=document.createElement("span"),total=document.createElement("span");
            title.textContent=(open?"▼ ":"▶ ")+cue.name;
            total.className="cueTime";
            total.textContent=formatTime(cueTotalTime(cue.name));
            h.className="cueHead";
            h.append(title,total);
            h.style.cursor="pointer";

            Object.assign(h.style,{
                marginTop:"10px",
                marginBottom:"4px",
                fontWeight:"bold",
                padding:"6px 8px",
                borderRadius:"6px",
                background:open?"#1976d2":"transparent",
                color:open?"#fff":"inherit"
            });

            h.onclick=()=>{
                const same=expandedCueName===cue.name;

                if(same){
                    if(editEnabled){
                        renameCue(i);
                    }else{
                        expandedCueName=null;
                        selectedCue=-1;
                    }
                }else{
                    expandedCueName=cue.name;
                    selectedCue=i;
                    splitAfter=-1;
                    routeHover=null;
                }

                updateCueList();
                updateCueInfo();
                draw();
            };

            cueList.appendChild(h);
        }else{
            step++;
        }

        if(expandedCueName!==cue.name)return;

        const b=document.createElement("button");
        b.className="cueItem"+(i===selectedCue?" selected":"");

        b.textContent=
            `${step}. ${cue.type}`+
            (cue.type==="ROT"
                ?` ${cue.rotateDeg>0?"+":""}${cue.rotateDeg}°`
                :"")+
            (cue.endOfCue?" ■":"");

        // % · tid · väntan
        const detail=document.createElement("span");
        const sec=segmentPlannedTime(cue),wait=Math.max(0,cue.waitSec||0);
        const late=cue.speedMode==="time"&&sec>(cue.durationSec||0)+.01;
        detail.className="cueDetail"+(late?" late":"");
        detail.textContent=
            `${roundTo(segmentPct(cue),1)}% · ${formatTime(sec)}${late?" ⚠":""}`+
            (wait>0?` · W ${formatTime(wait)}`:"");
        if(late)detail.title=`Inställd tid ${cue.durationSec} s går inte att köra`;
        b.appendChild(detail);

        b.onclick=()=>{
    const alreadySelected=selectedCue===i;

    selectedCue=i;
    splitAfter=-1;
    routeHover=null;

    updateCueList();
    updateCueInfo();
    draw();

    if(editEnabled && alreadySelected){
        editSegmentSettings(cue);
    }
};

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
    $("driveSpeedInfo").textContent="";

    // Ej startad: visa vilken Cue testet startar på
    // Normalläge: första Cue. REDIGERA: vald Cue.
    if(!simActive){
        const start=editEnabled&&selectedCue>=0?cueStartIndex(selectedCue):0;
        $("driveCueName").textContent="▶ STARTA";
        $("driveSegmentInfo").textContent=cues.length?cues[start].name:"--";
        $("driveDisplay").classList.toggle("empty",!cues.length);
        return;
    }

    const cue=currentCue();
    if(!cue)return;

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

function updateRunButtons(){
    const prev=$("prevCueButton"),next=$("nextCueButton");
    prev.classList.remove("runAllowed","runSelected");
    next.classList.remove("runAllowed","runSelected");
    if(!simActive||!simRunEnabled)return;
    if(simRunDir>0)next.classList.add("runSelected");
    else if(simRunDir<0)prev.classList.add("runSelected");
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
        // START: framåt genom aktuell Cue är tillåtet
        next.disabled=false;

        // I single-cue test får vi inte gå till föregående Cue
        if(simSingleCue){
            prev.disabled=true;
        }else{
            let i=simCueIndex-1;
            while(i>=0&&cues[i].name===name)i--;
            prev.disabled=i<0;
        }

    }else if(simBoundary>0){
        // SLUT: aktuell Cue får backas
        prev.disabled=false;

        // I single-cue test får vi inte gå till nästa Cue
        if(simSingleCue){
            next.disabled=true;
        }else{
            let i=simCueIndex+1;
            while(i<cues.length&&cues[i].name===name)i++;
            next.disabled=i>=cues.length;
        }
    }

    updateRunButtons();
}

$("nextCueButton").onclick=()=>{
    if(!simActive||simRunEnabled)return;

    if(simBoundary<0){
        setSimSegment(cueStartIndex(simCueIndex),false,true);
    }else{
        if(simSingleCue){
            setStatus("Test av vald Cue klart");
            return;
        }

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
        let i=simCueIndex;
        const name=cues[i].name;

        while(i+1<cues.length&&cues[i+1].name===name)i++;

        setSimSegment(i,true,true);
    }else{
        if(simSingleCue){
            setStatus("Test av vald Cue - står vid START");
            return;
        }

        const name=cues[simCueIndex].name;
        let i=simCueIndex-1;

        while(i>=0&&cues[i].name===name)i--;

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
const SIM_SPEED_MM_S=1000;
const SIM_NORMAL_MAX=0.80;
const SIM_WHEEL_MAX_MM_S=SIM_SPEED_MM_S/SIM_NORMAL_MAX;
const SIM_ROT_DEG_S=180;

let sliderPct=0,simActive=false,simPose=null,simCueIndex=-1;
let simProgress=0,simLastTime=0,simFrame=0;
let simAlign=false,simAlignTarget=0,simReverse=false;
let simRotStartHeading=0,simRotFinalDeg=0;
let simVandStartHeading=0,simVandTargetHeading=0,simVandTotalDeg=0,simVandTurnDir=0;
let simWaitUntil=0,simWaitDone=false,simMovedAfterArm=false;
let simBoundary=0,simRunEnabled=false,simRunDir=0;
let rotCache=new WeakMap(),vandCache=new WeakMap();
let simCurrentPct=0;
let simSingleCue=false;
const SIM_RAMP_PCT_S=100;

// --------------------------------------------------
// FARTGRÄNS I KURVOR
// Ytterhjulet får inte gå över maxfart och sidoaccelerationen
// begränsas. Profilen rampas så att vagnen bromsar före kurvan
// och accelererar ut ur den.
// --------------------------------------------------

const SIM_LAT_ACC_MM_S2=1500,SIM_LONG_ACC_MM_S2=1000,SIM_MIN_CURVE_SPEED_MM_S=30;
let simSpeedMmS=0,simSpeedLimited=false;

function curveSpeedLimit(r){
    if(!Number.isFinite(r))return Infinity;
    const half=vehicle.trackWidthMm/2;
    return Math.max(SIM_MIN_CURVE_SPEED_MM_S,Math.min(
        SIM_WHEEL_MAX_MM_S*r/(r+half),
        Math.sqrt(SIM_LAT_ACC_MM_S2*r)
    ));
}

function speedProfile(s){
    const n=s.p.length,v=new Array(n).fill(Infinity);
    for(let i=1;i<n-1;i++)v[i]=curveSpeedLimit(pointRadius(s.p[i-1],s.p[i],s.p[i+1]));
    for(let i=1;i<n;i++)v[i]=Math.min(v[i],Math.sqrt(v[i-1]**2+2*SIM_LONG_ACC_MM_S2*(s.cum[i]-s.cum[i-1])));
    for(let i=n-2;i>=0;i--)v[i]=Math.min(v[i],Math.sqrt(v[i+1]**2+2*SIM_LONG_ACC_MM_S2*(s.cum[i+1]-s.cum[i])));
    return v;
}

function profileSpeedAt(s,d){
    const v=speedProfile(s);
    let i=1;
    while(i<s.cum.length-1&&s.cum[i]<d)i++;
    const d0=s.cum[i-1],d1=s.cum[i],a=v[i-1],b=v[i];
    if(!Number.isFinite(a)||!Number.isFinite(b))return Math.min(a,b);
    return a+(b-a)*(d1>d0?Math.max(0,Math.min(1,(d-d0)/(d1-d0))):0);
}

// --------------------------------------------------
// HASTIGHET % <-> TID
// Tiden räknas med fartprofilen, dvs inklusive inbromsning
// i kurvor. Det fält som skrevs senast (speedMode) styr.
// --------------------------------------------------

function isRotationCue(cue){return cue.type==="ROT"||cue.type==="VÄND"}

function segmentDegrees(cue){
    if(cue.type==="VÄND")return vandCache.get(cue)?.total??180;
    const index=cues.indexOf(cue);
    return index<0?Math.abs(cue.rotateDeg):makeRotPlan(index,headingBefore(index)).finalDeg;
}

function linearTimeAtPct(s,prof,pct){
    const base=SIM_SPEED_MM_S*Math.min(100,Math.max(.1,pct))/100;
    let t=0;
    for(let i=1;i<s.p.length;i++){
        const a=prof[i-1],b=prof[i];
        const limit=Number.isFinite(a)&&Number.isFinite(b)?(a+b)/2:Math.min(a,b);
        t+=(s.cum[i]-s.cum[i-1])/Math.min(base,limit);
    }
    return t;
}

function segmentTimeAtPct(cue,pct){
    if(isRotationCue(cue))return segmentDegrees(cue)/(SIM_ROT_DEG_S*Math.min(100,Math.max(.1,pct))/100);
    const s=simPath(cue);
    return linearTimeAtPct(s,speedProfile(s),pct);
}

function segmentMinTime(cue){return segmentTimeAtPct(cue,100)}

// Procent som ger önskad tid. Går tiden inte att nå blir det 100 %.
function segmentPctForTime(cue,sec){
    if(isRotationCue(cue))return Math.min(100,Math.max(.1,segmentDegrees(cue)/sec/SIM_ROT_DEG_S*100));
    const s=simPath(cue),prof=speedProfile(s);
    if(sec<=linearTimeAtPct(s,prof,100))return 100;
    let lo=.1,hi=100;
    for(let n=0;n<40;n++){
        const mid=(lo+hi)/2;
        if(linearTimeAtPct(s,prof,mid)>sec)lo=mid;else hi=mid;
    }
    return hi;
}

function segmentPct(cue){
    if(cue.speedMode==="time")return segmentPctForTime(cue,Math.max(.1,cue.durationSec||1));
    return Math.min(100,Math.max(.1,cue.speedPct??100));
}

// Körtid för segmentet med inställd % eller tid (utan väntan)
function segmentPlannedTime(cue){
    if(cue.startNode===null)return 0;
    if(!isRotationCue(cue)&&cue.endNode===null)return 0;
    return segmentTimeAtPct(cue,segmentPct(cue));
}

// Total tid för en Cue: alla segment + väntan
function cueTotalTime(name){
    let t=0;
    for(const c of cues)if(c.name===name)t+=segmentPlannedTime(c)+Math.max(0,c.waitSec||0);
    return t;
}

function formatTime(sec){
    if(sec<60)return `${sec.toFixed(1)}s`;
    const m=Math.floor(sec/60),s=sec-m*60;
    return `${m}:${s.toFixed(1).padStart(4,"0")}`;
}

function segmentLinearSpeed(cue,power){
    return SIM_SPEED_MM_S*segmentPct(cue)/100*power;
}

function segmentRotationSpeed(cue,totalDeg,power){
    if(cue.speedMode==="time"){
        const sec=Math.max(.1,cue.durationSec||1);
        return Math.min(SIM_ROT_DEG_S,Math.max(.001,totalDeg)/sec)*power;
    }
    const pct=Math.min(100,Math.max(.1,cue.speedPct??100));
    return SIM_ROT_DEG_S*pct/100*power;
}

function segmentWait(t,cue){
    const wait=Math.max(0,cue.waitSec||0);

    if(wait<=0){
        simWaitUntil=0;
        simWaitDone=false;
        return false;
    }

    if(simWaitDone){
        simWaitDone=false;
        return false;
    }

    if(simWaitUntil===0){
        simWaitUntil=t+wait*1000;
        simCurrentPct=0;
    }

    if(t<simWaitUntil){
        const left=(simWaitUntil-t)/1000;

        $("driveSegmentInfo").textContent=
            `${cueSegmentNumber(simCueIndex)}. WAIT ${left.toFixed(1)}s`;

        return true;
    }

    simWaitUntil=0;
    simWaitDone=true;
    return false;
}

function setDriveSlider(v){
    let x=+v;
    if(!simActive||!simRunEnabled)x=0;
    sliderPct=x;driveSlider.value=x;driveValue.textContent=x;
    const p=50-x/2,a=Math.min(50,p),b=Math.max(50,p),c=simActive?"#20b84b":"#777";
    driveSlider.style.background=`linear-gradient(to bottom,#ddd 0%,#ddd ${a}%,${c} ${a}%,${c} ${b}%,#ddd ${b}%,#ddd 100%)`;
}

function cueStartIndex(index){
    if(index<0||index>=cues.length)return -1;
    const name=cues[index].name;
    while(index>0&&cues[index-1].name===name)index--;
    return index;
}

function simPath(cue){
    const p=sampleCue(cue,30),cum=[0];let total=0;
    for(let i=1;i<p.length;i++){
        total+=Math.hypot(p[i].x-p[i-1].x,p[i].y-p[i-1].y);
        cum.push(total);
    }
    return{p,cum,total};
}

function poseOnPath(cue,d){
    const s=simPath(cue);if(s.p.length<2)return null;
    d=Math.max(0,Math.min(s.total,d));
    let i=1;
    while(i<s.cum.length&&s.cum[i]<d)i++;
    i=Math.min(i,s.p.length-1);
    const a=s.p[i-1],b=s.p[i],d0=s.cum[i-1],d1=s.cum[i];
    const u=d1>d0?(d-d0)/(d1-d0):0;
    return{x:a.x+(b.x-a.x)*u,y:a.y+(b.y-a.y)*u,heading:Math.atan2(b.y-a.y,b.x-a.x)};
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

function positiveAngle(a){
    const full=Math.PI*2;
    a%=full;
    if(a<0)a+=full;
    return a;
}

function angleInDirection(from,to,dir){
    return dir>0?positiveAngle(to-from):positiveAngle(from-to);
}

function driveHeading(pathHeading){return pathHeading+(simReverse?Math.PI:0)}

function vandPathHeading(index){
    for(let i=index+1;i<cues.length;i++){
        if(cues[i].name!==cues[index].name)break;
        const c=cues[i];
        if(c.type==="ROT"||c.type==="VÄND")continue;
        const p=poseOnPath(c,0);
        if(p)return p.heading;
    }
    return null;
}

function nextMoveHeading(index){
    const i=index+1;
    if(i>=cues.length||cues[i].name!==cues[index].name)return null;
    const c=cues[i];
    if(c.type==="ROT"||c.type==="VÄND")return null;
    const p=poseOnPath(c,0);
    return p?driveHeading(p.heading):null;
}

function makeRotPlan(index,startHeading){
    const cue=cues[index],programmed=Math.abs(cue.rotateDeg);
    const dir=cue.rotateDeg>=0?-1:1,target=nextMoveHeading(index);
    if(target===null)return{dir,finalDeg:programmed};

    const firstHit=angleInDirection(startHeading,target,dir)*180/Math.PI;
    if(programmed<360&&firstHit<=programmed+.001)return{dir,finalDeg:firstHit};

    const after=startHeading+dir*programmed*Math.PI/180;
    const extra=angleInDirection(after,target,dir)*180/Math.PI;
    return{dir,finalDeg:programmed+extra};
}

function setSimSegment(index,atEnd=false,keepHeading=false){
    if(index<0||index>=cues.length)return false;

    const cue=cues[index],oldHeading=simPose?simPose.heading:null;
    simCueIndex=index;selectedCue=index;simAlign=false;simWaitUntil=0;

    if(cue.type==="VÄND"){
        const n=nodes[cue.startNode];

        if(!atEnd){
            simVandStartHeading=oldHeading!==null?oldHeading:headingBefore(index);
            const pathHeading=vandPathHeading(index);
            const target=pathHeading!==null
                ? pathHeading+(!simReverse?Math.PI:0)
                : simVandStartHeading+Math.PI;
            simVandTurnDir=cue.vandDir>=0?-1:1;
            const delta=simVandTurnDir<0
                ? positiveAngle(simVandStartHeading-target)
                : positiveAngle(target-simVandStartHeading);

            simVandTotalDeg=delta*180/Math.PI;
            if(simVandTotalDeg<.001)simVandTotalDeg=360;
            simVandTargetHeading=simVandStartHeading+simVandTurnDir*simVandTotalDeg*Math.PI/180;
            simProgress=0;
            vandCache.set(cue,{
                start:simVandStartHeading,target:simVandTargetHeading,
                total:simVandTotalDeg,dir:simVandTurnDir
            });
        }else{
            const p=vandCache.get(cue);
            if(p){
                simVandStartHeading=p.start;simVandTargetHeading=p.target;
                simVandTotalDeg=p.total;simVandTurnDir=p.dir;
            }else{
                simVandStartHeading=oldHeading!==null?oldHeading-Math.PI:headingBefore(index);
                simVandTargetHeading=oldHeading!==null?oldHeading:simVandStartHeading+Math.PI;
                simVandTotalDeg=180;simVandTurnDir=cue.vandDir>=0?-1:1;
            }
            simProgress=simVandTotalDeg;
        }

        simPose={
            x:n.x,y:n.y,
            heading:simVandStartHeading+simVandTurnDir*simProgress*Math.PI/180
        };
    }else if(cue.type==="ROT"){
        const n=nodes[cue.startNode],rotDir=cue.rotateDeg>=0?-1:1;

        if(!atEnd){
            simRotStartHeading=oldHeading!==null?oldHeading:headingBefore(index);
            simRotFinalDeg=makeRotPlan(index,simRotStartHeading).finalDeg;
            simProgress=0;
            rotCache.set(cue,{start:simRotStartHeading,final:simRotFinalDeg});
        }else{
            const p=rotCache.get(cue);
            if(p){
                simRotStartHeading=p.start;
                simRotFinalDeg=p.final;
            }else{
                const base=headingBefore(index);
                simRotFinalDeg=makeRotPlan(index,base).finalDeg;
                simRotStartHeading=oldHeading!==null
                    ? oldHeading-rotDir*simRotFinalDeg*Math.PI/180
                    : base;
            }
            simProgress=simRotFinalDeg;
        }

        simPose={
            x:n.x,y:n.y,
            heading:simRotStartHeading+rotDir*simProgress*Math.PI/180
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
            simAlign=Math.abs(normAngle(wanted-oldHeading))>.01;
        }else{
            simPose={x:p.x,y:p.y,heading:wanted};
        }
    }

    updateCueList();updateCueInfo();updateSimDisplay();draw();
    return true;
}

function nextSimSegment(){
    const i=simCueIndex+1;
    return i<cues.length&&cues[i].name===cues[simCueIndex].name
        ? setSimSegment(i,false,true)
        : false;
}

function prevSimSegment(){
    const i=simCueIndex-1;
    return i>=0&&cues[i].name===cues[simCueIndex].name
        ? setSimSegment(i,true,true)
        : false;
}

function updateSimDisplay(){
    if(!simActive||simCueIndex<0)return;
    const cue=cues[simCueIndex],nr=cueSegmentNumber(simCueIndex);
    $("driveCueName").textContent=cue.name;

    if(cue.type==="ROT"){
        const left=Math.max(0,simRotFinalDeg-simProgress);
        $("driveSegmentInfo").textContent=`${nr}. ROT ${left.toFixed(0)}°`;
    }else if(cue.type==="VÄND"){
        $("driveSegmentInfo").textContent=`${nr}. VÄND`;
    }else{
        const left=Math.max(0,simPath(cue).total-simProgress);
        $("driveSegmentInfo").textContent=`${nr}. ${cue.type} ${(left/1000).toFixed(2)}m`;
        const moving=simCurrentPct!==0;
        $("driveSpeedInfo").textContent=moving?`${(simSpeedMmS/1000).toFixed(2)} m/s${simSpeedLimited?" ▼":""}`:"";
        $("driveSpeedInfo").classList.toggle("limited",moving&&simSpeedLimited);
        return;
    }
    $("driveSpeedInfo").textContent="";
}

function stopAtCueBoundary(side){
    simBoundary=side;simRunEnabled=false;simRunDir=0;simMovedAfterArm=false;simCurrentPct=0;
    setDriveSlider(0);
    updateBoundaryButtons();updateSimDisplay();
    setStatus(side<0?"Cue START - välj + eller -":"Cue SLUT - välj + eller -");
}

function simLoop(t){
    if(!simActive){simFrame=0;return}
    if(!simLastTime)simLastTime=t;
    const dt=Math.min((t-simLastTime)/1000,.05);
    simLastTime=t;
if(simWaitUntil!==0&&simCueIndex>=0){
    const cue=cues[simCueIndex];

    if(segmentWait(t,cue)){
        draw();
        simFrame=requestAnimationFrame(simLoop);
        return;
    }
}

const targetPct=simRunEnabled ? sliderPct : 0;
    
const rampStep=SIM_RAMP_PCT_S*dt;

if(Math.abs(targetPct-simCurrentPct)<=rampStep)
    simCurrentPct=targetPct;
else
    simCurrentPct+=Math.sign(targetPct-simCurrentPct)*rampStep;

if(Math.abs(simCurrentPct)<0.001)simCurrentPct=0;

if(simCurrentPct!==0&&simCueIndex>=0){
    const cue=cues[simCueIndex];
    const dir=Math.sign(simCurrentPct);
    const power=Math.abs(simCurrentPct)/100;

        if(!simMovedAfterArm){simMovedAfterArm=true;updateRunButtons()}

        if(cue.type==="VÄND"){
            const step=segmentRotationSpeed(cue,simVandTotalDeg,power)*dt;

            if(dir>0){
                simProgress=Math.min(simVandTotalDeg,simProgress+step);
                simPose={
                    x:nodes[cue.startNode].x,y:nodes[cue.startNode].y,
                    heading:simVandStartHeading+simVandTurnDir*simProgress*Math.PI/180
                };
                $("driveSegmentInfo").textContent=
                    `${cueSegmentNumber(simCueIndex)}. VÄND ${simProgress.toFixed(0)}°`;

                if(simProgress>=simVandTotalDeg-.001){
                    simPose.heading=simVandTargetHeading;
                    if(segmentWait(t,cue)){draw();simFrame=requestAnimationFrame(simLoop);return}
                    simReverse=!simReverse;
                    if(nextSimSegment()){simLastTime=t;simFrame=requestAnimationFrame(simLoop);return}
                    stopAtCueBoundary(1);simFrame=requestAnimationFrame(simLoop);return;
                }
            }else{
                simProgress=Math.max(0,simProgress-step);
                simPose={
                    x:nodes[cue.startNode].x,y:nodes[cue.startNode].y,
                    heading:simVandStartHeading+simVandTurnDir*simProgress*Math.PI/180
                };
                $("driveSegmentInfo").textContent=
                    `${cueSegmentNumber(simCueIndex)}. VÄND ${simProgress.toFixed(0)}°`;

                if(simProgress<=.001){
                    simPose.heading=simVandStartHeading;
                    if(segmentWait(t,cue)){draw();simFrame=requestAnimationFrame(simLoop);return}
                    simReverse=!simReverse;
                    if(prevSimSegment()){simLastTime=t;simFrame=requestAnimationFrame(simLoop);return}
                    stopAtCueBoundary(-1);simFrame=requestAnimationFrame(simLoop);return;
                }
            }

            draw();simFrame=requestAnimationFrame(simLoop);return;
        }

        if(simAlign&&cue.type!=="ROT"){
            const diff=normAngle(simAlignTarget-simPose.heading);
            const step=SIM_ROT_DEG_S*Math.PI/180*power*dt;
            if(Math.abs(diff)<=step){simPose.heading+=diff;simAlign=false}
            else simPose.heading+=Math.sign(diff)*step;

            const left=Math.abs(normAngle(simAlignTarget-simPose.heading))*180/Math.PI;
            $("driveSegmentInfo").textContent=
                `${cueSegmentNumber(simCueIndex)}. RIKTA ${left.toFixed(0)}°`;

            draw();
            if(simAlign){simFrame=requestAnimationFrame(simLoop);return}
        }

        if(cue.type==="ROT"){
            const rotDir=cue.rotateDeg>=0?-1:1;
            const step=segmentRotationSpeed(cue,simRotFinalDeg,power)*dt;

            if(dir>0){
                simProgress=Math.min(simRotFinalDeg,simProgress+step);
                simPose={
                    x:nodes[cue.startNode].x,y:nodes[cue.startNode].y,
                    heading:simRotStartHeading+rotDir*simProgress*Math.PI/180
                };

                if(simProgress>=simRotFinalDeg-.001){
                    if(segmentWait(t,cue)){updateSimDisplay();draw();simFrame=requestAnimationFrame(simLoop);return}
                    if(nextSimSegment()){simLastTime=t;simFrame=requestAnimationFrame(simLoop);return}
                    stopAtCueBoundary(1);simFrame=requestAnimationFrame(simLoop);return;
                }
            }else{
                simProgress=Math.max(0,simProgress-step);
                simPose={
                    x:nodes[cue.startNode].x,y:nodes[cue.startNode].y,
                    heading:simRotStartHeading+rotDir*simProgress*Math.PI/180
                };

                if(simProgress<=.001){
                    if(segmentWait(t,cue)){updateSimDisplay();draw();simFrame=requestAnimationFrame(simLoop);return}
                    if(prevSimSegment()){simLastTime=t;simFrame=requestAnimationFrame(simLoop);return}
                    stopAtCueBoundary(-1);simFrame=requestAnimationFrame(simLoop);return;
                }
            }
        }else{
            const s=simPath(cue),base=segmentLinearSpeed(cue,power);
            const limit=profileSpeedAt(s,simProgress);
            const speed=Math.min(base,limit);
            simSpeedMmS=speed;
            simSpeedLimited=limit<base;
            simProgress+=dir*speed*dt;

            if(simProgress>=s.total){
                simProgress=s.total;
                if(dir>0){
                    if(segmentWait(t,cue)){updateSimDisplay();draw();simFrame=requestAnimationFrame(simLoop);return}
                    if(nextSimSegment()){simLastTime=t;simFrame=requestAnimationFrame(simLoop);return}
                    simPose=poseOnPath(cue,s.total);
                    if(simPose)simPose.heading=driveHeading(simPose.heading);
                    draw();stopAtCueBoundary(1);simFrame=requestAnimationFrame(simLoop);return;
                }
            }

            if(simProgress<=0){
                simProgress=0;
                if(dir<0){
                    if(segmentWait(t,cue)){updateSimDisplay();draw();simFrame=requestAnimationFrame(simLoop);return}
                    if(prevSimSegment()){simLastTime=t;simFrame=requestAnimationFrame(simLoop);return}
                    simPose=poseOnPath(cue,0);
                    if(simPose)simPose.heading=driveHeading(simPose.heading);
                    draw();stopAtCueBoundary(-1);simFrame=requestAnimationFrame(simLoop);return;
                }
            }

            const p=poseOnPath(cue,simProgress);
            if(p)simPose={x:p.x,y:p.y,heading:driveHeading(p.heading)};
        }

        updateSimDisplay();draw();
    }

    simFrame=requestAnimationFrame(simLoop);
}

function setSimActive(on){
    if(on&&!cues.length){setStatus("Ingen Cue finns");return}
    simActive=on;
    $("driveDisplay").classList.toggle("simActive",on);
    driveSlider.classList.toggle("simActive",on);
    setDriveSlider(0);

    if(on){
        if(!cues.length){simActive=false;setStatus("Ingen Cue finns");return}
        simReverse=false;simRunEnabled=false;simRunDir=0;simBoundary=-1;
        simMovedAfterArm=false;simWaitUntil=0;simCurrentPct=0;
        rotCache=new WeakMap();vandCache=new WeakMap();
        simSingleCue=editEnabled;
        if(simSingleCue && selectedCue>=0){
    simCueIndex=cueStartIndex(selectedCue);
    selectedCue=simCueIndex;
}else{
    selectedCue=0;simCueIndex=0;}

setSimSegment(simCueIndex,false,false);
        $("prevCueButton").disabled=true;
        $("nextCueButton").disabled=false;
        updateRunButtons();
        simLastTime=0;
        if(!simFrame)simFrame=requestAnimationFrame(simLoop);
        setStatus(`${cues[simCueIndex].name} START - tryck +`);
    }else{
    simPose=null;
    simCueIndex=-1;
    simLastTime=0;
    simBoundary=0;
    simRunEnabled=false;
    simRunDir=0;
    simReverse=false;
    simWaitUntil=0;
    simMovedAfterArm=false;
    simCurrentPct=0;    

    $("prevCueButton").disabled=true;
    $("nextCueButton").disabled=true;

    updateRunButtons();

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

    const x=+$("nodeX").value;
    const y=+$("nodeY").value;

    if(!Number.isFinite(x)||!Number.isFinite(y)){
        setStatus("Ogiltigt värde");
        return;
    }

    nodes[dialogNodeId].x=Math.round(x);
    nodes[dialogNodeId].y=Math.round(y);

    const cue=currentCue();
    if(cue&&dialogNodeId===cue.endNode)connectNext(cue);

    syncNodeCurves(dialogNodeId);

    updateCueInfo();
    updateCueList();
    draw();
    setStatus(`${dialogNodeLabel} uppdaterad${radiusWarning(cues.filter(c=>c.startNode===dialogNodeId||c.endNode===dialogNodeId))}`);
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
    for(const c of cues){
        if(c.startNode===editNodeId||c.endNode===editNodeId)syncCurveControls(c);
    }
    updateCueInfo();draw();
}

function editSegmentSettings(cue){
    segmentTarget=cue;

    $("segmentTitle").textContent=
        `${cue.type}${cue.type==="ROT"
            ? ` ${cue.rotateDeg>0?"+":""}${cue.rotateDeg}°`
            : ""}`;

    segMaster=cue.speedMode==="time"?"time":"percent";

    if(segMaster==="time"){
        $("segDuration").value=roundTo(Math.max(cue.durationSec??1,segmentMinTime(cue)),2);
        $("segSpeedPct").value=roundTo(segmentPctForTime(cue,+$("segDuration").value),1);
    }else{
        const pct=Math.min(100,Math.max(.1,cue.speedPct??100));
        $("segSpeedPct").value=roundTo(pct,1);
        $("segDuration").value=ceilTo(segmentTimeAtPct(cue,pct),2);
    }
    $("segWait").value=cue.waitSec??0;

    updateSegmentDialog();

    $("segmentDialog").showModal();
}

// Fältet som skrevs senast styr ("percent" eller "time")
let segMaster="percent";

function roundTo(x,d){const f=10**d;return Math.round(x*f)/f}
function ceilTo(x,d){const f=10**d;return Math.ceil(x*f-1e-6)/f}

// Snabbaste tid avrundad uppåt så att den alltid går att köra
function segMinTimeRounded(cue){return ceilTo(segmentMinTime(cue),2)}

function segPctValid(){
    const pct=+$("segSpeedPct").value;
    return $("segSpeedPct").value!==""&&pct>=.1&&pct<=100;
}

function segTimeValid(){
    const t=+$("segDuration").value;
    return $("segDuration").value!==""&&t>=segMinTimeRounded(segmentTarget);
}

function updateSegmentDialog(){
    const cue=segmentTarget;if(!cue)return;
    const minT=segMinTimeRounded(cue);
    const pctOK=segPctValid(),timeOK=segTimeValid();
    const info=$("segInfo");

    $("segSpeedPct").classList.toggle("invalid",!pctOK);
    $("segDuration").classList.toggle("invalid",!timeOK);
    $("segmentSave").disabled=!pctOK||!timeOK;

    if(!timeOK){
        info.textContent=`För kort tid - snabbast ${minT.toFixed(2)} s`;
    }else if(!pctOK){
        info.textContent="Hastighet måste vara 0.1-100 %";
    }else{
        info.textContent=`Snabbast ${minT.toFixed(2)} s · styr: ${segMaster==="time"?"tid":"%"}`;
    }
    info.classList.toggle("error",!pctOK||!timeOK);
}

$("segSpeedPct").oninput=()=>{
    segMaster="percent";
    if(segPctValid())$("segDuration").value=ceilTo(segmentTimeAtPct(segmentTarget,+$("segSpeedPct").value),2);
    updateSegmentDialog();
};

$("segDuration").oninput=()=>{
    segMaster="time";
    if(segTimeValid())$("segSpeedPct").value=roundTo(segmentPctForTime(segmentTarget,+$("segDuration").value),1);
    updateSegmentDialog();
};

// När fältet lämnas: för kort tid höjs till snabbaste möjliga
$("segDuration").onchange=()=>{
    if(segTimeValid()||$("segDuration").value==="")return;
    $("segDuration").value=segMinTimeRounded(segmentTarget);
    $("segDuration").oninput();
};

$("segSpeedPct").onchange=()=>{
    if(segPctValid()||$("segSpeedPct").value==="")return;
    $("segSpeedPct").value=Math.min(100,Math.max(.1,+$("segSpeedPct").value));
    $("segSpeedPct").oninput();
};

$("segmentSave").onclick=e=>{
    if(!segmentTarget)return;
    if(!segPctValid()||!segTimeValid()){e.preventDefault();return}

    segmentTarget.speedMode=segMaster;

    segmentTarget.speedPct=
        Math.min(100,Math.max(.1,+$("segSpeedPct").value));

    segmentTarget.durationSec=
        Math.max(segMinTimeRounded(segmentTarget),+$("segDuration").value);

    segmentTarget.waitSec=
        Math.max(
            0,
            +$("segWait").value||0
        );

    segmentTarget=null;

    updateCueList();
    updateCueInfo();
    draw();

    setStatus("Segmentinställningar sparade");
};

function editRotation(cue){
    rotateTarget=cue;
    rotateInsert=false;
    $("rotateDeg").value=cue.rotateDeg;
    $("rotateDialog").showModal();
}

function editReverse(cue){
    reverseTarget=cue;
    reverseInsert=false;
    $("reverseDialog").showModal();
}

function pairLines(text){
    const raw=text.replace(/\r/g,"").split("\n"),pairs=[];
    for(let i=0;i+1<raw.length;i+=2)pairs.push({code:parseInt(raw[i].trim()),value:raw[i+1].trim()});
    return pairs;
}

// --------------------------------------------------
// DXF
// Stöd: LINE, CIRCLE, ARC, LWPOLYLINE/POLYLINE (med bågar),
// ELLIPSE, SPLINE och INSERT (block). Allt annat räknas
// i dxfSkipped och visas i ritningsinfon.
// --------------------------------------------------

let dxfSkipped={};

const DXF_ARC_STEP=Math.PI/36;

// Läs en entitet: {type,codes,vertices} och index efter den
function readDxfEntity(pairs,i){
    const ent={type:pairs[i].value,codes:[],vertices:[]};i++;
    while(i<pairs.length&&pairs[i].code!==0)ent.codes.push(pairs[i++]);
    if(ent.type==="POLYLINE"){
        while(i<pairs.length&&pairs[i].code===0&&pairs[i].value==="VERTEX"){
            const v=[];i++;
            while(i<pairs.length&&pairs[i].code!==0)v.push(pairs[i++]);
            ent.vertices.push(v);
        }
        if(i<pairs.length&&pairs[i].value==="SEQEND"){i++;while(i<pairs.length&&pairs[i].code!==0)i++}
    }
    return{ent,next:i};
}

function dxfNum(codes,code,def=0){
    const c=codes.find(q=>q.code===code);
    if(!c)return def;
    const v=+c.value;
    return Number.isFinite(v)?v:def;
}

function dxfStr(codes,code,def=""){
    const c=codes.find(q=>q.code===code);
    return c?c.value:def;
}

function arcPoints(cx,cy,r,a0,a1,out){
    if(a1<a0)a1+=Math.PI*2;
    const n=Math.max(2,Math.ceil((a1-a0)/DXF_ARC_STEP));
    for(let k=0;k<=n;k++){const a=a0+(a1-a0)*k/n;out.push({x:cx+r*Math.cos(a),y:cy+r*Math.sin(a)})}
    return out;
}

// Båge mellan två polylinjepunkter (bulge = tan(vinkel/4))
function bulgePoints(p1,p2,b,out){
    const dx=p2.x-p1.x,dy=p2.y-p1.y,d=Math.hypot(dx,dy);
    if(Math.abs(b)<1e-9||d<1e-9){out.push({x:p2.x,y:p2.y});return}
    const theta=4*Math.atan(b),k=d*(1-b*b)/(4*b);
    const cx=(p1.x+p2.x)/2-dy/d*k,cy=(p1.y+p2.y)/2+dx/d*k;
    const r=Math.hypot(p1.x-cx,p1.y-cy),a0=Math.atan2(p1.y-cy,p1.x-cx);
    const n=Math.max(2,Math.ceil(Math.abs(theta)/DXF_ARC_STEP));
    for(let s=1;s<=n;s++){const a=a0+theta*s/n;out.push({x:cx+r*Math.cos(a),y:cy+r*Math.sin(a)})}
}

function polyPoints(verts,closed){
    const out=[];
    if(!verts.length)return out;
    out.push({x:verts[0].x,y:verts[0].y});
    const n=closed?verts.length:verts.length-1;
    for(let i=0;i<n;i++)bulgePoints(verts[i],verts[(i+1)%verts.length],verts[i].b,out);
    return out;
}

function splinePoints(codes){
    const deg=dxfNum(codes,71,3),knots=[],ctrl=[],fit=[];
    let last=null;
    for(const q of codes){
        if(q.code===40)knots.push(+q.value);
        if(q.code===10){last={x:+q.value,y:0};ctrl.push(last)}
        if(q.code===20&&last)last.y=+q.value;
        if(q.code===11){last={x:+q.value,y:0};fit.push(last)}
        if(q.code===21&&last)last.y=+q.value;
    }
    const n=ctrl.length;
    // Utan giltiga knutar: rita genom fit-punkter eller kontrollpunkter
    if(n<=deg||knots.length!==n+deg+1)return fit.length>1?fit:ctrl;

    const out=[],u0=knots[deg],u1=knots[n],steps=n*8;
    for(let s=0;s<=steps;s++){
        const u=u0+(u1-u0)*s/steps;
        let k=deg;
        while(k<n-1&&u>=knots[k+1])k++;
        const d=[];
        for(let j=0;j<=deg;j++)d.push({...ctrl[j+k-deg]});
        for(let r=1;r<=deg;r++){
            for(let j=deg;j>=r;j--){
                const den=knots[j+1+k-r]-knots[j+k-deg];
                const a=den?(u-knots[j+k-deg])/den:0;
                d[j]={x:(1-a)*d[j-1].x+a*d[j].x,y:(1-a)*d[j-1].y+a*d[j].y};
            }
        }
        out.push(d[deg]);
    }
    return out;
}

function ellipsePoints(codes){
    const cx=dxfNum(codes,10),cy=dxfNum(codes,20),mx=dxfNum(codes,11),my=dxfNum(codes,21);
    const ratio=dxfNum(codes,40,1);
    let t0=dxfNum(codes,41,0),t1=dxfNum(codes,42,Math.PI*2);
    if(t1<=t0)t1+=Math.PI*2;
    const n=Math.max(8,Math.ceil((t1-t0)/DXF_ARC_STEP)),out=[];
    for(let k=0;k<=n;k++){
        const t=t0+(t1-t0)*k/n,c=Math.cos(t),s=Math.sin(t);
        out.push({x:cx+mx*c-ratio*my*s,y:cy+my*c+ratio*mx*s});
    }
    return out;
}

// Entitet -> ritbara objekt (LINE/CIRCLE/ARC/POLYLINE)
function dxfToPrims(ent,blocks,depth){
    const c=ent.codes;
    if(dxfNum(c,67,0)===1)return[];   // pappersrymd
    const flip=dxfNum(c,230,1)<0;     // speglad (normal Z=-1): x byter tecken
    const fx=x=>flip?-x:x;
    switch(ent.type){
        case"LINE":
            return[{type:"LINE",x1:dxfNum(c,10),y1:dxfNum(c,20),x2:dxfNum(c,11),y2:dxfNum(c,21)}];
        case"CIRCLE":
            return[{type:"CIRCLE",x:fx(dxfNum(c,10)),y:dxfNum(c,20),r:dxfNum(c,40)}];
        case"ARC":{
            const a0=dxfNum(c,50),a1=dxfNum(c,51);
            return[{type:"ARC",x:fx(dxfNum(c,10)),y:dxfNum(c,20),r:dxfNum(c,40),start:flip?180-a1:a0,end:flip?180-a0:a1}];
        }
        case"LWPOLYLINE":{
            const verts=[];let cur=null;
            for(const q of c){
                if(q.code===10){cur={x:fx(+q.value),y:0,b:0};verts.push(cur)}
                if(q.code===20&&cur)cur.y=+q.value;
                if(q.code===42&&cur)cur.b=flip?-q.value:+q.value;
            }
            const pts=polyPoints(verts,(dxfNum(c,70)&1)!==0);
            return pts.length>1?[{type:"POLYLINE",points:pts,closed:false}]:[];
        }
        case"POLYLINE":{
            const verts=ent.vertices.map(v=>({x:dxfNum(v,10),y:dxfNum(v,20),b:dxfNum(v,42)}));
            const pts=polyPoints(verts,(dxfNum(c,70)&1)!==0);
            return pts.length>1?[{type:"POLYLINE",points:pts,closed:false}]:[];
        }
        case"ELLIPSE":
            return[{type:"POLYLINE",points:ellipsePoints(c),closed:false}];
        case"SPLINE":{
            const pts=splinePoints(c);
            return pts.length>1?[{type:"POLYLINE",points:pts,closed:false}]:[];
        }
        case"INSERT":{
            const block=blocks[dxfStr(c,2)];
            if(!block||depth>8)return null;
            const sx=dxfNum(c,41,1),sy=dxfNum(c,42,sx),rot=dxfNum(c,50)*Math.PI/180;
            const ix=dxfNum(c,10),iy=dxfNum(c,20),cos=Math.cos(rot),sin=Math.sin(rot);
            const tf=p=>{
                const x=(p.x-block.baseX)*sx,y=(p.y-block.baseY)*sy;
                return{x:ix+x*cos-y*sin,y:iy+x*sin+y*cos};
            };
            const out=[];
            for(const be of block.entities){
                const prims=dxfToPrims(be,blocks,depth+1);
                if(prims===null){dxfSkipped[be.type]=(dxfSkipped[be.type]||0)+1;continue}
                for(const p of prims)out.push({type:"POLYLINE",points:primPoints(p).map(tf),closed:false});
            }
            return out;
        }
    }
    return null;
}

function primPoints(p){
    if(p.type==="LINE")return[{x:p.x1,y:p.y1},{x:p.x2,y:p.y2}];
    if(p.type==="POLYLINE")return p.points;
    if(p.type==="CIRCLE")return arcPoints(p.x,p.y,p.r,0,Math.PI*2,[]);
    return arcPoints(p.x,p.y,p.r,p.start*Math.PI/180,p.end*Math.PI/180,[]);
}

function parseDXF(text){
    const pairs=pairLines(text),out=[],blocks={};
    let i=0,section="",block=null;
    dxfSkipped={};

    while(i<pairs.length){
        const p=pairs[i];
        if(p.code===0&&p.value==="SECTION"){if(i+1<pairs.length&&pairs[i+1].code===2)section=pairs[i+1].value;i+=2;continue}
        if(p.code===0&&p.value==="ENDSEC"){section="";i++;continue}
        if(p.code!==0||(section!=="ENTITIES"&&section!=="BLOCKS")){i++;continue}

        const{ent,next}=readDxfEntity(pairs,i);i=next;

        if(section==="BLOCKS"){
            if(ent.type==="BLOCK"){
                block={entities:[],baseX:dxfNum(ent.codes,10),baseY:dxfNum(ent.codes,20)};
                blocks[dxfStr(ent.codes,2)]=block;
            }else if(ent.type==="ENDBLK")block=null;
            else if(block)block.entities.push(ent);
            continue;
        }

        const prims=dxfToPrims(ent,blocks,0);
        if(prims===null)dxfSkipped[ent.type]=(dxfSkipped[ent.type]||0)+1;
        else out.push(...prims);
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

        // Orange: vagnen måste sakta in från full fart
        // Rött: radie < halva spårvidden, innerhjulet backar
        const limit=vehicle.trackWidthMm/2;
        const orange=new Path2D(),red=new Path2D(),rings=[];
        for(let i=1;i<s.length-1;i++){
            const r=pointRadius(s[i-1],s[i],s[i+1]);
            const tight=r<limit;
            if(!tight&&curveSpeedLimit(r)>=SIM_SPEED_MM_S)continue;
            const a=worldToScreen(s[i-1].x,s[i-1].y),b=worldToScreen(s[i+1].x,s[i+1].y);
            const path=tight?red:orange;
            path.moveTo(a.x,a.y);path.lineTo(b.x,b.y);
            if(!tight)continue;
            const c=worldToScreen(s[i].x,s[i].y);
            if(!rings.some(q=>Math.hypot(q.x-c.x,q.y-c.y)<20))rings.push(c);
        }
        ctx.lineWidth=index===selectedCue?6:4;
        ctx.strokeStyle="#ff9800";ctx.stroke(orange);
        ctx.strokeStyle="#e32626";ctx.stroke(red);
        ctx.lineWidth=2;
        for(const q of rings){ctx.beginPath();ctx.arc(q.x,q.y,10,0,Math.PI*2);ctx.stroke()}
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

// Ta bort noder som ingen Cue använder och numrera om
function compactNodes(){
    const map=new Map(),next=[];
    const use=id=>{
        if(id===null||!nodes[id])return null;
        if(!map.has(id)){map.set(id,next.length);next.push({...nodes[id],id:next.length})}
        return map.get(id);
    };
    for(const c of cues){c.startNode=use(c.startNode);c.endNode=use(c.endNode)}
    const removed=nodes.length-next.length;
    nodes=next;
    return removed;
}

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

function maxCurveControls(cue){
    return Math.max(0,Math.round(cueLength(cue)/500)-1);
}

function addCurveControls(cue){
    if(!cue||(cue.type!=="KURVA"&&cue.type!=="FÖLJ")||cue.startNode===null||cue.endNode===null)return false;
    const count=maxCurveControls(cue);
    let added=false;
    while(cue.path.length<count){
        const pts=sampleCue(cue,24),lengths=Array(cue.path.length+1).fill(0);
        for(let i=1;i<pts.length;i++)lengths[pts[i-1].segment]+=Math.hypot(pts[i].x-pts[i-1].x,pts[i].y-pts[i-1].y);
        const segment=lengths.indexOf(Math.max(...lengths)),target=lengths[segment]/2;
        let distance=0;
        for(let i=1;i<pts.length;i++){
            if(pts[i-1].segment!==segment)continue;
            const step=Math.hypot(pts[i].x-pts[i-1].x,pts[i].y-pts[i-1].y);
            if(step>0&&distance+step>=target){
                const t=(target-distance)/step;
                cue.path.splice(segment,0,{
                    x:Math.round(pts[i-1].x+(pts[i].x-pts[i-1].x)*t),
                    y:Math.round(pts[i-1].y+(pts[i].y-pts[i-1].y)*t)
                });
                added=true;
                break;
            }
            distance+=step;
        }
        if(cue.path.length===lengths.length-1)break;
    }
    return added;
}

// Ta bort dragpunkter när kurvan blir kortare.
// Hysteres så att punkter inte läggs till/tas bort om vartannat vid gränsen.
// Punkten som tas bort är den som påverkar formen minst. "keep" tas aldrig bort.
// Minst en punkt behålls så att kurvan alltid går att dra i.
const CURVE_CONTROL_HYST_MM=150;

function removeCurveControls(cue,keep=null){
    if(!cue||(cue.type!=="KURVA"&&cue.type!=="FÖLJ")||cue.startNode===null||cue.endNode===null)return false;
    let removed=false;
    while(cue.path.length>1){
        const pts=getCuePoints(cue);
        let best=-1,bestDist=Infinity;
        for(let i=0;i<cue.path.length;i++){
            if(cue.path[i]===keep)continue;
            const d=perpendicularDistance(pts[i+1],pts[i],pts[i+2]);
            if(d<bestDist){bestDist=d;best=i}
        }
        if(best<0)break;
        // Bedöm längden utan punkten, annars kan punkten själv hålla kvar längden
        const path=cue.path;
        cue.path=path.filter((_,i)=>i!==best);
        if(path.length<=Math.max(1,Math.round((cueLength(cue)+CURVE_CONTROL_HYST_MM)/500)-1)){
            cue.path=path;
            break;
        }
        removed=true;
    }
    return removed;
}

function syncCurveControls(cue,keep=null){
    const added=addCurveControls(cue);
    const removed=removeCurveControls(cue,keep);
    return added||removed;
}

// Efter nodflytt: räkna om antalet punkter när kurvan är färdigdragen.
function syncNodeCurves(id){
    for(const c of cues){
        if((c.startNode===id||c.endNode===id))syncCurveControls(c);
    }
}

function spaceFollowControls(cue){
    const pts=sampleCue(cue,24),dist=[0];
    for(let i=1;i<pts.length;i++)dist.push(dist[i-1]+Math.hypot(pts[i].x-pts[i-1].x,pts[i].y-pts[i-1].y));
    const length=dist[dist.length-1],count=Math.max(0,Math.round(length/500)-1),path=[];
    let j=1;
    for(let i=1;i<=count;i++){
        const target=length*i/(count+1);
        while(j<pts.length-1&&dist[j]<target)j++;
        const t=(target-dist[j-1])/(dist[j]-dist[j-1]);
        path.push({x:Math.round(pts[j-1].x+(pts[j].x-pts[j-1].x)*t),y:Math.round(pts[j-1].y+(pts[j].y-pts[j-1].y)*t)});
    }
    cue.path=path;
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

// Radie i punkt b genom a,b,c (Infinity om rakt)
function pointRadius(a,b,c){
    const ab=Math.hypot(b.x-a.x,b.y-a.y);
    const bc=Math.hypot(c.x-b.x,c.y-b.y);
    const ca=Math.hypot(a.x-c.x,a.y-c.y);

    const cross=Math.abs(
        (b.x-a.x)*(c.y-a.y)-
        (b.y-a.y)*(c.x-a.x)
    );

    return cross<0.001?Infinity:ab*bc*ca/(2*cross);
}

function minCurveRadius(cue){
    const p=sampleCue(cue,24);
    let minR=Infinity;
    for(let i=1;i<p.length-1;i++)minR=Math.min(minR,pointRadius(p[i-1],p[i],p[i+1]));
    return minR;
}

// Varningstext om någon kurva är snävare än halva spårvidden (innerhjulet backar)
function radiusWarning(list=cues){
    const limit=vehicle.trackWidthMm/2;
    let minR=Infinity;
    for(const c of list){
        if((c.type==="KURVA"||c.type==="FÖLJ")&&c.startNode!==null&&c.endNode!==null)minR=Math.min(minR,minCurveRadius(c));
    }
    return minR<limit?` - VARNING: kurvradie ${Math.round(minR)} mm < ${Math.round(limit)} mm (rött)`:"";
}

function enforceMinCurveRadius(cue){
    if(cue.startNode===null||cue.endNode===null)return;

    const minR=vehicle.trackWidthMm/2;

    for(let pass=0;pass<20;pass++){
        if(minCurveRadius(cue)>=minR)return;

        const pts=[
            {...nodes[cue.startNode]},
            ...cue.path.map(p=>({...p})),
            {...nodes[cue.endNode]}
        ];

        if(pts.length<3)return;

        const next=[pts[0]];

        for(let i=1;i<pts.length-1;i++){
            next.push({
                x:pts[i-1].x*.25+pts[i].x*.5+pts[i+1].x*.25,
                y:pts[i-1].y*.25+pts[i].y*.5+pts[i+1].y*.25
            });
        }

        next.push(pts[pts.length-1]);

        cue.path=next.slice(1,-1).map(p=>({
            x:Math.round(p.x),
            y:Math.round(p.y)
        }));
    }
}

function beginControlEdit(i){
    const cue=currentCue();
    editMode="control";
    editPathIndex=i;
    canvas.style.cursor="grabbing";
}

function updateControlEdit(w){
    const cue=currentCue();
    if(!cue||editPathIndex<0||editPathIndex>=cue.path.length)return;

    // Ingen radiespärr: för snäva partier ritas röda i stället
    const activePoint={
        x:Math.round(w.x),
        y:Math.round(w.y)
    };

    cue.path[editPathIndex]=activePoint;

    if(syncCurveControls(cue,activePoint))
        editPathIndex=cue.path.indexOf(activePoint);

    updateCueInfo();
    draw();
}

function beginCurveEdit(hit){
    const cue=currentCue();
    if(cue.type==="RAK"||cue.type==="KURVA"||cue.type==="FÖLJ"){
        const max=Math.max(1,maxCurveControls(cue));
        if(cue.path.length>=max){
            if(!cue.path.length)return;
            let nearest=0;
            for(let i=1;i<cue.path.length;i++){
                if(Math.hypot(cue.path[i].x-hit.x,cue.path[i].y-hit.y)<Math.hypot(cue.path[nearest].x-hit.x,cue.path[nearest].y-hit.y))nearest=i;
            }
            beginControlEdit(nearest);
            return;
        }
    }

    cue.path.splice(hit.segment,0,{
        x:Math.round(hit.x),
        y:Math.round(hit.y)
    });

    editMode="control";
    editPathIndex=hit.segment;

    if(cue.type==="RAK")cue.type="KURVA";

    updateCueList();
    updateCueInfo();
    canvas.style.cursor="grabbing";
}

function endEdit(){
    editMode=null;
    editPathIndex=-1;
    canvas.style.cursor="crosshair";
    updateCueInfo();
    updateCueList();
    draw();
    const cue=currentCue();
    if(cue)setStatus(`Kontrollpunkt flyttad${radiusWarning([cue])}`);
}

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
    const skipped=Object.entries(dxfSkipped);
    const total=skipped.reduce((s,[,n])=>s+n,0);
    drawingInfo.textContent=`${currentFileName} | ${entities.length} objekt | ${(w/1000).toFixed(2)} × ${(h/1000).toFixed(2)} m`+
        (total?` | ${total} visas ej (${skipped.map(([t,n])=>`${t} ${n}`).join(", ")})`:"");
}

canvas.addEventListener("wheel",e=>{
    if(!bounds)return;e.preventDefault();const p=getCanvasPosition(e);zoomAt(p.x,p.y,e.deltaY<0?1.15:1/1.15);
},{passive:false});

canvas.addEventListener("pointerdown",e=>{
    if(!bounds)return;
    const p=getCanvasPosition(e),w=screenToWorld(p.x,p.y),cue=currentCue();

    if(tool==="pan"&&cue&&editEnabled){
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
        if(h){beginCurveEdit(h);canvas.setPointerCapture(e.pointerId);setStatus(editMode==="control"?"Flyttar kontrollpunkt":"Inga interna kontrollpunkter för denna banlängd");return}
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
    const cue=currentCue();
    if(cue&&id===cue.endNode)connectNext(cue);

    syncNodeCurves(id);

    updateCueInfo();
    updateCueList();
    draw();
    setStatus(`${label}-punkt flyttad${radiusWarning(cues.filter(c=>c.startNode===id||c.endNode===id))}`);
}
        if(e&&canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
        return;
    }

    if(editMode)endEdit();
    else if(routeDrawing){
        routeDrawing=false;
        const p=getCanvasPosition(e),w=screenToWorld(p.x,p.y),cue=currentCue();
        cue.endNode=addNode(w.x,w.y);smoothFollowCue(cue);spaceFollowControls(cue);enforceMinCurveRadius(cue);connectNext(cue);
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

panButton.onclick=()=>{
    editEnabled=!editEnabled;
    if(editEnabled)takeEditSnapshot();
    else{
        // Ur REDIGERA: tillbaka till Cue 1
        editSnapshot=null;
        if(simActive)setSimActive(false);
        if(cues.length){selectedCue=0;expandedCueName=cues[0].name}
    }
    splitAfter=-1;
    $("splitButton").classList.remove("active");
    setTool("pan");
    updateCueList();updateCueInfo();
    setStatus(editEnabled?"REDIGERA aktiv":"REDIGERA av");
};

$("newCueButton").onclick=()=>{
    const last=lastSegment();
    if(last&&!last.endOfCue){setStatus("Avsluta nuvarande Cue med SLUT CUE först");return}
    const name=askCueName("Namn på Cue:",freeCueName());if(name===null)return;
    cueNumber++;activeCueName=name;
    const cue=createEmptyCue(activeCueName);if(last&&last.endNode!==null)cue.startNode=last.endNode;
    cues.push(cue);selectedCue=cues.length-1;splitAfter=-1;
    updateCueList();updateCueInfo();draw();setTool("pan");setStatus(`Ny Cue: ${activeCueName} - välj RAK, ROT eller FÖLJ`);
};

$("splitButton").onclick=()=>{
    const cue=currentCue();

    if(!cue||cue.endNode===null){
        setStatus("Välj ett färdigt segment först");
        return;
    }

    splitAfter=selectedCue;
    updateEditButtons();

    $("splitButton").classList.add("active");

    updateSegmentButtons();

    setStatus("SPLIT aktiv - välj RAK, ROT, VÄND eller FÖLJ");
};

straightButton.onclick=()=>{
    const current=currentCue();

    // REDIGERA: räta ut markerad kurva
    if(splitAfter<0 &&
       editEnabled &&
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
    $("rotateDialog").showModal();
    $("rotateDeg").focus();
};

$("reverseButton").onclick=()=>{
    const cue=currentCue();
    const canAppend=cue&&selectedCue===cues.length-1&&!cue.endOfCue&&cue.endNode!==null;
    if(splitAfter<0&&!canAppend)return;

    reverseTarget=null;
    reverseInsert=splitAfter>=0;
    setTool("reverse");
    $("reverseDialog").showModal();
};

function chooseReverse(dir){
    if(reverseTarget){
        reverseTarget.vandDir=dir;
        reverseTarget=null;
        updateCueList();updateCueInfo();draw();
        setStatus(dir>0?"VÄND medsols":"VÄND motsols");
        return;
    }

    let seg;
    if(reverseInsert){
        seg=insertSegment("VÄND");
        if(!seg)return;
    }else{
        const last=lastSegment();
        if(!last){setStatus("Skapa NY CUE först");return}
        if(last.endOfCue){setStatus("Cue är avslutad");return}
        if(last.endNode===null){setStatus("Gör färdigt segmentet först");return}
        seg=createEmptyCue(last.name,"VÄND");
        seg.startNode=last.endNode;
        cues.push(seg);
        selectedCue=cues.length-1;
    }

    seg.vandDir=dir;
    seg.endNode=seg.startNode;
    connectNext(seg);
    reverseInsert=false;
    updateCueList();updateCueInfo();draw();
    setTool("pan");
    setStatus(dir>0?"VÄND medsols tillagd":"VÄND motsols tillagd");
}

$("reverseCW").onclick=()=>chooseReverse(1);
$("reverseCCW").onclick=()=>chooseReverse(-1);

$("rotateSave").onclick=()=>{
    const a=+$("rotateDeg").value;
    if(!Number.isFinite(a)||a===0){setStatus("Ogiltig rotation");return}

    if(rotateTarget){
    rotateTarget.rotateDeg=a;
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
    if(!editEnabled||!editSnapshot)return;
    if(JSON.stringify({nodes,cues,vehicle})===editSnapshot){setStatus("Inga ändringar att ångra");return}
    if(!confirm("Ångra alla ändringar sedan REDIGERA slogs på?"))return;
    restoreEditSnapshot();
    setStatus("Ändringar ångrade - tillbaka till läget när REDIGERA slogs på");
};

$("deleteSegButton").onclick=()=>{
    if(!editEnabled&&!isSegmentMode()){
        setStatus("Aktivera REDIGERA eller skapa segment för att använda TA BORT SEG");
        return;
    }

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

    selectedCue=cues.length?Math.min(i,cues.length-1):-1;
    splitAfter=-1;
    if(!simActive)compactNodes();

    updateCueList();updateCueInfo();updateEditButtons();draw();
    setStatus("Segment borttaget");
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
    vehicle.maxMotorRpm=Math.max(1,+$("vRpm").value||1);
vehicle.encoderPulsesPerRev=Math.max(1,+$("vEncoder").value||1);

draw();
setStatus(
    `Vagn: ${vehicle.name} | min radie ${(vehicle.trackWidthMm/2).toFixed(0)} mm${radiusWarning()}`
);
};

$("fitButton").onclick=fitDrawing;
$("zoomInButton").onclick=()=>zoomAt(canvas.width/2,canvas.height/2,1.25);
$("zoomOutButton").onclick=()=>zoomAt(canvas.width/2,canvas.height/2,1/1.25);

// Fråga innan filväljaren öppnas om det finns osparade ändringar
$("dxfFile").onclick=e=>{
    if(cues.length&&!confirm(isDirty()
        ?"Ny DXF tar bort alla Cuer och du har osparade ändringar.\nFortsätta?"
        :"Ny DXF tar bort alla Cuer.\nFortsätta?"))e.preventDefault();
};

$("dxfFile").onchange=async e=>{
    const file=e.target.files[0];if(!file)return;
    e.target.value="";
    try{
        const dxf=await file.text();resetCues();
        if(loadDrawing(file.name,dxf))setStatus("DXF laddad - ej sparad i ESP32");
        markSaved();
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
    if(!simActive)compactNodes();
    const json=JSON.stringify({version:8,drawing:{fileName:currentFileName,dxf:currentDXF},vehicle,nodes,cues});

    // Alltid lokal kopia först, så att inget går förlorat om ESP32 inte svarar
    const blob=new Blob([json],{type:"application/json"}),a=document.createElement("a");
    a.href=URL.createObjectURL(blob);a.download="Roborock_Show.json";a.click();
    setTimeout(()=>URL.revokeObjectURL(a.href),1000);
    markSaved();

    setStatus("Sparad på datorn - sparar i ESP32...");
    try{
        const r=await fetch("/show",{method:"POST",headers:{"Content-Type":"application/json"},body:json});
        setStatus(r.ok?"Show sparad på datorn och i ESP32":"Sparad på datorn - FEL vid sparning i ESP32");
    }catch{setStatus("Sparad på datorn - ESP32 svarar inte")}
};

function loadShow(show,msg){
    if(!show.drawing||!show.drawing.dxf)return false;
    nodes=show.nodes||[];
    cues=(show.cues||[]).map(c=>{
        const x={speedMode:"percent",speedPct:100,durationSec:1,waitSec:0,...c};
        Object.keys(x).filter(k=>k.startsWith("sim")).forEach(k=>delete x[k]);
        return x;
    });
    if(show.vehicle)vehicle={...vehicle,...show.vehicle};
    compactNodes();
    selectedCue=cues.length?0:-1;splitAfter=-1;expandedCueName=null;
    cueNumber=new Set(cues.map(c=>c.name)).size;
    if(editEnabled)takeEditSnapshot();
    updateCueList();updateCueInfo();updateEditButtons();
    if(loadDrawing(show.drawing.fileName,show.drawing.dxf)){markSaved();setStatus(msg);return true}
    return false;
}

$("showFile").onclick=e=>{
    if(!confirmDiscard("Ladda en annan showfil"))e.preventDefault();
};

$("showFile").onchange=async e=>{
    const file=e.target.files[0];if(!file)return;
    e.target.value="";
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
        markSaved();
    }catch{}
}

// Varna om fliken stängs med osparade ändringar
window.addEventListener("beforeunload",e=>{
    if(!isDirty())return;
    e.preventDefault();
    e.returnValue="";
});

resetCues();draw();markSaved();
loadSavedShow().then(ok=>{if(!ok)loadSavedDrawing()});
updateEditButtons();
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
    // Namnet är URL-kodat av webbsidan och sparas orört.
    // Webbsidan avkodar det själv när ritningen hämtas.
    if (!name.length())
        name = "drawing.dxf";

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