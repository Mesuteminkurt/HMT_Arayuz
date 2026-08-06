<template>
  <div class="app-shell">
    <div class="bg-grid"></div>
    <aside class="sidebar">
      <div class="sidebar-brand">
        <div class="brand-icon"><img src="/logo.png" alt="Logo" style="width: 100%; height: 100%; object-fit: contain; border-radius: 50%;" /></div>
        <div><h2 class="brand-title" style="font-size: 0.95rem;">SUBÜ TETRA - HMT</h2><span class="brand-sub">Telemetri</span></div>
      </div>
      <nav class="sidebar-nav">
        <button :class="['nav-btn',{active:view==='dashboard'}]" @click="view='dashboard'">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/></svg>
          <span>Gösterge Paneli</span>
        </button>
        <button :class="['nav-btn',{active:view==='battery'}]" @click="view='battery'">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><rect x="1" y="6" width="18" height="12" rx="2"/><line x1="23" y1="10" x2="23" y2="14"/><line x1="7" y1="10" x2="7" y2="14"/><line x1="12" y1="10" x2="12" y2="14"/></svg>
          <span>Batarya Detay</span>
        </button>
        <button :class="['nav-btn',{active:view==='csv'}]" @click="view='csv'">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
          <span>Veri Kayıtları</span>
        </button>
      </nav>
      <div class="sidebar-footer">
        <div class="connection-status">
          <span class="status-dot" :class="connected?'online':'offline'"></span>
          <span>{{ connected ? 'Bağlı' : 'Bağlantı Yok' }}</span>
        </div>
        <div class="connection-status" style="margin-top:6px">
          <span class="status-dot" :class="isMqtt?'mqtt-on':'mqtt-off'"></span>
          <span>{{ isMqtt ? 'MQTT' : 'Simülasyon' }}</span>
        </div>
      </div>
    </aside>

    <main class="main-content">
      <header class="top-header">
        <div class="header-left"><h1>🕐 Telemetri İzleme</h1></div>
        <div class="header-right">
          <div class="live-badge" :class="isMqtt ? 'mqtt-badge' : 'sim-badge'">
            <span class="live-dot" :class="isMqtt ? '' : 'sim-dot'"></span>
            {{ isMqtt ? 'MQTT CANLI' : 'SİMÜLASYON' }}
          </div>
          <div class="header-time font-mono">{{ telemetry.time || '--:--:--' }}</div>
        </div>
      </header>

      <!-- Dashboard -->
      <section v-if="view==='dashboard'" class="dashboard-view">
        <div class="top-row">
          <div class="speed-hero">
            <div class="speed-ring">
              <svg viewBox="0 0 160 160">
                <circle cx="80" cy="80" r="70" fill="none" stroke="rgba(0,229,160,0.1)" stroke-width="8"/>
                <circle cx="80" cy="80" r="70" fill="none" stroke="url(#sg)" stroke-width="8" stroke-linecap="round" :stroke-dasharray="440" :stroke-dashoffset="440-(440*Math.min(Number(telemetry.speed)||0,100)/100)" transform="rotate(-90 80 80)" style="transition:stroke-dashoffset .8s cubic-bezier(.16,1,.3,1)"/>
                <defs><linearGradient id="sg" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" stop-color="#00e5a0"/><stop offset="100%" stop-color="#3b82f6"/></linearGradient></defs>
              </svg>
              <div class="speed-value">
                <span class="speed-num font-mono">{{ Number(telemetry.speed||0).toFixed(0) }}</span>
                <span class="speed-unit">km/h</span>
              </div>
            </div>
            <div class="speed-label">Araç Hızı</div>
          </div>
          <div class="soc-card glass-card">
            <div class="soc-top"><span class="soc-title">Batarya SOC</span><span class="soc-percent font-mono" :style="{color:socColor}">{{ telemetry.soc||0 }}%</span></div>
            <div class="soc-bar-track"><div class="soc-bar-fill" :style="{width:telemetry.soc+'%',background:socColor}"></div></div>
          </div>
        </div>

        <div class="telemetry-grid">
          <div v-for="(item,idx) in cardItems" :key="item.key" class="tele-card glass-card" :style="{animationDelay:(idx*0.05)+'s','--card-accent':item.color}">
            <div class="tele-card-header">
              <span class="tele-icon" v-html="item.icon"></span>
              <span class="tele-label">{{ item.label }}</span>
            </div>
            <div v-if="item.isDual" class="tele-value font-mono" style="display: flex; align-items: baseline; gap: 16px; font-size: 1.6rem; white-space: nowrap;">
              <span><span style="font-size: 0.55em; color: var(--text-muted); font-family: sans-serif; text-transform: lowercase; margin-right: 2px;">{{ item.label1 }}</span>{{ item.formatter ? item.formatter(telemetry[item.key1]) : (telemetry[item.key1] ?? '0.00') }}</span>
              <span><span style="font-size: 0.55em; color: var(--text-muted); font-family: sans-serif; text-transform: lowercase; margin-right: 2px;">{{ item.label2 }}</span>{{ item.formatter ? item.formatter(telemetry[item.key2]) : (telemetry[item.key2] ?? '0.00') }}</span>
            </div>
            <div v-else class="tele-value font-mono">
              {{ telemetry[item.key] ?? '--' }}<small class="tele-unit">{{ item.unit }}</small>
            </div>
            <div class="card-accent-line"></div>
          </div>
        </div>

        <!-- 4 Adet Sistem ve Uyarı Kutucuğu -->
        <div class="status-section">
          <div class="section-header-sm">
            <h3>Sistem ve Uyarı Durumları</h3>
          </div>
          <div class="status-grid">
            <!-- 1. Düşük Hücre Gerilimi -->
            <div class="status-card glass-card" :class="isLowCellV ? 'status-alert' : 'status-ok'">
              <div class="status-card-top">
                <div class="status-card-icon">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
                </div>
                <div class="status-badge" :class="isLowCellV ? 'badge-danger' : 'badge-success'">
                  {{ isLowCellV ? 'TEHLİKE' : 'NORMAL' }}
                </div>
              </div>
              <div class="status-card-title">Düşük Hücre Gerilimi</div>
              <div class="status-card-value font-mono">
                {{ isLowCellV ? '2.6V Altında Hücre Var!' : 'Tüm Hücreler Normal' }}
              </div>
              <div class="status-card-sub">Eşik: 2.600V · Min: {{ minCellV }}V</div>
            </div>

            <!-- 2. BMS Fan -->
            <div class="status-card glass-card" :class="isBmsFanActive ? 'status-active-fan' : 'status-neutral'">
              <div class="status-card-top">
                <div class="status-card-icon" :class="{'fan-spin': isBmsFanActive}">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M12 12A3 3 0 0 0 12 6A3 3 0 0 0 12 12M12 12A3 3 0 0 0 18 12A3 3 0 0 0 12 12M12 12A3 3 0 0 0 12 18A3 3 0 0 0 12 12M12 12A3 3 0 0 0 6 12A3 3 0 0 0 12 12"/></svg>
                </div>
                <div class="status-badge" :class="isBmsFanActive ? 'badge-info' : 'badge-muted'">
                  {{ isBmsFanActive ? 'ÇALIŞIYOR' : 'KAPALI' }}
                </div>
              </div>
              <div class="status-card-title">BMS Fan Durumu</div>
              <div class="status-card-value font-mono">
                {{ isBmsFanActive ? 'Soğutma Aktif (>30°C)' : 'Fan Kapalı (≤30°C)' }}
              </div>
              <div class="status-card-sub">Max Sıcaklık: {{ telemetry.bat_temp ?? '--' }}°C</div>
            </div>

            <!-- 3. BMS SPI -->
            <div class="status-card glass-card" :class="isBmsSpiOk ? 'status-ok' : 'status-alert'">
              <div class="status-card-top">
                <div class="status-card-icon">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 12h-4l-3 9L9 3l-3 9H2"/></svg>
                </div>
                <div class="status-badge" :class="isBmsSpiOk ? 'badge-success' : 'badge-danger'">
                  {{ isBmsSpiOk ? 'HABERLEŞME VAR' : 'HABERLEŞME YOK' }}
                </div>
              </div>
              <div class="status-card-title">BMS SPI Bağlantısı</div>
              <div class="status-card-value font-mono">
                {{ isBmsSpiOk ? 'Haberleşme Aktif (0)' : 'Haberleşme Yok (1)' }}
              </div>
              <div class="status-card-sub">Sinyal: {{ telemetry.bms_spi ?? 0 }}</div>
            </div>

            <!-- 4. Motor Sürücü Kontak -->
            <div class="status-card glass-card" :class="isMotorContactOn ? 'status-active' : 'status-neutral'">
              <div class="status-card-top">
                <div class="status-card-icon">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/></svg>
                </div>
                <div class="status-badge" :class="isMotorContactOn ? 'badge-success' : 'badge-muted'">
                  {{ isMotorContactOn ? 'KONTAK AÇILDI' : 'KONTAK KAPALI' }}
                </div>
              </div>
              <div class="status-card-title">Motor Sürücü Kontak</div>
              <div class="status-card-value font-mono">
                {{ isMotorContactOn ? 'Kontak Açık (1)' : 'Kontak Kapalı (0)' }}
              </div>
              <div class="status-card-sub">Sürücü Kontak: {{ isMotorContactOn ? '1' : '0' }}</div>
            </div>
          </div>
        </div>
      </section>

      <!-- Battery Detail -->
      <section v-if="view==='battery'" class="battery-view">
        <!-- Batarya Sıcaklıkları -->
        <div class="battery-section">
          <div class="section-header">
            <div class="section-icon temp-icon">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M14 14.76V3.5a2.5 2.5 0 00-5 0v11.26a4.5 4.5 0 105 0z"/></svg>
            </div>
            <div>
              <h2 class="section-title">Batarya Sıcaklıkları</h2>
              <span class="section-sub">7 Sensör · Max: <strong class="font-mono" :style="{color: maxBatTempColor}">{{ telemetry.bat_temp ?? '--' }}°C</strong></span>
            </div>
          </div>
          <div class="bat-temp-grid">
            <div v-for="i in 7" :key="'bt'+i" class="bat-temp-card glass-card">
              <div class="bat-temp-card-header">
                <span class="bat-temp-index">T{{ i }}</span>
                <span class="bat-temp-label">Sensör {{ i }}</span>
              </div>
              <div class="bat-temp-value font-mono" :style="{color: getBatTempColor(telemetry['bat_temp_'+i])}">
                {{ telemetry['bat_temp_'+i] ?? '--' }}<small>°C</small>
              </div>
              <div class="bat-temp-bar-track">
                <div class="bat-temp-bar-fill" :style="{width: getBatTempPercent(telemetry['bat_temp_'+i])+'%', background: getBatTempColor(telemetry['bat_temp_'+i])}"></div>
              </div>
              <div class="bat-temp-minmax">
                <span>20°C</span><span>50°C</span>
              </div>
            </div>
          </div>
        </div>

        <!-- Hücre Gerilimleri -->
        <div class="battery-section">
          <div class="section-header">
            <div class="section-icon volt-icon">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>
            </div>
            <div>
              <h2 class="section-title">Hücre Gerilimleri</h2>
              <span class="section-sub">21 Hücre · Min: <strong class="font-mono text-amber">{{ minCellV }}V</strong> · Max: <strong class="font-mono text-accent">{{ maxCellV }}V</strong> · Ort: <strong class="font-mono text-blue">{{ avgCellV }}V</strong></span>
            </div>
          </div>
          <div class="cell-volt-grid">
            <div v-for="i in 21" :key="'cv'+i" class="cell-volt-card glass-card" :class="getCellClass(telemetry['cell_v_'+i])">
              <div class="cell-volt-header">
                <span class="cell-volt-index">C{{ i }}</span>
              </div>
              <div class="cell-volt-value font-mono" :style="{color: getCellColor(telemetry['cell_v_'+i])}">
                {{ telemetry['cell_v_'+i] ?? '--' }}
              </div>
              <div class="cell-volt-unit">V</div>
              <div class="cell-volt-bar-track">
                <div class="cell-volt-bar-fill" :style="{width: getCellPercent(telemetry['cell_v_'+i])+'%', background: getCellColor(telemetry['cell_v_'+i])}"></div>
              </div>
            </div>
          </div>
        </div>
      </section>

      <!-- CSV -->
      <section v-if="view==='csv'" class="csv-view">
        <!-- Kayıt Kontrol Paneli -->
        <div class="glass-card rec-control-card">
          <div class="rec-control-header">
            <div class="rec-title-area">
              <h3>Veri Kayıt Kontrolü</h3>
              <div class="rec-status-badge" :class="isRecording ? 'recording' : 'idle'">
                <span class="rec-dot"></span>
                {{ isRecording ? 'KAYIT YAPILIYOR' : 'HAZIR' }}
              </div>
            </div>
            <div class="rec-actions">
              <button v-if="!isRecording" class="rec-btn rec-start" @click="startRecording">
                <svg viewBox="0 0 24 24" fill="currentColor" width="16" height="16"><circle cx="12" cy="12" r="8"/></svg>
                Kaydı Başlat
              </button>
              <button v-else class="rec-btn rec-stop" @click="stopRecording">
                <svg viewBox="0 0 24 24" fill="currentColor" width="16" height="16"><rect x="6" y="6" width="12" height="12" rx="2"/></svg>
                Kaydı Durdur
              </button>
            </div>
          </div>
          <div v-if="isRecording" class="rec-live-info">
            <div class="rec-stat"><span class="rec-stat-label">Dosya</span><span class="rec-stat-val font-mono">{{ recFileName }}</span></div>
            <div class="rec-stat"><span class="rec-stat-label">Kayıt Sayısı</span><span class="rec-stat-val font-mono">{{ recCount }}</span></div>
            <div class="rec-stat"><span class="rec-stat-label">Süre</span><span class="rec-stat-val font-mono">{{ recDuration }}</span></div>
          </div>
        </div>

        <!-- Canlı Veri Tablosu -->
        <div v-if="isRecording && liveRows.length > 0" class="glass-card csv-card">
          <div class="csv-top-bar">
            <h3>Canlı Kayıt Önizleme</h3>
            <div class="csv-info">Son {{ liveRows.length }} / {{ liveTotalRows }} kayıt</div>
          </div>
          <div class="csv-table-wrap">
            <table class="csv-table">
              <thead><tr><th v-for="h in liveHeaders" :key="h">{{ h }}</th></tr></thead>
              <tbody>
                <tr v-for="(row,i) in liveRows" :key="i">
                  <td v-for="h in liveHeaders" :key="h" class="font-mono">{{ row[h] }}</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>

        <!-- Kayıtlı Dosyalar -->
        <div class="glass-card csv-card">
          <div class="csv-top-bar">
            <h3>Kayıtlı Dosyalar</h3>
            <button class="refresh-btn" @click="fetchRecordings">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:16px;height:16px"><path d="M23 4v6h-6"/><path d="M1 20v-6h6"/><path d="M3.51 9a9 9 0 0114.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0020.49 15"/></svg>
              Yenile
            </button>
          </div>
          <div v-if="recordings.length === 0" class="empty-files">
            <p>Henüz kayıtlı dosya yok. Kaydı başlatarak veri toplamaya başlayın.</p>
          </div>
          <div v-else class="file-list">
            <div v-for="file in recordings" :key="file.name" class="file-item glass-card" @click="previewFile(file.name)">
              <div class="file-icon">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
              </div>
              <div class="file-info">
                <span class="file-name font-mono">{{ file.name }}</span>
                <span class="file-meta">{{ file.rowCount }} kayıt · {{ file.sizeFormatted }}</span>
              </div>
              <div class="file-actions" @click.stop>
                <button class="icon-btn" @click="downloadFile(file.name)" title="İndir">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
                </button>
                <button class="icon-btn icon-btn-danger" @click="deleteFile(file.name)" title="Sil">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6l-2 14H7L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V4a1 1 0 011-1h4a1 1 0 011 1v2"/></svg>
                </button>
              </div>
            </div>
          </div>
        </div>

        <!-- Dosya Önizleme -->
        <div v-if="previewData" class="glass-card csv-card">
          <div class="csv-top-bar">
            <h3>{{ previewFileName }} <small class="text-muted">({{ previewTotalRows }} kayıt)</small></h3>
            <button class="icon-btn" @click="previewData=null" title="Kapat">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>
            </button>
          </div>
          <div class="csv-table-wrap">
            <table class="csv-table">
              <thead><tr><th v-for="h in previewHeaders" :key="h">{{ h }}</th></tr></thead>
              <tbody>
                <tr v-for="(row,i) in previewData" :key="i">
                  <td v-for="h in previewHeaders" :key="h" class="font-mono">{{ row[h] }}</td>
                </tr>
                <tr v-if="previewData.length===0"><td :colspan="previewHeaders.length" class="empty-row">Veri yok</td></tr>
              </tbody>
            </table>
          </div>
        </div>
      </section>
    </main>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, computed, watch } from 'vue';
