/*
 * Super Leo World HD: Dino Island
 * True 1080p (1920x1080) 60fps Original Mario-Style Platformer with 4 Rideable Dinos
 */
(function () {
  "use strict";

  const canvas = document.getElementById("game-canvas");
  const ctx = canvas.getContext("2d");
  const W = 1920;
  const H = 1080;
  const TILE = 64;
  const GRAVITY = 0.68;

  // =========================================================================
  // 1. WEBAUDIO POLYPHONIC SYNTH & SOUNDTRACK ENGINE
  // =========================================================================
  let audioCtx = null;
  let sfxMuted = false;
  let musicEnabled = true;
  let musicTimer = 0;
  let musicStep = 0;

  function ensureAudio() {
    if (!audioCtx) {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (AC) audioCtx = new AC();
    }
    if (audioCtx && audioCtx.state === "suspended") {
      audioCtx.resume();
    }
  }

  function playNotes(notes, stepDur, wave = "triangle", vol = 0.09) {
    if (sfxMuted || !audioCtx) return;
    const now = audioCtx.currentTime;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = wave;
    notes.forEach((f, i) => {
      osc.frequency.setValueAtTime(Math.max(40, f), now + i * stepDur);
    });
    gain.gain.setValueAtTime(vol, now);
    gain.gain.exponentialRampToValueAtTime(0.001, now + notes.length * stepDur + 0.02);
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start(now);
    osc.stop(now + notes.length * stepDur + 0.03);
  }

  function playSweep(f0, f1, dur, wave = "sine", vol = 0.1) {
    if (sfxMuted || !audioCtx) return;
    const now = audioCtx.currentTime;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = wave;
    osc.frequency.setValueAtTime(f0, now);
    osc.frequency.exponentialRampToValueAtTime(Math.max(30, f1), now + dur);
    gain.gain.setValueAtTime(vol, now);
    gain.gain.exponentialRampToValueAtTime(0.001, now + dur);
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start(now);
    osc.stop(now + dur + 0.01);
  }

  function sfx(name) {
    ensureAudio();
    if (sfxMuted || !audioCtx) return;
    switch (name) {
      case "jump":
        playSweep(240, 680, 0.15, "sine", 0.11);
        break;
      case "flutter":
        playNotes([320, 420, 580, 760], 0.045, "triangle", 0.1);
        break;
      case "tongue":
        playSweep(300, 920, 0.11, "sawtooth", 0.08);
        break;
      case "swallow":
        playNotes([260, 390, 520, 780], 0.04, "sine", 0.11);
        break;
      case "spit":
        playSweep(820, 180, 0.14, "sawtooth", 0.12);
        break;
      case "hatch":
        playNotes([523.25, 659.25, 783.99, 1046.5, 987.77, 1174.66], 0.055, "triangle", 0.12);
        break;
      case "quake":
        playSweep(160, 38, 0.32, "sawtooth", 0.16);
        break;
      case "coin":
        playNotes([987.77, 1318.51, 1318.51], 0.055, "sine", 0.09);
        break;
      case "stomp":
        playSweep(480, 160, 0.11, "triangle", 0.12);
        break;
      case "powerup":
        playNotes([330, 392, 659, 523, 587, 784, 659, 880], 0.045, "triangle", 0.1);
        break;
      case "brick":
        playSweep(210, 55, 0.15, "sawtooth", 0.12);
        break;
      case "bail":
        playNotes([440, 660, 880, 1174], 0.04, "sine", 0.11);
        break;
      case "hurt":
        playSweep(580, 170, 0.24, "sawtooth", 0.11);
        break;
      case "clear":
        playNotes([261.6, 329.6, 392.0, 523.2, 659.2, 784.0, 1046.5], 0.085, "triangle", 0.12);
        break;
      case "checkpoint":
        playNotes([523.25, 659.25, 783.99, 1046.5], 0.07, "sine", 0.1);
        break;
    }
  }

  // Upbeat Tropical Dino Island background melody
  const MELODY_OVERWORLD = [
    523.25, 659.25, 783.99, 659.25, 880.0, 783.99, 659.25, 523.25,
    587.33, 698.46, 880.0, 698.46, 783.99, 659.25, 587.33, 493.88,
    523.25, 659.25, 783.99, 1046.5, 987.77, 880.0, 783.99, 659.25,
    587.33, 659.25, 698.46, 783.99, 880.0, 987.77, 1046.5, 0
  ];
  const MELODY_CAVE = [
    329.63, 392.0, 493.88, 440.0, 392.0, 329.63, 293.66, 329.63,
    349.23, 440.0, 523.25, 493.88, 440.0, 392.0, 329.63, 0
  ];

  function updateMusic() {
    if (!musicEnabled || sfxMuted || !audioCtx || audioCtx.state !== "running") return;
    if (gameState !== "playing") return;
    musicTimer++;
    if (musicTimer >= 13) {
      musicTimer = 0;
      const seq = (currentStage % 2 === 1) ? MELODY_CAVE : MELODY_OVERWORLD;
      const note = seq[musicStep % seq.length];
      musicStep++;
      if (note > 0) {
        playNotes([note], 0.12, "sine", 0.028);
      }
    }
  }

  // =========================================================================
  // 2. DINO TYPES & STAGE DEFINITIONS (8 HD STAGES ACROSS 4 WORLDS)
  // =========================================================================
  const DINO_TYPES = {
    green: {
      id: "green",
      name: "Emerald Rex",
      body: "#10b981",
      dark: "#047857",
      belly: "#d1fae5",
      crest: "#ef4444",
      saddle: "#dc2626",
      abilityText: "Tongue-Grab & Flutter-Jump!"
    },
    red: {
      id: "red",
      name: "Blaze Rex",
      body: "#ef4444",
      dark: "#b91c1c",
      belly: "#fef08a",
      crest: "#f97316",
      saddle: "#fbbf24",
      abilityText: "Spits 3x Giant Fireballs!"
    },
    blue: {
      id: "blue",
      name: "Sky Rex",
      body: "#3b82f6",
      dark: "#1d4ed8",
      belly: "#e0f2fe",
      crest: "#a855f7",
      saddle: "#f43f5e",
      abilityText: "Winged Multi-Flap Flight!"
    },
    yellow: {
      id: "yellow",
      name: "Quake Rex",
      body: "#f59e0b",
      dark: "#b45309",
      belly: "#fef9c3",
      crest: "#10b981",
      saddle: "#2563eb",
      abilityText: "Seismic Shockwave Stomp!"
    }
  };

  const STAGES = [
    {
      code: "1-1",
      title: "Emerald Dino Lagoon",
      theme: "tropics",
      skyTop: "#0ea5e9",
      skyBot: "#bae6fd",
      hillFar: "#0d9488",
      hillNear: "#10b981",
      groundTop: "#22c55e",
      groundBody: "#854d0e",
      defaultDino: "green",
      secondDino: "red",
      seed: 101
    },
    {
      code: "1-2",
      title: "Amber Crystal Cavern",
      theme: "cave",
      skyTop: "#0f172a",
      skyBot: "#1e1b4b",
      hillFar: "#312e81",
      hillNear: "#4338ca",
      groundTop: "#f59e0b",
      groundBody: "#334155",
      defaultDino: "yellow",
      secondDino: "green",
      seed: 102
    },
    {
      code: "2-1",
      title: "Sunburst Oasis & Dunes",
      theme: "desert",
      skyTop: "#f97316",
      skyBot: "#fef08a",
      hillFar: "#d97706",
      hillNear: "#f59e0b",
      groundTop: "#fde047",
      groundBody: "#9a3412",
      defaultDino: "red",
      secondDino: "yellow",
      seed: 201
    },
    {
      code: "2-2",
      title: "Tricera Magma Keep",
      theme: "castle",
      skyTop: "#18181b",
      skyBot: "#450a0a",
      hillFar: "#7f1d1d",
      hillNear: "#991b1b",
      groundTop: "#94a3b8",
      groundBody: "#3f3f46",
      defaultDino: "red",
      secondDino: "blue",
      hasBoss: true,
      bossName: "TRICERA-KING",
      bossHp: 6,
      seed: 202
    },
    {
      code: "3-1",
      title: "Skyward Cloud Archipelago",
      theme: "sky",
      skyTop: "#2563eb",
      skyBot: "#e0f2fe",
      hillFar: "#60a5fa",
      hillNear: "#93c5fd",
      groundTop: "#f43f5e",
      groundBody: "#f8fafc",
      defaultDino: "blue",
      secondDino: "green",
      seed: 301
    },
    {
      code: "3-2",
      title: "Aurora Glacier Peaks",
      theme: "ice",
      skyTop: "#0f172a",
      skyBot: "#06b6d4",
      hillFar: "#0284c7",
      hillNear: "#38bdf8",
      groundTop: "#cffafe",
      groundBody: "#1e3a8a",
      defaultDino: "red",
      secondDino: "blue",
      seed: 302
    },
    {
      code: "4-1",
      title: "Starlight Neon Canyon",
      theme: "night",
      skyTop: "#090d16",
      skyBot: "#3b0764",
      hillFar: "#581c87",
      hillNear: "#7e22ce",
      groundTop: "#c084fc",
      groundBody: "#1e1b4b",
      defaultDino: "yellow",
      secondDino: "blue",
      seed: 401
    },
    {
      code: "4-2",
      title: "King Tyrannor's Citadel",
      theme: "castle",
      skyTop: "#09090b",
      skyBot: "#7f1d1d",
      hillFar: "#450a0a",
      hillNear: "#991b1b",
      groundTop: "#f87171",
      groundBody: "#27272a",
      defaultDino: "red",
      secondDino: "yellow",
      hasBoss: true,
      bossName: "KING TYRANNOR",
      bossHp: 10,
      seed: 402
    }
  ];

  // =========================================================================
  // 3. PROCEDURAL HD STAGE BUILDER (180x17 TILES PER STAGE)
  // =========================================================================
  const MAP_W = 185;
  const MAP_H = 17; // 17 * 64 = 1088px
  let map = [];
  let bumpOffsets = [];

  // Tile legend:
  // '.' = empty, '#' = ground, 'B' = brick, '?' = coin block, 'M' = powerup block
  // 'E' = Primary Dino Egg block, 'D' = Secondary Dino Egg block
  // 'F' = Berry bush (edible by Dino tongue!), 'C' = Coin, 'J' = Springboard
  // 'P' = Pipe top, 'p' = Pipe body, 'I' = Slippery Ice, 'K' = Checkpoint, 'G' = Goal Flagpole

  function createEmptyMap() {
    map = [];
    bumpOffsets = [];
    for (let y = 0; y < MAP_H; y++) {
      map.push(new Array(MAP_W).fill("."));
      bumpOffsets.push(new Array(MAP_W).fill(0));
    }
  }

  function setTile(tx, ty, ch) {
    if (tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H) {
      map[ty][tx] = ch;
    }
  }

  function getTile(tx, ty) {
    if (tx < 0 || tx >= MAP_W) return "#";
    if (ty < 0) return ".";
    if (ty >= MAP_H) return ".";
    return map[ty][tx];
  }

  function isSolidTile(ch) {
    return (
      ch === "#" ||
      ch === "B" ||
      ch === "?" ||
      ch === "M" ||
      ch === "E" ||
      ch === "D" ||
      ch === "U" ||
      ch === "P" ||
      ch === "p" ||
      ch === "I"
    );
  }

  // =========================================================================
  // 4. GAME STATE, PLAYER, DINO, ENTITIES, PARTICLES, SHOCKWAVES
  // =========================================================================
  let gameState = "title"; // "title", "playing", "stageclear", "victory"
  let currentStage = 0;
  let globalTick = 0;
  let cameraX = 0;
  let score = 0;
  let coins = 0;
  let lives = 5;
  let berriesEaten = 0;
  let checkpointX = 0;
  let bannerText = "";
  let bannerTimer = 0;
  let stateTimer = 0;

  const keys = {
    left: false,
    right: false,
    up: false,
    down: false,
    jump: false,
    tongue: false,
    bail: false,
    start: false
  };
  const prevKeys = { ...keys };

  const player = {
    x: 160,
    y: 640,
    vx: 0,
    vy: 0,
    w: 46,
    h: 60,
    dir: 1,
    power: 1, // 0=Small, 1=Super, 2=Fire
    invuln: 0,
    starTimer: 0,
    onGround: false,
    coyote: 0,
    wallSlide: 0, // -1 left wall, +1 right wall
    groundPounding: false,
    gpTimer: 0,
    animFrame: 0,
    squashX: 1,
    squashY: 1,
    scarfWave: 0,
    // Rideable Dino state!
    dino: null, // null or "green" | "red" | "blue" | "yellow"
    dinoMouth: null, // null or { type: "shell"|"fire" }
    tongueState: 0, // 0=idle, 1=extending, 2=retracting
    tongueLen: 0,
    tongueMax: 245,
    tongueTarget: null,
    flutterCooldown: 0,
    wingFlaps: 0,
    mouthHoldTimer: 0
  };

  let entities = [];
  let fireballs = [];
  let particles = [];
  let shockwaves = [];

  function showBanner(msg, dur = 135) {
    bannerText = msg;
    bannerTimer = dur;
  }

  function spawnParticle(x, y, vx, vy, color, size = 8, life = 35, text = null) {
    particles.push({ x, y, vx, vy, color, size, life, maxLife: life, text });
  }

  function spawnShockwave(x, y, color = "#fbbf24") {
    shockwaves.push({ x, y, r: 16, maxR: 290, color, alpha: 1 });
    sfx("quake");
    // Flip and launch all nearby ground enemies & shatter low bricks!
    entities.forEach((e) => {
      if (!e.active) return;
      const dist = Math.hypot((e.x + e.w / 2) - x, (e.y + e.h / 2) - y);
      if (dist < 300 && (e.type === "walker" || e.type === "koopa" || e.type === "shell" || e.type === "pokey")) {
        e.vy = -13;
        e.vx = ((e.x < x) ? -1 : 1) * 5;
        if (e.type === "walker" || e.type === "pokey") {
          e.active = false;
          score += 200;
          spawnParticle(e.x, e.y, 0, -3, "#fde047", 10, 40, "+200");
        } else if (e.type === "koopa") {
          e.type = "shell";
          e.vx = 0;
        }
      }
    });
  }

  // =========================================================================
  // 5. STAGE BUILDER & ENTITY SPAWNER
  // =========================================================================
  function loadStage(idx, fromCheckpoint = false) {
    currentStage = ((idx % STAGES.length) + STAGES.length) % STAGES.length;
    const st = STAGES[currentStage];
    const sel = document.getElementById("stage-select");
    if (sel && parseInt(sel.value, 10) !== currentStage) {
      sel.value = String(currentStage);
    }

    createEmptyMap();
    entities = [];
    fireballs = [];
    particles = [];
    shockwaves = [];

    const groundTile = st.theme === "ice" ? "I" : "#";

    // Base ground floor (rows 14, 15, 16)
    for (let x = 0; x < MAP_W; x++) {
      // Stage-specific scenic pits
      const isPit =
        (x >= 36 && x <= 38) ||
        (x >= 68 && x <= 71 && st.theme !== "castle") ||
        (x >= 104 && x <= 107) ||
        (x >= 138 && x <= 141 && !st.hasBoss);
      if (!isPit) {
        for (let y = 14; y < MAP_H; y++) {
          setTile(x, y, groundTile);
        }
      }
    }

    // Immediate Starter Dino Egg Block right near spawn so the player gets a Dino within 3 seconds!
    setTile(7, 10, "?");
    setTile(8, 10, "E"); // Hatches stage's signature Rideable Dino!
    setTile(9, 10, "B");
    setTile(10, 10, "?");
    // Starter Berry Bush & Coins to test Dino tongue right away!
    setTile(13, 13, "F");
    setTile(14, 12, "C");
    setTile(15, 12, "C");
    setTile(16, 13, "F");

    // Spawn a friendly waiting Dino right at x=11 on Stage 1-1 so kids can hop on immediately!
    if (currentStage === 0 && !fromCheckpoint && !player.dino) {
      entities.push({
        active: true,
        type: "dino_mount",
        dinoType: st.defaultDino,
        x: 11 * TILE,
        y: 12 * TILE,
        vx: 0,
        vy: 0,
        w: 62,
        h: 64,
        dir: 1,
        timer: 0
      });
    }

    // Build rich platform sections across x = 18 .. 160
    for (let sec = 0; sec < 7; sec++) {
      const bx = 20 + sec * 20;

      // Elevated ledges & question/dino blocks
      if (sec % 2 === 0) {
        for (let dx = 0; dx < 5; dx++) {
          setTile(bx + dx, 10, dx === 2 ? (sec === 2 ? "D" : "M") : dx % 2 === 0 ? "B" : "?");
          setTile(bx + dx, 6, "C");
        }
        // High platform above
        for (let dx = 1; dx <= 3; dx++) {
          setTile(bx + dx, 6, dx === 2 ? "E" : "B");
        }
      } else {
        // Staircase / Wall-Jump Chimney + Berry Garden
        setTile(bx, 13, "#");
        setTile(bx + 1, 13, "#");
        setTile(bx + 1, 12, "#");
        setTile(bx + 2, 13, "P");
        setTile(bx + 2, 12, "P");
        setTile(bx + 5, 13, "F");
        setTile(bx + 8, 13, "F");
        setTile(bx + 6, 9, "?");
        setTile(bx + 7, 9, "D"); // Secondary Dino Egg block!
        setTile(bx + 8, 9, "?");
      }

      // Springboard before wide gaps
      if (bx === 60 || bx === 100 || bx === 120) {
        setTile(bx + 3, 13, "J");
      }

      // Moving platforms over gaps
      if (sec === 1 || sec === 4) {
        entities.push({
          active: true,
          type: "platform",
          x: (bx + 15) * TILE,
          y: 10 * TILE,
          startX: (bx + 15) * TILE,
          startY: 10 * TILE,
          vx: 2.2,
          vy: 0,
          w: 160,
          h: 26,
          dir: 1,
          timer: 0
        });
      }

      // Enemies per section
      entities.push({
        active: true,
        type: sec % 2 === 0 ? "koopa" : "walker",
        x: (bx + 4) * TILE,
        y: 12 * TILE,
        vx: -1.7,
        vy: 0,
        w: 50,
        h: 54,
        dir: -1,
        timer: 0
      });

      entities.push({
        active: true,
        type: "flyer",
        x: (bx + 11) * TILE,
        y: 8 * TILE,
        startY: 8 * TILE,
        vx: -1.4,
        vy: 0,
        w: 54,
        h: 54,
        dir: -1,
        timer: sec * 25
      });

      if (st.theme === "desert" || st.theme === "night") {
        entities.push({
          active: true,
          type: "pokey",
          x: (bx + 9) * TILE,
          y: 11 * TILE,
          vx: -1.0,
          vy: 0,
          w: 48,
          h: 86,
          dir: -1,
          timer: 0
        });
      }
    }

    // Mid-stage Checkpoint Flag at x = 88
    setTile(88, 13, "K");

    // Boss Arena on 2-2 and 4-2
    if (st.hasBoss) {
      for (let x = 148; x < 170; x++) {
        setTile(x, 14, "#");
        setTile(x, 9, x % 5 === 0 ? "E" : ".");
      }
      entities.push({
        active: true,
        type: "boss",
        name: st.bossName,
        hp: st.bossHp,
        maxHp: st.bossHp,
        x: 162 * TILE,
        y: 10 * TILE,
        vx: -2.2,
        vy: 0,
        w: 118,
        h: 128,
        dir: -1,
        timer: 0,
        invuln: 0
      });
    }

    // Goal Flagpole at x = 174
    for (let y = 4; y <= 13; y++) {
      setTile(174, y, "G");
    }
    setTile(174, 13, "#");

    // Position Leo
    const spawnX = fromCheckpoint && checkpointX > 0 ? checkpointX : 3 * TILE;
    player.x = spawnX;
    player.y = 11 * TILE;
    player.vx = 0;
    player.vy = 0;
    player.groundPounding = false;
    player.tongueState = 0;
    player.tongueLen = 0;
    cameraX = Math.max(0, player.x - W * 0.28);

    showBanner(`STAGE ${st.code}: ${st.title.toUpperCase()}`, 150);
  }

  // =========================================================================
  // 6. COLLISION & BLOCK INTERACTION
  // =========================================================================
  function triggerBlockHit(tx, ty) {
    const ch = getTile(tx, ty);
    if (!isSolidTile(ch)) return;
    bumpOffsets[ty][tx] = 14;
    const wx = tx * TILE + TILE / 2;
    const wy = ty * TILE;

    if (ch === "?") {
      setTile(tx, ty, "U");
      coins++;
      score += 100;
      sfx("coin");
      spawnParticle(wx, wy - 16, 0, -4, "#fde047", 12, 35, "+100");
    } else if (ch === "M") {
      setTile(tx, ty, "U");
      sfx("powerup");
      entities.push({
        active: true,
        type: player.power >= 1 ? "flower" : "mushroom",
        x: tx * TILE + 8,
        y: (ty - 1) * TILE,
        vx: 2.2,
        vy: -5,
        w: 48,
        h: 48,
        dir: 1,
        timer: 0
      });
    } else if (ch === "E" || ch === "D") {
      setTile(tx, ty, "U");
      const st = STAGES[currentStage];
      const dType = ch === "E" ? st.defaultDino : st.secondDino;
      sfx("hatch");
      // Spawn a Spotted Dino Egg that pops up and hatches into a Rideable Dino!
      entities.push({
        active: true,
        type: "dino_mount",
        dinoType: dType,
        x: tx * TILE,
        y: (ty - 1) * TILE - 6,
        vx: 0,
        vy: -6,
        w: 62,
        h: 64,
        dir: 1,
        timer: 0
      });
      for (let i = 0; i < 10; i++) {
        spawnParticle(wx, wy, (Math.random() - 0.5) * 8, -Math.random() * 7 - 2, "#f8fafc", 9, 32);
      }
      showBanner(`${DINO_TYPES[dType].name.toUpperCase()} HATCHED! ${DINO_TYPES[dType].abilityText}`, 140);
    } else if (ch === "B") {
      if (player.power > 0 || player.dino || player.groundPounding) {
        setTile(tx, ty, ".");
        sfx("brick");
        score += 50;
        for (let i = 0; i < 8; i++) {
          spawnParticle(
            wx,
            wy + 24,
            (Math.random() - 0.5) * 11,
            -Math.random() * 9 - 2,
            "#d97706",
            12,
            38
          );
        }
      } else {
        sfx("stomp");
      }
    }
  }

  function aabb(ax, ay, aw, ah, bx, by, bw, bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
  }

  // =========================================================================
  // 7. DINO TONGUE, SPIT, & BAIL-OUT MECHANICS
  // =========================================================================
  function useDinoTongueOrFire() {
    if (player.dino) {
      // If Dino's mouth is holding a shell/enemy, SPIT IT OUT!
      if (player.dinoMouth) {
        sfx("spit");
        const isFireBurst = player.dino === "red" || player.dinoMouth.type === "fire";
        const sx = player.dir > 0 ? player.x + player.w + 18 : player.x - 32;
        const sy = player.y + 24;

        if (isFireBurst) {
          // 3-Way Spread of Giant HD Fireballs!
          [-3.2, 0, 3.2].forEach((vyVal) => {
            fireballs.push({
              x: sx,
              y: sy,
              vx: player.dir * 13.5,
              vy: vyVal,
              r: 18,
              fromDino: true,
              life: 110
            });
          });
        } else {
          // High-speed Turbo Shell!
          entities.push({
            active: true,
            type: "shell",
            x: sx,
            y: player.y + 10,
            vx: player.dir * 14,
            vy: 0,
            w: 50,
            h: 48,
            dir: player.dir,
            sliding: true,
            timer: 0
          });
        }
        player.dinoMouth = null;
        player.mouthHoldTimer = 0;
        return;
      }

      // Otherwise shoot out the Chameleon Tongue!
      if (player.tongueState === 0) {
        player.tongueState = 1;
        player.tongueLen = 20;
        player.tongueTarget = null;
        sfx("tongue");
      }
    } else if (player.power === 2 && fireballs.length < 4) {
      // Fire Leo fireball
      sfx("spit");
      fireballs.push({
        x: player.dir > 0 ? player.x + player.w + 6 : player.x - 18,
        y: player.y + 22,
        vx: player.dir * 12,
        vy: 3.5,
        r: 12,
        fromDino: false,
        life: 95
      });
    }
  }

  function performDinoBailOut() {
    if (!player.dino) return;
    const oldDino = player.dino;
    player.dino = null;
    player.dinoMouth = null;
    player.tongueState = 0;
    player.vy = -17.5; // Massive double-height bail-out jump!
    player.squashX = 0.72;
    player.squashY = 1.35;
    sfx("bail");

    // Spawn the dismounted Dino right below Leo so it can even be re-mounted!
    entities.push({
      active: true,
      type: "dino_mount",
      dinoType: oldDino,
      x: player.x - player.dir * 20,
      y: player.y + 24,
      vx: -player.dir * 2.5,
      vy: 2,
      w: 62,
      h: 64,
      dir: player.dir,
      timer: 22 // brief re-mount delay
    });
  }

  // =========================================================================
  // 8. MAIN PHYSICS & GAMEPLAY UPDATE STEP (60 FPS)
  // =========================================================================
  function hurtPlayer() {
    if (player.invuln > 0 || player.starTimer > 0) return;
    if (player.dino) {
      // Dino absorbs the hit and hops off safely!
      performDinoBailOut();
      player.vy = -12;
      player.invuln = 90;
      showBanner("DINO PROTECTED YOU! HOP BACK ON!", 95);
      return;
    }
    if (player.power > 0) {
      player.power--;
      player.invuln = 90;
      sfx("hurt");
      return;
    }
    // Lose a life and respawn at checkpoint
    sfx("hurt");
    lives = Math.max(1, lives - 1);
    player.power = 1;
    loadStage(currentStage, true);
  }

  function updateGame() {
    globalTick++;
    updateMusic();
    if (bannerTimer > 0) bannerTimer--;

    // Decay block bump animations
    for (let y = 0; y < MAP_H; y++) {
      for (let x = 0; x < MAP_W; x++) {
        if (bumpOffsets[y][x] > 0) bumpOffsets[y][x] = Math.max(0, bumpOffsets[y][x] - 2);
      }
    }

    if (gameState === "stageclear") {
      stateTimer--;
      if (stateTimer <= 0) {
        checkpointX = 0;
        if (currentStage + 1 < STAGES.length) {
          gameState = "playing";
          loadStage(currentStage + 1, false);
        } else {
          gameState = "victory";
        }
      }
      return;
    }

    if (gameState === "title" || gameState === "victory") {
      if (keys.start && !prevKeys.start) {
        gameState = "playing";
        if (gameState === "victory") currentStage = 0;
        loadStage(currentStage, false);
      }
      // Demo auto-run on title screen
      player.animFrame += 0.18;
      player.scarfWave += 0.22;
      return;
    }

    // --- Playing State ---
    if (player.invuln > 0) player.invuln--;
    if (player.starTimer > 0) player.starTimer--;
    if (player.flutterCooldown > 0) player.flutterCooldown--;

    player.h = player.dino ? 84 : player.power > 0 ? 64 : 48;

    // Tongue / Fireball trigger
    if (keys.tongue && !prevKeys.tongue) {
      useDinoTongueOrFire();
    }
    // Auto-spit after holding a shell for a moment if player holds action on mobile
    if (player.dino && player.dinoMouth) {
      player.mouthHoldTimer++;
    }

    // Bail-out dismount trigger (E / X / Bail button)
    if (keys.bail && !prevKeys.bail && player.dino) {
      performDinoBailOut();
    }

    // Update Dino Tongue extension/retraction
    if (player.tongueState > 0 && player.dino) {
      if (player.tongueState === 1) {
        player.tongueLen += 26;
        if (player.tongueLen >= player.tongueMax) {
          player.tongueState = 2;
        }
      } else {
        player.tongueLen -= 24;
        if (player.tongueLen <= 0) {
          player.tongueLen = 0;
          player.tongueState = 0;
          if (player.tongueTarget) {
            sfx("swallow");
            if (player.tongueTarget.kind === "berry" || player.tongueTarget.kind === "coin") {
              coins += 3;
              score += 500;
              berriesEaten++;
              spawnParticle(player.x, player.y - 10, 0, -3, "#fbbf24", 12, 40, "YUM! +500");
              if (berriesEaten % 3 === 0) {
                player.starTimer = 360;
                sfx("powerup");
                showBanner("3 BERRIES EATEN — STAR POWER!", 110);
              }
            } else {
              player.dinoMouth = {
                type: player.tongueTarget.isFire ? "fire" : "shell"
              };
              player.mouthHoldTimer = 0;
            }
            player.tongueTarget = null;
          }
        }
      }

      // Check tongue tip collision against berries, coins, and enemies
      const tipX =
        player.dir > 0
          ? player.x + player.w + player.tongueLen
          : player.x - player.tongueLen;
      const tipY = player.y + 28;
      const ttx = Math.floor(tipX / TILE);
      const tty = Math.floor(tipY / TILE);
      const tch = getTile(ttx, tty);
      if ((tch === "F" || tch === "C") && !player.tongueTarget) {
        setTile(ttx, tty, ".");
        player.tongueTarget = { kind: tch === "F" ? "berry" : "coin" };
        player.tongueState = 2;
      }

      if (!player.tongueTarget) {
        for (const e of entities) {
          if (!e.active || e.type === "platform" || e.type === "dino_mount" || e.type === "boss") continue;
          if (aabb(tipX - 24, tipY - 24, 48, 48, e.x, e.y, e.w, e.h)) {
            e.active = false;
            player.tongueTarget = {
              kind: "enemy",
              isFire: e.type === "flyer" || e.type === "pokey" || player.dino === "red"
            };
            player.tongueState = 2;
            break;
          }
        }
      }
    } else {
      player.tongueState = 0;
      player.tongueLen = 0;
    }

    // Ground-Pound initiation in mid-air
    if (!player.onGround && keys.down && !prevKeys.down && !player.groundPounding) {
      player.groundPounding = true;
      player.gpTimer = 10;
      player.vx = 0;
      player.vy = 0;
    }

    if (player.groundPounding) {
      if (player.gpTimer > 0) {
        player.gpTimer--;
        player.vy = 0;
      } else {
        player.vy = 20;
      }
    } else {
      // Horizontal movement
      const onIce = getTile(Math.floor((player.x + player.w / 2) / TILE), Math.floor((player.y + player.h + 2) / TILE)) === "I";
      const maxSpd = keys.tongue || player.dino ? 8.2 : 6.4;
      const accel = player.onGround ? (onIce ? 0.25 : 0.65) : 0.42;
      const fric = onIce ? 0.96 : 0.82;

      if (keys.left) {
        player.vx = Math.max(-maxSpd, player.vx - accel);
        player.dir = -1;
      } else if (keys.right) {
        player.vx = Math.min(maxSpd, player.vx + accel);
        player.dir = 1;
      } else {
        player.vx *= fric;
        if (Math.abs(player.vx) < 0.08) player.vx = 0;
      }

      // Gravity
      player.vy = Math.min(16, player.vy + GRAVITY);
    }

    // Coyote time & Jump / Wall-Jump / Flutter-Jump / Blue Dino Wing-Flap
    if (player.onGround) {
      player.coyote = 7;
      player.wingFlaps = 4;
    } else if (player.coyote > 0) {
      player.coyote--;
    }

    if (keys.jump && !prevKeys.jump) {
      if (player.coyote > 0) {
        player.vy = player.dino ? -15.2 : -14.4;
        player.onGround = false;
        player.coyote = 0;
        player.squashX = 0.78;
        player.squashY = 1.26;
        sfx("jump");
      } else if (player.wallSlide !== 0 && !player.dino) {
        // Wall-Jump!
        player.vy = -14.0;
        player.vx = -player.wallSlide * 8.5;
        player.dir = -player.wallSlide;
        player.wallSlide = 0;
        sfx("jump");
        for (let i = 0; i < 6; i++) {
          spawnParticle(player.x + player.w / 2, player.y + player.h / 2, (Math.random() - 0.5) * 5, -Math.random() * 4, "#f8fafc", 7, 22);
        }
      } else if (player.dino === "blue" && player.wingFlaps > 0) {
        // Blue Sky Rex Winged Multi-Flap Flight!
        player.wingFlaps--;
        player.vy = -13.2;
        sfx("flutter");
        for (let i = 0; i < 5; i++) {
          spawnParticle(player.x + player.w / 2, player.y + player.h, (Math.random() - 0.5) * 6, 3, "#93c5fd", 9, 25);
        }
      }
    }

    // Variable jump height release
    if (!keys.jump && player.vy < -5 && !player.groundPounding) {
      player.vy *= 0.65;
    }

    // Classic Rideable Dino Flutter-Jump Hover (Hold Jump while falling!)
    if (player.dino && keys.jump && player.vy > 1.5 && player.flutterCooldown === 0 && !player.groundPounding) {
      player.vy = -9.6;
      player.flutterCooldown = 26;
      sfx("flutter");
      spawnParticle(player.x + player.w / 2, player.y + player.h, 0, 2.5, "#fde047", 14, 20);
    }

    // Horizontal Tile Collision & Wall-Slide Detection
    player.x += player.vx;
    if (player.x < 0) {
      player.x = 0;
      player.vx = 0;
    }
    player.wallSlide = 0;

    const tx1 = Math.floor(player.x / TILE);
    const tx2 = Math.floor((player.x + player.w - 0.01) / TILE);
    const ty1 = Math.floor((player.y + 4) / TILE);
    const ty2 = Math.floor((player.y + player.h - 4) / TILE);

    for (let ty = ty1; ty <= ty2; ty++) {
      for (let tx = tx1; tx <= tx2; tx++) {
        const ch = getTile(tx, ty);
        if (isSolidTile(ch)) {
          if (player.vx > 0) {
            player.x = tx * TILE - player.w;
            player.vx = 0;
            if (!player.onGround && keys.right) player.wallSlide = 1;
          } else if (player.vx < 0) {
            player.x = (tx + 1) * TILE;
            player.vx = 0;
            if (!player.onGround && keys.left) player.wallSlide = -1;
          }
        }
      }
    }

    if (player.wallSlide !== 0 && player.vy > 2.6) {
      player.vy = 2.6; // slow wall-slide friction
    }

    // Vertical Tile Collision (with orig_vy snapshot so ? block hits never snap Leo onto roof!)
    const prevBottom = player.y + player.h;
    const origVy = player.vy;
    player.y += player.vy;
    player.onGround = false;

    const vtx1 = Math.floor((player.x + 4) / TILE);
    const vtx2 = Math.floor((player.x + player.w - 4) / TILE);
    const vty1 = Math.floor(player.y / TILE);
    const vty2 = Math.floor((player.y + player.h) / TILE);
    let resolvedY = false;

    for (let ty = vty1; ty <= vty2; ty++) {
      for (let tx = vtx1; tx <= vtx2; tx++) {
        const ch = getTile(tx, ty);
        if (ch === "C" || ch === "F") {
          setTile(tx, ty, ".");
          coins += ch === "F" ? 2 : 1;
          score += ch === "F" ? 300 : 100;
          sfx("coin");
          continue;
        }
        if (ch === "K") {
          setTile(tx, ty, ".");
          checkpointX = tx * TILE;
          sfx("checkpoint");
          showBanner("CHECKPOINT REACHED!", 100);
          continue;
        }
        if (ch === "G") {
          sfx("clear");
          score += 2000;
          gameState = "stageclear";
          stateTimer = 110;
          showBanner(`STAGE ${STAGES[currentStage].code} CLEAR!`, 110);
          return;
        }
        if (ch === "J" && origVy >= 0) {
          player.y = ty * TILE - player.h;
          player.vy = -19.5;
          player.groundPounding = false;
          sfx("jump");
          resolvedY = true;
          break;
        }
        if (!resolvedY && isSolidTile(ch)) {
          if (origVy < 0) {
            player.y = (ty + 1) * TILE;
            player.vy = 1.0;
            triggerBlockHit(tx, ty);
            resolvedY = true;
            break;
          } else if (origVy >= 0 && prevBottom <= ty * TILE + 24) {
            player.y = ty * TILE - player.h;
            player.vy = 0;
            player.onGround = true;

            if (player.groundPounding) {
              player.groundPounding = false;
              triggerBlockHit(tx, ty);
              if (player.dino === "yellow") {
                spawnShockwave(player.x + player.w / 2, player.y + player.h);
              } else {
                sfx("stomp");
              }
            } else if (player.dino === "yellow" && origVy > 6) {
              spawnShockwave(player.x + player.w / 2, player.y + player.h);
            }
            resolvedY = true;
            break;
          }
        }
      }
      if (resolvedY) break;
    }

    // Pit fall check
    if (player.y > H + 120) {
      if (player.dino) {
        // Emergency auto-bailout save if falling in a pit while riding Dino!
        performDinoBailOut();
        player.y = H - 140;
      } else {
        hurtPlayer();
      }
      return;
    }

    // Squash & stretch recovery
    player.squashX += (1 - player.squashX) * 0.22;
    player.squashY += (1 - player.squashY) * 0.22;
    player.animFrame += Math.abs(player.vx) * 0.06 + 0.04;
    player.scarfWave += 0.2;

    // Update Fireballs
    for (let i = fireballs.length - 1; i >= 0; i--) {
      const f = fireballs[i];
      f.x += f.vx;
      f.y += f.vy;
      if (!f.fromDino) {
        f.vy += 0.45;
        const ftx = Math.floor(f.x / TILE);
        const fty = Math.floor((f.y + f.r) / TILE);
        if (isSolidTile(getTile(ftx, fty))) {
          f.vy = -6.5;
        }
      }
      f.life--;
      if (f.life <= 0) {
        fireballs.splice(i, 1);
        continue;
      }
      for (const e of entities) {
        if (!e.active || e.type === "platform" || e.type === "dino_mount") continue;
        if (aabb(f.x - f.r, f.y - f.r, f.r * 2, f.r * 2, e.x, e.y, e.w, e.h)) {
          f.life = 0;
          if (e.type === "boss") {
            if (e.invuln <= 0) {
              e.hp--;
              e.invuln = 35;
              sfx("stomp");
              if (e.hp <= 0) {
                e.active = false;
                score += 5000;
                sfx("clear");
                showBanner(`${e.name} DEFEATED! TOUCH THE GOAL FLAG!`, 140);
              }
            }
          } else {
            e.active = false;
            score += 200;
            sfx("stomp");
            spawnParticle(e.x, e.y, 0, -3, "#fbbf24", 10, 35, "+200");
          }
          break;
        }
      }
    }

    // Update Entities
    for (const e of entities) {
      if (!e.active) continue;
      e.timer++;

      if (e.type === "platform") {
        e.x += e.vx;
        if (Math.abs(e.x - e.startX) > 190) e.vx = -e.vx;
        if (
          player.vy >= 0 &&
          player.x + player.w > e.x &&
          player.x < e.x + e.w &&
          player.y + player.h >= e.y - 10 &&
          player.y + player.h <= e.y + 22
        ) {
          player.y = e.y - player.h;
          player.vy = 0;
          player.onGround = true;
          player.x += e.vx;
        }
        continue;
      }

      if (e.type === "dino_mount") {
        e.vy = Math.min(12, e.vy + GRAVITY * 0.8);
        e.x += e.vx;
        e.y += e.vy;
        e.vx *= 0.92;
        const dty = Math.floor((e.y + e.h) / TILE);
        const dtx = Math.floor((e.x + e.w / 2) / TILE);
        if (isSolidTile(getTile(dtx, dty))) {
          e.y = dty * TILE - e.h;
          e.vy = 0;
        }
        if (e.timer > 18 && aabb(player.x, player.y, player.w, player.h, e.x, e.y, e.w, e.h)) {
          if (!player.dino) {
            player.dino = e.dinoType;
            e.active = false;
            sfx("hatch");
            const info = DINO_TYPES[e.dinoType];
            showBanner(`RIDING ${info.name.toUpperCase()} — ${info.abilityText}`, 130);
          } else {
            // Already riding a Dino -> grant bonus score + Starman!
            e.active = false;
            score += 1000;
            sfx("powerup");
            spawnParticle(e.x, e.y, 0, -3, "#fde047", 12, 40, "+1000");
          }
        }
        continue;
      }

      if (e.type === "flyer") {
        e.x += e.vx;
        e.y = e.startY + Math.sin(e.timer * 0.06) * 58;
        if (e.timer % 180 === 0) e.vx = -e.vx;
      } else if (e.type === "boss") {
        if (e.invuln > 0) e.invuln--;
        e.vy = Math.min(14, e.vy + GRAVITY);
        e.x += e.vx;
        e.y += e.vy;
        const bty = Math.floor((e.y + e.h) / TILE);
        const btx = Math.floor((e.x + e.w / 2) / TILE);
        if (isSolidTile(getTile(btx, bty))) {
          e.y = bty * TILE - e.h;
          e.vy = 0;
          if (e.timer % 95 === 0) e.vy = -14;
        }
        if (e.x < 149 * TILE) e.vx = Math.abs(e.vx);
        if (e.x > 168 * TILE) e.vx = -Math.abs(e.vx);
        // Boss breathes fire or throws shells that Dino can eat & spit back!
        if (e.timer % 110 === 0) {
          entities.push({
            active: true,
            type: "shell",
            x: e.x - 36,
            y: e.y + 50,
            vx: -6.5,
            vy: -4,
            w: 50,
            h: 48,
            dir: -1,
            sliding: true,
            timer: 0
          });
        }
      } else {
        // Ground enemy physics
        e.vy = Math.min(14, e.vy + GRAVITY);
        e.x += e.vx;
        const etx = Math.floor((e.vx > 0 ? e.x + e.w : e.x) / TILE);
        const ety = Math.floor((e.y + e.h / 2) / TILE);
        if (isSolidTile(getTile(etx, ety))) {
          e.vx = -e.vx;
          e.dir = -e.dir;
        }
        e.y += e.vy;
        const gty = Math.floor((e.y + e.h) / TILE);
        const gtx = Math.floor((e.x + e.w / 2) / TILE);
        if (isSolidTile(getTile(gtx, gty))) {
          e.y = gty * TILE - e.h;
          e.vy = 0;
        }
      }

      // Sliding shell bowling through other enemies
      if (e.type === "shell" && Math.abs(e.vx) > 4) {
        for (const other of entities) {
          if (!other.active || other === e || other.type === "platform" || other.type === "dino_mount") continue;
          if (aabb(e.x, e.y, e.w, e.h, other.x, other.y, other.w, other.h)) {
            if (other.type === "boss") {
              if (other.invuln <= 0) {
                other.hp -= 2;
                other.invuln = 40;
                e.active = false;
                sfx("quake");
                if (other.hp <= 0) {
                  other.active = false;
                  score += 5000;
                  sfx("clear");
                  showBanner(`${other.name} DEFEATED!`, 140);
                }
              }
            } else {
              other.active = false;
              score += 300;
              sfx("stomp");
              spawnParticle(other.x, other.y, 0, -3, "#fde047", 11, 35, "+300");
            }
          }
        }
      }

      // Player <-> Entity collision
      if (aabb(player.x, player.y, player.w, player.h, e.x, e.y, e.w, e.h)) {
        if (e.type === "mushroom" || e.type === "flower") {
          e.active = false;
          player.power = e.type === "flower" ? 2 : Math.max(1, player.power);
          score += 1000;
          sfx("powerup");
          continue;
        }
        if (player.starTimer > 0) {
          if (e.type !== "boss") {
            e.active = false;
            score += 200;
            sfx("stomp");
          }
          continue;
        }

        const isStomp = player.vy > 0 && player.y + player.h - player.vy <= e.y + 32;
        if (isStomp) {
          player.vy = -12.5;
          sfx("stomp");
          if (player.dino === "yellow") {
            spawnShockwave(player.x + player.w / 2, e.y + e.h);
          }
          if (e.type === "flyer") {
            e.type = "koopa";
            e.vx = -2;
          } else if (e.type === "koopa") {
            e.type = "shell";
            e.vx = 0;
            e.h = 46;
          } else if (e.type === "shell") {
            e.vx = Math.abs(e.vx) > 1 ? 0 : player.dir * 12;
          } else if (e.type === "boss") {
            if (e.invuln <= 0) {
              e.hp--;
              e.invuln = 40;
              if (e.hp <= 0) {
                e.active = false;
                score += 5000;
                sfx("clear");
                showBanner(`${e.name} DEFEATED!`, 140);
              }
            }
          } else {
            e.active = false;
            score += 200;
            spawnParticle(e.x, e.y, 0, -3, "#fde047", 10, 35, "+200");
          }
        } else if (e.type === "shell" && Math.abs(e.vx) < 1) {
          e.vx = player.dir * 12;
          e.x += player.dir * 18;
          sfx("stomp");
        } else {
          hurtPlayer();
        }
      }
    }

    // Update Shockwaves & Particles
    for (let i = shockwaves.length - 1; i >= 0; i--) {
      const sw = shockwaves[i];
      sw.r += 14;
      sw.alpha = Math.max(0, 1 - sw.r / sw.maxR);
      if (sw.r >= sw.maxR) shockwaves.splice(i, 1);
    }

    for (let i = particles.length - 1; i >= 0; i--) {
      const p = particles[i];
      p.x += p.vx;
      p.y += p.vy;
      if (!p.text) p.vy += 0.35;
      p.life--;
      if (p.life <= 0) particles.splice(i, 1);
    }

    // Smooth Deadzone Camera (no abrupt lurch when turning Left/Right!)
    const screenPlayerX = player.x - cameraX;
    const leftBound = W * 0.33;
    const rightBound = W * 0.47;
    let targetCamX = cameraX;
    if (screenPlayerX < leftBound) {
      targetCamX = player.x - leftBound;
    } else if (screenPlayerX > rightBound) {
      targetCamX = player.x - rightBound;
    }
    targetCamX = Math.max(0, Math.min(MAP_W * TILE - W, targetCamX));
    cameraX += (targetCamX - cameraX) * 0.22;
    const camX = Math.round(cameraX);
  }

  // =========================================================================
  // 9. 1080P HD VECTOR & SHADED RENDERING ENGINE
  // =========================================================================
  function drawRoundedRect(x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
  }

  function renderBackground(st, camX) {
    // 1. Multi-stop HD Sky Gradient
    const skyGrad = ctx.createLinearGradient(0, 0, 0, H);
    skyGrad.addColorStop(0, st.skyTop);
    skyGrad.addColorStop(1, st.skyBot);
    ctx.fillStyle = skyGrad;
    ctx.fillRect(0, 0, W, H);

    // 2. Glowing Sun / Moon / Stars
    ctx.save();
    if (st.theme === "cave" || st.theme === "night" || st.theme === "castle") {
      ctx.fillStyle = "rgba(255,255,255,0.75)";
      for (let i = 0; i < 45; i++) {
        const sx = ((i * 197 - camX * 0.05) % W + W) % W;
        const sy = (i * 89) % (H * 0.55);
        const sr = (i % 3) + 1.5;
        ctx.beginPath();
        ctx.arc(sx, sy, sr, 0, Math.PI * 2);
        ctx.fill();
      }
    } else {
      const sunGrad = ctx.createRadialGradient(W * 0.82, 170, 20, W * 0.82, 170, 170);
      sunGrad.addColorStop(0, "rgba(254, 240, 138, 0.95)");
      sunGrad.addColorStop(0.4, "rgba(253, 224, 71, 0.35)");
      sunGrad.addColorStop(1, "rgba(253, 224, 71, 0)");
      ctx.fillStyle = sunGrad;
      ctx.beginPath();
      ctx.arc(W * 0.82, 170, 170, 0, Math.PI * 2);
      ctx.fill();
    }

    // 3. Parallax Mountain Range (Far)
    ctx.fillStyle = st.hillFar;
    ctx.beginPath();
    ctx.moveTo(0, H);
    for (let x = -260; x <= W + 260; x += 260) {
      const wx = x - (((camX * 0.15) % 260 + 260) % 260);
      ctx.lineTo(wx, H - 360);
      ctx.quadraticCurveTo(wx + 130, H - 590, wx + 260, H - 360);
    }
    ctx.lineTo(W, H);
    ctx.fill();

    // 4. Parallax Rolling Hills (Near)
    ctx.fillStyle = st.hillNear;
    ctx.beginPath();
    ctx.moveTo(0, H);
    for (let x = -320; x <= W + 320; x += 320) {
      const wx = x - (((camX * 0.32) % 320 + 320) % 320);
      ctx.quadraticCurveTo(wx + 160, H - 420, wx + 320, H - 190);
    }
    ctx.lineTo(W, H);
    ctx.fill();

    // 5. Fluffy Bezier Clouds
    if (st.theme !== "cave") {
      ctx.fillStyle = "rgba(255, 255, 255, 0.82)";
      for (let i = 0; i < 7; i++) {
        const cx = (((i * 340 - camX * 0.22 - globalTick * 0.3) % (W + 300)) + (W + 300)) % (W + 300) - 150;
        const cy = 110 + (i % 3) * 85;
        ctx.beginPath();
        ctx.arc(cx, cy, 42, 0, Math.PI * 2);
        ctx.arc(cx + 38, cy - 14, 52, 0, Math.PI * 2);
        ctx.arc(cx + 82, cy, 40, 0, Math.PI * 2);
        ctx.fill();
      }
    }
    ctx.restore();
  }

  function renderTiles(st, camX) {
    const startCol = Math.max(0, Math.floor(camX / TILE) - 1);
    const endCol = Math.min(MAP_W - 1, Math.ceil((camX + W) / TILE) + 1);

    for (let ty = 0; ty < MAP_H; ty++) {
      for (let tx = startCol; tx <= endCol; tx++) {
        const ch = map[ty][tx];
        if (ch === ".") continue;
        const sx = tx * TILE - camX;
        const sy = Math.round(ty * TILE - bumpOffsets[ty][tx]);

        if (ch === "#" || ch === "I") {
          ctx.fillStyle = ch === "I" ? "#0284c7" : st.groundBody;
          drawRoundedRect(sx, sy, TILE, TILE, 8);
          ctx.fill();
          if (ty === 0 || !isSolidTile(getTile(tx, ty - 1))) {
            ctx.fillStyle = ch === "I" ? "#e0f2fe" : st.groundTop;
            drawRoundedRect(sx, sy, TILE, 18, 8);
            ctx.fill();
          }
        } else if (ch === "B") {
          const bGrad = ctx.createLinearGradient(sx, sy, sx, sy + TILE);
          bGrad.addColorStop(0, "#ea580c");
          bGrad.addColorStop(1, "#9a3412");
          ctx.fillStyle = bGrad;
          drawRoundedRect(sx + 2, sy + 2, TILE - 4, TILE - 4, 10);
          ctx.fill();
          ctx.strokeStyle = "#431407";
          ctx.lineWidth = 3;
          ctx.strokeRect(sx + 6, sy + 6, TILE - 12, TILE - 12);
        } else if (ch === "?" || ch === "M") {
          const qGrad = ctx.createLinearGradient(sx, sy, sx, sy + TILE);
          qGrad.addColorStop(0, "#fde047");
          qGrad.addColorStop(1, "#ca8a04");
          ctx.fillStyle = qGrad;
          drawRoundedRect(sx + 2, sy + 2, TILE - 4, TILE - 4, 12);
          ctx.fill();
          ctx.fillStyle = "#713f12";
          ctx.font = "900 34px sans-serif";
          ctx.textAlign = "center";
          ctx.fillText("?", sx + TILE / 2, sy + 44);
        } else if (ch === "E" || ch === "D") {
          // Spotted Dino Egg Mystery Block!
          const dColor = ch === "E" ? DINO_TYPES[st.defaultDino].body : DINO_TYPES[st.secondDino].body;
          ctx.fillStyle = "#f8fafc";
          drawRoundedRect(sx + 2, sy + 2, TILE - 4, TILE - 4, 14);
          ctx.fill();
          ctx.strokeStyle = dColor;
          ctx.lineWidth = 4;
          ctx.stroke();
          ctx.fillStyle = dColor;
          ctx.beginPath();
          ctx.arc(sx + 20, sy + 22, 8, 0, Math.PI * 2);
          ctx.arc(sx + 44, sy + 28, 10, 0, Math.PI * 2);
          ctx.arc(sx + 26, sy + 46, 7, 0, Math.PI * 2);
          ctx.fill();
        } else if (ch === "U") {
          ctx.fillStyle = "#475569";
          drawRoundedRect(sx + 2, sy + 2, TILE - 4, TILE - 4, 10);
          ctx.fill();
        } else if (ch === "P" || ch === "p") {
          const pGrad = ctx.createLinearGradient(sx, sy, sx + TILE, sy);
          pGrad.addColorStop(0, "#059669");
          pGrad.addColorStop(0.35, "#34d399");
          pGrad.addColorStop(1, "#065f46");
          ctx.fillStyle = pGrad;
          drawRoundedRect(sx - 2, sy, TILE + 4, TILE, 6);
          ctx.fill();
        } else if (ch === "F") {
          // Tropical Dino Berry Bush!
          ctx.fillStyle = "#15803d";
          ctx.beginPath();
          ctx.arc(sx + 32, sy + 38, 24, 0, Math.PI * 2);
          ctx.fill();
          ctx.fillStyle = "#f43f5e";
          ctx.beginPath();
          ctx.arc(sx + 24, sy + 30, 9, 0, Math.PI * 2);
          ctx.arc(sx + 42, sy + 34, 9, 0, Math.PI * 2);
          ctx.fill();
        } else if (ch === "C") {
          const spinW = Math.max(6, Math.abs(Math.cos(globalTick * 0.08)) * 20);
          ctx.fillStyle = "#fde047";
          ctx.beginPath();
          ctx.ellipse(sx + 32, sy + 32, spinW, 20, 0, 0, Math.PI * 2);
          ctx.fill();
          ctx.strokeStyle = "#ca8a04";
          ctx.lineWidth = 3;
          ctx.stroke();
        } else if (ch === "J") {
          ctx.fillStyle = "#ef4444";
          drawRoundedRect(sx + 6, sy + 22, TILE - 12, 16, 6);
          ctx.fill();
          ctx.fillStyle = "#fde047";
          ctx.fillRect(sx + 16, sy + 38, TILE - 32, 24);
        } else if (ch === "K") {
          ctx.fillStyle = "#e2e8f0";
          ctx.fillRect(sx + 28, sy - 32, 6, 96);
          ctx.fillStyle = "#3b82f6";
          ctx.beginPath();
          ctx.moveTo(sx + 34, sy - 32);
          ctx.lineTo(sx + 80, sy - 14);
          ctx.lineTo(sx + 34, sy + 6);
          ctx.fill();
        } else if (ch === "G") {
          ctx.fillStyle = "#f8fafc";
          ctx.fillRect(sx + 28, sy, 8, TILE);
          if (ty === 4) {
            ctx.fillStyle = "#fbbf24";
            ctx.beginPath();
            ctx.arc(sx + 32, sy, 16, 0, Math.PI * 2);
            ctx.fill();
            ctx.fillStyle = "#10b981";
            ctx.beginPath();
            ctx.moveTo(sx + 36, sy + 8);
            ctx.lineTo(sx + 110, sy + 32);
            ctx.lineTo(sx + 36, sy + 56);
            ctx.fill();
          }
        }
      }
    }
  }

  function renderDino(x, y, dir, dinoId, animFrame, mouthFull, tongueLen, isFluttering) {
    const d = DINO_TYPES[dinoId] || DINO_TYPES.green;
    ctx.save();
    ctx.translate(x, y);
    if (dir < 0) ctx.scale(-1, 1);

    const legBob = Math.sin(animFrame * 2.4) * (isFluttering ? 12 : 7);

    // Chameleon Tongue (if active)
    if (tongueLen > 0) {
      ctx.strokeStyle = "#fb7185";
      ctx.lineWidth = 10;
      ctx.lineCap = "round";
      ctx.beginPath();
      ctx.moveTo(22, -32);
      ctx.lineTo(28 + tongueLen, -32);
      ctx.stroke();
      ctx.fillStyle = "#f43f5e";
      ctx.beginPath();
      ctx.arc(28 + tongueLen, -32, 9, 0, Math.PI * 2);
      ctx.fill();
    }

    // Feathered Wings for Blue Sky Rex!
    if (dinoId === "blue") {
      const wingAng = Math.sin(globalTick * 0.25) * 0.45;
      ctx.save();
      ctx.translate(-8, -36);
      ctx.rotate(wingAng);
      ctx.fillStyle = "#f8fafc";
      ctx.beginPath();
      ctx.ellipse(-18, -14, 24, 11, -0.4, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = "#93c5fd";
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.restore();
    }

    // Wagging Tail
    ctx.fillStyle = d.body;
    ctx.beginPath();
    ctx.moveTo(-12, -18);
    ctx.quadraticCurveTo(-38, -28 + Math.sin(animFrame) * 6, -34, -10);
    ctx.lineTo(-10, -8);
    ctx.fill();

    // Back Leg
    ctx.fillStyle = d.dark;
    drawRoundedRect(-14 - legBob * 0.5, -14, 14, 16, 6);
    ctx.fill();

    // Plump Dino Body & Belly
    ctx.fillStyle = d.body;
    ctx.beginPath();
    ctx.ellipse(0, -22, 24, 18, 0, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = d.belly;
    ctx.beginPath();
    ctx.ellipse(6, -19, 16, 12, 0.2, 0, Math.PI * 2);
    ctx.fill();

    // Red Saddle on Back
    ctx.fillStyle = d.saddle;
    ctx.beginPath();
    ctx.arc(-4, -34, 11, Math.PI, 0);
    ctx.fill();

    // Front Leg & Boot
    ctx.fillStyle = d.body;
    drawRoundedRect(4 + legBob * 0.5, -14, 14, 16, 6);
    ctx.fill();

    // Dorsal Spines
    ctx.fillStyle = d.crest;
    ctx.beginPath();
    ctx.arc(4, -52, 6, 0, Math.PI * 2);
    ctx.arc(-2, -44, 5, 0, Math.PI * 2);
    ctx.fill();

    // Dino Head & Big Snout (Puffed out if holding shell/fire!)
    ctx.fillStyle = d.body;
    ctx.beginPath();
    ctx.arc(12, -46, 16, 0, Math.PI * 2);
    ctx.ellipse(28, -42, mouthFull ? 20 : 15, mouthFull ? 16 : 12, 0, 0, Math.PI * 2);
    ctx.fill();

    if (mouthFull) {
      ctx.fillStyle = "rgba(251, 191, 36, 0.45)";
      ctx.beginPath();
      ctx.arc(28, -40, 12, 0, Math.PI * 2);
      ctx.fill();
    }

    // Expressive Eye
    ctx.fillStyle = "#ffffff";
    ctx.beginPath();
    ctx.ellipse(16, -54, 7, 9, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "#0f172a";
    ctx.beginPath();
    ctx.arc(18, -54, 4, 0, Math.PI * 2);
    ctx.fill();

    ctx.restore();
  }

  function renderLeoAndDino(camX) {
    if (player.invuln > 0 && Math.floor(player.invuln / 4) % 2 === 1) return;

    const cx = player.x + player.w / 2 - camX;
    const bottomY = player.y + player.h;

    ctx.save();
    ctx.translate(cx, bottomY);
    ctx.scale(player.squashX, player.squashY);

    const leoOffsetY = player.dino ? -30 : 0;

    // Draw Leo
    ctx.save();
    ctx.translate(player.dino ? -4 * player.dir : 0, leoOffsetY);
    if (player.dir < 0) ctx.scale(-1, 1);

    // Flowing Hero Scarf
    ctx.strokeStyle = player.starTimer > 0 ? `hsl(${(globalTick * 12) % 360}, 90%, 60%)` : "#ef4444";
    ctx.lineWidth = 8;
    ctx.lineCap = "round";
    ctx.beginPath();
    ctx.moveTo(-6, -34);
    ctx.quadraticCurveTo(-24, -36 + Math.sin(player.scarfWave) * 8, -36, -28 + Math.cos(player.scarfWave) * 6);
    ctx.stroke();

    const step = Math.sin(player.animFrame * 2.2) * 7;
    // Legs
    ctx.fillStyle = "#1d4ed8";
    drawRoundedRect(-10 - step * 0.5, -16, 11, 16, 5);
    ctx.fill();
    drawRoundedRect(2 + step * 0.5, -16, 11, 16, 5);
    ctx.fill();

    // Torso / Overalls
    const tunicColor =
      player.starTimer > 0
        ? `hsl(${(globalTick * 15) % 360}, 90%, 58%)`
        : player.power === 2
        ? "#f8fafc"
        : "#ef4444";
    ctx.fillStyle = tunicColor;
    drawRoundedRect(-13, -38, 26, 24, 9);
    ctx.fill();

    // Head
    ctx.fillStyle = "#fed7aa";
    ctx.beginPath();
    ctx.arc(0, -48, 14, 0, Math.PI * 2);
    ctx.fill();

    // Cap & Visor
    ctx.fillStyle = tunicColor;
    ctx.beginPath();
    ctx.arc(0, -51, 15, Math.PI, 0);
    ctx.fill();
    ctx.fillRect(0, -54, 21, 6);

    // Eyes
    ctx.fillStyle = "#0f172a";
    ctx.beginPath();
    ctx.ellipse(6, -47, 2.8, 4.5, 0, 0, Math.PI * 2);
    ctx.fill();

    ctx.restore();

    // Draw Rideable Dino underneath/in front of Leo!
    if (player.dino) {
      renderDino(
        0,
        0,
        player.dir,
        player.dino,
        player.animFrame,
        !!player.dinoMouth,
        player.tongueLen,
        player.flutterCooldown > 14
      );
    }

    ctx.restore();
  }

  function renderEntities(camX) {
    for (const e of entities) {
      if (!e.active) continue;
      const sx = e.x - camX;
      const sy = e.y;
      if (sx + e.w < -120 || sx > W + 120) continue;

      if (e.type === "platform") {
        ctx.fillStyle = "#fbbf24";
        drawRoundedRect(sx, sy, e.w, e.h, 10);
        ctx.fill();
        ctx.strokeStyle = "#b45309";
        ctx.lineWidth = 3;
        ctx.stroke();
      } else if (e.type === "dino_mount") {
        renderDino(sx + e.w / 2, sy + e.h, e.dir, e.dinoType, globalTick * 0.08, false, 0, false);
        // Floating "HOP ON!" indicator
        ctx.fillStyle = "#fde047";
        ctx.font = "800 18px sans-serif";
        ctx.textAlign = "center";
        ctx.fillText("▼ HOP ON!", sx + e.w / 2, sy - 14 + Math.sin(globalTick * 0.12) * 5);
      } else if (e.type === "mushroom" || e.type === "flower") {
        ctx.fillStyle = e.type === "flower" ? "#fb923c" : "#ef4444";
        ctx.beginPath();
        ctx.arc(sx + 24, sy + 22, 20, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = "#fef08a";
        ctx.beginPath();
        ctx.arc(sx + 18, sy + 18, 6, 0, Math.PI * 2);
        ctx.arc(sx + 31, sy + 20, 5, 0, Math.PI * 2);
        ctx.fill();
      } else if (e.type === "shell") {
        ctx.fillStyle = "#10b981";
        ctx.beginPath();
        ctx.ellipse(sx + 25, sy + 26, 24, 20, 0, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = "#fef08a";
        ctx.lineWidth = 4;
        ctx.stroke();
      } else if (e.type === "boss") {
        ctx.fillStyle = e.invuln > 0 && e.invuln % 4 < 2 ? "#f8fafc" : "#dc2626";
        drawRoundedRect(sx, sy, e.w, e.h, 24);
        ctx.fill();
        // Boss Crown & Horns
        ctx.fillStyle = "#fbbf24";
        ctx.beginPath();
        ctx.moveTo(sx + 20, sy);
        ctx.lineTo(sx + 40, sy - 28);
        ctx.lineTo(sx + 60, sy);
        ctx.lineTo(sx + 80, sy - 28);
        ctx.lineTo(sx + 100, sy);
        ctx.fill();
        // Boss HP Bar
        ctx.fillStyle = "rgba(0,0,0,0.7)";
        ctx.fillRect(sx, sy - 46, e.w, 12);
        ctx.fillStyle = "#22c55e";
        ctx.fillRect(sx + 2, sy - 44, (e.w - 4) * (e.hp / e.maxHp), 8);
      } else {
        // Walker / Koopa / Flyer / Pokey
        ctx.fillStyle = e.type === "koopa" ? "#22c55e" : e.type === "flyer" ? "#ec4899" : e.type === "pokey" ? "#eab308" : "#a855f7";
        drawRoundedRect(sx, sy, e.w, e.h, 18);
        ctx.fill();
        if (e.type === "flyer") {
          ctx.fillStyle = "#f8fafc";
          ctx.beginPath();
          ctx.ellipse(sx + e.w / 2, sy + 6, 22, 10, Math.sin(globalTick * 0.25) * 0.4, 0, Math.PI * 2);
          ctx.fill();
        }
        ctx.fillStyle = "#fff";
        ctx.beginPath();
        ctx.arc(sx + 16, sy + 20, 6, 0, Math.PI * 2);
        ctx.arc(sx + 34, sy + 20, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = "#0f172a";
        ctx.beginPath();
        ctx.arc(sx + 15, sy + 20, 3, 0, Math.PI * 2);
        ctx.arc(sx + 33, sy + 20, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  function renderHUD() {
    const st = STAGES[currentStage];
    ctx.save();
    ctx.fillStyle = "rgba(8, 12, 20, 0.72)";
    drawRoundedRect(28, 22, W - 56, 66, 16);
    ctx.fill();
    ctx.strokeStyle = "rgba(251, 191, 36, 0.4)";
    ctx.lineWidth = 2;
    ctx.stroke();

    ctx.fillStyle = "#f8fafc";
    ctx.font = "800 26px sans-serif";
    ctx.textAlign = "left";
    ctx.fillText(`LEO x${lives}`, 56, 64);
    ctx.fillStyle = "#fde047";
    ctx.fillText(`COINS: ${String(coins).padStart(2, "0")}`, 230, 64);
    ctx.fillStyle = "#f8fafc";
    ctx.fillText(`SCORE: ${String(score).padStart(6, "0")}`, 440, 64);
    ctx.fillStyle = "#38bdf8";
    ctx.fillText(`STAGE ${st.code}: ${st.title}`, 760, 64);

    // Dino Companion Status Badge on HUD
    ctx.textAlign = "right";
    if (player.dino) {
      const d = DINO_TYPES[player.dino];
      ctx.fillStyle = d.body;
      const mouthMsg = player.dinoMouth ? " [MOUTH FULL — PRESS Z TO SPIT!]" : ` [${d.abilityText}]`;
      ctx.fillText(`DINO: ${d.name.toUpperCase()}${mouthMsg}`, W - 56, 64);
    } else {
      ctx.fillStyle = "#94a3b8";
      ctx.fillText("HIT SPOTTED EGG BLOCKS TO HATCH A DINO!", W - 56, 64);
    }

    if (bannerTimer > 0 && bannerText) {
      ctx.fillStyle = "rgba(8, 12, 20, 0.85)";
      drawRoundedRect(W / 2 - 460, 112, 920, 56, 999);
      ctx.fill();
      ctx.strokeStyle = "#fbbf24";
      ctx.stroke();
      ctx.fillStyle = "#fde047";
      ctx.font = "900 26px sans-serif";
      ctx.textAlign = "center";
      ctx.fillText(bannerText, W / 2, 149);
    }

    if (gameState === "title" || gameState === "victory") {
      ctx.fillStyle = "rgba(8, 12, 20, 0.78)";
      drawRoundedRect(W / 2 - 520, H / 2 - 230, 1040, 460, 28);
      ctx.fill();
      ctx.strokeStyle = "#fbbf24";
      ctx.lineWidth = 4;
      ctx.stroke();

      ctx.textAlign = "center";
      ctx.fillStyle = "#fde047";
      ctx.font = "900 68px sans-serif";
      ctx.fillText(
        gameState === "victory" ? "DINO ISLAND SAVED!" : "SUPER LEO WORLD HD",
        W / 2,
        H / 2 - 115
      );
      ctx.fillStyle = "#34d399";
      ctx.font = "800 34px sans-serif";
      ctx.fillText("1080p 60FPS • 4 RIDEABLE DINOS • 8 HD WORLDS", W / 2, H / 2 - 52);

      ctx.fillStyle = "#e2e8f0";
      ctx.font = "700 25px sans-serif";
      ctx.fillText("Emerald Rex (Tongue & Flutter) • Blaze Rex (3x Fireballs)", W / 2, H / 2 + 18);
      ctx.fillText("Sky Rex (Winged Flight) • Quake Rex (Seismic Shockwave Stomp)", W / 2, H / 2 + 58);

      ctx.fillStyle = "#fbbf24";
      ctx.font = "900 32px sans-serif";
      ctx.fillText(
        `▶ TAP SCREEN OR PRESS ENTER / SPACE TO PLAY (STAGE ${st.code}) ◀`,
        W / 2,
        H / 2 + 150
      );
    }
    ctx.restore();
  }

  function renderFrame() {
    const st = STAGES[currentStage];
    const camX = cameraX;
    renderBackground(st, camX);
    renderTiles(st, camX);

    // Shockwaves
    shockwaves.forEach((sw) => {
      ctx.save();
      ctx.strokeStyle = sw.color;
      ctx.globalAlpha = sw.alpha;
      ctx.lineWidth = 8;
      ctx.beginPath();
      ctx.arc(sw.x - camX, sw.y, sw.r, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
    });

    // Fireballs
    fireballs.forEach((f) => {
      ctx.save();
      ctx.fillStyle = "#ef4444";
      ctx.beginPath();
      ctx.arc(f.x - camX, f.y, f.r, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = "#fde047";
      ctx.beginPath();
      ctx.arc(f.x - camX, f.y, f.r * 0.55, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    });

    renderEntities(camX);
    renderLeoAndDino(camX);

    // Particles & Floating Score Popups
    particles.forEach((p) => {
      ctx.save();
      ctx.globalAlpha = Math.max(0, p.life / p.maxLife);
      if (p.text) {
        ctx.fillStyle = p.color;
        ctx.font = "900 24px sans-serif";
        ctx.textAlign = "center";
        ctx.fillText(p.text, p.x - camX, p.y);
      } else {
        ctx.fillStyle = p.color;
        ctx.beginPath();
        ctx.arc(p.x - camX, p.y, p.size, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.restore();
    });

    renderHUD();
  }

  // =========================================================================
  // 10. KEYBOARD, SCREEN GESTURES, TILT STEERING, & TOOLBAR WIRING
  // =========================================================================
  const kbState = {};
  const touchBtnState = {};
  const gestureState = {};
  const tiltState = {};

  function recomputeKeys() {
    const actions = ["left", "right", "up", "down", "jump", "tongue", "bail", "start"];
    actions.forEach((a) => {
      keys[a] = !!(kbState[a] || touchBtnState[a] || gestureState[a] || tiltState[a]);
    });
  }

  const CODE_MAP = {
    ArrowLeft: "left",
    KeyA: "left",
    ArrowRight: "right",
    KeyD: "right",
    ArrowUp: "up",
    KeyW: "up",
    ArrowDown: "down",
    KeyS: "down",
    Space: "jump",
    KeyZ: "tongue",
    KeyJ: "tongue",
    ShiftLeft: "tongue",
    ShiftRight: "tongue",
    KeyE: "bail",
    KeyX: "bail",
    KeyK: "bail",
    Enter: "start"
  };

  window.addEventListener("keydown", (e) => {
    const act = CODE_MAP[e.code];
    if (act) {
      e.preventDefault();
      ensureAudio();
      kbState[act] = true;
      if (act === "up") kbState.jump = true;
      if (act === "jump" && gameState !== "playing") kbState.start = true;
      recomputeKeys();
    }
  });

  window.addEventListener("keyup", (e) => {
    const act = CODE_MAP[e.code];
    if (act) {
      e.preventDefault();
      kbState[act] = false;
      if (act === "up") kbState.jump = false;
      if (act === "jump") kbState.start = false;
      recomputeKeys();
    }
  });

  // Toolbar controls
  document.getElementById("stage-select").addEventListener("change", (e) => {
    ensureAudio();
    gameState = "playing";
    checkpointX = 0;
    loadStage(parseInt(e.target.value, 10), false);
  });

  document.getElementById("mute-btn").addEventListener("click", () => {
    sfxMuted = !sfxMuted;
    document.getElementById("mute-btn").textContent = sfxMuted ? "SFX: OFF" : "SFX: ON";
  });

  document.getElementById("music-btn").addEventListener("click", () => {
    ensureAudio();
    musicEnabled = !musicEnabled;
    document.getElementById("music-btn").textContent = musicEnabled ? "Music: ON" : "Music: OFF";
  });

  document.getElementById("fs-btn").addEventListener("click", () => {
    const el = document.getElementById("cabinet");
    if (!document.fullscreenElement) {
      (el.requestFullscreen || el.webkitRequestFullscreen).call(el);
    } else {
      document.exitFullscreen();
    }
  });

  let touchMode = "screen";
  document.getElementById("touch-btn").addEventListener("click", () => {
    if (touchMode === "screen") {
      touchMode = "buttons";
      document.body.classList.add("show-gamepad");
      document.getElementById("touch-btn").textContent = "Touch: BUTTONS";
    } else {
      touchMode = "screen";
      document.body.classList.remove("show-gamepad");
      document.getElementById("touch-btn").textContent = "Touch: SCREEN";
    }
  });

  // Tilt Steering
  let tiltEnabled = false;
  let tiltAttached = false;
  const tiltBtn = document.getElementById("tilt-btn");
  const tiltBar = document.getElementById("tilt-bar");
  const tiltDot = document.getElementById("tilt-dot");

  function onDeviceOrientation(e) {
    if (!tiltEnabled) return;
    let angle = (screen.orientation && screen.orientation.angle) || window.orientation || 0;
    let tilt = angle === 90 ? (e.beta || 0) : angle === -90 || angle === 270 ? -(e.beta || 0) : (e.gamma || 0);
    const clamped = Math.max(-25, Math.min(25, tilt));
    tiltDot.style.left = 50 + (clamped / 25) * 44 + "%";
    tiltState.left = tilt < -4.5;
    tiltState.right = tilt > 4.5;
    recomputeKeys();
  }

  tiltBtn.addEventListener("click", async () => {
    ensureAudio();
    if (tiltEnabled) {
      tiltEnabled = false;
      tiltBtn.textContent = "Tilt: OFF";
      tiltBtn.classList.remove("active-mode");
      tiltBar.classList.remove("visible");
      tiltState.left = false;
      tiltState.right = false;
      recomputeKeys();
      return;
    }
    if (typeof DeviceOrientationEvent !== "undefined" && typeof DeviceOrientationEvent.requestPermission === "function") {
      try {
        const res = await DeviceOrientationEvent.requestPermission();
        if (res !== "granted") return;
      } catch (_) {}
    }
    if (!tiltAttached) {
      window.addEventListener("deviceorientation", onDeviceOrientation, true);
      tiltAttached = true;
    }
    tiltEnabled = true;
    tiltBtn.textContent = "Tilt: ON";
    tiltBtn.classList.add("active-mode");
    tiltBar.classList.add("visible");
  });

  // On-Screen Direct Gesture Controls
  const gestureLayer = document.getElementById("gesture-layer");
  const stickBase = document.getElementById("stick-base");
  const stickKnob = document.getElementById("stick-knob");
  const actionRipple = document.getElementById("action-ripple");
  const gestureHint = document.getElementById("gesture-hint");
  const activeTouches = new Map();

  function syncGestures() {
    Object.keys(gestureState).forEach((k) => (gestureState[k] = false));
    let moveT = null;
    let actT = null;

    activeTouches.forEach((t) => {
      let dx = t.curX - t.startX;
      const dy = t.curY - t.startY;
      if (t.role === "action" && activeTouches.size === 1 && Math.abs(dx) > (tiltEnabled ? 28 : 18)) {
        t.role = "move";
      }
      if (t.role === "move") {
        moveT = t;
        // Dynamic floating thumbstick leash: base follows thumb within 26px so reversing direction is instant!
        const maxLeash = 26;
        if (dx < -maxLeash) {
          t.startX = t.curX + maxLeash;
          dx = -maxLeash;
        } else if (dx > maxLeash) {
          t.startX = t.curX - maxLeash;
          dx = maxLeash;
        }
        if (Math.abs(dx) > 5) {
          t.hasDragged = true;
        }

        if (dx < -5) {
          gestureState.left = true;
        } else if (dx > 5) {
          gestureState.right = true;
        } else if (!t.hasDragged) {
          // Only use tap-zone direction before the user starts sliding their thumb
          if (t.relX < 0.22) gestureState.left = true;
          else if (t.relX > 0.28) gestureState.right = true;
        }

        if (dy < -20) gestureState.jump = true;
        else if (dy > 24) gestureState.down = true;
      } else {
        actT = t;
        if (dy > 26) gestureState.down = true;
        else {
          gestureState.jump = true;
          gestureState.tongue = true;
        }
      }
    });

    if (moveT) {
      stickBase.style.display = "block";
      stickBase.style.left = moveT.startX + "px";
      stickBase.style.top = moveT.startY + "px";
      stickKnob.style.display = "block";
      stickKnob.style.left = moveT.curX + "px";
      stickKnob.style.top = moveT.curY + "px";
    } else {
      stickBase.style.display = "none";
      stickKnob.style.display = "none";
    }

    if (actT) {
      actionRipple.style.display = "block";
      actionRipple.style.left = actT.curX + "px";
      actionRipple.style.top = actT.curY + "px";
    } else {
      actionRipple.style.display = "none";
    }

    recomputeKeys();
  }

  gestureLayer.addEventListener(
    "touchstart",
    (e) => {
      e.preventDefault();
      ensureAudio();
      if (gestureHint) gestureHint.classList.add("hidden");
      if (gameState !== "playing") {
        gameState = "playing";
        loadStage(currentStage, false);
        return;
      }
      const rect = gestureLayer.getBoundingClientRect();
      for (let i = 0; i < e.changedTouches.length; i++) {
        const t = e.changedTouches[i];
        const x = t.clientX - rect.left;
        const y = t.clientY - rect.top;
        const relX = x / Math.max(1, rect.width);
        let hasMove = false;
        activeTouches.forEach((gt) => {
          if (gt.role === "move") hasMove = true;
        });
        const role = !tiltEnabled && relX < 0.54 && !hasMove ? "move" : "action";
        if (role === "action") useDinoTongueOrFire();
        activeTouches.set(t.identifier, { startX: x, startY: y, curX: x, curY: y, relX, role });
      }
      syncGestures();
    },
    { passive: false }
  );

  gestureLayer.addEventListener(
    "touchmove",
    (e) => {
      e.preventDefault();
      const rect = gestureLayer.getBoundingClientRect();
      for (let i = 0; i < e.changedTouches.length; i++) {
        const t = e.changedTouches[i];
        const st = activeTouches.get(t.identifier);
        if (st) {
          st.curX = t.clientX - rect.left;
          st.curY = t.clientY - rect.top;
        }
      }
      syncGestures();
    },
    { passive: false }
  );

  function endTouches(e) {
    e.preventDefault();
    for (let i = 0; i < e.changedTouches.length; i++) {
      activeTouches.delete(e.changedTouches[i].identifier);
    }
    syncGestures();
  }
  gestureLayer.addEventListener("touchend", endTouches, { passive: false });
  gestureLayer.addEventListener("touchcancel", endTouches, { passive: false });
  gestureLayer.addEventListener("mousedown", () => {
    ensureAudio();
    if (gameState !== "playing") {
      gameState = "playing";
      loadStage(currentStage, false);
    }
  });

  // Virtual Gamepad Buttons
  const touchRoot = document.getElementById("touch-controls");
  const allBtns = Array.from(touchRoot.querySelectorAll(".tbtn"));
  function syncPadBtns(pressedSet) {
    Object.keys(touchBtnState).forEach((k) => (touchBtnState[k] = false));
    allBtns.forEach((b) => {
      const on = pressedSet.has(b);
      b.classList.toggle("active", on);
      if (on) {
        (b.getAttribute("data-action") || "").split(",").forEach((a) => {
          touchBtnState[a.trim()] = true;
        });
      }
    });
    recomputeKeys();
  }
  touchRoot.addEventListener(
    "touchstart",
    (e) => {
      e.preventDefault();
      ensureAudio();
      const set = new Set();
      for (let i = 0; i < e.touches.length; i++) {
        const el = document.elementFromPoint(e.touches[i].clientX, e.touches[i].clientY);
        const btn = el && el.closest(".tbtn");
        if (btn) set.add(btn);
      }
      syncPadBtns(set);
    },
    { passive: false }
  );
  touchRoot.addEventListener(
    "touchmove",
    (e) => {
      e.preventDefault();
      const set = new Set();
      for (let i = 0; i < e.touches.length; i++) {
        const el = document.elementFromPoint(e.touches[i].clientX, e.touches[i].clientY);
        const btn = el && el.closest(".tbtn");
        if (btn) set.add(btn);
      }
      syncPadBtns(set);
    },
    { passive: false }
  );
  touchRoot.addEventListener("touchend", () => syncPadBtns(new Set()), { passive: false });

  // =========================================================================
  // 11. INITIALIZE & START 60FPS MAIN LOOP
  // =========================================================================
  player.dino = "green"; // Start Title Showcase riding Emerald Rex!
  loadStage(0, false);

  let lastTime = performance.now();
  let accum = 0;
  const STEP = 1000 / 60;

  function mainLoop(now) {
    let dt = now - lastTime;
    lastTime = now;
    if (dt > 100) dt = 100;
    accum += dt;
    while (accum >= STEP) {
      updateGame();
      Object.assign(prevKeys, keys);
      accum -= STEP;
    }
    renderFrame();
    requestAnimationFrame(mainLoop);
  }
  requestAnimationFrame(mainLoop);
})();
