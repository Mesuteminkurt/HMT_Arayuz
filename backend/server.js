const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const mqtt = require('mqtt');
require('dotenv').config();

const app = express();
app.use(cors());
app.use(express.json());

// CSV kayıt dizini
const csvDir = path.join(__dirname, 'csv_records');
if (!fs.existsSync(csvDir)) {
    fs.mkdirSync(csvDir, { recursive: true });
}

// Kayıt durumu (Manuel — kullanıcı başlatır/durdurur)
let isRecording = false;
let recordingStartTime = null;
let currentCsvPath = null;
let recordCount = 0;

// Türkiye saati (GMT+3) için dosya ismi formatlayıcı
function getTurkeyTimestamp(date = new Date()) {
    const p = new Intl.DateTimeFormat('en-US', { 
        timeZone: 'Europe/Istanbul', 
        year: 'numeric', month: '2-digit', day: '2-digit', 
        hour: '2-digit', minute: '2-digit', second: '2-digit', 
        hour12: false 
    }).formatToParts(date).reduce((acc, part) => { 
        acc[part.type] = part.value; 
        return acc; 
    }, {});
    return `${p.year}-${p.month}-${p.day}-${p.hour}-${p.minute}-${p.second}`;
}

// ============================================================
// OTOMATİK CSV KAYDI (Hidromobil Yarışma Formatı)
// MQTT'den ilk veri geldiğinde otomatik başlar
// Format: zaman_ms;hiz_kmh;T_bat_C;T_tank_C;V_bat_C;kalan_enerji_Wh
// ============================================================
const autoCsvDir = path.join(__dirname, 'csv_auto');
if (!fs.existsSync(autoCsvDir)) {
    fs.mkdirSync(autoCsvDir, { recursive: true });
}

const AUTO_CSV_HEADERS = 'zaman_ms;hiz_kmh;T_bat_C;T_tank_C;V_bat_C;kalan_enerji_Wh';
let autoRecording = false;
let autoRecordStartMs = null;   // Date.now() referans zamanı
let autoCsvPath = null;
let autoRecordCount = 0;
let autoLastWriteMs = 0;        // Son yazma zamanı (5s kontrolü için)
let autoLastMs = -1;            // Son gelen ms değeri (araç reset algılama için)

// Tüm telemetri alanları için CSV header
const CSV_HEADERS = [
    'Tarih', 'Zaman', 'Hiz_kmh', 'Bat_Sicaklik_Max_C', 'Bat_Voltaj_V', 'Bat_Akim_A',
    'SOC_%', 'Hesaplanan_Enerji_Wh', 'Telemetri_Enerji_Wh', 'Tank_Sicaklik_C',
    'BMS_SPI_Hata', 'Motor_Kontak', 'ISO_N', 'ISO_P'
];
for (let i = 1; i <= 7; i++) CSV_HEADERS.push(`Bat_Sic_Sens_${i}_C`);
for (let i = 1; i <= 32; i++) CSV_HEADERS.push(`Hucre_Gerilim_${i}_V`);

// Başlangıç değerleri (Veri gelene kadar her şey 0)
let currentData = {
    bat_v: 0, bat_a: 0, soc: 0,
    speed: 0, tank_temp: 0, energy: 0, time: "",
    // Uyarı alanları
    bms_spi: 0,         // 0 = haberleşme var, 1 = haberleşme yok
    motor_contact: 0,   // 1 = kontak açıldı, 0 = kontak kapalı
    // 32 adet hücre gerilimi
    cell_v_1: 0, cell_v_2: 0, cell_v_3: 0, cell_v_4: 0,
    cell_v_5: 0, cell_v_6: 0, cell_v_7: 0, cell_v_8: 0,
    cell_v_9: 0, cell_v_10: 0, cell_v_11: 0, cell_v_12: 0,
    cell_v_13: 0, cell_v_14: 0, cell_v_15: 0, cell_v_16: 0,
    cell_v_17: 0, cell_v_18: 0, cell_v_19: 0, cell_v_20: 0,
    cell_v_21: 0, cell_v_22: 0, cell_v_23: 0, cell_v_24: 0,
    cell_v_25: 0, cell_v_26: 0, cell_v_27: 0, cell_v_28: 0,
    cell_v_29: 0, cell_v_30: 0, cell_v_31: 0, cell_v_32: 0,
    // EYS ve H2 değerleri
    eys_current: 0,
    h2: 0
};

