const mqtt = require('mqtt');

const client = mqtt.connect('mqtt://subutetrahmt2.cloud.shiftr.io:1883', {
    username: 'subutetrahmt2',
    password: 'pPXOqugkEF24x0dH'
});

client.on('connect', () => {
    console.log('Bağlandı, test verisi gönderiliyor...');
    
    const testData = {
        speed: 78,
        bat_v: 96.2,
        bat_a: 14.8,
        energy: 1985.4,
        soc: 84,
        mot_temp: 46.5,
        bat_temp_1: 32.5,
        bat_temp_2: 31.2,
        bat_temp_3: 34.8,
        bat_temp_4: 32.1,
        bat_temp_5: 30.5,
        bat_temp_6: 35.2,
        bat_temp_7: 33.0,
        iso_n: 0.05,
        iso_p: 0.02,
        tank_temp: 23.5,
        bms_spi: 0,         // 0 = Haberleşme var
        motor_contact: 0,   // 0 = Kontak kapalı
        cell_v_1: 3.72, cell_v_2: 3.68, cell_v_3: 3.71,
        cell_v_4: 3.69, cell_v_5: 3.73, cell_v_6: 3.70,
        cell_v_7: 3.67, cell_v_8: 3.71, cell_v_9: 3.69,
        cell_v_10: 3.72, cell_v_11: 3.68, cell_v_12: 3.70,
        cell_v_13: 3.71, cell_v_14: 3.69, cell_v_15: 3.73,
        cell_v_16: 3.67, cell_v_17: 3.70, cell_v_18: 3.72,
        cell_v_19: 3.68, cell_v_20: 3.71, cell_v_21: 3.69
    };

    client.publish('telemetry', JSON.stringify(testData), { qos: 1 }, (err) => {
        if (err) console.error('Hata:', err);
        else console.log('✅ Test verisi gönderildi!');
        client.end();
    });
});