import axios from 'axios';

const API = '';
const view = ref('dashboard');
const telemetry = ref({});
const connected = ref(false);
let interval = null;
let csvInterval = null;

// Kayıt durumu
const isRecording = ref(false);
const recFileName = ref('');
const recCount = ref(0);
const recStartTime = ref(null);
const recDuration = ref('00:00:00');

// Canlı veri
const liveHeaders = ref([]);
const liveRows = ref([]);
const liveTotalRows = ref(0);

// Dosya listesi
const recordings = ref([]);

// Önizleme
const previewData = ref(null);
const previewHeaders = ref([]);
const previewFileName = ref('');
const previewTotalRows = ref(0);

const socColor = computed(() => {
  const s = Number(telemetry.value.soc || 0);
  if (s > 60) return '#00e5a0';
  if (s > 30) return '#f59e0b';
  return '#ef4444';
});

const isMqtt = computed(() => telemetry.value._source === 'mqtt');

const iconBolt = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>';
const iconThermo = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M14 14.76V3.5a2.5 2.5 0 00-5 0v11.26a4.5 4.5 0 105 0z"/></svg>';
const iconShield = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>';
const iconCircle = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><circle cx="12" cy="12" r="10"/><path d="M8 12h8M12 8v8"/></svg>';
const iconBattery = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><rect x="2" y="7" width="16" height="10" rx="2"/><line x1="22" y1="11" x2="22" y2="13"/><line x1="7" y1="11" x2="11" y2="11"/><line x1="9" y1="9" x2="9" y2="13"/></svg>';