// ============================================================
// MQTT BAĞLANTISI (shiftr.io)
// ============================================================
let mqttConnected = false;
let mqttLastMessage = null;
let dataSource = 'simulation'; // 'mqtt' veya 'simulation'

const MQTT_BROKER = 'mqtt://subutetrahmt2.cloud.shiftr.io:1883';
const MQTT_USERNAME = 'subutetrahmt2';
const MQTT_PASSWORD = 'pPXOqugkEF24x0dH';
// İki topic'i de dinlemek için (telemetri ana verileri ve hücreler)
const MQTT_TOPICS = ['hmt_telemetry', 'hmt_cells', 'hmt_offline'];

let mqttClient = null;

if (MQTT_PASSWORD) {
    console.log(`🔌 MQTT broker'a bağlanılıyor: ${MQTT_BROKER}`);
    
    mqttClient = mqtt.connect(MQTT_BROKER, {
        username: MQTT_USERNAME,
        password: MQTT_PASSWORD,
        clientId: `hmt-backend-${Date.now()}`,
        reconnectPeriod: 5000,
        connectTimeout: 10000
    });

    mqttClient.on('connect', () => {
        mqttConnected = true;
        dataSource = 'mqtt';
        console.log(`✅ shiftr.io MQTT bağlantısı kuruldu!`);
        console.log(`📡 Topic dinleniyor: ${MQTT_TOPICS.join(', ')}`);
        mqttClient.subscribe(MQTT_TOPICS, { qos: 0 }, (err) => {
            if (err) console.error('❌ Subscribe hatası:', err);
        });
    });

    mqttClient.on('message', (topic, message) => {
        try {
            // STM32'den tek tırnaklı (') gelebilir, JSON formatı çift tırnak (") gerektirir
            const msgStr = message.toString().replace(/'/g, '"');
            const data = JSON.parse(msgStr);
            mqttLastMessage = new Date();

            if (topic === 'hmt_offline') {
                // Offline veride ms reset kontrolü
                const offlineMs = Number(data.ms || 0);
                const msReset = autoLastMs >= 0 && offlineMs < autoLastMs;
                if (msReset) {
                    console.log(`🔄 Offline veride araç reset algılandı! ms geri sardı: ${autoLastMs} → ${offlineMs}. Yeni CSV açılıyor...`);
                    autoCsvPath = null; // Yeni dosya açılmasını zorla
                }
                autoLastMs = offlineMs;

                // Kayıt aktif değilse: ms devam ediyorsa aynı CSV'ye devam et, reset olduysa veya hiç yoksa yeni aç
                if (!autoRecording) {
                    if (autoCsvPath) {
                        // ms devam ediyor, aynı CSV'ye devam et
                        autoRecording = true;
                        console.log(`♻️ Offline veri geldi, aynı CSV'ye devam ediliyor: ${path.basename(autoCsvPath)}`);
                    } else {
                        // İlk başlangıç veya ms reset sonrası: yeni CSV aç
                        autoRecordStartMs = Date.now();
                        autoLastWriteMs = 0;
                        autoRecordCount = 0;
                        const ts = getTurkeyTimestamp();
                        const autoFileName = `hidromobil_${ts}.csv`;
                        autoCsvPath = path.join(autoCsvDir, autoFileName);
                        fs.writeFileSync(autoCsvPath, '\uFEFF' + AUTO_CSV_HEADERS + '\n');
                        autoRecording = true;
                        console.log(`📝 Otomatik CSV kaydı başladı (Offline veri ile): ${autoFileName}`);
                    }
                }

                if (autoRecording && autoCsvPath) {
                    const formatDec = (val) => String(val).replace(/\./g, ',');
                    const row = [
                        data.ms,
                        formatDec(data.spd || 0),
                        formatDec(data.tmax || 0),
                        formatDec(data.tt || 0),
                        formatDec(data.v || 0),
                        formatDec(data.e || 0)
                    ].join(';');
                    fs.appendFileSync(autoCsvPath, row + '\n');
                }
                return; // Offline paketini sadece CSV'ye işliyoruz, anlık veriyi bozmuyoruz.
            }

            // === OTOMATİK CSV: ms bazlı araç reset algılama ===
            // Gelen ms değeri bir öncekinden küçükse → araç sayacı sıfırlanmış → yeni CSV aç
            const incomingMs = Number(data.ms || 0);
            const msReset = autoLastMs >= 0 && incomingMs < autoLastMs;
            if (msReset) {
                console.log(`🔄 Araç reset algılandı! ms geri sardı: ${autoLastMs} → ${incomingMs}. Yeni CSV açılıyor...`);
                autoCsvPath = null; // Yeni dosya açılmasını zorla
            }
            autoLastMs = incomingMs;

            // Kayıt aktif değilse: ms devam ediyorsa aynı CSV'ye devam et, reset olduysa veya hiç yoksa yeni aç
            if (!autoRecording) {
                if (autoCsvPath) {
                    // Timeout sonrası veri geldi ama ms devam ediyor → aynı CSV'ye devam
                    autoRecording = true;
                    console.log(`♻️ Veri akışı devam ediyor, aynı CSV'ye yazılıyor: ${path.basename(autoCsvPath)}`);
                } else {
                    // İlk başlangıç veya ms reset sonrası: yeni CSV aç
                    autoRecordStartMs = Date.now();
                    autoLastWriteMs = 0;
                    autoRecordCount = 0;
                    const ts = getTurkeyTimestamp();
                    const autoFileName = `hidromobil_${ts}.csv`;
                    autoCsvPath = path.join(autoCsvDir, autoFileName);
                    fs.writeFileSync(autoCsvPath, '\uFEFF' + AUTO_CSV_HEADERS + '\n');
                    autoRecording = true;
                    console.log(`📝 Otomatik CSV kaydı başladı: ${autoFileName}`);
                }
            }
            
            // Gelen veriyi currentData'ya aktar (Kısaltmaları uzun isimlere çevir)
            Object.keys(data).forEach(key => {
                if (key.startsWith('c') && !isNaN(key.substring(1))) {
                    currentData[`cell_v_${key.substring(1)}`] = (Number(data[key]) / 1000).toFixed(3);
                } else if (key.startsWith('t') && !isNaN(key.substring(1))) {
                    currentData[`bat_temp_${key.substring(1)}`] = data[key];
                } else if (key === 'soc') {
                    currentData.soc = (Number(data[key]) / 10).toFixed(1);
                } else if (key === 'spd') {
                    currentData.speed = data[key];
                } else if (key === 'v') {
                    currentData.bat_v = data[key];
                } else if (key === 'a') {
                    currentData.bat_a = data[key];
                } else if (key === 'e') {
                    currentData.energy = data[key];
                } else if (key === 'spi') {
                    currentData.bms_spi = data[key];
                } else if (key === 'mc') {
                    currentData.motor_contact = data[key];
                } else if (key === 'tt') {
                    currentData.tank_temp = data[key];
                } else if (key === 'in') {
                    currentData.iso_n = data[key];
                } else if (key === 'ip') {
                    currentData.iso_p = data[key];
                } else if (key === 'eyc') {
                    currentData.eys_current = data[key];
                } else if (key === 'h2') {
                    currentData.h2 = data[key];
                } else {
                    currentData[key] = data[key];
                }
            });
            
            // Zamanı güncelle
            currentData.time = new Date().toLocaleTimeString('tr-TR', { timeZone: 'Europe/Istanbul' });

            console.log(`📨 MQTT veri alındı: speed=${data.speed || '-'}, bat_v=${data.bat_v || '-'}, soc=${data.soc || '-'}`);
        } catch (err) {
            console.error('❌ MQTT mesaj parse hatası:', err.message);
        }
    });

    mqttClient.on('error', (err) => {
        console.error('❌ MQTT bağlantı hatası:', err.message);
    });

    mqttClient.on('offline', () => {
        mqttConnected = false;
        dataSource = 'simulation';
        console.log('⚠️ MQTT bağlantısı kesildi, simülasyon moduna geçildi');
    });

    mqttClient.on('reconnect', () => {
        console.log('🔄 MQTT yeniden bağlanılıyor...');
    });
} else {
    console.log('⚠️ MQTT_PASSWORD tanımlı değil, simülasyon modunda çalışılıyor');
}

// ============================================================
// DÜZENLİ İŞLEMLER (Her saniye)
// ============================================================
setInterval(() => {
    const now = new Date();
    currentData.time = now.toLocaleTimeString('tr-TR', { timeZone: 'Europe/Istanbul' });

    // Araç bağlantı kopma / kapanma kontrolü (Örn: Son veriden bu yana 6 saniye geçtiyse)
    let isVehicleOnline = true;
    if (dataSource === 'mqtt' && mqttConnected) {
        if (mqttLastMessage && (Date.now() - mqttLastMessage.getTime() > 6000)) {
            isVehicleOnline = false;
        }
    }

    // Araç kapalıysa veya veri gelmiyorsa
    if (!isVehicleOnline && dataSource === 'mqtt') {
        // Eski verilerin arayüzde donup kalmaması için tüm verileri sıfırla
        currentData.speed = 0;
        currentData.bat_a = 0;
        currentData.bat_v = 0;
        currentData.soc = 0;
        currentData.tank_temp = 0;
        currentData.energy = 0;
        currentData.iso_n = 0;
        currentData.iso_p = 0;
        currentData.eys_current = 0;
        currentData.h2 = 0;
        
        for (let i = 1; i <= 21; i++) {
            currentData[`cell_v_${i}`] = 0;
        }
        for (let i = 1; i <= 7; i++) {
            currentData[`bat_temp_${i}`] = 0;
        }

        // Otomatik kaydı durdur ama CSV dosyasını ve ms takibini koru
        // Böylece veri tekrar geldiğinde ms karşılaştırmasıyla aynı CSV'ye devam edebilir
        if (autoRecording) {
            console.log(`⏹️ Veri akışı durdu. Kayıt duraklatıldı (ms takibi korunuyor, autoLastMs=${autoLastMs}).`);
            autoRecording = false;
            // autoLastMs ve autoCsvPath KORUNUYOR → veri gelince ms'e bakılacak
        }
    }

    // Max batarya sıcaklığını hesapla
    let maxBatTemp = -999;
    for (let i = 1; i <= 7; i++) {
        const t = Number(currentData[`bat_temp_${i}`] || 0);
        if (t > 0 && t > maxBatTemp) maxBatTemp = t;
    }
    currentData.bat_temp = maxBatTemp !== -999 ? maxBatTemp : 0;

    // Excel'in sayıları tarih sanmaması için noktayı virgüle çeviren fonksiyon
    const formatDec = (val) => String(val).replace(/\./g, ',');

    // === MANUEL CSV KAYDI ===
    // Sadece araç online ise (veya simülasyon ise) manuel dosyaya yazmaya devam et
    if (isRecording && currentCsvPath && isVehicleOnline) {
        const timeWithMs = `${now.toLocaleTimeString('tr-TR', { timeZone: 'Europe/Istanbul' })}.${String(now.getMilliseconds()).padStart(3, '0')}`;

        const rowData = [
            now.toLocaleDateString('tr-TR', { timeZone: 'Europe/Istanbul' }),
            timeWithMs,
            formatDec(currentData.speed),
            formatDec(currentData.bat_temp),
            formatDec(currentData.bat_v),
            formatDec(currentData.bat_a),
            formatDec(currentData.soc),
            formatDec((currentData.soc / 100 * 2840).toFixed(0)), // Hesaplanan_Enerji_Wh
            formatDec(currentData.energy),
            formatDec(currentData.tank_temp),
            formatDec(currentData.bms_spi),
            formatDec(currentData.motor_contact),
            formatDec(currentData.iso_n),
            formatDec(currentData.iso_p)
        ];
        for (let i = 1; i <= 7; i++) rowData.push(formatDec(currentData[`bat_temp_${i}`] || 0));
        for (let i = 1; i <= 32; i++) rowData.push(formatDec(currentData[`cell_v_${i}`] || 0));

        const row = rowData.join(';');

        fs.appendFileSync(currentCsvPath, row + '\n');
        recordCount++;
    }

    // === OTOMATİK CSV KAYDI (Hidromobil Yarışma Formatı) ===
    if (autoRecording && autoCsvPath && isVehicleOnline && dataSource === 'mqtt') {
        const elapsedMs = currentData.ms || 0;

        // İlk kayıt, modemin resetlenmesi durumu veya son yazımdan bu yana 1 saniye geçtiyse yaz
        if (autoRecordCount === 0 || elapsedMs < autoLastWriteMs || (elapsedMs - autoLastWriteMs) >= 1000) {
            // Sıcaklık: bat_temp yoksa bat_temp_1 kullan (en yüksek)
            const batTemp = currentData.bat_temp || currentData.bat_temp_1 || 0;
            const tankTemp = currentData.tank_temp || 0;

            const row = [
                elapsedMs,
                formatDec(currentData.speed || 0),
                formatDec(batTemp),
                formatDec(tankTemp),
                formatDec(currentData.bat_v || 0),
                formatDec(currentData.energy || 0)
            ].join(';');

            fs.appendFileSync(autoCsvPath, row + '\n');
            autoLastWriteMs = elapsedMs;
            autoRecordCount++;
        }
    }
}, 1000);

// Telemetri verileri
app.get('/api/telemetry', (req, res) => res.json({
    ...currentData,
    _source: dataSource,
    _mqttConnected: mqttConnected
}));

// MQTT bağlantı durumu
app.get('/api/mqtt-status', (req, res) => {
    res.json({
        connected: mqttConnected,
        source: dataSource,
        broker: MQTT_BROKER,
        topic: MQTT_TOPIC,
        lastMessage: mqttLastMessage
    });
});

// Otomatik CSV kayıt durumu
app.get('/api/auto-recording-status', (req, res) => {
    res.json({
        autoRecording,
        autoRecordCount,
        elapsedMs: autoRecording ? Date.now() - autoRecordStartMs : 0,
        fileName: autoCsvPath ? path.basename(autoCsvPath) : null
    });
});

// Kayıt durumu
app.get('/api/recording-status', (req, res) => {
    res.json({
        isRecording,
        recordCount,
        startTime: recordingStartTime,
        fileName: currentCsvPath ? path.basename(currentCsvPath) : null
    });
});

// Kaydı başlat
app.post('/api/recording/start', (req, res) => {
    if (isRecording) {
        return res.status(400).json({ error: 'Kayıt zaten devam ediyor' });
    }

    const now = new Date();
    const timestamp = getTurkeyTimestamp(now);
    const fileName = `telemetri_${timestamp}.csv`;
    currentCsvPath = path.join(csvDir, fileName);
    recordCount = 0;
    recordingStartTime = now.toLocaleString('tr-TR', { timeZone: 'Europe/Istanbul' });

    // Header satırını yaz
    // UTF-8 BOM + Header (Excel uyumluluğu için)
    fs.writeFileSync(currentCsvPath, '\uFEFF' + CSV_HEADERS.join(';') + '\n');

    isRecording = true;
    res.json({
        message: 'Kayıt başlatıldı',
        fileName,
        startTime: recordingStartTime
    });
});

// Kaydı durdur
app.post('/api/recording/stop', (req, res) => {
    if (!isRecording) {
        return res.status(400).json({ error: 'Aktif kayıt yok' });
    }

    isRecording = false;
    const result = {
        message: 'Kayıt durduruldu',
        fileName: path.basename(currentCsvPath),
        totalRecords: recordCount,
        startTime: recordingStartTime,
        endTime: new Date().toLocaleString('tr-TR', { timeZone: 'Europe/Istanbul' })
    };

    recordingStartTime = null;
    res.json(result);
});

// Kayıtlı CSV dosyalarını listele (manuel + otomatik)
app.get('/api/recordings', (req, res) => {
    try {
        const allFiles = [];

        // Manuel kayıtlar (csv_records)
        if (fs.existsSync(csvDir)) {
            fs.readdirSync(csvDir)
                .filter(f => f.endsWith('.csv'))
                .forEach(f => {
                    const filePath = path.join(csvDir, f);
                    const stats = fs.statSync(filePath);
                    const content = fs.readFileSync(filePath, 'utf-8');
                    const lines = content.trim().split('\n').filter(l => l.trim());
                    allFiles.push({
                        name: f,
                        type: 'manuel',
                        size: stats.size,
                        sizeFormatted: formatFileSize(stats.size),
                        createdAt: stats.birthtime,
                        rowCount: Math.max(0, lines.length - 1)
                    });
                });
        }

        // Otomatik kayıtlar (csv_auto)
        if (fs.existsSync(autoCsvDir)) {
            fs.readdirSync(autoCsvDir)
                .filter(f => f.endsWith('.csv'))
                .forEach(f => {
                    const filePath = path.join(autoCsvDir, f);
                    const stats = fs.statSync(filePath);
                    const content = fs.readFileSync(filePath, 'utf-8');
                    const lines = content.trim().split('\n').filter(l => l.trim());
                    allFiles.push({
                        name: f,
                        type: 'auto',
                        size: stats.size,
                        sizeFormatted: formatFileSize(stats.size),
                        createdAt: stats.birthtime,
                        rowCount: Math.max(0, lines.length - 1)
                    });
                });
        }

        allFiles.sort((a, b) => new Date(b.createdAt) - new Date(a.createdAt));
        res.json({ files: allFiles });
    } catch (e) {
        res.json({ files: [] });
    }
});

// CSV dosyasını indir (her iki klasörü de kontrol et)
app.get('/api/download-recording', (req, res) => {
    const fileName = req.query.file;
    if (!fileName) return res.status(400).json({ error: 'Dosya adı belirtilmedi' });

    // Önce manuel klasörde ara, yoksa otomatik klasörde ara
    let filePath = path.join(csvDir, fileName);
    if (!fs.existsSync(filePath)) {
        filePath = path.join(autoCsvDir, fileName);
    }
    if (fs.existsSync(filePath)) {
        res.download(filePath, fileName, (err) => {
            if (err && !res.headersSent) {
                res.status(500).json({ error: 'Dosya indirilirken bir hata oluştu' });
            }
        });
    } else {
        res.status(404).json({ error: 'CSV dosyası bulunamadı' });
    }
});

// Belirli bir CSV dosyasının son satırlarını getir
app.get('/api/recording-preview', (req, res) => {
    try {
        const fileName = req.query.file;
        if (!fileName) return res.status(400).json({ error: 'Dosya adı belirtilmedi' });

        const filePath = path.join(csvDir, fileName);
        if (!fs.existsSync(filePath)) {
            return res.status(404).json({ error: 'Dosya bulunamadı' });
        }

        const content = fs.readFileSync(filePath, 'utf-8');
        // BOM karakterini temizle
        const cleanContent = content.replace(/^\uFEFF/, '');
        const lines = cleanContent.trim().split('\n').filter(l => l.trim());

        if (lines.length === 0) {
            return res.json({ headers: [], rows: [], totalRows: 0 });
        }

        const headers = lines[0].split(';');
        const dataLines = lines.slice(1);
        const last100 = dataLines.slice(-100);
        const rows = last100.map(line => {
            const values = line.split(';');
            const row = {};
            headers.forEach((h, i) => { row[h] = values[i] || ''; });
            return row;
        });

        res.json({ headers, rows, totalRows: dataLines.length });
    } catch (e) {
        res.status(500).json({ error: 'Dosya okunamadı' });
    }
});

// Aktif kaydın canlı verilerini getir
app.get('/api/recording/live-data', (req, res) => {
    if (!isRecording || !currentCsvPath || !fs.existsSync(currentCsvPath)) {
        return res.json({ headers: CSV_HEADERS, rows: [], totalRows: 0, isRecording: false });
    }

    try {
        const content = fs.readFileSync(currentCsvPath, 'utf-8');
        const cleanContent = content.replace(/^\uFEFF/, '');
        const lines = cleanContent.trim().split('\n').filter(l => l.trim());
        const headers = lines[0] ? lines[0].split(';') : CSV_HEADERS;
        const dataLines = lines.slice(1);
        const last50 = dataLines.slice(-50);
        const rows = last50.map(line => {
            const values = line.split(';');
            const row = {};
            headers.forEach((h, i) => { row[h] = values[i] || ''; });
            return row;
        });

        res.json({ headers, rows, totalRows: dataLines.length, isRecording: true });
    } catch (e) {
        res.json({ headers: CSV_HEADERS, rows: [], totalRows: 0, isRecording });
    }
});

// CSV dosyasını sil (her iki klasörü de kontrol et)
app.delete('/api/delete-recording', (req, res) => {
    const fileName = req.query.file;
    if (!fileName) return res.status(400).json({ error: 'Dosya adı belirtilmedi' });

    let filePath = path.join(csvDir, fileName);
    if (!fs.existsSync(filePath)) {
        filePath = path.join(autoCsvDir, fileName);
    }

    // Aktif manuel kayıt dosyasını silmeye çalışıyorsa engelle
    if (isRecording && currentCsvPath === filePath) {
        return res.status(400).json({ error: 'Aktif manuel kayıt dosyası silinemez. Önce kaydı durdurun.' });
    }

    // Aktif otomatik kayıt dosyasını silmeye çalışıyorsa engelle
    if (autoRecording && autoCsvPath === filePath) {
        return res.status(400).json({ error: 'Aktif otomatik kayıt dosyası silinemez.' });
    }

    if (fs.existsSync(filePath)) {
        fs.unlinkSync(filePath);
        res.json({ message: 'Dosya silindi' });
    } else {
        res.status(404).json({ error: 'Dosya bulunamadı' });
    }
});

function formatFileSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

// Serve static frontend files (if frontend is built into public folder)
const publicDir = path.join(__dirname, 'public');
if (fs.existsSync(publicDir)) {
    app.use(express.static(publicDir));
    // Catch-all route to serve index.html for Vue/Vite routing
    app.get(/.*/, (req, res) => {
        // Exclude API routes from catch-all
        if (!req.path.startsWith('/api/')) {
            res.sendFile(path.join(publicDir, 'index.html'));
        } else {
            res.status(404).json({ error: 'API route not found' });
        }
    });
}

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => console.log(`Backend ${PORT} portunda çalışıyor.`));