#include <WiFi.h>
#include <WebServer.h>

// ===== WIFI =====
const char* ssid     = "SSID";
const char* password = "PASSWORD";

WebServer server(80);

// ===== MOTOR PINS =====
// LEFT side  — Driver A  (3 motors in parallel)
#define L_RPWM 25
#define L_LPWM 26
#define L_REN  18
#define L_LEN  21

// RIGHT side — Driver B  (3 motors in parallel)
#define R_RPWM 27      // was 270 — typo fixed
#define R_LPWM 14
#define R_REN  13
#define R_LEN  12

// ===== PWM CONFIG =====
#define PWM_FREQ 1000
#define PWM_RES  8     // 0-255

#define CH_L_FWD 0
#define CH_L_REV 1
#define CH_R_FWD 2
#define CH_R_REV 3

int speedVal = 180;    // default speed (0-255)

// =====================================================================
//  MOTOR HELPERS
// =====================================================================
void enableDrivers() {
  digitalWrite(L_REN, HIGH); digitalWrite(L_LEN, HIGH);
  digitalWrite(R_REN, HIGH); digitalWrite(R_LEN, HIGH);
}
void disableDrivers() {
  digitalWrite(L_REN, LOW);  digitalWrite(L_LEN, LOW);
  digitalWrite(R_REN, LOW);  digitalWrite(R_LEN, LOW);
}

void setLeft(int spd) {
  ledcWrite(CH_L_FWD, spd > 0 ?  spd : 0);
  ledcWrite(CH_L_REV, spd < 0 ? -spd : 0);
}
void setRight(int spd) {
  spd = -spd;  // Right motors physically wired inverted — negate to correct
  ledcWrite(CH_R_FWD, spd > 0 ?  spd : 0);
  ledcWrite(CH_R_REV, spd < 0 ? -spd : 0);
}
void stopAll() { setLeft(0); setRight(0); }

// =====================================================================
//  MOVEMENT
// =====================================================================
void forward()  { enableDrivers(); setLeft( speedVal); setRight( speedVal); }
void backward() { enableDrivers(); setLeft(-speedVal); setRight(-speedVal); }
void turnLeft() { enableDrivers(); setLeft( speedVal); setRight(-speedVal); }
void turnRight(){ enableDrivers(); setLeft(-speedVal); setRight( speedVal); }
void stopCar()  { stopAll(); disableDrivers(); }