// Direnç değerini okunabilir birime çevir (örn. 10000000 → "10 MΩ", 100000 → "100 kΩ")
function formatResistance(val) {
  const v = Number(val);
  if (isNaN(v) || v === 0) return '0 Ω';
  if (v >= 1000000) return (v / 1000000).toFixed(v % 1000000 === 0 ? 0 : 1) + ' MΩ';
  if (v >= 1000) return (v / 1000).toFixed(v % 1000 === 0 ? 0 : 1) + ' kΩ';
  return v + ' Ω';
}

const cardItems = [
  { key:'bat_v', label:'Batarya Gerilimi', unit:'V', color:'#00e5a0', icon:iconBolt },
  { key:'bat_a', label:'Batarya Akımı', unit:'A', color:'#3b82f6', icon:iconCircle },
  { key:'energy', label:'Kalan Enerji', unit:'Wh', color:'#10b981', icon:iconBattery },
  { key:'mot_temp', label:'Motor Sıcaklığı', unit:'°C', color:'#f59e0b', icon:iconThermo },
  { key:'bat_temp', label:'Batarya Sıcaklığı (Max)', unit:'°C', color:'#ef4444', icon:iconThermo },
  { key:'iso', label:'İzolasyon', color:'#a855f7', icon:iconShield, isDual: true, key1: 'iso_n', key2: 'iso_p', label1: 'n:', label2: 'p:', formatter: formatResistance },
  { key:'tank_temp', label:'Tank Sıcaklığı', unit:'°C', color:'#06b6d4', icon:iconThermo },
];

// 4 adet Sistem ve Uyarı durumu kontrolleri
const isLowCellV = computed(() => {
  for (let i = 1; i <= 21; i++) {
    const val = Number(telemetry.value['cell_v_' + i]);
    if (!isNaN(val) && val > 0 && val < 2.6) return true;
  }
  return false;
});

const isBmsFanActive = computed(() => {
  for (let i = 1; i <= 7; i++) {
    const val = Number(telemetry.value['bat_temp_' + i]);
    if (!isNaN(val) && val > 30) return true;
  }
  const maxT = Number(telemetry.value.bat_temp);
  return !isNaN(maxT) && maxT > 30;
});

const isBmsSpiOk = computed(() => {
  return Number(telemetry.value.bms_spi ?? 0) === 0;
});

const isMotorContactOn = computed(() => {
  return Number(telemetry.value.motor_contact ?? 0) === 1;
});

// Batarya sıcaklık renkleri
const maxBatTempColor = computed(() => {
  const t = Number(telemetry.value.bat_temp || 0);
  if (t < 35) return '#00e5a0';
  if (t < 42) return '#f59e0b';
  return '#ef4444';
});

const getBatTempColor = (val) => {
  const t = Number(val || 0);
  if (t < 35) return '#00e5a0';
  if (t < 42) return '#f59e0b';
  return '#ef4444';
};

const getBatTempPercent = (val) => {
  const t = Number(val || 0);
  return Math.min(100, Math.max(0, ((t - 20) / 30) * 100));
};

// Hücre gerilimi hesaplamaları
const minCellV = computed(() => {
  const vals = [];
  for (let i = 1; i <= 21; i++) {
    const v = Number(telemetry.value['cell_v_'+i] || 0);
    if (v > 0) vals.push(v);
  }
  return vals.length ? Math.min(...vals).toFixed(3) : '--';
});

const maxCellV = computed(() => {
  const vals = [];
  for (let i = 1; i <= 21; i++) {
    const v = Number(telemetry.value['cell_v_'+i] || 0);
    if (v > 0) vals.push(v);
  }
  return vals.length ? Math.max(...vals).toFixed(3) : '--';
});

const avgCellV = computed(() => {
  const vals = [];
  for (let i = 1; i <= 21; i++) {
    const v = Number(telemetry.value['cell_v_'+i] || 0);
    if (v > 0) vals.push(v);
  }
  return vals.length ? (vals.reduce((a,b)=>a+b,0)/vals.length).toFixed(3) : '--';
});

const getCellColor = (val) => {
  const v = Number(val || 0);
  if (v < 3.0) return '#ef4444';
  if (v < 3.5) return '#f59e0b';
  return '#00e5a0';
};

const getCellPercent = (val) => {
  const v = Number(val || 0);
  return Math.min(100, Math.max(0, ((v - 2.5) / 1.7) * 100));
};

const getCellClass = (val) => {
  const v = Number(val || 0);
  if (v < 3.0) return 'cell-danger';
  if (v < 3.5) return 'cell-warning';
  return '';
};

// Telemetri verisi çek
const fetchData = async () => {
  try {
    const res = await axios.get(`${API}/api/telemetry`);
    telemetry.value = res.data;
    connected.value = true;
  } catch { connected.value = false; }
};

// Kayıt durumunu sorgula
const fetchRecordingStatus = async () => {
  try {
    const res = await axios.get(`${API}/api/recording-status`);
    isRecording.value = res.data.isRecording;
    recCount.value = res.data.recordCount;
    recFileName.value = res.data.fileName || '';
    if (res.data.startTime) {
      recStartTime.value = new Date(res.data.startTime);
    }
  } catch {}
};

// Kaydı başlat
const startRecording = async () => {
  try {
    const res = await axios.post(`${API}/api/recording/start`);
    isRecording.value = true;
    recFileName.value = res.data.fileName;
    recStartTime.value = new Date(res.data.startTime);
    recCount.value = 0;
    startCsvPolling();
  } catch (e) {
    alert(e.response?.data?.error || 'Kayıt başlatılamadı');
  }
};

// Kaydı durdur
const stopRecording = async () => {
  try {
    await axios.post(`${API}/api/recording/stop`);
    isRecording.value = false;
    recStartTime.value = null;
    stopCsvPolling();
    fetchRecordings();
  } catch (e) {
    alert(e.response?.data?.error || 'Kayıt durdurulamadı');
  }
};

// Dosya listesini çek
const fetchRecordings = async () => {
  try {
    const res = await axios.get(`${API}/api/recordings`);
    recordings.value = res.data.files;
  } catch {}
};

// Canlı veri çek
const fetchLiveData = async () => {
  try {
    const res = await axios.get(`${API}/api/recording/live-data`);
    liveHeaders.value = res.data.headers;
    liveRows.value = res.data.rows;
    liveTotalRows.value = res.data.totalRows;
    recCount.value = res.data.totalRows;
  } catch {}
};