// =====================================================================
//  WEB PAGE  (served once, fetch-based control — no page reloads)
// =====================================================================
void handleRoot() {
  const char* html = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>6WD Robot</title>
<style>
  :root{
    --bg:#0a0a0f;
    --panel:#12121a;
    --border:#1e1e2e;
    --accent:#00c8ff;
    --accent2:#7c3aed;
    --red:#ff4060;
    --text:#e2e8f0;
    --muted:#64748b;
  }
  *{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
  body{
    background:var(--bg);
    color:var(--text);
    font-family:'Segoe UI',system-ui,sans-serif;
    min-height:100dvh;
    display:flex;
    flex-direction:column;
    align-items:center;
    padding:24px 16px 32px;
    gap:24px;
  }

  /* ── header ── */
  header{text-align:center}
  header h1{
    font-size:1.5rem;font-weight:700;letter-spacing:6px;
    text-transform:uppercase;
    background:linear-gradient(90deg,var(--accent),var(--accent2));
    -webkit-background-clip:text;-webkit-text-fill-color:transparent;
  }
  header p{font-size:.7rem;letter-spacing:3px;color:var(--muted);margin-top:4px}

  /* ── status bar ── */
  .statusbar{
    display:flex;align-items:center;gap:10px;
    background:var(--panel);border:1px solid var(--border);
    border-radius:999px;padding:6px 18px;font-size:.72rem;
    letter-spacing:1px;color:var(--muted);
  }
  .dot{width:8px;height:8px;border-radius:50%;background:var(--muted);
       transition:background .3s}
  .dot.on{background:#22c55e;box-shadow:0 0 6px #22c55e}
  #cmdlabel{color:var(--text);min-width:80px;text-align:center}

  /* ── speed section ── */
  .speed-section{
    width:100%;max-width:340px;
    background:var(--panel);border:1px solid var(--border);
    border-radius:16px;padding:18px 20px;
  }
  .speed-header{
    display:flex;justify-content:space-between;align-items:baseline;
    margin-bottom:14px;
  }
  .speed-header span:first-child{
    font-size:.65rem;letter-spacing:3px;text-transform:uppercase;color:var(--muted);
  }
  #speedDisplay{
    font-size:1.4rem;font-weight:700;color:var(--accent);
    font-variant-numeric:tabular-nums;
  }
  .bar-track{
    width:100%;height:8px;background:#1e1e2e;border-radius:99px;
    position:relative;cursor:pointer;
  }
  .bar-fill{
    height:100%;border-radius:99px;
    background:linear-gradient(90deg,var(--accent2),var(--accent));
    pointer-events:none;transition:width .05s;
  }
  input[type=range]{
    width:100%;margin-top:10px;
    -webkit-appearance:none;appearance:none;
    background:transparent;cursor:pointer;
  }
  input[type=range]::-webkit-slider-runnable-track{
    height:8px;border-radius:99px;
    background:linear-gradient(90deg,
      var(--accent2) calc(var(--pct) * 1%),
      #1e1e2e calc(var(--pct) * 1%));
  }
  input[type=range]::-webkit-slider-thumb{
    -webkit-appearance:none;
    width:22px;height:22px;border-radius:50%;
    background:var(--accent);
    box-shadow:0 0 8px var(--accent);
    margin-top:-7px;
    transition:transform .15s;
  }
  input[type=range]:active::-webkit-slider-thumb{transform:scale(1.25)}

  /* ── dpad ── */
  .dpad{
    display:grid;
    grid-template-columns:repeat(3,76px);
    grid-template-rows:repeat(3,70px);
    gap:10px;
  }
  .btn{
    background:var(--panel);
    border:1px solid var(--border);
    border-radius:14px;
    color:var(--text);
    font-size:.6rem;letter-spacing:1.5px;text-transform:uppercase;
    display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;
    cursor:pointer;
    transition:background .1s,border-color .1s,transform .08s;
    user-select:none;
  }
  .btn .ico{font-size:1.5rem;line-height:1}
  .btn:active,.btn.active{
    background:rgba(0,200,255,.12);
    border-color:var(--accent);
    color:var(--accent);
    transform:scale(.94);
  }
  .btn.stop-btn{border-color:rgba(255,64,96,.35);color:var(--red)}
  .btn.stop-btn:active{background:rgba(255,64,96,.12);border-color:var(--red)}
  .empty{pointer-events:none}

  /* ── footer ── */
  footer{font-size:.6rem;color:#2a2a3a;letter-spacing:2px}
</style>
</head>
<body>

<header>
  <h1>6WD Robot</h1>
  <p>ESP32 · BTS7960 · WiFi</p>
</header>

<div class="statusbar">
  <div class="dot on" id="dot"></div>
  <span>STATUS</span>
  <span id="cmdlabel">READY</span>
</div>

<!-- Speed -->
<div class="speed-section">
  <div class="speed-header">
    <span>Speed</span>
    <span id="speedDisplay">180</span>
  </div>
  <input type="range" id="slider" min="60" max="255" value="180"
         style="--pct:47"
         oninput="onSpeed(this)">
</div>

<!-- D-Pad -->
<div class="dpad">
  <div class="empty"></div>
  <button class="btn" id="bF"
    onpointerdown="press('F',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">▲</span>FORWARD
  </button>
  <div class="empty"></div>

  <button class="btn" id="bL"
    onpointerdown="press('L',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">◀</span>LEFT
  </button>
  <button class="btn stop-btn" id="bS"
    onpointerdown="cmd('S')" ontouchstart="cmd('S')">
    <span class="ico">■</span>STOP
  </button>
  <button class="btn" id="bR"
    onpointerdown="press('R',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">▶</span>RIGHT
  </button>

  <div class="empty"></div>
  <button class="btn" id="bB"
    onpointerdown="press('B',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">▼</span>BACK
  </button>
  <div class="empty"></div>
</div>

<footer>6WD · LEFT 3 MOTORS — RIGHT 3 MOTORS</footer>

<script>
let activeBtn = null;
let busy = false;

function cmd(c) {
  fetch('/'+c).catch(()=>{});
  document.getElementById('cmdlabel').textContent =
    {F:'FORWARD',B:'BACKWARD',L:'LEFT',R:'RIGHT',S:'STOP'}[c] || c;
}

function press(c, btn) {
  if (activeBtn) activeBtn.classList.remove('active');
  activeBtn = btn;
  btn.classList.add('active');
  cmd(c);
}

function release() {
  if (activeBtn) { activeBtn.classList.remove('active'); activeBtn = null; }
  cmd('S');
}

function onSpeed(el) {
  const v = parseInt(el.value);
  const pct = Math.round((v - 60) / (255 - 60) * 100);
  el.style.setProperty('--pct', pct);
  document.getElementById('speedDisplay').textContent = v;
  if (!busy) {
    busy = true;
    fetch('/SPD?v=' + v).finally(() => busy = false);
  }
}
</script>
</body>
</html>
)HTML";
  server.send(200, "text/html", html);
}

// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);

  // Enable pins
  pinMode(L_REN, OUTPUT); pinMode(L_LEN, OUTPUT);
  pinMode(R_REN, OUTPUT); pinMode(R_LEN, OUTPUT);
  disableDrivers();

  // PWM
  ledcSetup(CH_L_FWD, PWM_FREQ, PWM_RES); ledcAttachPin(L_RPWM, CH_L_FWD);
  ledcSetup(CH_L_REV, PWM_FREQ, PWM_RES); ledcAttachPin(L_LPWM, CH_L_REV);
  ledcSetup(CH_R_FWD, PWM_FREQ, PWM_RES); ledcAttachPin(R_RPWM, CH_R_FWD);
  ledcSetup(CH_R_REV, PWM_FREQ, PWM_RES); ledcAttachPin(R_LPWM, CH_R_REV);
  stopAll();

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  int t = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print('.');
    if (++t > 40) { Serial.println("\nWiFi failed!"); return; }
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  // Routes
  server.on("/",   handleRoot);
  server.on("/F",  [](){ forward();   server.send(200,"text/plain","F"); });
  server.on("/B",  [](){ backward();  server.send(200,"text/plain","B"); });
  server.on("/L",  [](){ turnLeft();  server.send(200,"text/plain","L"); });
  server.on("/R",  [](){ turnRight(); server.send(200,"text/plain","R"); });
  server.on("/S",  [](){ stopCar();   server.send(200,"text/plain","S"); });
  server.on("/SPD",[](){
    if (server.hasArg("v"))
      speedVal = constrain(server.arg("v").toInt(), 0, 255);
    server.send(200,"text/plain", String(speedVal));
  });

  server.begin();
}

// =====================================================================
//  LOOP
// =====================================================================
void loop() {
  server.handleClient();
}