// Dosya önizle
const previewFile = async (fileName) => {
  try {
    const res = await axios.get(`${API}/api/recording-preview`, { params: { file: fileName } });
    previewHeaders.value = res.data.headers;
    previewData.value = res.data.rows;
    previewFileName.value = fileName;
    previewTotalRows.value = res.data.totalRows;
  } catch {}
};

// Dosya indir
const downloadFile = (fileName) => {
  // Backend'e (localhost:3000) doğrudan yönlendirme yaparak dosyanın tarayıcı tarafından 
  // kendi orijinal ismiyle (Content-Disposition: attachment) inmesini sağlıyoruz.
  window.location.href = `${API}/api/download-recording?file=${encodeURIComponent(fileName)}`;
};

// Dosya sil
const deleteFile = async (fileName) => {
  if (!confirm(`"${fileName}" dosyasını silmek istediğinize emin misiniz?`)) return;
  try {
    await axios.delete(`${API}/api/delete-recording`, { params: { file: fileName } });
    fetchRecordings();
    if (previewFileName.value === fileName) previewData.value = null;
  } catch (e) {
    alert(e.response?.data?.error || 'Dosya silinemedi');
  }
};

// CSV polling
const startCsvPolling = () => {
  stopCsvPolling();
  csvInterval = setInterval(fetchLiveData, 2000);
};
const stopCsvPolling = () => {
  if (csvInterval) { clearInterval(csvInterval); csvInterval = null; }
};

// Süre hesapla
let durationInterval = null;
const updateDuration = () => {
  if (!recStartTime.value) { recDuration.value = '00:00:00'; return; }
  const diff = Math.floor((Date.now() - recStartTime.value.getTime()) / 1000);
  const h = String(Math.floor(diff / 3600)).padStart(2, '0');
  const m = String(Math.floor((diff % 3600) / 60)).padStart(2, '0');
  const s = String(diff % 60).padStart(2, '0');
  recDuration.value = `${h}:${m}:${s}`;
};

watch(view, (v) => {
  if (v === 'csv') {
    fetchRecordings();
    fetchRecordingStatus().then(() => {
      if (isRecording.value) startCsvPolling();
    });
  } else {
    stopCsvPolling();
  }
});

onMounted(() => {
  fetchData();
  interval = setInterval(fetchData, 1000);
  durationInterval = setInterval(updateDuration, 1000);
});
onUnmounted(() => {
  if (interval) clearInterval(interval);
  if (durationInterval) clearInterval(durationInterval);
  stopCsvPolling();
});
</script>

<style>
.app-shell { display:flex; height:100vh; position:relative; overflow:hidden; background:var(--bg-deep); }
.bg-grid { position:fixed; inset:0; z-index:0; pointer-events:none;
  background-image:linear-gradient(rgba(0,229,160,0.03) 1px,transparent 1px),linear-gradient(90deg,rgba(0,229,160,0.03) 1px,transparent 1px);
  background-size:60px 60px; }

/* Sidebar */
.sidebar { width:var(--sidebar-width); min-width:var(--sidebar-width); height:100vh; display:flex; flex-direction:column;
  background:rgba(15,22,35,0.92); backdrop-filter:blur(24px); border-right:1px solid var(--border); z-index:10; padding:24px 16px; }
.sidebar-brand { display:flex; align-items:center; gap:12px; padding:0 4px 24px; border-bottom:1px solid var(--border); margin-bottom:24px; }
.brand-icon { width:36px; height:36px; }
.brand-icon svg { width:100%; height:100%; }
.brand-title { font-size:1rem; font-weight:700; letter-spacing:1.5px; color:var(--accent); margin:0; line-height:1.2; }
.brand-sub { font-size:0.7rem; color:var(--text-muted); letter-spacing:0.5px; text-transform:uppercase; }
.sidebar-nav { display:flex; flex-direction:column; gap:4px; flex:1; }
.nav-btn { display:flex; align-items:center; gap:12px; padding:11px 14px; border-radius:var(--radius-sm);
  background:transparent; border:1px solid transparent; color:var(--text-secondary); font-size:0.875rem; font-weight:500;
  cursor:pointer; transition:all var(--duration-fast) var(--ease-out); font-family:var(--font-sans); }
.nav-btn svg { width:20px; height:20px; flex-shrink:0; }
.nav-btn:hover { background:rgba(0,229,160,0.06); color:var(--text-primary); border-color:var(--border-accent); }
.nav-btn.active { background:rgba(0,229,160,0.1); color:var(--accent); border-color:rgba(0,229,160,0.25); }
.sidebar-footer { padding-top:16px; border-top:1px solid var(--border); }
.connection-status { display:flex; align-items:center; gap:8px; font-size:0.75rem; color:var(--text-muted); }
.status-dot { width:8px; height:8px; border-radius:50%; }
.status-dot.online { background:var(--accent); box-shadow:0 0 8px var(--accent-glow); animation:pulse-live 2s infinite; }
.status-dot.offline { background:var(--accent-red); }
.status-dot.mqtt-on { background:#3b82f6; box-shadow:0 0 8px rgba(59,130,246,0.4); animation:pulse-live 2s infinite; }
.status-dot.mqtt-off { background:var(--accent-amber); }

/* MQTT/Simulation badges */
.live-badge.sim-badge { color:var(--accent-amber); background:rgba(245,158,11,0.12); border-color:rgba(245,158,11,0.25); }
.live-badge.sim-badge .sim-dot { background:var(--accent-amber); }
.live-badge.mqtt-badge { color:#3b82f6; background:rgba(59,130,246,0.12); border-color:rgba(59,130,246,0.25); }
.live-badge.mqtt-badge .live-dot { background:#3b82f6; }

/* Main */
.main-content { flex:1; display:flex; flex-direction:column; overflow-y:auto; z-index:5; padding:24px 32px 32px; }

/* Header */
.top-header { display:flex; justify-content:space-between; align-items:center; margin-bottom:24px; padding-bottom:16px; border-bottom:1px solid var(--border); }
.header-left h1 { display:flex; align-items:center; gap:10px; font-size:1.3rem; font-weight:600; color:var(--text-primary); margin:0; }
.header-right { display:flex; align-items:center; gap:16px; }
.live-badge { display:flex; align-items:center; gap:6px; font-size:0.7rem; font-weight:700; letter-spacing:1px;
  color:var(--accent); padding:5px 12px; background:var(--accent-dim); border-radius:20px; border:1px solid rgba(0,229,160,0.2); }
.live-dot { width:7px; height:7px; border-radius:50%; background:var(--accent); animation:pulse-live 1.5s infinite; }
.header-time { font-size:1.05rem; color:var(--text-secondary); letter-spacing:1px; }

/* Glass Card */
.glass-card { background:var(--bg-card); backdrop-filter:blur(16px); border:1px solid var(--border); border-radius:var(--radius-lg);
  transition:all var(--duration-med) var(--ease-out); }
.glass-card:hover { background:var(--bg-card-hover); border-color:rgba(160,174,192,0.18); box-shadow:var(--shadow-card); }

/* Top Row — speed + soc side by side */
.top-row { display:flex; gap:16px; margin-bottom:18px; }

/* Speed */
.speed-hero { flex:1; display:flex; flex-direction:column; align-items:center; justify-content:center; padding:28px 20px 20px;
  background:linear-gradient(145deg,rgba(0,229,160,0.07),rgba(59,130,246,0.04),rgba(26,37,64,0.7));
  border:1px solid rgba(0,229,160,0.12); border-radius:var(--radius-xl); animation:fade-in-up .5s var(--ease-out) both; position:relative; overflow:hidden; }
.speed-hero::before { content:''; position:absolute; inset:0; background:radial-gradient(ellipse at center,rgba(0,229,160,0.06) 0%,transparent 70%); pointer-events:none; }
.speed-ring { position:relative; width:150px; height:150px; }
.speed-ring svg { width:100%; height:100%; }
.speed-value { position:absolute; inset:0; display:flex; flex-direction:column; align-items:center; justify-content:center; }
.speed-num { font-size:2.6rem; font-weight:800; color:var(--accent); line-height:1; letter-spacing:-1px; }
.speed-unit { font-size:0.7rem; color:var(--text-muted); margin-top:2px; letter-spacing:1px; }
.speed-label { font-size:0.75rem; color:var(--text-secondary); margin-top:10px; text-transform:uppercase; letter-spacing:2px; font-weight:500; }

/* SOC — compact */
.soc-card { flex:0 0 280px; padding:18px 20px; display:flex; flex-direction:column; justify-content:center;
  animation:fade-in-up .5s .08s var(--ease-out) both; }
.soc-top { display:flex; justify-content:space-between; align-items:center; margin-bottom:10px; }
.soc-title { font-size:0.75rem; color:var(--text-muted); text-transform:uppercase; letter-spacing:1.5px; }
.soc-percent { font-size:1.4rem; font-weight:700; }
.soc-bar-track { height:12px; border-radius:6px; background:rgba(160,174,192,0.08); overflow:hidden; }
.soc-bar-fill { height:100%; border-radius:6px; transition:width .8s var(--ease-out),background .5s; box-shadow:0 0 10px rgba(0,229,160,0.25); }

/* Telemetry Grid — bigger cards */
.telemetry-grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(250px,1fr)); gap:16px; }
.tele-card { padding:24px; position:relative; overflow:hidden; animation:fade-in-up .5s var(--ease-out) both; }
.tele-card:hover { transform:translateY(-2px); }
.tele-card-header { display:flex; align-items:center; gap:8px; margin-bottom:14px; }
.tele-icon { width:22px; height:22px; color:var(--card-accent,var(--accent)); display:flex; }
.tele-icon svg { width:100%; height:100%; }
.tele-label { font-size:0.8rem; color:var(--text-muted); font-weight:500; }
.tele-value { font-size:2rem; font-weight:700; color:var(--text-primary); line-height:1; }
.tele-unit { font-size:0.8rem; color:var(--text-muted); font-weight:400; margin-left:4px; }
.card-accent-line { position:absolute; bottom:0; left:0; right:0; height:3px; background:var(--card-accent,var(--accent)); opacity:0; transition:opacity var(--duration-fast); }
.tele-card:hover .card-accent-line { opacity:.7; }

/* Charts placeholder */
.placeholder-card { display:flex; flex-direction:column; align-items:center; justify-content:center; padding:60px; text-align:center; animation:fade-in-up .5s var(--ease-out) both; }
.placeholder-card h3 { font-size:1.2rem; color:var(--text-primary); margin-bottom:8px; }
.placeholder-card p { color:var(--text-muted); font-size:0.9rem; }

/* CSV View */
.csv-view { display:flex; flex-direction:column; gap:16px; }
.csv-card { padding:24px; animation:fade-in-up .5s var(--ease-out) both; }
.csv-top-bar { display:flex; justify-content:space-between; align-items:center; margin-bottom:12px; }
.csv-top-bar h3 { font-size:1.1rem; color:var(--text-primary); }
.csv-info { font-size:0.8rem; color:var(--text-muted); margin-bottom:16px; }
.csv-table-wrap { max-height:480px; overflow:auto; border-radius:var(--radius-sm); border:1px solid var(--border); }
.csv-table { width:100%; border-collapse:collapse; font-size:0.8rem; white-space:nowrap; }
.csv-table thead { position:sticky; top:0; z-index:2; }
.csv-table th { background:var(--bg-surface); color:var(--text-secondary); padding:10px 14px; text-align:left; font-weight:600;
  border-bottom:1px solid var(--border); font-size:0.7rem; text-transform:uppercase; letter-spacing:1px; }
.csv-table td { padding:7px 14px; border-bottom:1px solid var(--border); color:var(--text-primary); }
.csv-table tbody tr:hover { background:rgba(0,229,160,0.04); }
.empty-row { text-align:center; color:var(--text-muted); padding:32px !important; }

/* Recording Control Card */
.rec-control-card { padding:24px; animation:fade-in-up .4s var(--ease-out) both; }
.rec-control-header { display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:16px; }
.rec-title-area { display:flex; align-items:center; gap:16px; flex-wrap:wrap; }
.rec-title-area h3 { font-size:1.1rem; color:var(--text-primary); margin:0; }

.rec-status-badge { display:inline-flex; align-items:center; gap:8px; padding:5px 14px; border-radius:20px;
  font-size:0.7rem; font-weight:700; letter-spacing:1px; }
.rec-status-badge.idle { background:rgba(160,174,192,0.1); color:var(--text-muted); border:1px solid var(--border); }
.rec-status-badge.recording { background:rgba(239,68,68,0.12); color:#ef4444; border:1px solid rgba(239,68,68,0.3);
  animation:glow-rec 2s infinite; }
.rec-dot { width:8px; height:8px; border-radius:50%; }
.rec-status-badge.idle .rec-dot { background:var(--text-muted); }
.rec-status-badge.recording .rec-dot { background:#ef4444; animation:pulse-live 1.5s infinite; }

@keyframes glow-rec {
  0%,100% { box-shadow:0 0 8px rgba(239,68,68,0.15); }
  50% { box-shadow:0 0 20px rgba(239,68,68,0.3); }
}

.rec-btn { display:inline-flex; align-items:center; gap:8px; padding:10px 22px; border-radius:var(--radius-sm);
  font-weight:600; font-size:0.85rem; border:none; cursor:pointer; font-family:var(--font-sans);
  transition:all var(--duration-fast) var(--ease-out); }
.rec-btn svg { width:16px; height:16px; }
.rec-btn:hover { transform:translateY(-1px); }

.rec-start { background:linear-gradient(135deg,#ef4444,#dc2626); color:#fff; }
.rec-start:hover { box-shadow:0 8px 24px rgba(239,68,68,0.35); }
.rec-stop { background:rgba(239,68,68,0.15); color:#ef4444; border:1px solid rgba(239,68,68,0.3); }
.rec-stop:hover { background:rgba(239,68,68,0.25); box-shadow:0 4px 16px rgba(239,68,68,0.2); }

.rec-live-info { display:flex; gap:24px; margin-top:18px; padding-top:16px; border-top:1px solid var(--border); flex-wrap:wrap; }
.rec-stat { display:flex; flex-direction:column; gap:4px; }
.rec-stat-label { font-size:0.7rem; color:var(--text-muted); text-transform:uppercase; letter-spacing:1px; }
.rec-stat-val { font-size:0.95rem; color:var(--text-primary); font-weight:600; }

/* File List */
.file-list { display:flex; flex-direction:column; gap:8px; }
.file-item { display:flex; align-items:center; gap:16px; padding:14px 18px; cursor:pointer;
  transition:all var(--duration-fast) var(--ease-out); }
.file-item:hover { transform:translateX(4px); }
.file-icon { width:36px; height:36px; display:flex; align-items:center; justify-content:center; flex-shrink:0;
  background:rgba(0,229,160,0.08); border-radius:var(--radius-sm); color:var(--accent); }
.file-icon svg { width:20px; height:20px; }
.file-info { flex:1; display:flex; flex-direction:column; gap:2px; min-width:0; }
.file-name { font-size:0.85rem; color:var(--text-primary); font-weight:500; overflow:hidden; text-overflow:ellipsis; }
.file-meta { font-size:0.75rem; color:var(--text-muted); }
.file-actions { display:flex; gap:6px; flex-shrink:0; }

.icon-btn { display:flex; align-items:center; justify-content:center; width:34px; height:34px; border-radius:var(--radius-sm);
  background:rgba(160,174,192,0.06); border:1px solid var(--border); color:var(--text-secondary);
  cursor:pointer; transition:all var(--duration-fast) var(--ease-out); }
.icon-btn svg { width:16px; height:16px; }
.icon-btn:hover { background:rgba(0,229,160,0.1); color:var(--accent); border-color:var(--border-accent); }
.icon-btn-danger:hover { background:rgba(239,68,68,0.1); color:#ef4444; border-color:rgba(239,68,68,0.3); }

.empty-files { padding:40px 20px; text-align:center; }
.empty-files p { color:var(--text-muted); font-size:0.9rem; }

.refresh-btn { display:inline-flex; align-items:center; gap:8px; padding:8px 16px; border-radius:var(--radius-sm);
  background:rgba(160,174,192,0.06); border:1px solid var(--border); color:var(--text-secondary);
  font-size:0.82rem; font-weight:500; cursor:pointer; font-family:var(--font-sans);
  transition:all var(--duration-fast) var(--ease-out); }
.refresh-btn:hover { background:rgba(0,229,160,0.08); color:var(--accent); border-color:var(--border-accent); }

.download-btn { display:inline-flex; align-items:center; gap:8px; padding:10px 20px; border-radius:var(--radius-sm);
  background:linear-gradient(135deg,var(--accent),#00c98b); color:#0a0e1a; font-weight:600; font-size:0.85rem;
  border:none; cursor:pointer; font-family:var(--font-sans); transition:all var(--duration-fast) var(--ease-out); }
.download-btn:hover { transform:translateY(-1px); box-shadow:0 8px 24px rgba(0,229,160,0.3); }

/* Battery Detail View */
.battery-view { display:flex; flex-direction:column; gap:28px; }
.battery-section { animation:fade-in-up .5s var(--ease-out) both; }
.battery-section:nth-child(2) { animation-delay:.15s; }

.section-header { display:flex; align-items:center; gap:14px; margin-bottom:18px; }
.section-icon { width:44px; height:44px; border-radius:var(--radius-md); display:flex; align-items:center; justify-content:center; flex-shrink:0; }
.section-icon svg { width:22px; height:22px; }
.temp-icon { background:rgba(239,68,68,0.1); color:#ef4444; border:1px solid rgba(239,68,68,0.2); }
.volt-icon { background:rgba(0,229,160,0.1); color:#00e5a0; border:1px solid rgba(0,229,160,0.2); }
.section-title { font-size:1.2rem; font-weight:700; color:var(--text-primary); margin:0; line-height:1.3; }
.section-sub { font-size:0.8rem; color:var(--text-muted); }

/* Battery Temperature Cards */
.bat-temp-grid { display:grid; grid-template-columns:repeat(auto-fill, minmax(170px, 1fr)); gap:12px; }
.bat-temp-card { padding:18px; position:relative; overflow:hidden; transition:all var(--duration-fast) var(--ease-out); }
.bat-temp-card:hover { transform:translateY(-3px); box-shadow:0 8px 32px rgba(0,0,0,0.3); }
.bat-temp-card-header { display:flex; align-items:center; gap:8px; margin-bottom:12px; }
.bat-temp-index { display:inline-flex; align-items:center; justify-content:center; width:28px; height:28px;
  border-radius:8px; background:rgba(239,68,68,0.1); color:#ef4444; font-size:0.7rem; font-weight:800; letter-spacing:0.5px; font-family:var(--font-mono); }
.bat-temp-label { font-size:0.75rem; color:var(--text-muted); font-weight:500; }
.bat-temp-value { font-size:1.7rem; font-weight:700; line-height:1; margin-bottom:12px; transition:color .5s; }
.bat-temp-value small { font-size:0.7rem; color:var(--text-muted); font-weight:400; margin-left:2px; }
.bat-temp-bar-track { height:6px; border-radius:3px; background:rgba(160,174,192,0.08); overflow:hidden; }
.bat-temp-bar-fill { height:100%; border-radius:3px; transition:width .8s var(--ease-out), background .5s; box-shadow:0 0 8px rgba(0,229,160,0.2); }
.bat-temp-minmax { display:flex; justify-content:space-between; margin-top:4px; font-size:0.6rem; color:var(--text-muted); font-family:var(--font-mono); }

/* Cell Voltage Cards */
.cell-volt-grid { display:grid; grid-template-columns:repeat(7, 1fr); gap:10px; }
.cell-volt-card { padding:14px 12px; text-align:center; position:relative; overflow:hidden;
  transition:all var(--duration-fast) var(--ease-out); }
.cell-volt-card:hover { transform:translateY(-3px); box-shadow:0 8px 28px rgba(0,0,0,0.3); }
.cell-volt-card.cell-danger { border-color:rgba(239,68,68,0.35) !important; background:rgba(239,68,68,0.06) !important; }
.cell-volt-card.cell-warning { border-color:rgba(245,158,11,0.3) !important; background:rgba(245,158,11,0.04) !important; }
.cell-volt-header { margin-bottom:8px; }
.cell-volt-index { display:inline-flex; align-items:center; justify-content:center; width:30px; height:22px;
  border-radius:6px; background:rgba(0,229,160,0.08); color:var(--accent); font-size:0.65rem; font-weight:800; letter-spacing:0.5px; font-family:var(--font-mono); }
.cell-volt-value { font-size:1.25rem; font-weight:700; line-height:1; transition:color .5s; }
.cell-volt-unit { font-size:0.6rem; color:var(--text-muted); margin-top:2px; letter-spacing:1px; }
.cell-volt-bar-track { height:4px; border-radius:2px; background:rgba(160,174,192,0.08); overflow:hidden; margin-top:8px; }
.cell-volt-bar-fill { height:100%; border-radius:2px; transition:width .8s var(--ease-out), background .5s; }

/* Status & Alert Section */
.status-section { margin-top:24px; animation:fade-in-up .5s .2s var(--ease-out) both; }
.section-header-sm { margin-bottom:14px; }
.section-header-sm h3 { font-size:1.05rem; font-weight:600; color:var(--text-primary); }
.status-grid { display:grid; grid-template-columns:repeat(auto-fill, minmax(240px, 1fr)); gap:16px; }

.status-card { padding:18px 20px; display:flex; flex-direction:column; justify-content:space-between;
  border-radius:var(--radius-lg); position:relative; overflow:hidden; transition:all var(--duration-fast) var(--ease-out); }
.status-card:hover { transform:translateY(-2px); }

.status-card-top { display:flex; justify-content:space-between; align-items:center; margin-bottom:10px; }
.status-card-icon { width:36px; height:36px; border-radius:var(--radius-sm); display:flex; align-items:center; justify-content:center; }
.status-card-icon svg { width:20px; height:20px; }

.status-card-title { font-size:0.85rem; font-weight:600; color:var(--text-primary); margin-bottom:4px; }
.status-card-value { font-size:0.95rem; font-weight:700; margin-bottom:4px; }
.status-card-sub { font-size:0.75rem; color:var(--text-muted); }

.status-badge { font-size:0.65rem; font-weight:800; letter-spacing:0.5px; padding:3px 8px; border-radius:12px; font-family:var(--font-mono); }

/* Status card themes */
.status-ok { border-color:rgba(0,229,160,0.2) !important; }
.status-ok .status-card-icon { background:rgba(0,229,160,0.1); color:var(--accent); }
.status-ok .status-card-value { color:var(--accent); }

.status-alert { border-color:rgba(239,68,68,0.35) !important; background:rgba(239,68,68,0.06) !important; }
.status-alert .status-card-icon { background:rgba(239,68,68,0.15); color:#ef4444; }
.status-alert .status-card-value { color:#ef4444; }

.status-active-fan { border-color:rgba(59,130,246,0.3) !important; background:rgba(59,130,246,0.06) !important; }
.status-active-fan .status-card-icon { background:rgba(59,130,246,0.15); color:#3b82f6; }
.status-active-fan .status-card-value { color:#3b82f6; }

.status-active { border-color:rgba(0,229,160,0.3) !important; background:rgba(0,229,160,0.05) !important; }
.status-active .status-card-icon { background:rgba(0,229,160,0.15); color:var(--accent); }
.status-active .status-card-value { color:var(--accent); }

.status-neutral { border-color:var(--border) !important; }
.status-neutral .status-card-icon { background:rgba(160,174,192,0.08); color:var(--text-muted); }
.status-neutral .status-card-value { color:var(--text-secondary); }

/* Badges */
.badge-success { background:rgba(0,229,160,0.15); color:var(--accent); border:1px solid rgba(0,229,160,0.3); }
.badge-danger { background:rgba(239,68,68,0.15); color:#ef4444; border:1px solid rgba(239,68,68,0.3); }
.badge-info { background:rgba(59,130,246,0.15); color:#3b82f6; border:1px solid rgba(59,130,246,0.3); }
.badge-muted { background:rgba(160,174,192,0.1); color:var(--text-muted); border:1px solid var(--border); }

/* Fan animation */
@keyframes spin { 0% { transform:rotate(0deg); } 100% { transform:rotate(360deg); } }
.fan-spin svg { animation:spin 1.5s linear infinite; }

@media(max-width:1200px){
  .cell-volt-grid{grid-template-columns:repeat(4,1fr)}
}
@media(max-width:768px){
  .sidebar{display:none}
  .main-content{padding:16px}
  .top-row{flex-direction:column}
  .soc-card{flex:unset}
  .telemetry-grid{grid-template-columns:repeat(2,1fr);gap:10px}
  .rec-control-header{flex-direction:column;align-items:flex-start}
  .rec-live-info{flex-direction:column;gap:12px}
  .bat-temp-grid{grid-template-columns:repeat(2,1fr)}
  .cell-volt-grid{grid-template-columns:repeat(3,1fr)}
  .status-grid{grid-template-columns:1fr}
}
</style>